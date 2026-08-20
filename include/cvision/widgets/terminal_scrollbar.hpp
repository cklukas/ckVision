// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The scrollbar a terminal window's frame carries — the classic desktop's
// own place for one: a vertical Scrollbar standing on the window's right
// border, bound both ways to a TerminalView's scrollback. It is on screen
// exactly while the terminal is on its primary screen with more rows than
// the view can show — the moments a reader can actually go somewhere — and
// stands down otherwise. Living on the border, its coming and going costs
// the terminal nothing: not a column, and not a resize the child would have
// to reflow for. The border cell was always there.
#pragma once

#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// Installs the bar on `window`'s right border and wires it to `view`:
// dragging, paging or arrow-clicking the bar moves the view's scrollback,
// and the view's own movements — the wheel, PageUp/PageDown, history
// growth, the buffer flipping — move the bar. Claims
// `view.on_scroll_state_changed` as the sync path, so a host that needs
// that seam for itself wires the two by hand instead. `view` is typically
// `window`'s content; both belong to the window, which is what keeps the
// two-way wiring's lifetimes honest. Returns the bar, owned by `window`.
Scrollbar* attach_terminal_scrollbar(Window& window, TerminalView& view);

}  // namespace ckv::widgets
