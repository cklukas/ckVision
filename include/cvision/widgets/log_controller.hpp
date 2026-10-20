// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/widgets/log_follower.hpp"
#include "cvision/widgets/log_panel.hpp"
#include "cvision/widgets/log_stream_receiver.hpp"

namespace ckv::widgets {
// Instance-scoped nonzero attachment identity; zero means admission failed.
using LogLaneId = std::uint64_t;
// Borrowed coherent model/filter/search/panel and exactly one source path.
struct LogLane {
    // All objects enclose attachment; absent search/panel/source are optional.
    LogModel* model = nullptr;
    LogFilter* filter = nullptr;
    LogSearch* search = nullptr;
    LogPanel* panel = nullptr;
    LogFollower* file = nullptr;
    LogStreamReceiver* stream = nullptr;
};
// Fixed lane capacity and fair per-turn work/presentation deadlines.
struct LogControllerOptions {
    // Capacity/visited lanes per turn; positive yielding/frame periods in ns.
    std::size_t lanes = 8, lanes_per_turn = 1;
    std::int64_t yield_nanos = 1'000'000, frame_nanos = 33'333'334;
};
// Scheduling work and refresh counters, never a physical terminal receipt.
struct LogControllerStatus {
    // Cumulative work/coalescing/deferral counters, current lane occupancy and
    // greatest observed refresh delay under the caller-supplied clock.
    std::uint64_t turns = 0, lane_quanta = 0, refreshes = 0, coalesced_updates = 0, ingress_deferrals = 0;
    std::size_t last_turn_lanes = 0, active_lanes = 0;
    std::int64_t maximum_observed_refresh_lag_nanos = 0;
};

// Application-owned UI-thread orchestration, not a parallel terminal writer.
// All borrowed lane objects outlive attachment. Explicit monotonic time only.
// Source workers notify the borrowed LogActivity; the next turn probes lanes
// fairly, then returns a yielding deadline instead of posting per-line work.
// One ingress/filter/search quantum per visited lane; panel refresh is paced
// separately. Pending filters hold replayable file offsets, while stream queue
// overflow stays explicit. Input-driven repaint remains the Application's job.
class LogController {
public:
    // Validate options and reserve lanes; activity is borrowed for this life.
    explicit LogController(LogControllerOptions options = {}, LogActivity* activity = nullptr);
    // Attach coherent borrows, returning zero on invalid/full/exhausted admission.
    LogLaneId attach(LogLane lane) noexcept;
    // Retire an existing attachment before its borrowed objects are destroyed.
    bool detach(LogLaneId id) noexcept;
    // Mark a lane, or all lanes, for another bounded scheduling quantum.
    bool kick(LogLaneId id) noexcept;
    void kick_all() noexcept;
    // True when a paced panel refresh was emitted, not when host visibility
    // was proved. Native output/backpressure remains in Application/Presenter.
    bool step(std::int64_t now);
    // Next yielding/refresh/source deadline, absent while all lanes are quiet.
    std::optional<std::int64_t> next_deadline() const noexcept;
    // Borrow current counters and inspect reserved controller storage.
    const LogControllerStatus& status() const noexcept { return status_; }
    std::size_t allocated_bytes() const noexcept;
private:
    struct Slot {
        LogLaneId id = 0;
        LogLane lane;
        std::uint64_t model_revision = 0, filter_revision = 0, query_revision = 0, ingress_revision = 0;
        LogSearchStatus search_status;
        std::int64_t dirty_since = 0;
        bool kicked = false, dirty = false;
    };
    bool ready(const Slot& slot) const noexcept;
    void advance(Slot& slot);
    void mark_dirty(Slot& slot) noexcept;
    void refresh(Slot& slot);
    LogControllerOptions options_;
    LogActivity* activity_;
    std::vector<Slot> slots_;
    LogLaneId next_id_ = 1;
    std::size_t round_robin_ = 0;
    std::int64_t now_ = 0, next_turn_ = 0;
    std::optional<std::int64_t> next_frame_, last_frame_;
    LogControllerStatus status_;
    bool stepping_ = false;
};
}  // namespace ckv::widgets
