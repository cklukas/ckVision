// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_follower.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ckv::widgets {
namespace {
void add(std::uint64_t& value, std::uint64_t amount = 1) noexcept {
    value += std::min(amount, std::numeric_limits<std::uint64_t>::max() - value);
}
std::int64_t deadline(std::int64_t now, std::int64_t interval) noexcept {
    return interval > std::numeric_limits<std::int64_t>::max() - now
        ? std::numeric_limits<std::int64_t>::max() : now + interval;
}
}

bool LogFollowOptions::valid() const noexcept {
    return (follow == LogFollowMode::Path || follow == LogFollowMode::OpenFile) &&
        (start == LogStartMode::RecentTail || start == LogStartMode::Beginning || start == LogStartMode::Offset) &&
        tail_bytes > 0 && tail_bytes <= kLogBatchBytes && tail_lines > 0 && tail_lines <= kLogBatchBytes &&
        read_bytes > 0 && read_bytes <= kLogBatchBytes && records_per_step > 0 && records_per_step <= 256 && drain_bytes > 0 &&
        reconcile_nanos > 0 && retry_nanos > 0 && result_poll_nanos > 0;
}

LogFollower::LogFollower(LogModel& model, std::unique_ptr<LogFileSession> session, LogFollowOptions options)
    : model_(model), session_(std::move(session)), options_(options), generation_(model.generation()) {
    if (!session_ || !options.valid() || session_->read_limit() == 0)
        throw std::invalid_argument("Invalid log follower session or options");
    quantum_ = std::min({options_.read_bytes, model_.limits().batch_bytes, session_->read_limit(), kLogBatchBytes});
    path_.reserve(32768); request_.path.reserve(32768);
    line_starts_.resize(options_.tail_lines + 1);
}
LogFollower::~LogFollower() { stop(); }
std::size_t LogFollower::allocated_bytes() const noexcept {
    return sizeof(*this) + path_.capacity() + request_.path.capacity() +
        line_starts_.capacity() * sizeof(std::size_t);
}

bool LogFollower::start(std::string_view path, LogSourceId source, std::int64_t now) {
    const auto base_generation = std::max(generation_, model_.source() == source ? model_.generation() : 0);
    if (stopped_ || source == 0 || now < 0 || (started_ && now < last_now_) || path.size() > 32768 ||
        base_generation == std::numeric_limits<std::uint64_t>::max() ||
        revision_ == std::numeric_limits<std::uint64_t>::max()) return false;
    request_.path.assign(path);
    request_.revision = 1; request_.operation = LogFileOperation::Inspect;
    request_.follow = options_.follow; request_.bytes = quantum_;
    if (!request_.valid()) return false;
    model_.finish();
    path_.assign(path); generation_ = base_generation + 1;
    const auto previous_revision = status_.revision;
    status_ = {}; status_.source = source; status_.generation = generation_;
    status_.revision = previous_revision; add(status_.revision);
    status_.state = LogSourceState::Pending;
    started_ = true; last_now_ = now; startup_reconcile_ = true; model_started_ = false;
    unverified_change_ = false;
    awaiting_ = false; observed_.reset(); lost_handle_ = false; anchor_size_ = 0; probe_checked_ = 0;
    cursor_ = 0; tail_size_ = 0; buffer_ = Buffer::None; skip_prefix_ = false; skip_lf_ = false;
    next_ = Action::Inspect;
    const bool issued = issue(next_, now); status_.pending = awaiting_;
    return issued;
}

