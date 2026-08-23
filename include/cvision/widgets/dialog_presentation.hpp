// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Typed, non-blocking standard-dialog completion (D-038). Factories
// retain the internal state until their Window detaches; callers retain
// DialogPresentation<Result> to inspect or handle that one completion.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "cvision/core/assert.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {

namespace detail {
template <class Result>
struct DialogPresentationAccess;

// A dialog may outlive the view that had focus when it was presented. Keep
// both its address and per-instance lifetime identity, so close completion
// can restore focus only when that exact former view still exists. The final
// scope and attachment checks remain Application's responsibility because it
// owns modal routing and focus eligibility.
class DialogFocusRestore {
public:
    explicit DialogFocusRestore(ui::View* view) noexcept
        : view_(view), liveness_(view != nullptr ? view->lifetime_token() : std::weak_ptr<void>{}) {}

    void restore(ui::Application& app) const {
        if (!liveness_.expired() && view_ != nullptr && view_->focusable()) app.set_focus(view_);
    }

private:
    ui::View* view_ = nullptr;
    std::weak_ptr<void> liveness_;
};
}

template <class Result>
class [[nodiscard]] DialogPresentation {
public:
    DialogPresentation(const DialogPresentation&) = delete;
    DialogPresentation& operator=(const DialogPresentation&) = delete;
    DialogPresentation(DialogPresentation&&) noexcept = default;
    DialogPresentation& operator=(DialogPresentation&&) noexcept = default;

    // Dropping the presentation withdraws the handler registered through
    // it. The handler is a capability the caller installed FROM somewhere,
    // and at every call site that does anything with it, it captures the
    // object it was installed from. The shared state, though, outlives
    // this object — the presented Window holds it too. Left installed, the
    // handler is therefore still called when that Window finally detaches,
    // and the last thing to detach every Window is
    // `Application::~Application`: by then a caller that lived in a
    // narrower scope than its Application is already gone. The editor
    // example's `close_confirmation_` is exactly that — an EditorApp
    // member, so EditorApp is destroyed one step before the Application
    // whose teardown calls back into it, and the capture reads a dead
    // stack frame.
    //
    // This is the rule D-038 already states for the OTHER capability a
    // presentation retains across a modal interval — "a saved focus
    // target is a per-instance lifetime capability, never an unchecked
    // raw pointer" — applied to the completion handler; and it is the
    // rule `Desktop::show_window_list` already hand-rolls, by capturing
    // the presentation's own shared_ptr inside its handler to hold it
    // open. A caller that wants the completion keeps the presentation; a
    // caller that drops it has declined the completion, which is what
    // dropping a [[nodiscard]] handle ought to mean.
    ~DialogPresentation() {
        if (state_ != nullptr) state_->completion_handler = nullptr;
    }

    bool completed() const noexcept { return state_ != nullptr && state_->completed_result.has_value(); }
    std::optional<Result> result() const { return state_ != nullptr ? state_->completed_result : std::nullopt; }

    // May be called once. If the Window detached before registration,
    // invokes the handler immediately on the owning UI thread.
    void set_completion_handler(std::function<void(Result)> handler) {
        CKV_ASSERT(state_ != nullptr);
        CKV_ASSERT(!state_->handler_set);
        state_->handler_set = true;
        state_->completion_handler = std::move(handler);
        if (state_->completed_result && state_->completion_handler) {
            auto completion = std::move(state_->completion_handler);
            completion(*state_->completed_result);
        }
    }

private:
    struct State {
        std::optional<Result> selected_result;
        std::optional<Result> completed_result;
        std::function<void(Result)> completion_handler;
        bool handler_set = false;
    };

    explicit DialogPresentation(std::shared_ptr<State> state) : state_(std::move(state)) {}

    std::shared_ptr<State> state_;

    friend struct detail::DialogPresentationAccess<Result>;
};

namespace detail {

template <class Result>
struct DialogPresentationAccess {
    using Presentation = DialogPresentation<Result>;
    using State = typename Presentation::State;

    struct Parts {
        Presentation presentation;
        std::shared_ptr<State> state;
    };

    static Parts make() {
        auto state = std::make_shared<State>();
        return Parts{Presentation{state}, std::move(state)};
    }

    static void record(const std::shared_ptr<State>& state, Result result) { state->selected_result = std::move(result); }

    static void finish(const std::shared_ptr<State>& state, Result fallback) {
        if (state->completed_result) return;
        state->completed_result = state->selected_result.value_or(std::move(fallback));
        if (state->completion_handler) {
            auto handler = std::move(state->completion_handler);
            handler(*state->completed_result);
        }
    }
};

}  // namespace detail

}  // namespace ckv::widgets
