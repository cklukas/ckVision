// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Row and Column: explicit linear-container layouts computed in one
// top-down, non-iterative pass over each container's direct children
// (the architecture §5 "Layout and dialog construction" — "computed in
// one top-down pass over dirty subtrees"). Grid/Dock/Overlay (D-006's
// remaining container-set members, M10/WP-19) live in their own files
// — ui/grid.hpp, ui/dock.hpp, ui/overlay.hpp — since each has its own
// distinct layout algorithm; this file stays scoped to the linear,
// main-axis-plus-cross-axis case.
//
// Cross-axis sizing (M10/WP-19/WP-36): a child's own cross-axis extent
// defaults to filling the container (Alignment::Fill, matching every
// Row/Column built before this landed) — LayoutSpec::alignment opts a
// child OUT into Start/Center/End, sized to its own cross-axis preferred
// extent instead. For vertical extent after a width is known, that preferred
// extent includes View::height_for_width(width), so wrapped text participates
// in actual placement rather than remaining a widget-only query.
#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "cvision/ui/view.hpp"

namespace ckv::ui {

// How a Row or Column child's main-axis extent responds to the space the container has. When
// there is room to spare only Expanding children grow, each no further than its size hint's max;
// when there is too little, Minimum and Expanding children shrink toward their min in proportion
// to how far each can shrink, and any deficit left after that overflows the container and is
// clipped when painted.
enum class SizePolicy {
    // Always exactly its preferred size, even when that overflows the container.
    Fixed,       // always exactly its preferred size, never grows or shrinks
    // Its preferred size when there is room; shrinks toward its size hint's min when there is not,
    // and never grows past preferred.
    Minimum,     // shrinks with the container but never below its size hint's min; never grows
    // Like Minimum, but also claims a weighted share of any leftover space, up to its size hint's
    // max. What a capped child cannot take is shared among the other Expanding children; what
    // none of them can take stays empty after the last child.
    Expanding,   // like Minimum, but also claims a share of any leftover space
};

// Cross-axis positioning within whatever space margins leave available
// (M10/WP-19) — orthogonal to SizePolicy, which is main-axis only.
// Shared with Grid, whose own per-cell alignment uses the same values.
enum class Alignment {
    Fill,    // the container's own default: takes the whole cross extent
    Start,   // top (Row) / left (Column), sized to its own preferred hint
    Center,
    End,     // bottom (Row) / right (Column)
};

// How one Row or Column child is placed, given when it is added with add_item(). A child added
// through View::add_child() instead is laid out with the defaults.
struct LayoutSpec {
    // Main-axis policy, then its weight: the child's relative share of leftover space among
    // Expanding siblings (ignored for the other policies; when every Expanding sibling has weight
    // 0 the space is split evenly). Last, the cross-axis alignment.
    SizePolicy policy = SizePolicy::Minimum;
    int weight = 1;  // relative share of leftover space among Expanding siblings
    Alignment alignment = Alignment::Fill;
    // Reserved space before/after the child on the CROSS axis only —
    // top/bottom for a Row, left/right for a Column. The main axis
    // already has `Row::spacing()`/`Column::spacing()` for the
    // between-children case; margins are the per-child, cross-axis
    // equivalent (e.g. a Fill-aligned child that should not quite
    // touch the container's own top/bottom edge).
    int margin_before = 0;
    int margin_after = 0;

    // Memberwise equality.
    friend bool operator==(const LayoutSpec&, const LayoutSpec&) = default;
};

// Computes ONE child's cross-axis [offset, extent] within `available`
// cross-axis space, given its own cross-axis preferred size hint,
// alignment, and margins. Pure function, no View dependency — the
// cross-axis analogue of distribute_main_axis, and exhaustively
// testable in isolation the same way.
std::pair<int, int> align_cross_axis(int available, int preferred, Alignment alignment,
                                      int margin_before, int margin_after) noexcept;

// Shared main-axis distribution math (declared here so it is directly
// unit-testable without a View tree; Row/Column call it).
struct LayoutChild {
    // The child's main-axis size hint in cells and its LayoutSpec policy and weight — everything
    // distribute_main_axis() needs about one child. `max` bounds only Expanding growth
    // (kUnboundedExtent: none); a max below `preferred` is read as `preferred`.
    int min = 0;
    int preferred = 0;
    int max = kUnboundedExtent;
    SizePolicy policy = SizePolicy::Minimum;
    int weight = 1;
};

// Assigns each child a main-axis [offset, size] pair, in `children`
// order, packed left-to-right (or top-to-bottom) with `spacing`
// between consecutive children, fit into `available`. Pure function,
// no View dependency, so it is exhaustively testable in isolation.
std::vector<std::pair<int, int>> distribute_main_axis(const std::vector<LayoutChild>& children,
                                                        int available, int spacing);

// A container that places its visible children left to right, each as tall as the row allows
// (or as its LayoutSpec alignment and margins say), sharing the width by SizePolicy. Relayout
// runs on every resize, spacing change, item change, and child size-hint change.
class Row : public View {
public:
    // Same constructor as View: parent-local bounds and a focus policy, both usually defaulted.
    using View::View;