void LogFollower::warning(LogFollowNotice notice) noexcept {
    status_.notice = notice; status_.uncertain = true; add(status_.continuity_warnings);
}
void LogFollower::failure(LogSourceState state, std::uint32_t error, std::int64_t now) noexcept {
    status_.state = state; status_.native_error = error;
    awaiting_ = false; buffer_ = Buffer::None; next_ = Action::Idle;
    retry_at_ = deadline(now, options_.retry_nanos);
}
void LogFollower::restart(LogFollowNotice notice, const LogFileObservation& observation) noexcept {
    if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
        status_.notice = LogFollowNotice::Exhausted; failure(LogSourceState::Failed, 0, last_now_); return;
    }
    model_.finish(); ++generation_; status_.generation = generation_; warning(notice);
    observed_ = observation; cursor_ = 0; anchor_size_ = 0; probe_checked_ = 0; model_started_ = false;
    startup_reconcile_ = false; unverified_change_ = false; lost_handle_ = false; buffer_ = Buffer::None;
    skip_prefix_ = false; skip_lf_ = false; next_ = Action::Read;
}

bool LogFollower::issue(Action action, std::int64_t now) {
    if (action == Action::Idle) return false;
    if (revision_ == std::numeric_limits<std::uint64_t>::max()) {
        status_.notice = LogFollowNotice::Exhausted; failure(LogSourceState::Failed, 0, now); return false;
    }
    request_.revision = ++revision_; request_.follow = options_.follow;
    request_.path.clear(); pending_anchor_ = 0;
    if (action == Action::Inspect) {
        request_.operation = LogFileOperation::Inspect; request_.path.assign(path_);
        request_.offset = 0; request_.bytes = quantum_;
    } else {
        request_.operation = LogFileOperation::Read;
        if (action == Action::TailRead) {
            request_.offset = tail_read_offset_;
            request_.bytes = static_cast<std::size_t>(std::min<std::uint64_t>(quantum_, tail_end_ - tail_read_offset_));
        } else {
            pending_anchor_ = anchor_size_ - probe_checked_;
            request_.offset = cursor_ - anchor_size_ + probe_checked_;
            auto fresh = quantum_ - anchor_size_;
            if (action == Action::Drain) fresh = std::min(fresh, drain_remaining_);
            request_.bytes = fresh + pending_anchor_;
        }
    }
    if (!request_.valid()) {
        warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); return false;
    }
    expected_revision_ = request_.revision; pending_action_ = action;
    pending_offset_ = request_.offset; pending_bytes_ = request_.bytes;
    awaiting_ = true; next_ = Action::Idle; add(status_.requests);
    try { session_->request(request_); }
    catch (...) { warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); return false; }
    return true;
}

