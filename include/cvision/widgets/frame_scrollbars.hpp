// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Window-frame scrollbars (D-051, D-166): mounts a scrollable view's vertical
// and/or horizontal Scrollbars directly onto the enclosing Window's border
// overlays (Edge::Right and Edge::Bottom with Alignment::Fill). Living on the
// border, the bars consume zero interior cells from the content area while
// leaving the window's corner resize grips uncovered and sharing the bottom
// border cleanly with FrameText status readouts or Window::footer().
#pragma once

#include "cvision/core/export.hpp"

#include <memory>
#include <utility>

#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// Non-owning pointers to the frame-mounted scrollbars installed on a Window.
struct CVISION_TYPE_API FrameScrollbars {
    // Vertical scrollbar mounted on the window frame, or nullptr when absent.
    Scrollbar* vertical = nullptr;
    // Horizontal scrollbar mounted on the window frame, or nullptr when absent.
    Scrollbar* horizontal = nullptr;
};

namespace detail {

// Replaces any superseded Scrollbar occupying `slot` on `window` and mounts `owned`.
inline Scrollbar* mount_frame_scrollbar(Window& window, std::unique_ptr<Scrollbar> owned, FrameSlot slot) {
    if (!owned) return nullptr;
    if (auto* prior = dynamic_cast<Scrollbar*>(window.frame_overlay(slot.edge, slot.alignment))) {
        (void)window.remove_frame_overlay(prior);
    }
    return window.add_frame_overlay(std::move(owned), slot);
}

}  // namespace detail

// Detaches `view`'s vertical Scrollbar via `take_vertical_scrollbar()` and
// mounts it on `window` (Edge::Right, Alignment::Fill by default), replacing
// any superseded Scrollbar already occupying that border slot. Returns the
// Window-owned bar, or nullptr when `view` already handed its bar away.
template <class Scrollable>
    requires requires(Scrollable& v) { v.take_vertical_scrollbar(); }
Scrollbar* attach_vertical_frame_scrollbar(
    Window& window, Scrollable& view,
    FrameSlot slot = FrameSlot{Edge::Right, ui::Alignment::Fill}) {
    return detail::mount_frame_scrollbar(window, view.take_vertical_scrollbar(), slot);
}

// Detaches `view`'s horizontal Scrollbar via `take_horizontal_scrollbar()` and
// mounts it on `window` (Edge::Bottom, Alignment::Fill by default), replacing
// any superseded Scrollbar already occupying that border slot. Returns the
// Window-owned bar, or nullptr when `view` already handed its bar away.
template <class Scrollable>
    requires requires(Scrollable& v) { v.take_horizontal_scrollbar(); }
Scrollbar* attach_horizontal_frame_scrollbar(
    Window& window, Scrollable& view,
    FrameSlot slot = FrameSlot{Edge::Bottom, ui::Alignment::Fill}) {
    return detail::mount_frame_scrollbar(window, view.take_horizontal_scrollbar(), slot);
}

// Mounts every scrollbar `view` exposes (`take_vertical_scrollbar()` on the
// right border and, when `view` also provides `take_horizontal_scrollbar()`,
// the horizontal bar along the bottom border) onto `window`.
template <class Scrollable>
    requires(requires(Scrollable& v) { v.take_vertical_scrollbar(); } ||
             requires(Scrollable& v) { v.take_horizontal_scrollbar(); })
FrameScrollbars attach_frame_scrollbars(Window& window, Scrollable& view) {
    FrameScrollbars out;
    if constexpr (requires { view.take_vertical_scrollbar(); }) {
        out.vertical = attach_vertical_frame_scrollbar(window, view);
    }
    if constexpr (requires { view.take_horizontal_scrollbar(); }) {
        out.horizontal = attach_horizontal_frame_scrollbar(window, view);
    }
    return out;
}

// Installs a vertical Scrollbar on `window` (Edge::Right, Alignment::Fill by
// default) and binds it both ways to `view`'s primary-screen scrollback: shown
// in Auto mode on the primary screen while history exceeds the viewport,
// hidden on the alternate screen, and consuming zero terminal columns without
// claiming `view.on_scroll_state_changed`. Returns nullptr when `view` already
// has an active frame scrollbar mounted.
CVISION_API Scrollbar* attach_vertical_frame_scrollbar(
    Window& window, TerminalView& view,
    FrameSlot slot = FrameSlot{Edge::Right, ui::Alignment::Fill});

// Mounts `view`'s scrollback bar on `window`'s right border.
CVISION_API FrameScrollbars attach_frame_scrollbars(Window& window, TerminalView& view);

}  // namespace ckv::widgets
