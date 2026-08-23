// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TreeView: expand/collapse, keyboard navigation (the widget catalog
// M6b baseline). A forest of TreeNode value trees — reparenting or
// inserting/removing nodes while a TreeView displays them is not
// supported in v1 (pointers into the tree stay stable across
// expand/collapse since that only flips a bool, never reallocates,
// but adding/removing a sibling could reallocate a containing
// std::vector<TreeNode> and invalidate them — set_roots() with a
// freshly built forest is the sanctioned way to change the content).
//
// Per D-041, the 0.1 tree model is materialized: the whole forest is flattened
// to its visible entries on navigation/draw. A virtualized provider is
// post-0.1 scope; lazy expansion through on_expand_request remains part of the
// materialized model.
#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

enum class TreeConnectorStyle {
    Minimal,     // existing compact "+ "/"- " twisties
    Ascii,       // "+-"/"--"/"`-" printable connectors
    BoxDrawing,  // box-drawing connectors for capable terminals
    Outline,     // classic outline branches: ─+ groups, ── leaves, and │ ancestry guides
};

struct TreeNode {
    std::string label;
    std::vector<TreeNode> children;
    bool expanded = false;

    // Lazy population (M10/WP-22): false means "children not yet
    // listed" — a node in this state still shows an expander (twisty)
    // even though children is currently empty, since whether it HAS
    // children simply isn't known yet. TreeView::on_expand_request
    // fires the first time such a node is asked to expand, giving the
    // application one synchronous chance to populate `children` in
    // place; TreeView sets this true immediately afterward regardless
    // of whether anything was actually added — a genuinely empty
    // directory is thereafter indistinguishable from any other leaf,
    // which is the correct end state, not one that keeps re-asking on
    // every later expand attempt.
    bool children_known = true;

    // User payload (M10/WP-22), non-template so TreeNode itself stays
    // a plain, ABI-stable value type that doesn't need to know an
    // application's own payload type at TreeNode's own definition
    // site. `id` is for a caller that just wants a stable numeric
    // handle (a database row id, a file descriptor, an index into its
    // own side array); `user_data` is the general escape hatch for
    // anything else — e.g. the file browser stores each node's own
    // full path here, replacing what used to be a separate sidecar
    // std::unordered_map<const TreeNode*, std::string>.
    std::uint64_t id = 0;
    std::any user_data;

    // Whether this node should show an expander at all — true either
    // because it already has known children, or because whether it
    // has any is simply not known yet (children_known == false).
    // False only for a genuine, confirmed leaf.
    bool might_have_children() const noexcept { return !children.empty() || !children_known; }
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.list.normal"/"ckv.list.selected" (shared with
// ListView — the two widgets are visually siblings in the M6a
// scrolling/selection group); its embedded Scrollbar resolves its own
// roles independently.
class TreeView : public ui::View {
public:
    TreeView();

    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        normal_role_ = normal_role;
        selected_role_ = selected_role;
    }

    void set_connector_style(TreeConnectorStyle style);
    TreeConnectorStyle connector_style() const noexcept { return connector_style_; }

    void set_roots(std::vector<TreeNode> roots);
    TreeNode* selected() const noexcept { return cursor_node_; }

    // Fires whenever the cursor moves to a DIFFERENT node (navigation,
    // mouse click, Left-to-parent, or a set_roots() call whose
    // resulting selection is non-null) — never on a no-op move (e.g.
    // Down at the last entry), a pure expand/collapse of the already-
    // selected node, or a set_roots() call that leaves the tree empty
    // (nothing to pass a TreeNode& reference to). This is the hook a
    // master-detail pane (e.g. a file list showing the selected
    // folder's contents) subscribes to, mirroring
    // ListView::on_selection_changed.
    std::function<void(TreeNode&)> on_selection_changed;

    // Fires on Enter, or a second click on the already-selected node —
    // "act on this node" distinct from merely browsing to it, mirroring
    // ListView::on_activate. Fires regardless of whether the node also
    // has an expand/collapse state to toggle (a leaf's Enter must still
    // reach the application).
    std::function<void(TreeNode&)> on_activate;

    // Lazy population (M10/WP-22): fires the first (and only the
    // first) time a node whose children_known is false is asked to
    // expand — via Right, Enter/Space, or a twisty click. The
    // callback should populate `node.children` in place before
    // returning (synchronous — this framework has no async/threading
    // model for widgets to await); TreeView sets children_known true
    // itself immediately afterward regardless of what the callback
    // did. Unset by default: a node built with children_known left at
    // its own true default never triggers this at all.
    std::function<void(TreeNode&)> on_expand_request;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    struct VisibleEntry {
        TreeNode* node;
        int depth;
        TreeNode* parent;  // nullptr for a root — the parent-tracking pass Left-to-parent needs
        // Whether this node is the final child of its parent, and which
        // ancestor depths still have a following sibling. Branch drawing
        // needs both: the first picks the elbow over the tee, the second
        // says where a vertical stem passes through this row. A row knows
        // this only from its whole ancestry, so the flattening pass — the
        // one place that walks it — records it here.
        bool last_sibling = true;
        std::uint32_t stem_mask = 0;
    };

    void flatten_into(std::vector<TreeNode>& nodes, int depth, TreeNode* parent,
                       std::vector<VisibleEntry>& out, std::uint32_t stem_mask);
    std::vector<VisibleEntry> visible_entries();
    // Columns one nesting level occupies, which is also the width of a
    // branch prefix. The outline convention draws a junction, a rule and
    // a marker; the compact styles draw a two-cell twisty.
    int branch_columns() const noexcept;
    void move_cursor(int delta);
    // Jumps the cursor directly to `node` (Left-to-parent) — the
    // absolute-target counterpart to move_cursor's relative delta.
    void select_node(TreeNode* node);
    // Shared expand/collapse core: gates lazy population (see
    // on_expand_request) behind the false->true transition only, so
    // every expand call site (Right, Enter/Space, a twisty click)
    // gets the same lazy-population behavior from one place.
    void set_expanded(TreeNode& node, bool expanded);
    void ensure_cursor_visible(int cursor_index);

    std::vector<TreeNode> roots_;
    TreeNode* cursor_node_ = nullptr;
    Scrollbar* scrollbar_ = nullptr;
    TreeConnectorStyle connector_style_ = TreeConnectorStyle::Minimal;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
