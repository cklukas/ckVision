// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Animation (U4-k): one bounded, interruptible run of frames over the
// injected Clock — and the whole of what "animation" means in this toolkit.
//
// It is deliberately small, because a text user interface is not a game
// loop. There is no scene graph of animated properties, no timeline, no
// implicit transitions on setters: there is a duration, a callback that
// receives PROGRESS, and a callback that says the run is over. Everything an
// effect actually does — what it draws, where it draws it, what it
// interpolates — belongs to the effect.
//
// Three rules make it safe to build on, and they are the reason this is a
// type rather than a `start_timer` call at each site:
//
//   * **Progress comes from the clock, never from a frame count.** A host
//     that delivers half the frames sees the same run in the same wall time,
//     covering more ground per frame; a host that delivers none sees the run
//     end at its deadline having drawn nothing. This is what "degrades
//     honestly on a slow host" means concretely — the alternative, advancing
//     a fixed step per tick, turns a slow terminal into a slow-motion effect
//     that outlives the thing it was describing.
//
//   * **`on_finished` runs exactly once, for every ending.** Completed,
//     ended early, started with no duration at all: one terminal callback.
//     An effect therefore has exactly one place to tear its decoration down,
//     and cannot leak one by ending along a path its author forgot. The one
//     exception is destruction, which is silent — an owner being destroyed
//     is already tearing down, and calling back into it then would be
//     reaching through a half-destroyed object.
//
//   * **It never owns the end state.** An animation in this toolkit
//     describes a change that has ALREADY happened. The caller applies the
//     end state first and then, optionally, animates; nothing downstream can
//     depend on a frame ever being drawn, because by the time the first one
//     could be the state is already what it will be. That is what makes
//     "interruptible" cheap rather than delicate: there is no half-applied
//     state for an interruption to resolve, only a decoration to stop
//     drawing.
//
// The third rule is the design decision, and it is the opposite of how a
// retained-mode UI toolkit usually animates (where the animation drives the
// property and the end state arrives with the last frame). It is chosen
// because the failure it removes is the one that matters here: an effect
// that is skipped, cut short, or never ticked must not be able to leave the
// application in a state no reader asked for. See the decision log D-060.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "cvision/ui/application.hpp"

namespace ckv::ui {

class Animation {
public:
    // ~30 frames a second. Fast enough that a short flight reads as motion
    // rather than as three stills, slow enough that it is not competing with
    // a terminal's own redraw rate for a decoration nobody is measuring.
    static constexpr std::int64_t kDefaultFrameIntervalNanos = 33'000'000;

    Animation() = default;
    ~Animation();

    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;
    Animation(Animation&&) = delete;
    Animation& operator=(Animation&&) = delete;

    // Begins a run of `duration_nanos`, calling `on_frame` with progress in
    // [0, 1] about every `frame_interval_nanos`, and `on_finished` once when
    // it ends.
    //
    // `on_frame` is never called with a progress this run has already passed,
    // and the last value it sees before `on_finished` is not necessarily 1.0
    // — an effect that needs its end state drawn must draw it from
    // `on_finished`, or better, must already have applied it before calling
    // here (see the file comment). A duration of zero or less is a run that
    // ends immediately: `on_finished` and no frames at all, which is exactly
    // what "animations disabled" should cost a call site — one branch that
    // is not there.
    //
    // Starting a run while one is going ends the previous one first, so its
    // `on_finished` still runs and its decoration still goes.
    void start(Application& app, std::int64_t duration_nanos,
               std::function<void(double)> on_frame, std::function<void()> on_finished,
               std::int64_t frame_interval_nanos = kDefaultFrameIntervalNanos);

    // Ends the run now. `on_finished` runs; no further frames. Harmless when
    // nothing is running, so a host can call it on any input it likes without
    // asking first.
    void finish();

    bool running() const noexcept { return timer_ != 0; }

private:
    void stop_timer() noexcept;

    Application* app_ = nullptr;
    Application::TimerId timer_ = 0;
    std::int64_t started_nanos_ = 0;
    std::int64_t duration_nanos_ = 0;
    std::function<void(double)> on_frame_;
    std::function<void()> on_finished_;
    // Guards the callback the timer holds. A run can end from inside its own
    // frame callback — an effect that decides it is done, or a host that
    // finishes it while a frame is being drawn — and the timer may still fire
    // once afterwards.
    std::shared_ptr<int> alive_ = std::make_shared<int>(0);
};

}  // namespace ckv::ui