void LogFollower::receive(std::int64_t now) {
    if (packet_.revision != expected_revision_) { add(status_.stale_results); return; }
    awaiting_ = false;
    const auto expected_op = pending_action_ == Action::Inspect ? LogFileOperation::Inspect : LogFileOperation::Read;
    if (packet_.operation != expected_op || packet_.bytes().size() > pending_bytes_ ||
        (expected_op == LogFileOperation::Read && packet_.offset != pending_offset_)) {
        warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); return;
    }
    status_.watch = packet_.watch;
    if (packet_.state != LogSourceState::Following || !packet_.file) {
        lost_handle_ = lost_handle_ || pending_action_ == Action::Inspect;
        failure(packet_.state == LogSourceState::Following || packet_.state == LogSourceState::Pending
                    ? LogSourceState::Failed : packet_.state,
                packet_.native_error, now);
        return;
    }
    status_.state = LogSourceState::Following; status_.native_error = 0;
    if (packet_.rescan_required) warning(LogFollowNotice::NotificationLoss);
    const auto observation = *packet_.file;
    if (expected_op == LogFileOperation::Inspect) {
        reconcile_at_ = deadline(now, options_.reconcile_nanos);
        if (observed_) startup_reconcile_ = false;
        if (!observed_) {
            // A first successful open has no prior admitted file generation
            // to reconnect. Do not replay its startup tail on reconciliation.
            lost_handle_ = false;
            observed_ = observation;
            cursor_ = options_.start == LogStartMode::Offset ? options_.offset : 0;
            if (options_.start == LogStartMode::RecentTail) {
                cursor_ = observation.size > options_.tail_bytes ? observation.size - options_.tail_bytes : 0;
                tail_read_offset_ = cursor_; tail_end_ = observation.size; tail_size_ = 0;
                next_ = cursor_ < tail_end_ ? Action::TailRead : Action::Idle;
            } else next_ = cursor_ < observation.size ? Action::Read : Action::Idle;
            if (cursor_ > observation.size) { cursor_ = observation.size; warning(LogFollowNotice::Truncated); }
        } else if (lost_handle_) {
            add(status_.unread_skipped_bytes, observed_->size > cursor_ ? observed_->size - cursor_ : 0);
            restart(LogFollowNotice::Reconnected, observation);
        } else if (observed_->identity != observation.identity) {
            add(status_.unread_skipped_bytes, observed_->size > cursor_ ? observed_->size - cursor_ : 0);
            restart(LogFollowNotice::Rotated, observation);
        } else if (observation.size < cursor_) restart(LogFollowNotice::Truncated, observation);
        else {
            const bool version_changed = observation.version != observed_->version;
            unverified_change_ = version_changed && observation.size == observed_->size && cursor_ != 0;
            observed_ = observation;
            // A changed token at the same extent may be metadata-only. Probe
            // the boundary before assuming rewrite or restarting history.
            next_ = cursor_ < observation.size || version_changed ? Action::Read : Action::Idle;
        }
        return;
    }
    if (!observed_ || observed_->identity != observation.identity ||
        (packet_.before_read && packet_.before_read->identity != observation.identity)) {
        warning(LogFollowNotice::InvalidResult); next_ = Action::Inspect; return;
    }
    add(status_.bytes_read, packet_.bytes().size());
    if (observation.size < cursor_ ||
        (pending_action_ == Action::TailRead && observation.size < tail_read_offset_)) {
        restart(LogFollowNotice::Truncated, observation); next_ = Action::Inspect; return;
    }
    unverified_change_ = unverified_change_ || (observed_->version != observation.version &&
        observed_->size == observation.size && cursor_ != 0);
    observed_ = observation;
    if (pending_action_ == Action::TailRead) {
        const auto bytes = packet_.bytes();
        if (bytes.empty() && tail_read_offset_ < tail_end_) {
            restart(LogFollowNotice::Truncated, observation); next_ = Action::Inspect; return;
        }
        if (!bytes.empty()) std::memcpy(tail_.data() + tail_size_, bytes.data(), bytes.size());
        tail_size_ += bytes.size(); tail_read_offset_ += bytes.size();
        if (tail_read_offset_ < tail_end_) next_ = Action::TailRead;
        else prepare_tail();
        return;
    }
    const auto bytes = packet_.bytes();
    const auto checked = std::min(bytes.size(), pending_anchor_);
    if (checked != 0 && std::memcmp(bytes.data(), anchor_.data() + probe_checked_, checked) != 0) {
        restart(observation.size < cursor_ ? LogFollowNotice::Truncated : LogFollowNotice::Rewritten, observation);
        next_ = Action::Inspect; return;
    }
    if (bytes.size() < pending_anchor_) {
        if (bytes.empty()) { warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); }
        else { probe_checked_ += checked; next_ = pending_action_; }
        return;
    }
    probe_checked_ = 0;
    const auto fresh = bytes.size() - pending_anchor_;
    if (unverified_change_) { warning(LogFollowNotice::UnverifiedChange); unverified_change_ = false; }
    if (pending_action_ == Action::Drain) drain_remaining_ -= std::min(drain_remaining_, fresh);
    if (fresh == 0) {
        if (cursor_ < observation.size) {
            if (pending_anchor_ != 0 && !bytes.empty()) {
                // A legal short read can finish only the overlap. Continue
                // at the cursor without re-reading that verified overlap.
                probe_checked_ = anchor_size_; next_ = pending_action_;
            } else { warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); }
        } else if (pending_action_ == Action::Drain) next_ = Action::Inspect;
        else if (startup_reconcile_) { drain_remaining_ = options_.drain_bytes; next_ = Action::Drain; }
        else next_ = Action::Idle;
        return;
    }
    buffer_ = Buffer::Packet; buffer_pos_ = pending_anchor_; buffer_end_ = bytes.size();
    buffer_action_ = pending_action_; discard_before_ = 0;
}

