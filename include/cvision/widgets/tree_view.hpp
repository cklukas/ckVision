// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TreeView: expand/collapse, keyboard navigation, and caller-owned provider
// trees (the widget catalog M6b baseline, D-043). Compact static clients may
// still use a forest of TreeNode values. Those visible entries are flattened
// after roots or expansion state changes and retained for steady-state drawing
// and navigation. TreeModel serves large or refreshable hierarchies: TreeView
// resolves only visible paths and retains expansion/cursor state by stable id.
#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

using TreeItemId = std::uint64_t;
inline constexpr TreeItemId kInvalidTreeItemId = 0;

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

// The compact value returned by a TreeModel for one requested node. It does
// not contain children or expansion state: hierarchy belongs to the model and
// expansion belongs to TreeView. `children_known == false` keeps an expander
// visible while a caller arranges loading and later calls model_changed().
struct TreeItem {
    std::string label;
    bool children_known = true;
    std::any user_data;
};

// A synchronous, caller-owned hierarchy provider. Every id is non-zero,
// stable for as long as its node exists, and unique in the forest. Reverse
// parent/index lookups let TreeView preserve expansion and selection through a
// refresh without enumerating every sibling. Calls occur only on the UI thread;
// a provider never starts work or calls back into a widget from a worker.
class TreeModel {
public:
    virtual ~TreeModel() = default;

    virtual std::size_t root_count() const = 0;
    virtual TreeItemId root_id_at(std::size_t root_index) const = 0;
    virtual std::optional<std::size_t> root_index_of(TreeItemId id) const = 0;
    // A root has no parent. A non-root item must return its stable parent id.
    virtual std::optional<TreeItemId> parent_id_of(TreeItemId id) const = 0;
    virtual std::size_t child_count(TreeItemId parent) const = 0;
    virtual TreeItemId child_id_at(TreeItemId parent, std::size_t child_index) const = 0;
    virtual std::optional<std::size_t> child_index_of(TreeItemId parent, TreeItemId child) const = 0;
    // A missing id returns std::nullopt. TreeView discards stale selection and
    // expansion state deterministically when model_changed() observes this.
    virtual std::optional<TreeItem> item(TreeItemId id) const = 0;
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

    // Borrows `model`; it must outlive this TreeView or be replaced/cleared
    // before destruction. Changing models clears view state; model_changed()
    // retains surviving stable identities across a refresh.
    void set_model(TreeModel& model);
    void clear_model();
    TreeModel* model() const noexcept { return model_; }
    void model_changed();

    // Compact materialized convenience for static trees. This clears any
    // borrowed provider and selects the first visible root when non-empty.
    void set_roots(std::vector<TreeNode> roots);
    // Materialized mode returns an owned node. Provider mode returns a snapshot
    // valid until the next TreeView state change; retain selected_id(), not the
    // pointer, across a refresh.
    TreeNode* selected() const noexcept;
    std::optional<TreeItemId> selected_id() const noexcept;

    // Provider-mode expansion state is view-owned. A false result means `id`
    // is absent or describes a confirmed leaf.
    bool set_item_expanded(TreeItemId id, bool expanded);
    bool item_expanded(TreeItemId id) const noexcept;

    // Selects the first depth-first node with `id`, expanding every ancestor
    // needed to make it visible. Returns false without changing selection
    // when no node has that id. Applications that navigate to a result they
    // have kept outside the view (for example a search match) assign their
    // own stable, unique TreeNode::id values and call this instead of trying
    // to manufacture keyboard input or retain pointers across set_roots().
    bool reveal_and_select(std::uint64_t id);

    // Provider clients receive stable identities. The TreeNode callbacks stay
    // available for compact materialized trees and provider snapshots.
    std::function<void(TreeItemId)> on_selection_changed_id;
    std::function<void(TreeItemId)> on_activate_id;
    // Fires once for an unknown provider item when the reader first asks to
    // expand it. The application owns loading and later calls model_changed().
    std::function<void(TreeItemId)> on_expand_request_id;

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

    struct ProviderEntry {
        TreeItemId id = kInvalidTreeItemId;
        TreeItemId parent = kInvalidTreeItemId;
        TreeItem item;
        int depth = 0;
        bool expanded = false;
        bool might_have_children = false;
        bool last_sibling = true;
        std::uint32_t stem_mask = 0;
    };

    struct IndexedChild {
        std::size_t index = 0;
        TreeItemId id = kInvalidTreeItemId;
    };

    void flatten_into(std::vector<TreeNode>& nodes, int depth, TreeNode* parent,
                       std::vector<VisibleEntry>& out, std::uint32_t stem_mask);
    const std::vector<VisibleEntry>& visible_entries();
    void invalidate_visible_entries() noexcept { visible_entries_valid_ = false; }
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
    void rebuild_model_expansion_index();
    std::optional<TreeItem> model_item(TreeItemId id) const;
    std::size_t model_visible_count() const;
    std::size_t model_visible_span(TreeItemId id) const;
    std::optional<ProviderEntry> model_entry_at(std::size_t display_index) const;
    std::optional<ProviderEntry> model_sequence_entry_at(TreeItemId parent, std::size_t display_index,
                                                          int depth, bool parent_last_sibling,
                                                          std::uint32_t parent_stem_mask) const;
    std::optional<ProviderEntry> model_direct_entry_at(TreeItemId parent, std::size_t sibling_index,
                                                        int depth, bool parent_last_sibling,
                                                        std::uint32_t parent_stem_mask) const;
    std::optional<ProviderEntry> model_subtree_entry_at(const ProviderEntry& root,
                                                         std::size_t display_index) const;
    std::optional<std::size_t> model_row_of(TreeItemId id) const;
    void select_provider_entry(const ProviderEntry& entry, std::size_t display_index, bool notify);
    void refresh_provider_selection_snapshot();
    TreeNode provider_snapshot(const ProviderEntry& entry) const;
    bool model_item_might_have_children(TreeItemId id, const TreeItem& item) const;
    void set_model_item_expanded(TreeItemId id, bool expanded);
    void notify_provider_selection();
    void notify_provider_activation();

    std::vector<TreeNode> roots_;
    std::vector<VisibleEntry> visible_entries_;
    bool visible_entries_valid_ = false;
    TreeNode* cursor_node_ = nullptr;

    TreeModel* model_ = nullptr;
    TreeItemId model_cursor_id_ = kInvalidTreeItemId;
    std::optional<TreeNode> model_selected_node_;
    std::set<TreeItemId> model_expanded_items_;
    std::set<TreeItemId> model_expand_requested_items_;
    std::map<TreeItemId, std::vector<IndexedChild>> model_expanded_children_;
    Scrollbar* scrollbar_ = nullptr;
    TreeConnectorStyle connector_style_ = TreeConnectorStyle::Minimal;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
