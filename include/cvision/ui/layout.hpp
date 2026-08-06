// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Row and Column: explicit linear-container layouts computed in one
// top-down, non-iterative pass over each container's direct children
// (ARCHITECTURE.md §5 "Layout and dialog construction" — "computed in
// one top-down pass over dirty subtrees"). Grid/Dock/Overlay are a
// natural follow-on once a widget exists that actually needs them;
// Row/Column alone already cover every M4 exit-criteria form (a
// labeled input line stacked in a dialog).
//
// v1 scope (documented, not an oversight): cross-axis sizing always
// fills the container's cross extent — a Row's children are all as
// tall as the Row, a Column's children are all as wide as the Column.
// Per-child cross-axis alignment/sizing is a straightforward future
// extension once a widget wants to be narrower than its row.
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

struct LayoutSpec {
    SizePolicy policy = SizePolicy::Minimum;
    int weight = 1;  // relative share of leftover space among Expanding siblings

    friend bool operator==(const LayoutSpec&, const LayoutSpec&) = default;
};

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

    void on_resized() override { relayout(); }

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

    void on_resized() override { relayout(); }

private:
    void relayout();

    int spacing_ = 0;
    std::unordered_map<View*, LayoutSpec> specs_;
};

}  // namespace ckv::ui
