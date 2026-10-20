// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/log_source.hpp"
#include "cvision/widgets/log_model.hpp"

namespace ckv::widgets {

// File-follow/start policy and fixed byte/record work and time limits.
struct LogFollowOptions {
    // Path versus open-object follow; recent-tail/beginning/offset startup.
    // Tail/read/drain sizes and record quota bound each reconciliation/step.
    // Positive reconcile/retry/result-poll periods are explicit monotonic ns.
    LogFollowMode follow = LogFollowMode::Path;
    LogStartMode start = LogStartMode::RecentTail;
    std::uint64_t offset = 0;
    std::size_t tail_bytes = kLogBatchBytes;
    std::size_t tail_lines = 500;
    std::size_t read_bytes = 16 * 1024;
    std::size_t records_per_step = 256;
    std::size_t drain_bytes = 4 * kLogBatchBytes;
    std::int64_t reconcile_nanos = 1'000'000'000;
    std::int64_t retry_nanos = 1'000'000'000;
    std::int64_t result_poll_nanos = 1'000'000;
    bool valid() const noexcept;
};

// Distinct identity/content/recovery/loss/validation/exhaustion observations.
enum class LogFollowNotice : std::uint8_t {
    None, Rotated, Truncated, Rewritten, Reconnected, NotificationLoss,
    UnverifiedChange, InvalidResult, Exhausted, Stopped
};

// Per-current-source counters, separate from model eviction/oversized-line
// counters. Known skipped bytes are lower bounds when old writers continue;
// uncertain continuity is not misreported as an exact dropped-record count.
struct LogFollowStatus {
    // State/mode/notice, source identity and cursor/backlog, delivered/skipped
    // bytes, request/revision counts, native error and continuity/pending flags.
    LogSourceState state = LogSourceState::Cancelled;
    LogWatchMode watch = LogWatchMode::Unavailable;
    LogFollowNotice notice = LogFollowNotice::None;
    LogSourceId source = 0;
    std::uint64_t generation = 0, offset = 0, file_size = 0, backlog_bytes = 0;
    std::uint64_t bytes_read = 0, bytes_delivered = 0, startup_skipped_bytes = 0;
    std::uint64_t unread_skipped_bytes = 0, continuity_warnings = 0;
    std::uint64_t requests = 0, stale_results = 0, drain_yields = 0, revision = 0;
    std::uint32_t native_error = 0;
    bool uncertain = false, pending = false;
    friend bool operator==(const LogFollowStatus&, const LogFollowStatus&) = default;
};

// Exclusive UI-thread stream writer for a borrowed LogModel. Owns one session;
// its service must outlive this object. Construction reserves all storage.
// start/step/retry receive nondecreasing monotonic nanoseconds explicitly.
// One step consumes one publication and delivers at most read_bytes of new
// source data; it never waits for a worker or reads a native file/clock.
class LogFollower {
public:
    // Borrow an exclusive model and own a session whose service encloses this life.
    LogFollower(LogModel& model, std::unique_ptr<LogFileSession> session,
                LogFollowOptions options = {});
    // Retire pending publication; never join the native worker here.
    ~LogFollower();
    LogFollower(const LogFollower&) = delete;
    LogFollower& operator=(const LogFollower&) = delete;

    // First start or source switch; preserves retained history and retires
    // old results through a new request revision. Invalid requests change no
    // source/model state. Stop is final: use a new session after stopping.
    bool start(std::string_view absolute_path, LogSourceId source, std::int64_t now);
    bool step(std::int64_t now);
    bool retry(std::int64_t now);
    void stop() noexcept;
    const LogFollowStatus& status() const noexcept { return status_; }
    const LogModel& model() const noexcept { return model_; }
    std::string_view path() const noexcept { return path_; }
    // Immediate when buffered work remains, a deadline for reconciliation or
    // retry, or no deadline while an explicit wake target owns delivery.
    std::optional<std::int64_t> next_deadline() const noexcept;
    // Object plus reserved path/tail/line/probe staging, excluding borrows.
    std::size_t allocated_bytes() const noexcept;

private:
    enum class Action { Idle, Inspect, TailRead, Read, Drain };
    enum class Buffer { None, Packet, Tail };
    bool issue(Action action, std::int64_t now);
    void receive(std::int64_t now);
    void consume(std::int64_t now);
    void prepare_tail() noexcept;
    void after_buffer(std::int64_t now) noexcept;
    void restart(LogFollowNotice notice, const LogFileObservation& observation) noexcept;
    void warning(LogFollowNotice notice) noexcept;
    void failure(LogSourceState state, std::uint32_t error, std::int64_t now) noexcept;
    void remember(std::span<const char> bytes) noexcept;
    void update_status(const LogFollowStatus& previous) noexcept;

    LogModel& model_;
    std::unique_ptr<LogFileSession> session_;
    LogFollowOptions options_;
    std::string path_;
    LogFileRequest request_;
    LogFileResult packet_;
    std::array<char, kLogBatchBytes> tail_{};
    std::vector<std::size_t> line_starts_;
    std::array<char, 32> anchor_{};
    std::optional<LogFileObservation> observed_;
    LogFollowStatus status_;
    std::uint64_t revision_ = 0, expected_revision_ = 0, generation_ = 0, cursor_ = 0;
    std::uint64_t pending_offset_ = 0, tail_end_ = 0, tail_read_offset_ = 0;
    std::size_t quantum_ = 0, pending_bytes_ = 0, pending_anchor_ = 0, anchor_size_ = 0;
    std::size_t probe_checked_ = 0;
    std::size_t tail_size_ = 0, buffer_pos_ = 0, buffer_end_ = 0, discard_before_ = 0;
    std::size_t drain_remaining_ = 0;
    std::int64_t last_now_ = 0, reconcile_at_ = 0, retry_at_ = 0;
    Action next_ = Action::Idle, pending_action_ = Action::Idle, buffer_action_ = Action::Idle;
    Buffer buffer_ = Buffer::None;
    bool started_ = false, stopped_ = false, awaiting_ = false, startup_reconcile_ = true;
    bool unverified_change_ = false;
    bool model_started_ = false, lost_handle_ = false, skip_prefix_ = false, skip_lf_ = false;
};

}  // namespace ckv::widgets
