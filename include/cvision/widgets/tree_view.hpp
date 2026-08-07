// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// TreeView: expand/collapse, keyboard navigation (plans/widgets.md
// M6b baseline). A forest of TreeNode value trees — reparenting or
// inserting/removing nodes while a TreeView displays them is not
// supported in v1 (pointers into the tree stay stable across
// expand/collapse since that only flips a bool, never reallocates,
// but adding/removing a sibling could reallocate a containing
// std::vector<TreeNode> and invalidate them — set_roots() with a
// freshly built forest is the sanctioned way to change the content).
//
// Scope note (documented, not an oversight): no virtualized model
// (explicitly beyond-baseline) — the whole forest is flattened to its
// visible entries on every navigation/draw, fine for the tree sizes a
// TUI actually shows. Left collapses the current node (or does
// nothing if already collapsed/a leaf) rather than also jumping to the
// parent — TreeNode carries no parent pointer in v1, so "jump to
// parent" isn't reachable without a separate parent-tracking pass; a
// real, later increment if a concrete use case needs it, not a gap
// hiding a design flaw today.
#pragma once

#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

struct TreeNode {
    std::string label;
    std::vector<TreeNode> children;
    bool expanded = false;
};

class TreeView : public ui::View {
public:
    TreeView(const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId selected_role, ui::RoleId track_role,
              ui::RoleId thumb_role);

    void set_roots(std::vector<TreeNode> roots);
    TreeNode* selected() const noexcept { return cursor_node_; }

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    struct VisibleEntry {
        TreeNode* node;
        int depth;
    };

    void flatten_into(std::vector<TreeNode>& nodes, int depth, std::vector<VisibleEntry>& out);
    std::vector<VisibleEntry> visible_entries();
    void move_cursor(int delta);
    void ensure_cursor_visible(int cursor_index);

    std::vector<TreeNode> roots_;
    TreeNode* cursor_node_ = nullptr;
    Scrollbar* scrollbar_ = nullptr;

    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId selected_role_;
};

}  // namespace ckv::widgets
