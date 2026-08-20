// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/terminal_scrollbar.hpp"

#include <algorithm>
#include <memory>

namespace ckv::widgets {

Scrollbar* attach_terminal_scrollbar(Window& window, TerminalView& view) {
    auto owned = std::make_unique<Scrollbar>(Orientation::Vertical);
    Scrollbar* const bar =
        window.add_frame_overlay(std::move(owned), FrameSlot{Edge::Right, ui::Alignment::Fill});
    const auto sync = [bar, &view] {
        const TerminalView::ScrollState state = view.scroll_state();
        // Policy before range: on the alternate screen the bar is not merely
        // at its end, it has nothing to say — a full-screen program owns the
        // whole window. Auto elsewhere is the bar's own rule, shown exactly
        // while there is somewhere to go.
        bar->set_policy(state.primary_screen ? ScrollbarPolicy::Auto : ScrollbarPolicy::Hidden);
        bar->set_range(state.total_rows, std::max(1, state.viewport_rows));
        // The bar counts from the top of history; the view counts back from
        // the live edge. Same line, two ends of the same ruler.
        bar->set_position(bar->max_position() - state.offset);
    };
    bar->on_position_changed = [bar, &view](int position) {
        view.set_scrollback_offset(bar->max_position() - position);
    };
    view.on_scroll_state_changed = sync;
    sync();
    return bar;
}

}  // namespace ckv::widgets
