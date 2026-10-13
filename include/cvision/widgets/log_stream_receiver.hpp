// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/log_mailbox.hpp"
#include "cvision/widgets/log_model.hpp"

namespace ckv::widgets {
// Maximum consumed bytes and newly admitted records in one UI-thread step.
struct LogReceiveBudget {
    // Positive byte and record quanta; a held record may continue across steps.
    std::size_t bytes = 16 * 1024, records = 256;
};
// Source/epoch, delivery/loss/lag and bounded-work observations.
struct LogReceiveStatus {
    // Admission identity/revision, cumulative byte/record/gap/retirement counts,
    // mailbox state, native error, last quanta, staging, lag and pending work.
    LogSourceId source = 0;
    std::uint64_t epoch = 0, generation = 0, revision = 0;
    std::uint64_t bytes_delivered = 0, records_started = 0, resync_bytes = 0, gap_markers = 0, retired_bytes = 0;
    LogMailboxStatus mailbox;
    LogSourceState state = LogSourceState::Pending;
    std::uint32_t native_error = 0;
    std::size_t last_step_bytes = 0, last_step_records = 0, held_bytes = 0;
    std::int64_t oldest_lag_nanos = 0;
    bool pending = false;
};

// Single UI-thread consumer/exclusive model writer. Message staging is fixed;
// each step visits one publication and consumes byte/new-record quanta. Source
// time is explicit. Byte loss cannot splice unrelated fragments: finish, new
// generation, diagnostic record, then resync through a delimiter. A held record
// is decoded incrementally with its original metadata and one stable identity.
class LogStreamReceiver {
public:
    // Borrow an exclusively written model and enclosing mailbox; validate budget.
    LogStreamReceiver(LogModel& model, LogMailbox& mailbox, LogReceiveBudget budget = {});
    // Consume at most one bounded publication quantum at explicit monotonic time.
    bool step(std::int64_t now) noexcept;
    // Refresh queue/loss/lag diagnostics while ingestion is intentionally held.
    // Does not take data or mutate the model.
    bool observe(std::int64_t now) noexcept;
    // Cancel/retire queued and held bytes; retry false on mailbox contention.
    bool stop(std::int64_t now) noexcept;
    // Borrow current diagnostics/model; pending includes held or closing work.
    const LogReceiveStatus& status() const noexcept { return status_; }
    const LogModel& model() const noexcept { return model_; }
    bool pending() const noexcept { return held_ || closing_ || mailbox_.pending(); }
    // Complete fixed receiver staging/object storage, excluding its borrows.
    std::size_t allocated_bytes() const noexcept { return sizeof(*this); }
private:
    bool generation(LogSourceId source) noexcept;
    bool gap(std::size_t& records) noexcept;
    void update(std::int64_t now) noexcept;
    void fail() noexcept;
    LogModel& model_;
    LogMailbox& mailbox_;
    LogReceiveBudget budget_;
    LogMessage message_;
    LogReceiveStatus status_;
    std::size_t position_ = 0;
    std::int64_t last_now_ = 0;
    bool held_ = false, record_started_ = false, skip_prefix_ = false, skip_lf_ = false, stopped_ = false, closing_ = false;
};
}  // namespace ckv::widgets
