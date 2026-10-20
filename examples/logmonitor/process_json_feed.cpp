// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "process_json_feed.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
namespace ckv::logmonitor {
bool supports_native_json(NativeJsonSchema schema, const core::ProcessRunResult& result) noexcept {
    if (result.state != core::ProcessRunState::Completed || !result.exit || result.exit->kind != core::ProcessExitKind::Normal ||
        result.error.code || result.stdout_capture.truncated || result.stderr_capture.truncated) return false;
    const bool apple = schema == NativeJsonSchema::Apple;
    if (result.exit->code != 0 && !(apple && result.exit->code == 64)) return false;
    const std::string_view help = result.stdout_capture.bytes.empty() ? result.stderr_capture.bytes : result.stdout_capture.bytes;
    return apple ? help.find("--style") != std::string_view::npos && help.find("ndjson") != std::string_view::npos :
        help.find("json") != std::string_view::npos && help.find("--follow") != std::string_view::npos;
}
namespace {
std::int64_t soon(std::int64_t now) { return now > std::numeric_limits<std::int64_t>::max() - 1'000'000 ? std::numeric_limits<std::int64_t>::max() : now + 1'000'000; }
bool terminal(core::ProcessStreamState state) { return state == core::ProcessStreamState::Completed || state == core::ProcessStreamState::Cancelled || state == core::ProcessStreamState::LaunchFailed || state == core::ProcessStreamState::IoFailed; }
}
struct ProcessJsonFeed::Impl {
    LogSourceId source; NativeJsonSchema schema;
    std::unique_ptr<core::ProcessStreamSession> session;
    LogMailbox output;
    LogMessage input, errors, record;
    NativeJsonDecoder decoder;
    core::ProcessStreamStatus process;
    std::string frame, explanation, contract, stderr_prefix;
    std::size_t position = 0; std::int64_t now = 0, frame_arrival = 0;
    std::uint64_t malformed = 0, oversized = 0, clipped_metadata = 0;
    bool held = false, staged = false, draining_oversize = false, raw_lost = false, stdout_end = false, stderr_end = false, stopped = false, closed = false;
    Impl(LogSourceId id, NativeJsonSchema format, std::unique_ptr<core::ProcessStreamSession> native,
         std::shared_ptr<const LogWake> wake, std::string reason)
        : source(id), schema(format), session(std::move(native)), output(id, 1, {8, kLogBatchBytes, LogOverflowPolicy::DropOldest}, std::move(wake)), contract(reason.substr(0, 1024)) {
        if (!session) throw std::invalid_argument("process JSON feed requires an owned session");
        frame.reserve(kLogBatchBytes); explanation.reserve(4096); stderr_prefix.reserve(1024);
        record.source = source; record.epoch = 1;
    }
    void describe() {
        explanation.assign(contract);
        const auto number = [&](auto value) { std::array<char, 32> text{}; const auto result = std::to_chars(text.data(), text.data() + text.size(), value); explanation.append(text.data(), static_cast<std::size_t>(result.ptr - text.data())); };
        explanation += "\nMalformed envelopes: "; number(malformed); explanation += "; oversized: "; number(oversized);
        explanation += "; clipped metadata prefixes (raw retained): "; number(clipped_metadata);
        if (process.exit) { explanation += "\nHelper exit/signal: "; number(process.exit->code); }
        if (process.error.code) { explanation += "\nNative helper error: "; number(process.error.code); }
        if (!stderr_prefix.empty()) { explanation += "\nPrivate helper stderr (first 1024 bytes): "; explanation += stderr_prefix; }
        if (state() == LogSourceState::Failed) explanation += "\nRetry starts a new subscription; choose File/Sample if permission or helper failure persists.";
    }
    bool publish() {
        if (!staged) return true;
        const auto result = output.offer(record);
        if (result == LogOfferResult::Busy || result == LogOfferResult::WouldBlock) return false;
        staged = false; return true;
    }
    void notice(std::string_view reason) {
        const std::array fields{LogField{"adapter", schema == NativeJsonSchema::Apple ? "Apple log" : "journalctl"}, LogField{"diagnostic", reason}};
        record.set_record(reason, {LogSeverity::Warning, {}, fields}); record.arrival_nanos = frame_arrival; staged = true;
    }
    void finish_frame() {
        record.arrival_nanos = frame_arrival;
        if (draining_oversize) { if (!raw_lost) { ++oversized; notice("[native adapter: envelope exceeded 64 KiB; discarded through newline]"); } }
        else if (!frame.empty()) {
            const auto result = decoder.decode(frame, schema, record);
            if (result == NativeJsonResult::Record) { staged = true; if (decoder.metadata_clipped()) ++clipped_metadata; }
            else { ++malformed; notice(result == NativeJsonResult::Limit ? "[native adapter: envelope exceeded parser nesting/metadata limits]" : "[native adapter: malformed JSON or missing event payload]"); }
        }
        frame.clear(); draining_oversize = false; raw_lost = false;
    }
    LogSourceState state() const {
        if (stopped || process.state == core::ProcessStreamState::Cancelled) return LogSourceState::Cancelled;
        if (process.state == core::ProcessStreamState::LaunchFailed || process.state == core::ProcessStreamState::IoFailed) return LogSourceState::Failed;
        if (process.state == core::ProcessStreamState::Completed) return process.exit && process.exit->kind == core::ProcessExitKind::Normal && process.exit->code == 0 ? LogSourceState::Ended : LogSourceState::Failed;
        return process.state == core::ProcessStreamState::Preparing ? LogSourceState::Pending : LogSourceState::Following;
    }
};
ProcessJsonFeed::ProcessJsonFeed(LogSourceId source, NativeJsonSchema schema, std::unique_ptr<core::ProcessStreamSession> session,
                               std::shared_ptr<const LogWake> wake, std::string explanation)
    : impl_(std::make_unique<Impl>(source, schema, std::move(session), std::move(wake), std::move(explanation))) {}
ProcessJsonFeed::~ProcessJsonFeed() = default;
LogMailbox& ProcessJsonFeed::publications() noexcept { return impl_->output; }
void ProcessJsonFeed::step(std::int64_t now) {
    auto& data = *impl_; if (now < data.now || now < 0 || data.closed) return; data.now = now;
    if (data.stopped) { data.closed = data.output.cancel(now); return; }
    data.session->status(data.process);
    if (!data.publish()) return;
    // Stderr remains a private bounded diagnostic, never a JSON log event.
    if (!data.stderr_end) {
        if (data.session->stderr_mailbox().take(data.errors) == LogTakeResult::Taken) {
            if (data.errors.kind == LogMessageKind::End) data.stderr_end = true;
            else { const auto bytes = data.errors.bytes(); data.stderr_prefix.append(bytes.data(), std::min(bytes.size(), 1024 - data.stderr_prefix.size())); }
        }
        data.session->stderr_mailbox().acknowledge();
    }
    std::size_t bytes = 0, frames = 0, publications = 0;
    while (bytes < 16 * 1024 && frames < 4 && publications < 4) {
        if (!data.held) {
            const auto result = data.session->stdout_mailbox().take(data.input);
            if (result != LogTakeResult::Taken) { data.session->stdout_mailbox().acknowledge(); break; }
            ++publications;
            if (data.input.kind == LogMessageKind::End) {
                data.stdout_end = true; data.session->stdout_mailbox().acknowledge();
                if (!data.frame.empty() || data.draining_oversize) {
                    ++data.malformed; data.notice("[native adapter: incomplete envelope at helper EOF]"); data.frame.clear(); data.draining_oversize = false;
                }
                break;
            }
            data.position = 0; data.held = true;
            if (!data.input.loss_before.empty()) {
                data.frame.clear(); data.draining_oversize = true; data.raw_lost = true; ++data.malformed;
                data.notice("[native adapter: raw transport loss; partial envelope discarded]"); break;
            }
        }
        const auto input = data.input.bytes();
        while (data.position < input.size() && bytes < 16 * 1024 && frames < 4) {
            const char byte = input[data.position++]; ++bytes;
            if (data.frame.empty() && !data.draining_oversize) data.frame_arrival = data.input.arrival_nanos;
            if (byte == '\n') { data.finish_frame(); ++frames; if (!data.publish()) break; }
            else if (!data.draining_oversize) {
                if (data.frame.size() == kLogBatchBytes) { data.frame.clear(); data.draining_oversize = true; }
                else data.frame += byte;
            }
        }
        if (data.position == input.size()) { data.held = false; data.session->stdout_mailbox().acknowledge(); }
        if (data.staged) break;
    }
    if (!data.publish()) return;
    data.describe();
    if (terminal(data.process.state) && data.stdout_end && data.stderr_end && !data.held) {
        data.closed = data.output.close(data.state(), static_cast<std::uint32_t>(data.process.error.code), now);
    }
}
std::optional<std::int64_t> ProcessJsonFeed::next_deadline() const noexcept {
    const auto& data = *impl_;
    if (data.closed) return {};
    if (data.stopped) return soon(data.now);
    if (data.held || data.staged || data.session->stdout_mailbox().pending() || data.session->stderr_mailbox().pending() ||
        (terminal(data.process.state) && data.stdout_end && data.stderr_end)) return soon(data.now);
    return {};
}
widgets::LogPanelSourceStatus ProcessJsonFeed::diagnostic() const {
    const auto& data = *impl_; widgets::LogPanelSourceStatus status;
    status.state = data.state(); status.watch = LogWatchMode::Native;
    status.native_error = static_cast<std::uint32_t>(data.process.error.code);
    // Live subscription/reopen never promises historical continuity.
    status.uncertain = true;
    status.backlog_bytes = data.frame.size() + data.process.stdout_pending_bytes + data.process.stderr_pending_bytes;
    LogMailboxStatus queued;
    if (data.session->stdout_mailbox().status(queued)) status.backlog_bytes += queued.queued_bytes;
    if (data.session->stderr_mailbox().status(queued)) status.backlog_bytes += queued.queued_bytes;
    if (data.held) status.backlog_bytes += data.input.bytes().size() - data.position;
    return status;
}
std::string_view ProcessJsonFeed::explanation() const noexcept {
    // Stable bounded source contract plus private diagnostic bytes are surfaced
    // separately; do not mutate this borrowed string from a const inspection.
    return impl_->explanation;
}
void ProcessJsonFeed::stop(std::int64_t now) noexcept {
    auto& data = *impl_; data.session->cancel(); data.stopped = true; data.closed = data.output.cancel(std::max(now, data.now));
}
}  // namespace ckv::logmonitor
