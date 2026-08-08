// Copyright (c) 2026 C. Klukas. All rights reserved.
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

namespace ckv::widgets {

namespace detail {
template <class Result>
struct DialogPresentationAccess;
}

template <class Result>
class [[nodiscard]] DialogPresentation {
public:
    DialogPresentation(const DialogPresentation&) = delete;
    DialogPresentation& operator=(const DialogPresentation&) = delete;
    DialogPresentation(DialogPresentation&&) noexcept = default;
    DialogPresentation& operator=(DialogPresentation&&) noexcept = default;

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
