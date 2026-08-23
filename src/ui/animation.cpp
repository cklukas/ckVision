// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/animation.hpp"

#include <algorithm>
#include <utility>

namespace ckv::ui {

Animation::~Animation() {
    // Silent: no `on_finished`. See the header — an owner being destroyed is
    // already tearing down, and its callback would be reaching back through a
    // half-destroyed object. Cancelling the timer still matters, because the
    // Application outlives this and would otherwise fire into freed storage;
    // `alive_` covers the case where it cannot be cancelled because the
    // Application is going too.
    stop_timer();
}

void Animation::stop_timer() noexcept {
    if (timer_ != 0 && app_ != nullptr) app_->cancel_timer(timer_);
    timer_ = 0;
}

void Animation::start(Application& app, std::int64_t duration_nanos,
                      std::function<void(double)> on_frame, std::function<void()> on_finished,
                      std::int64_t frame_interval_nanos) {
    // Whatever was running ends first, and ends properly: its `on_finished`
    // runs, so its decoration goes. Two effects overlapping is the caller's
    // business; two decorations left on screen is never anybody's.
    finish();

    app_ = &app;
    on_frame_ = std::move(on_frame);
    on_finished_ = std::move(on_finished);
    duration_nanos_ = duration_nanos;
    started_nanos_ = app.clock().now_nanos();

    // A run with no duration is a run that is already over. Answered here
    // rather than at every call site, so "animations are off" is a duration
    // of zero and not a branch each effect has to remember to write.
    if (duration_nanos_ <= 0) {
        std::function<void()> finished = std::move(on_finished_);
        on_finished_ = nullptr;
        on_frame_ = nullptr;
        if (finished) finished();
        return;
    }

    const std::weak_ptr<int> alive = alive_;
    timer_ = app.start_timer(std::max<std::int64_t>(1, frame_interval_nanos), /*repeating=*/true,
                             [this, alive] {
                                 // The run may have ended inside a previous
                                 // frame; the timer can still be holding this
                                 // callback when it does.
                                 if (alive.expired()) return;
                                 if (timer_ == 0) return;

                                 const std::int64_t elapsed =
                                     app_->clock().now_nanos() - started_nanos_;
                                 // From the clock, not a step per tick. A late
                                 // frame covers more ground rather than making
                                 // the whole effect longer.
                                 const double progress =
                                     duration_nanos_ <= 0
                                         ? 1.0
                                         : static_cast<double>(elapsed) /
                                               static_cast<double>(duration_nanos_);
                                 if (progress >= 1.0) {
                                     finish();
                                     return;
                                 }
                                 if (on_frame_) on_frame_(std::clamp(progress, 0.0, 1.0));
                             });
}

void Animation::finish() {
    if (timer_ == 0 && !on_finished_) return;
    stop_timer();
    // Moved out before running: `on_finished` is allowed to start the next
    // run on this same object, and it must not be re-entering a state that
    // still describes the one that just ended.
    std::function<void()> finished = std::move(on_finished_);
    on_finished_ = nullptr;
    on_frame_ = nullptr;
    if (finished) finished();
}

}  // namespace ckv::ui