void LogFollower::prepare_tail() noexcept {
    std::size_t first = 0;
    skip_prefix_ = cursor_ != 0;
    if (skip_prefix_) {
        while (first < tail_size_ && tail_[first] != '\n' && tail_[first] != '\r') ++first;
        if (first < tail_size_) {
            const bool cr = tail_[first++] == '\r';
            if (cr && first < tail_size_ && tail_[first] == '\n') ++first;
            skip_lf_ = cr && first == tail_size_;
            skip_prefix_ = false;
        }
    }
    std::size_t lines = 0;
    if (first < tail_size_) line_starts_[lines++ % line_starts_.size()] = first;
    for (std::size_t i = first; i < tail_size_; ++i) {
        if (tail_[i] != '\n' && tail_[i] != '\r') continue;
        if (tail_[i] == '\r' && i + 1 < tail_size_ && tail_[i + 1] == '\n') ++i;
        if (i + 1 < tail_size_) line_starts_[lines++ % line_starts_.size()] = i + 1;
    }
    if (lines > options_.tail_lines) first = line_starts_[(lines - options_.tail_lines) % line_starts_.size()];
    buffer_ = Buffer::Tail; buffer_pos_ = 0; buffer_end_ = tail_size_; discard_before_ = first;
    buffer_action_ = Action::TailRead;
}

void LogFollower::remember(std::span<const char> bytes) noexcept {
    const auto capacity = std::min(anchor_.size(), quantum_ / 2);
    if (capacity == 0 || bytes.empty()) return;
    if (bytes.size() >= capacity) {
        std::memcpy(anchor_.data(), bytes.data() + bytes.size() - capacity, capacity); anchor_size_ = capacity;
    } else {
        const auto retain = std::min(anchor_size_, capacity - bytes.size());
        if (retain != 0) std::memmove(anchor_.data(), anchor_.data() + anchor_size_ - retain, retain);
        std::memcpy(anchor_.data() + retain, bytes.data(), bytes.size()); anchor_size_ = retain + bytes.size();
    }
}

void LogFollower::consume(std::int64_t now) {
    const char* data = buffer_ == Buffer::Tail ? tail_.data() : packet_.bytes().data();
    const auto count = std::min(quantum_, buffer_end_ - buffer_pos_);
    std::size_t skipped = std::min(count, discard_before_);
    discard_before_ -= skipped;
    if (skipped == 0 && skip_lf_) {
        skip_lf_ = false; if (count != 0 && data[buffer_pos_] == '\n') skipped = 1;
    }
    if (discard_before_ == 0 && skip_prefix_ && skipped < count) {
        while (skipped < count) {
            const char byte = data[buffer_pos_ + skipped++];
            if (byte == '\n' || byte == '\r') {
                skip_prefix_ = false; skip_lf_ = byte == '\r'; break;
            }
        }
        if (skip_lf_ && skipped < count) { skip_lf_ = false; if (data[buffer_pos_ + skipped] == '\n') ++skipped; }
    }
    if (skipped != 0) {
        remember({data + buffer_pos_, skipped}); cursor_ += skipped; buffer_pos_ += skipped;
        add(status_.startup_skipped_bytes, skipped);
    }
    const auto delivered = count - skipped;
    if (delivered != 0) {
        if (!model_started_) {
            if (!model_.begin_source(status_.source, generation_, cursor_ == 0)) {
                status_.notice = LogFollowNotice::Exhausted; failure(LogSourceState::Failed, 0, now); return;
            }
            model_started_ = true;
        }
        const std::string_view bytes(data + buffer_pos_, delivered);
        const auto appended = model_.append_bytes_bounded(bytes, options_.records_per_step);
        if (!appended.accepted) { warning(LogFollowNotice::InvalidResult); failure(LogSourceState::Failed, 0, now); return; }
        remember({bytes.data(), appended.bytes}); cursor_ += appended.bytes; buffer_pos_ += appended.bytes;
        add(status_.bytes_delivered, appended.bytes);
    }
    if (buffer_pos_ == buffer_end_) { buffer_ = Buffer::None; after_buffer(now); }
}

