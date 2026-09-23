// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <memory>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/orientation.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// The widget catalog M6a catalog: "Keyboard-adjustable split panes."
// Splitter owns exactly two panes (first()/second()) separated by a
// one-cell divider bar it draws and holds keyboard focus on itself —
// unlike Row/Column/Grid/Dock/Overlay (ui:: layer layout primitives
// with no interaction of their own), Splitter is a genuine widget: a
// focusable, drawing, key-handling view, which is why it lives in
// widgets:: rather than ui::.
//
// `orientation` follows the panes' own arrangement (Horizontal: panes
// side by side, a vertical divider bar, Left/Right adjust; Vertical:
// panes stacked, a horizontal divider bar, Up/Down adjust) — the same
// convention Qt's QSplitter uses. `split_position()` is the first
// pane's own main-axis extent in cells (NOT counting the divider);
// the second pane always gets whatever remains. This absolute-cell
// position, not a proportional ratio, is deliberately what persists
// across a later resize — the standard splitter UX (VS Code, browser
// dev tools, most IDEs): the pane the user explicitly sized keeps that
// size, and the OTHER pane is what grows or shrinks with the window.
// The constructor's own default position (half of the initial bounds)
// is what gives a freshly built Splitter its exact 50/50 starting
// split.
//
// The divider is also draggable with the pointer. Keyboard adjustment
// is the catalog's own baseline and remains the complete way to work
// this widget without a mouse, but a divider that can only be moved
// after being focused is one most readers never discover: the pointer
// gesture is what a splitter looks like it affords, and a widget that
// declines the gesture it advertises reads as broken rather than as
// keyboard-first. Press the divider, move, release; the position runs
// through the same clamped set_split_position() the keys use, so both
// routes obey one rule about how small a pane may get.
//
// Which pane keeps its size is a choice (set_resize_anchor): the first by
// default, as above, and the second for a side panel on the far edge — a
// preview beside a document keeps its width while the window, and with it
// the document, grows. The anchored pane's extent is remembered as asked for
// (set_anchored_extent), not as last clamped, so a window made briefly too
// small gives the panel its width back when it grows again, and a width
// restored before the splitter has any size of its own is honoured at its
// first layout.
//
// A pane that is not visible takes no room: the other one fills the splitter,
// no divider is drawn or grabbed, and the splitter is no tab stop until both
// panes show again — so hiding a side panel is set_visible(false) on it, with
// nothing left behind where it was.
//
// Resolves its own theme roles from context() once attached (M9 WP-7,
// D-028): "ckv.splitter.normal/focused".
enum class SplitterPane { First, Second };

class Splitter : public View {
public:
    Splitter(Rect bounds, std::unique_ptr<View> first, std::unique_ptr<View> second,
             Orientation orientation = Orientation::Horizontal);

    View* first() const noexcept { return first_; }
    View* second() const noexcept { return second_; }
    Orientation orientation() const noexcept { return orientation_; }

    int split_position() const noexcept { return split_position_; }
    // Clamped to leave both panes at least their own size hint's min
    // (best-effort: a bounds too small for both mins to fit at once is
    // left to painting-time clipping, the same "v1 scope" distribute_
    // main_axis's own comment already documents for that case).
    void set_split_position(int position);

    // The pane that keeps its extent when the splitter itself is resized.
    // First by default. Changing it keeps the current arrangement.
    void set_resize_anchor(SplitterPane pane);
    SplitterPane resize_anchor() const noexcept { return anchor_; }
    // The extent, in cells along the main axis, the anchored pane is asked to
    // have: kept across resizes, clamped only when laid out. What an
    // application remembers and restores.
    int anchored_extent() const noexcept { return anchored_extent_; }
    void set_anchored_extent(int extent);

    // Whether one pane is hidden and the other fills the splitter.
    bool collapsed() const noexcept;

    // Fired after the reader moved the divider, by key or by pointer. A
    // programmatic change does not fire it: its caller already knows.
    std::function<void()> on_split_moved;

    // Overrides the theme-resolved divider roles. A splitter on a dialog
    // surface otherwise keeps the document-window colouring its theme
    // gives it, which reads as a seam of another window's chrome lying
    // across the panel — the same reason ListView and Scrollbar each
    // carry one of these.
    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalidate();
    }

    // Whether the divider is being dragged right now.
    bool dragging() const noexcept { return dragging_; }

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_resized() override { relayout(); }
    // A pane's own hint changing (M9/WP-16) can shift its own min,
    // which set_split_position's own clamp depends on — re-clamp and
    // relayout exactly like a resize would.
    void on_child_size_hint_changed(View&) override { relayout(); }
    void on_attached() override;

private:
    void relayout();
    int clamp_split(int position) const;
    int main_extent() const noexcept;
    // The split position the anchored extent asks for, before clamping.
    int requested_split() const noexcept;
    // The divider moved to `position` because the reader moved it.
    void move_split(int position);

    static constexpr int kDividerExtent = 1;

    View* first_ = nullptr;
    View* second_ = nullptr;
    Orientation orientation_;
    int split_position_ = 0;
    SplitterPane anchor_ = SplitterPane::First;
    int anchored_extent_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    bool dragging_ = false;
};

}  // namespace ckv::widgets
