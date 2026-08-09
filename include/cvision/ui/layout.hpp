// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Row and Column: explicit linear-container layouts computed in one
// top-down, non-iterative pass over each container's direct children
// (ARCHITECTURE.md §5 "Layout and dialog construction" — "computed in
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

enum class SizePolicy {
    Fixed,       // always exactly its preferred size, never grows or shrinks
    Minimum,     // grows/shrinks with the container but never below its size hint's min
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

struct LayoutSpec {
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
    int min = 0;
    int preferred = 0;
    SizePolicy policy = SizePolicy::Minimum;
    int weight = 1;
};

// Assigns each child a main-axis [offset, size] pair, in `children`
// order, packed left-to-right (or top-to-bottom) with `spacing`
// between consecutive children, fit into `available`. Pure function,
// no View dependency, so it is exhaustively testable in isolation.
std::vector<std::pair<int, int>> distribute_main_axis(const std::vector<LayoutChild>& children,
                                                        int available, int spacing);

class Row : public View {
public:
    using View::View;

    View* add_item(std::unique_ptr<View> child, LayoutSpec spec = {});
    std::unique_ptr<View> remove_item(View* child);

    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);

    // A container's own hints aggregate its children's (main axis:
    // sum plus spacing; cross axis: max) — this is what lets a Fixed-
    // policy Row of buttons nested inside a Column get its natural
    // height rather than the bare-View default of preferred 0, under
    // which a nested container silently collapsed to zero cells.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

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

class Column : public View {
public:
    using View::View;

    View* add_item(std::unique_ptr<View> child, LayoutSpec spec = {});
    std::unique_ptr<View> remove_item(View* child);

    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);

    // See Row: main axis (vertical here) sums, cross axis maxes.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

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
