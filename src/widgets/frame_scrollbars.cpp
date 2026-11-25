// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/frame_scrollbars.hpp"

#include <algorithm>
#include <memory>

#include "widgets/frame_scrollbar_internal.hpp"

namespace ckv::widgets {

Scrollbar* attach_vertical_frame_scrollbar(Window& window, TerminalView& view, FrameSlot slot) {
    if (view.frame_scrollbar_ != nullptr && !view.frame_scrollbar_liveness_.expired() &&
        view.frame_scrollbar_->parent() != nullptr) {
        return nullptr;
    }
    auto owned = std::make_unique<Scrollbar>(Orientation::Vertical);
    owned->bind_subject(&view);
    Scrollbar* const bar = detail::mount_frame_scrollbar(window, std::move(owned), slot);
    if (bar == nullptr) return nullptr;
    const std::weak_ptr<void> bar_alive = bar->lifetime_token();
    const std::weak_ptr<void> view_alive = view.lifetime_token();
    const auto sync = [bar, bar_alive, &view, view_alive] {
        if (bar_alive.expired() || view_alive.expired()) return;
        const TerminalView::ScrollState state = view.scroll_state();
        // Policy before range: on the alternate screen the bar is not merely
        // at its end, it has nothing to say — a full-screen program owns the
        // whole window. Auto elsewhere is the bar's own rule, shown exactly
        // while there is somewhere to go.
        const bool eligible = state.primary_screen && detail::subject_visible_for_bar(view, *bar);
        bar->set_policy(eligible ? ScrollbarPolicy::Auto : ScrollbarPolicy::Hidden);
        bar->set_range(state.total_rows, std::max(1, state.viewport_rows));
        // The bar counts from the top of history; the view counts back from
        // the live edge. Same line, two ends of the same ruler.
        bar->set_position(bar->max_position() - state.offset);
    };
    bar->on_change = [bar, bar_alive, &view, view_alive](int position) {
        if (bar_alive.expired()) return;
        if (view_alive.expired()) {
            bar->set_policy(ScrollbarPolicy::Hidden);
            return;
        }
        view.set_scrollback_offset(bar->max_position() - position);
    };
    view.frame_scrollbar_ = bar;
    view.frame_scrollbar_liveness_ = bar_alive;
    view.frame_scrollbar_sync_ = sync;
    sync();
    return bar;
}

FrameScrollbars attach_frame_scrollbars(Window& window, TerminalView& view) {
    FrameScrollbars out;
    out.vertical = attach_vertical_frame_scrollbar(window, view);
    return out;
}

}  // namespace ckv::widgets
