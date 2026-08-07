// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <memory>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/orientation.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// plans/widgets.md M6a catalog: "Keyboard-adjustable split panes."
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
// Mouse-drag resizing is deliberately out of v1 scope: the catalog's
// own spec for this widget is "keyboard-adjustable," and that is what
// this class implements — matching Grid's own "v1 scope" precedent of
// implementing exactly the documented baseline, not the superset of
// everything a real-world splitter widget could eventually grow.
//
// Resolves its own theme roles from context() once attached (M9 WP-7,
// D-028): "ckv.splitter.normal/focused".
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

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
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

    static constexpr int kDividerExtent = 1;

    View* first_ = nullptr;
    View* second_ = nullptr;
    Orientation orientation_;
    int split_position_ = 0;
    bool has_focus_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
