// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_controller.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ckv::widgets {
namespace {
void add_controller(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
std::int64_t after(std::int64_t now, std::int64_t delay) noexcept {
    return now > std::numeric_limits<std::int64_t>::max() - delay ? std::numeric_limits<std::int64_t>::max() : now + delay;
}
bool same_search_state(const LogSearchStatus& a, const LogSearchStatus& b) noexcept {
    return a.query_revision == b.query_revision && a.visibility_revision == b.visibility_revision &&
        a.phase == b.phase && a.error == b.error && a.wrapped == b.wrapped && a.incomplete == b.incomplete;
}
}
LogController::LogController(LogControllerOptions options, LogActivity* activity)
    : options_(options), activity_(activity) {
    if (!options.lanes || options.lanes > 64 || !options.lanes_per_turn || options.lanes_per_turn > options.lanes ||
        options.yield_nanos <= 0 || options.frame_nanos <= 0)
        throw std::invalid_argument("invalid log controller options");
    slots_.resize(options.lanes);
}
LogLaneId LogController::attach(LogLane lane) noexcept {
    if (stepping_ || !lane.model || !lane.filter || &lane.filter->model() != lane.model ||
        (lane.search && &lane.search->provider() != lane.filter) ||
        (lane.panel && &lane.panel->provider() != lane.filter) || (lane.file && lane.stream) ||
        (lane.file && &lane.file->model() != lane.model) || (lane.stream && &lane.stream->model() != lane.model) ||
        next_id_ == std::numeric_limits<LogLaneId>::max()) return 0;
    for (auto& slot : slots_) if (!slot.id) {
        slot = {}; slot.id = next_id_++; slot.lane = lane; slot.kicked = true;
        ++status_.active_lanes; return slot.id;
    }
    return 0;
}
bool LogController::detach(LogLaneId id) noexcept {
    if (stepping_) return false;
    for (auto& slot : slots_) if (slot.id == id && id) {
        slot = {}; --status_.active_lanes;
        if (!status_.active_lanes) next_frame_.reset();
        return true;
    }
    return false;
}
bool LogController::kick(LogLaneId id) noexcept {
    for (auto& slot : slots_) if (id && slot.id == id) { slot.kicked = true; return true; }
    return false;
}
void LogController::kick_all() noexcept { for (auto& slot : slots_) if (slot.id) slot.kicked = true; }
bool LogController::ready(const Slot& slot) const noexcept {
    if (!slot.id) return false;
    const auto& lane = slot.lane;
    if (slot.kicked || lane.filter->status().query_pending || lane.filter->status().live_pending ||
        lane.filter->status().requested_revision != slot.query_revision || lane.model->revision() != slot.model_revision ||
        (lane.search && (lane.search->status().phase == LogSearchPhase::Pending || !same_search_state(lane.search->status(), slot.search_status))) ||
        (lane.stream && lane.stream->pending())) return true;
    const auto deadline = lane.file ? lane.file->next_deadline() : std::nullopt;
    return deadline && *deadline <= now_;
}
void LogController::mark_dirty(Slot& slot) noexcept {
    if (slot.dirty) add_controller(status_.coalesced_updates);
    else { slot.dirty = true; slot.dirty_since = now_; }
    if (!next_frame_) next_frame_ = last_frame_ ? std::max(now_, after(*last_frame_, options_.frame_nanos)) : now_;
}
void LogController::advance(Slot& slot) {
    auto& lane = slot.lane;
    if (lane.stream) lane.stream->observe(now_);
    const bool held = lane.filter->status().query_pending || lane.filter->status().live_pending;
    if (held && (lane.file || lane.stream)) add_controller(status_.ingress_deferrals);
    else {
        if (lane.file) lane.file->step(now_);
        if (lane.stream) lane.stream->step(now_);
        slot.kicked = false;
    }
    lane.filter->step();
    if (lane.search) lane.search->step();
    const auto ingress = lane.file ? lane.file->status().revision : lane.stream ? lane.stream->status().revision : 0;
    bool changed = lane.model->revision() != slot.model_revision || lane.filter->revision() != slot.filter_revision ||
        lane.filter->status().requested_revision != slot.query_revision || ingress != slot.ingress_revision;
    if (lane.search) changed = changed || !same_search_state(lane.search->status(), slot.search_status);
    if (changed) mark_dirty(slot);
    slot.model_revision = lane.model->revision(); slot.filter_revision = lane.filter->revision();
    slot.query_revision = lane.filter->status().requested_revision; slot.ingress_revision = ingress;
    if (lane.search) slot.search_status = lane.search->status();
}
void LogController::refresh(Slot& slot) {
    if (slot.lane.panel) {
        LogPanelSourceStatus source = slot.lane.panel->source_status();
        if (slot.lane.file) {
            const auto& status = slot.lane.file->status();
            source.state = status.state; source.watch = status.watch; source.backlog_bytes = status.backlog_bytes;
            source.native_error = status.native_error; source.uncertain = status.uncertain;
        } else if (slot.lane.stream) {
            const auto& status = slot.lane.stream->status();
            source.state = status.state; source.watch = LogWatchMode::Unavailable;
            source.backlog_bytes = status.mailbox.queued_bytes + status.held_bytes;
            source.native_error = status.native_error; source.dropped = status.mailbox.dropped.records;
            source.dropped_bytes = status.mailbox.dropped.bytes; source.dropped_batches = status.mailbox.dropped.batches;
            source.dropped_records_unknown = status.mailbox.dropped.records_unknown;
            source.lag_nanos = status.oldest_lag_nanos;
        }
        slot.lane.panel->set_source_status(source);
        slot.lane.panel->refresh();
    }
    if (now_ >= slot.dirty_since)
        status_.maximum_observed_refresh_lag_nanos = std::max(status_.maximum_observed_refresh_lag_nanos, now_ - slot.dirty_since);
    slot.dirty = false;
}
bool LogController::step(std::int64_t now) {
    if (stepping_ || now < now_ || now < 0) return false;
    now_ = now; stepping_ = true;
    struct Release { bool& value; ~Release() { value = false; } } release{stepping_};
    if (activity_ && activity_->consume()) kick_all();
    status_.last_turn_lanes = 0;
    if (now >= next_turn_) {
        for (std::size_t examined = 0; examined < slots_.size() && status_.last_turn_lanes < options_.lanes_per_turn; ++examined) {
            auto& slot = slots_[round_robin_]; round_robin_ = (round_robin_ + 1) % slots_.size();
            if (!ready(slot)) continue;
            advance(slot); ++status_.last_turn_lanes; add_controller(status_.lane_quanta);
        }
        if (status_.last_turn_lanes) { next_turn_ = after(now, options_.yield_nanos); add_controller(status_.turns); }
    }
    if (!next_frame_ || now < *next_frame_) return false;
    for (auto& slot : slots_) if (slot.id && slot.dirty) refresh(slot);
    last_frame_ = now; next_frame_.reset(); add_controller(status_.refreshes); return true;
}
std::optional<std::int64_t> LogController::next_deadline() const noexcept {
    auto deadline = next_frame_;
    const auto include = [&](std::int64_t value) { if (!deadline || value < *deadline) deadline = value; };
    if (activity_ && activity_->pending()) include(std::max(now_, next_turn_));
    for (const auto& slot : slots_) if (slot.id) {
        if (ready(slot)) include(std::max(now_, next_turn_));
        if (slot.lane.file && !slot.lane.filter->status().query_pending && !slot.lane.filter->status().live_pending) {
            if (const auto value = slot.lane.file->next_deadline()) include(std::max(*value, next_turn_));
        }
    }
    return deadline;
}
std::size_t LogController::allocated_bytes() const noexcept { return sizeof(*this) + slots_.capacity() * sizeof(Slot); }
}  // namespace ckv::widgets