    // Adds `child` at the right end with its placement `spec` and relays out. `child` must not be
    // null. Returns the child as add_child() does, nullptr — keeping no spec — when its
    // attachment callback detached or destroyed it.
    View* add_item(std::unique_ptr<View> child, LayoutSpec spec = {});
    // Detaches `child` exactly as View::remove_child() does, then forgets its spec, relays out
    // the rest and returns ownership; nullptr (and no relayout) when `child` is not a child of
    // this row. Every way a child leaves — plain remove_child(), detach_child(), a view closing
    // itself — comes through here, so no spec of a departed child outlives it.
    std::unique_ptr<View> remove_child(View* child) override;

    // Blank columns between consecutive visible children (0 by default). set_spacing() asserts
    // `spacing` >= 0 and relays out when it changes.
    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);

    // Every visible child shares this row, so the row's last line is a shadow
    // only when all of them end in one -- which is what a row of nothing but
    // buttons is, and what a row mixing a button with a label is not. Hidden
    // children put nothing there and are not asked; with none visible, false.
    bool trailing_row_is_shadow() const noexcept override;

    // A container's own hints aggregate its visible children's min and
    // preferred (main axis: sum plus spacing; cross axis: the largest); its
    // max is unbounded on both axes, because a row given more room than its
    // children can use leaves the rest empty. This is what lets a Fixed-
    // policy Row of buttons nested inside a Column get its natural
    // height rather than the bare-View default of preferred 0, under
    // which a nested container silently collapsed to zero cells.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // A container that reports intrinsic sizes must answer the
    // height-for-width query too, or the one sanctioned second pass
    // (the architecture §5) stops at it and every wrapped descendant is
    // measured as though width did not matter.
    int height_for_width(int width) const override;

    void on_resized() override { relayout(); }
    // A child's own hint changing (M9/WP-16) needs the exact same
    // response as this Row's own bounds changing: this Row's aggregate
    // hint is never cached (computed fresh from children() every call),
    // so there is nothing to invalidate beyond re-running the pass.
    void on_child_size_hint_changed(View&) override { relayout(); }

private:
    void relayout();

    int spacing_ = 0;
    std::unordered_map<View*, LayoutSpec> specs_;
};

// A container that places its visible children top to bottom, each as wide as the column allows
// (or as its LayoutSpec alignment and margins say), sharing the height by SizePolicy. A child's
// preferred height is asked at the width it will get, so wrapped text is measured correctly.
// Relayout runs on the same triggers as Row.
class Column : public View {
public:
    // Same constructor as View: parent-local bounds and a focus policy, both usually defaulted.
    using View::View;

    // Adds `child` at the bottom with its placement `spec` and relays out. `child` must not be
    // null. Returns the child as add_child() does, nullptr — keeping no spec — when its
    // attachment callback detached or destroyed it.
    View* add_item(std::unique_ptr<View> child, LayoutSpec spec = {});
    // As Row::remove_child(): detaches `child`, forgets its spec, relays out the rest and
    // returns ownership; nullptr (and no relayout) when `child` is not a child of this column.
    std::unique_ptr<View> remove_child(View* child) override;

    // Blank rows between consecutive visible children (0 by default). set_spacing() asserts
    // `spacing` >= 0 and relays out when it changes.
    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);

    // The column's last row belongs to its last visible item, so the question
    // goes there; with none visible, false.
    bool trailing_row_is_shadow() const noexcept override;

    // See Row: main axis (vertical here) sums, cross axis maxes.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // See Row: the height-for-width query passes through a container that
    // reports intrinsic sizes. Here it is the main axis, so the children's
    // answers sum — each asked at the width this Column would give it.
    int height_for_width(int width) const override;

    void on_resized() override { relayout(); }
    // See Row: this Column's own aggregate hint is never cached, so a
    // child's own hint changing (M9/WP-16) just re-runs the same pass.
    void on_child_size_hint_changed(View&) override { relayout(); }

private:
    void relayout();

    int spacing_ = 0;
    std::unordered_map<View*, LayoutSpec> specs_;
};

}  // namespace ckv::ui
