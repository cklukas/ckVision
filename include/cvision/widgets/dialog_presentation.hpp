// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Typed, non-blocking standard-dialog completion (D-038). Factories
// retain the internal state until their Window detaches; callers retain
// DialogPresentation<Result> to inspect or handle that one completion.
#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <type_traits>
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
    // Remembers `view` (null means "restore nothing") together with a weak
    // reference to its lifetime token. The view is not owned or kept alive.
    explicit DialogFocusRestore(ui::View* view) noexcept
        : view_(view), liveness_(view != nullptr ? view->lifetime_token() : std::weak_ptr<void>{}) {}

    // Asks `app` to focus the remembered view, but only while that exact view
    // is still alive and currently focusable; otherwise does nothing.
    void restore(ui::Application& app) const {
        if (!liveness_.expired() && view_ != nullptr && view_->focusable()) app.set_focus(view_);
    }

private:
    ui::View* view_ = nullptr;
    std::weak_ptr<void> liveness_;
};
}

// The caller's handle on one non-blocking standard-dialog presentation, typed
// by the dialog family's result. Only the family's `present_modal_*` or
// `present_modeless_*` function creates one. The dialog records its result
// while it is on screen; the presentation completes exactly once, after the
// dialog's Window has detached, with the recorded result or — when none was
// recorded (close control, external detach, quit) — the family's documented
// fallback. Move-only; a moved-from
// handle reports no completion and must not register a handler.
template <class Result>
class [[nodiscard]] DialogPresentation {
public:
    // Move-only: one handle per presentation, so exactly one owner decides
    // whether the completion is still wanted.
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
    // rule `Desktop::present_modal_window_list` already hand-rolls, by capturing
    // the presentation's own shared_ptr inside its handler to hold it
    // open. A caller that wants the completion keeps the presentation; a
    // caller that drops it has declined the completion, which is what
    // dropping a [[nodiscard]] handle ought to mean.
    ~DialogPresentation() {
        if (state_ != nullptr) state_->completion_handler = nullptr;
    }

    // Whether the presentation has completed (its Window has detached), and
    // the result it completed with — empty until then, and always empty on a
    // moved-from handle. Polling these is an alternative to a handler.
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

    // The library's dialog factories construct presentations and record and
    // finish results through this accessor; applications cannot.
    friend struct detail::DialogPresentationAccess<Result>;
};

// The dialogs an owner is waiting on, each kept exactly as long as it is open.
//
// A presentation's completion reaches its handler only while the presentation
// is kept (see ~DialogPresentation), so an application keeps one per dialog it
// awaits — which, for a single member per dialog kind, is one optional per
// question the application can ask. An application that asks many questions
// wants the rule, not the members: hand each presentation over with what to do
// with its answer, and it is released as that answer arrives. The completion
// runs after the release, so it may present the next dialog in a chain.
//
// Destroying the set withdraws every completion still outstanding, which is
// the same promise dropping one presentation makes: an owner that is gone is
// never called back.
class PendingDialogs {
public:
    // Starts empty. Not copyable: the completions it holds capture this set's
    // address, so it must stay where it was made while any are outstanding.
    PendingDialogs() = default;
    PendingDialogs(const PendingDialogs&) = delete;
    PendingDialogs& operator=(const PendingDialogs&) = delete;

    // Keeps `presentation` until it completes, then runs `on_complete` with
    // its result. A presentation that has already completed runs it at once.
    template <class Result>
    void await(DialogPresentation<Result> presentation,
               std::type_identity_t<std::function<void(Result)>> on_complete) {
        auto held = std::make_unique<Held<Result>>(std::move(presentation));
        Held<Result>* const raw = held.get();
        entries_.push_back(std::move(held));
        const auto position = std::prev(entries_.end());
        // The presentation moves this handler out of its state before calling
        // it, so erasing the entry that owns the presentation cannot destroy
        // the closure while it runs.
        raw->presentation.set_completion_handler(
            [this, position, completion = std::move(on_complete)](Result result) {
                entries_.erase(position);
                if (completion) completion(std::move(result));
            });
    }

    // Keeps `presentation` open with nothing to do on completion — a notice
    // whose only answer is that the reader has read it.
    template <class Result>
    void await(DialogPresentation<Result> presentation) {
        await(std::move(presentation), std::function<void(Result)>{});
    }

    // How many dialogs are still open.
    std::size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

private:
    struct Entry {
        virtual ~Entry() = default;
    };
    template <class Result>
    struct Held final : Entry {
        explicit Held(DialogPresentation<Result> kept) : presentation(std::move(kept)) {}
        DialogPresentation<Result> presentation;
    };

    std::list<std::unique_ptr<Entry>> entries_;
};

namespace detail {

// Library-internal: how a dialog family's presentation function builds a
// DialogPresentation and drives its shared state. Not for application use.
template <class Result>
struct DialogPresentationAccess {
    // The handle type and its private shared state.
    using Presentation = DialogPresentation<Result>;
    using State = typename Presentation::State;

    // A fresh handle for the caller plus a second reference to the same state
    // for the factory, which keeps it alive until the Window detaches.
    struct Parts {
        // Returned to the caller of the family's presentation function.
        Presentation presentation;
        // Retained by the dialog's own callbacks.
        std::shared_ptr<State> state;
    };

    // Creates a new, not-yet-completed presentation and its state.
    static Parts make() {
        auto state = std::make_shared<State>();
        return Parts{Presentation{state}, std::move(state)};
    }

    // Records the result the dialog was dismissed with, while it is still
    // attached. A later record overwrites an earlier one; nothing completes
    // until finish.
    static void record(const std::shared_ptr<State>& state, Result result) { state->selected_result = std::move(result); }

    // Completes the presentation once, called when the Window detaches: the
    // recorded result if any, otherwise `fallback`. Calls the registered
    // handler (moved out first, so it runs at most once); later calls do
    // nothing.
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