void LogFollower::after_buffer(std::int64_t now) noexcept {
    if (buffer_action_ == Action::TailRead) {
        drain_remaining_ = options_.drain_bytes; next_ = Action::Drain;
    } else if (buffer_action_ == Action::Drain) {
        if (drain_remaining_ == 0) { add(status_.drain_yields); next_ = Action::Inspect; }
        else next_ = Action::Drain;
    } else if ((startup_reconcile_ && observed_ && cursor_ >= observed_->size) ||
               session_->changes_pending() || (reconcile_at_ != std::numeric_limits<std::int64_t>::max() && now >= reconcile_at_)) {
        drain_remaining_ = options_.drain_bytes; next_ = Action::Drain;
    } else next_ = observed_ && cursor_ < observed_->size ? Action::Read : Action::Idle;
}

void LogFollower::update_status(const LogFollowStatus& previous) noexcept {
    status_.offset = cursor_; status_.file_size = observed_ ? observed_->size : 0;
    status_.backlog_bytes = status_.file_size > cursor_ ? status_.file_size - cursor_ : 0;
    status_.pending = awaiting_ || buffer_ != Buffer::None;
    if (status_ != previous) add(status_.revision);
}
bool LogFollower::step(std::int64_t now) {
    if (!started_ || stopped_ || now < last_now_) return false;
    last_now_ = now;
    const auto before = status_; const auto model_revision = model_.stats().revision;
    if (awaiting_ && session_->take(packet_)) receive(now);
    if (!awaiting_ && buffer_ != Buffer::None) consume(now);
    if (!awaiting_ && buffer_ == Buffer::None) {
        if (next_ == Action::Idle) {
            if (status_.state == LogSourceState::Following &&
                (session_->changes_pending() || (reconcile_at_ != std::numeric_limits<std::int64_t>::max() && now >= reconcile_at_))) {
                drain_remaining_ = options_.drain_bytes; next_ = observed_ ? Action::Drain : Action::Inspect;
            } else if (status_.state != LogSourceState::Following &&
                       retry_at_ != std::numeric_limits<std::int64_t>::max() && now >= retry_at_) next_ = Action::Inspect;
        }
        if (next_ != Action::Idle) issue(next_, now);
    }
    update_status(before);
    return status_ != before || model_.stats().revision != model_revision;
}
bool LogFollower::retry(std::int64_t now) {
    if (!started_ || stopped_ || now < last_now_ || awaiting_ || buffer_ != Buffer::None ||
        status_.state == LogSourceState::Following) return false;
    last_now_ = now; const auto before = status_;
    const bool result = issue(Action::Inspect, now); update_status(before); return result;
}
void LogFollower::stop() noexcept {
    if (stopped_) return;
    stopped_ = true; session_->cancel();
    if (started_) model_.finish();
    awaiting_ = false; buffer_ = Buffer::None; next_ = Action::Idle;
    status_.state = LogSourceState::Cancelled; status_.notice = LogFollowNotice::Stopped;
    status_.pending = false; add(status_.revision);
}
std::optional<std::int64_t> LogFollower::next_deadline() const noexcept {
    if (!started_ || stopped_) return {};
    if (awaiting_) return session_->has_wake() ? std::optional<std::int64_t>{}
        : std::optional<std::int64_t>{deadline(last_now_, options_.result_poll_nanos)};
    if (buffer_ != Buffer::None || next_ != Action::Idle) return last_now_;
    const auto scheduled = status_.state == LogSourceState::Following ? reconcile_at_ : retry_at_;
    return scheduled == std::numeric_limits<std::int64_t>::max() && last_now_ == scheduled
        ? std::optional<std::int64_t>{} : std::optional<std::int64_t>{scheduled};
}

}  // namespace ckv::widgets
