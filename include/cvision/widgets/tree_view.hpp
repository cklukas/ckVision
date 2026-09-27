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

// A TreeModel's stable identity for one node. Ids are chosen by the model and
// mean nothing to TreeView beyond equality.
using TreeItemId = std::uint64_t;
// The id no node may have: TreeView uses it for "no item", so a model must
// never hand it out.
inline constexpr TreeItemId kInvalidTreeItemId = 0;

// How a TreeView draws the branch in front of each label. Outline gives each
// nesting level three columns, every other style two; a click within the
// branch columns of a row that may have children toggles it.
enum class TreeConnectorStyle {
    // The styles, each with the glyphs it draws noted beside it. Minimal is
    // the default. Outline's "├"/"└" follows sibling position; its "─+" marks
    // a collapsed group, and "──" both an expanded group and a leaf.
    Minimal,     // compact "+ "/"- " twisties, no guides
    Ascii,       // printable "+-"/"--" groups, "|-"/"`-" leaves by sibling position, "|" ancestry guides
    BoxDrawing,  // "├"/"└" by sibling position with ▶/▼/─ markers, "│" ancestry guides
    Outline,     // classic outline branches: ─+ groups, ── leaves, and │ ancestry guides
};

// One node of a materialized tree passed to TreeView::set_roots, which takes
// the forest by value; TreeView then owns the nodes and updates `expanded`
// (and, through lazy population, `children`) in place. In provider mode
// TreeView passes TreeNode snapshots of model items to the TreeNode
// callbacks; a snapshot carries no children.
struct TreeNode {
    // The row's text, drawn after the branch.
    std::string label;
    // Child nodes, in display order.
    std::vector<TreeNode> children;
    // Whether the children are shown. The reader toggles it (Left/Right,
    // Enter/Space, a twisty click), and reveal_and_select sets it on ancestors.
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
    // The row's text; whether the node may have children (false while the
    // model has not yet listed them); and an opaque payload TreeView copies
    // into the TreeNode snapshots it passes to the TreeNode callbacks.
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
    // Destroying a model a TreeView still borrows is the caller's error; see
    // TreeView::set_model.
    virtual ~TreeModel() = default;

    // The top-level nodes in display order: how many there are, the id at a
    // position in [0, root_count()), and the position of a root id (nullopt
    // when `id` is not a root).
    virtual std::size_t root_count() const = 0;
    virtual TreeItemId root_id_at(std::size_t root_index) const = 0;
    virtual std::optional<std::size_t> root_index_of(TreeItemId id) const = 0;
    // A root has no parent. A non-root item must return its stable parent id.
    virtual std::optional<TreeItemId> parent_id_of(TreeItemId id) const = 0;
    // The children of `parent` in display order, mirroring the root queries:
    // their count (zero for a leaf or a node not yet loaded), the id at a
    // position in [0, child_count(parent)), and the position of `child`
    // under `parent` (nullopt when it is not one of its children).
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
// roles independently. Disabled (D-076), it draws like a disabled ListView:
// "ckv.list.disabled"'s foreground, the cursor row on
// "ckv.list.selected.inactive"'s background.
class TreeView : public ui::View {
public:
    // An empty tab-stop tree with no model, Minimal connectors, and its
    // vertical Scrollbar in the rightmost column.
    TreeView();

    // Replaces the roles of ordinary rows and of the focused cursor row. The
    // unfocused cursor row and the disabled rows keep their standard roles.
    // A role left kInvalidRole when the view attaches falls back to its
    // standard one.
    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        if (normal_role_ == normal_role && selected_role_ == selected_role) return;
        normal_role_ = normal_role;
        selected_role_ = selected_role;
        invalidate();
    }

    // How branches are drawn (see TreeConnectorStyle); a change repaints.
    // Minimal by default.
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

    // Fires on Enter, or a double click (MouseEvent::click_count) on a
    // node's row outside its twisty — "act on this node" distinct from
    // merely browsing to it, mirroring ListView::on_activate. Fires
    // regardless of whether the node also has an expand/collapse state to
    // toggle (a leaf's Enter must still reach the application).
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
    // Measured as a ListView measures itself: tall enough for the rows now
    // showing up to ListView's preferred count, and never less than one row;
    // wide enough for the widest of the first rows with their branches. An
    // explicit set_preferred_size() outranks either measure. Without these a
    // tree laid out by a container got no height at all.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    // A press on a row selects it, and on its twisty also expands or collapses it; the second
    // press of a double click (MouseEvent::click_count) elsewhere on the row activates it. The
    // vertical wheel scrolls ui::kWheelRows rows per notch and leaves the selection.
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;
    // The cursor's highlight follows focus, so gaining or losing it repaints.
    void on_focus(const FocusEvent& event) override;

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
    // Scrolls so row `cursor_index` shows, counting from the top: the first
    // row when it fits, else just far enough to show it.
    void reveal_row_from_top(int cursor_index);
    // The cursor's row among the visible rows, when there is a cursor.
    std::optional<std::size_t> cursor_row();
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
    // The cursor was placed before the tree was first drawn. Until then every
    // size the tree is given may be a container's interim pass — a Column
    // lays each item out as it is added, at its minimum — so the reveal is
    // held and made again from the top at each size, and the first frame
    // shows it against the geometry that frame actually has.
    bool reveal_pending_ = false;
    // Whether the tree has been drawn: from then on a reveal scrolls the least
    // that shows the cursor, from wherever the reader left the tree.
    bool drawn_ = false;
    TreeConnectorStyle connector_style_ = TreeConnectorStyle::Minimal;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;

    Style row_style(bool cursor_row) const;
};

}  // namespace ckv::widgets
