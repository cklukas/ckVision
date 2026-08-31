// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tree_view.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace ckv::widgets {

namespace {

bool find_path_by_id(std::vector<TreeNode>& nodes, std::uint64_t id,
                     std::vector<TreeNode*>& path) {
    for (TreeNode& node : nodes) {
        path.push_back(&node);
        if (node.id == id || find_path_by_id(node.children, id, path)) return true;
        path.pop_back();
    }
    return false;
}

std::size_t saturated_add(std::size_t left, std::size_t right) {
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum - right ? maximum : left + right;
}

int scrollbar_range(std::size_t count) {
    const std::size_t maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(count, maximum));
}

}  // namespace

TreeView::TreeView() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    set_focus_policy(ui::FocusPolicy::TabStop);
}

TreeNode* TreeView::selected() const noexcept {
    if (model_ != nullptr)
        return model_selected_node_ ? const_cast<TreeNode*>(&*model_selected_node_) : nullptr;
    return cursor_node_;
}

std::optional<TreeItemId> TreeView::selected_id() const noexcept {
    if (model_ == nullptr || model_cursor_id_ == kInvalidTreeItemId) return std::nullopt;
    return model_cursor_id_;
}

bool TreeView::item_expanded(TreeItemId id) const noexcept {
    return id != kInvalidTreeItemId && model_expanded_items_.contains(id);
}

std::optional<TreeItem> TreeView::model_item(TreeItemId id) const {
    if (model_ == nullptr || id == kInvalidTreeItemId) return std::nullopt;
    return model_->item(id);
}

bool TreeView::model_item_might_have_children(TreeItemId id, const TreeItem& item) const {
    return !item.children_known || (model_ != nullptr && model_->child_count(id) != 0);
}

void TreeView::rebuild_model_expansion_index() {
    model_expanded_children_.clear();
    if (model_ == nullptr) return;

    std::set<TreeItemId> retained;
    for (TreeItemId id : model_expanded_items_) {
        if (!model_item(id)) continue;

        TreeItemId parent = kInvalidTreeItemId;
        std::optional<std::size_t> index;
        if (const auto direct_parent = model_->parent_id_of(id)) {
            if (*direct_parent == kInvalidTreeItemId || !model_item(*direct_parent)) continue;
            parent = *direct_parent;
            index = model_->child_index_of(parent, id);
        } else {
            index = model_->root_index_of(id);
        }
        if (!index) continue;

        retained.insert(id);
        model_expanded_children_[parent].push_back(IndexedChild{*index, id});
    }
    model_expanded_items_ = std::move(retained);

    for (auto& [parent, children] : model_expanded_children_) {
        (void)parent;
        std::sort(children.begin(), children.end(), [](const IndexedChild& left, const IndexedChild& right) {
            return left.index < right.index || (left.index == right.index && left.id < right.id);
        });
        children.erase(std::unique(children.begin(), children.end(),
                                   [](const IndexedChild& left, const IndexedChild& right) {
                                       return left.index == right.index;
                                   }),
                       children.end());
    }

    for (auto it = model_expand_requested_items_.begin(); it != model_expand_requested_items_.end();) {
        if (!model_item(*it))
            it = model_expand_requested_items_.erase(it);
        else
            ++it;
    }
}

std::size_t TreeView::model_visible_span(TreeItemId id) const {
    if (model_ == nullptr || !item_expanded(id)) return 1;

    const std::size_t child_count = model_->child_count(id);
    std::size_t span = saturated_add(1, child_count);
    const auto found = model_expanded_children_.find(id);
    if (found == model_expanded_children_.end()) return span;
    for (const IndexedChild& child : found->second) {
        if (child.index >= child_count) continue;
        const std::size_t child_span = model_visible_span(child.id);
        span = saturated_add(span, child_span - 1);
    }
    return span;
}

std::size_t TreeView::model_visible_count() const {
    if (model_ == nullptr) return 0;

    const std::size_t root_count = model_->root_count();
    std::size_t count = root_count;
    const auto roots = model_expanded_children_.find(kInvalidTreeItemId);
    if (roots == model_expanded_children_.end()) return count;
    for (const IndexedChild& root : roots->second) {
        if (root.index >= root_count) continue;
        const std::size_t span = model_visible_span(root.id);
        count = saturated_add(count, span - 1);
    }
    return count;
}

std::optional<TreeView::ProviderEntry> TreeView::model_direct_entry_at(
    TreeItemId parent, std::size_t sibling_index, int depth, bool parent_last_sibling,
    std::uint32_t parent_stem_mask) const {
    if (model_ == nullptr) return std::nullopt;
    const std::size_t sibling_count = parent == kInvalidTreeItemId ? model_->root_count() : model_->child_count(parent);
    if (sibling_index >= sibling_count) return std::nullopt;

    const TreeItemId id = parent == kInvalidTreeItemId ? model_->root_id_at(sibling_index)
                                                        : model_->child_id_at(parent, sibling_index);
    const auto item = model_item(id);
    if (id == kInvalidTreeItemId || !item) return std::nullopt;

    std::uint32_t stem_mask = parent_stem_mask;
    if (parent != kInvalidTreeItemId && !parent_last_sibling && depth > 0 && depth <= 32)
        stem_mask |= std::uint32_t{1} << (depth - 1);

    return ProviderEntry{
        .id = id,
        .parent = parent,
        .item = *item,
        .depth = depth,
        .expanded = item_expanded(id),
        .might_have_children = model_item_might_have_children(id, *item),
        .last_sibling = sibling_index + 1 == sibling_count,
        .stem_mask = stem_mask,
    };
}

std::optional<TreeView::ProviderEntry> TreeView::model_subtree_entry_at(const ProviderEntry& root,
                                                                          std::size_t display_index) const {
    if (display_index == 0) return root;
    if (!root.expanded) return std::nullopt;
    return model_sequence_entry_at(root.id, display_index - 1, root.depth + 1, root.last_sibling, root.stem_mask);
}

std::optional<TreeView::ProviderEntry> TreeView::model_sequence_entry_at(
    TreeItemId parent, std::size_t display_index, int depth, bool parent_last_sibling,
    std::uint32_t parent_stem_mask) const {
    if (model_ == nullptr) return std::nullopt;
    const std::size_t sibling_count = parent == kInvalidTreeItemId ? model_->root_count() : model_->child_count(parent);
    std::size_t next_sibling = 0;
    const auto expanded = model_expanded_children_.find(parent);
    if (expanded != model_expanded_children_.end()) {
        for (const IndexedChild& child : expanded->second) {
            if (child.index < next_sibling || child.index >= sibling_count) continue;
            const std::size_t ordinary_count = child.index - next_sibling;
            if (display_index < ordinary_count)
                return model_direct_entry_at(parent, next_sibling + display_index, depth, parent_last_sibling,
                                             parent_stem_mask);
            display_index -= ordinary_count;

            const auto entry = model_direct_entry_at(parent, child.index, depth, parent_last_sibling, parent_stem_mask);
            if (!entry) return std::nullopt;
            const std::size_t span = model_visible_span(child.id);
            if (display_index < span) return model_subtree_entry_at(*entry, display_index);
            display_index -= span;
            next_sibling = child.index + 1;
        }
    }

    if (display_index >= sibling_count - next_sibling) return std::nullopt;
    return model_direct_entry_at(parent, next_sibling + display_index, depth, parent_last_sibling, parent_stem_mask);
}

std::optional<TreeView::ProviderEntry> TreeView::model_entry_at(std::size_t display_index) const {
    if (display_index >= model_visible_count()) return std::nullopt;
    return model_sequence_entry_at(kInvalidTreeItemId, display_index, 0, true, 0);
}

std::optional<std::size_t> TreeView::model_row_of(TreeItemId id) const {
    if (model_ == nullptr || !model_item(id)) return std::nullopt;

    std::vector<TreeItemId> path;
    std::set<TreeItemId> seen;
    TreeItemId current = id;
    while (true) {
        if (!seen.insert(current).second) return std::nullopt;
        path.push_back(current);
        const auto parent = model_->parent_id_of(current);
        if (!parent) break;
        if (*parent == kInvalidTreeItemId || !model_item(*parent)) return std::nullopt;
        current = *parent;
    }
    std::reverse(path.begin(), path.end());

    const auto root_index = model_->root_index_of(path.front());
    if (!root_index) return std::nullopt;
    std::size_t row = *root_index;
    const auto root_expanded = model_expanded_children_.find(kInvalidTreeItemId);
    if (root_expanded != model_expanded_children_.end()) {
        for (const IndexedChild& root : root_expanded->second) {
            if (root.index >= *root_index) break;
            row = saturated_add(row, model_visible_span(root.id) - 1);
        }
    }

    for (std::size_t depth = 1; depth < path.size(); ++depth) {
        const TreeItemId parent = path[depth - 1];
        if (!item_expanded(parent)) return std::nullopt;
        const auto child_index = model_->child_index_of(parent, path[depth]);
        if (!child_index) return std::nullopt;
        row = saturated_add(row, saturated_add(1, *child_index));
        const auto expanded = model_expanded_children_.find(parent);
        if (expanded == model_expanded_children_.end()) continue;
        for (const IndexedChild& child : expanded->second) {
            if (child.index >= *child_index) break;
            row = saturated_add(row, model_visible_span(child.id) - 1);
        }
    }
    return row;
}

TreeNode TreeView::provider_snapshot(const ProviderEntry& entry) const {
    TreeNode snapshot;
    snapshot.label = entry.item.label;
    snapshot.expanded = entry.expanded;
    snapshot.children_known = entry.item.children_known;
    snapshot.id = entry.id;
    snapshot.user_data = entry.item.user_data;
    return snapshot;
}

void TreeView::refresh_provider_selection_snapshot() {
    if (model_cursor_id_ == kInvalidTreeItemId) {
        model_selected_node_.reset();
        return;
    }
    const auto row = model_row_of(model_cursor_id_);
    const auto entry = row ? model_entry_at(*row) : std::nullopt;
    if (!entry || entry->id != model_cursor_id_) {
        model_cursor_id_ = kInvalidTreeItemId;
        model_selected_node_.reset();
        return;
    }
    model_selected_node_ = provider_snapshot(*entry);
}

void TreeView::notify_provider_selection() {
    if (model_cursor_id_ == kInvalidTreeItemId || !model_selected_node_) return;
    if (on_selection_changed_id) on_selection_changed_id(model_cursor_id_);
    if (on_selection_changed) on_selection_changed(*model_selected_node_);
}

void TreeView::notify_provider_activation() {
    if (model_cursor_id_ == kInvalidTreeItemId || !model_selected_node_) return;
    if (on_activate_id) on_activate_id(model_cursor_id_);
    if (on_activate) on_activate(*model_selected_node_);
}

void TreeView::select_provider_entry(const ProviderEntry& entry, std::size_t display_index, bool notify) {
    const bool changed = model_cursor_id_ != entry.id;
    model_cursor_id_ = entry.id;
    model_selected_node_ = provider_snapshot(entry);
    ensure_cursor_visible(static_cast<int>(std::min(display_index,
                                                     static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    invalidate();
    if (notify && changed) notify_provider_selection();
}

void TreeView::set_model(TreeModel& model) {
    model_ = &model;
    roots_.clear();
    visible_entries_.clear();
    visible_entries_valid_ = false;
    cursor_node_ = nullptr;
    model_cursor_id_ = kInvalidTreeItemId;
    model_selected_node_.reset();
    model_expanded_items_.clear();
    model_expand_requested_items_.clear();
    model_expanded_children_.clear();
    if (const auto entry = model_entry_at(0)) select_provider_entry(*entry, 0, true);
    on_resized();
    invalidate();
}

void TreeView::clear_model() {
    model_ = nullptr;
    model_cursor_id_ = kInvalidTreeItemId;
    model_selected_node_.reset();
    model_expanded_items_.clear();
    model_expand_requested_items_.clear();
    model_expanded_children_.clear();
    if (scrollbar_ != nullptr) scrollbar_->set_range(0, std::max(1, bounds().height));
    invalidate();
}

void TreeView::model_changed() {
    if (model_ == nullptr) return;
    rebuild_model_expansion_index();
    refresh_provider_selection_snapshot();
    on_resized();
    if (const auto row = model_row_of(model_cursor_id_))
        ensure_cursor_visible(static_cast<int>(std::min(*row,
                                                         static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    invalidate();
}

void TreeView::set_model_item_expanded(TreeItemId id, bool expanded) {
    const auto item = model_item(id);
    if (!item) return;
    if (expanded) {
        if (!model_item_might_have_children(id, *item)) return;
        if (!item->children_known && model_expand_requested_items_.insert(id).second && on_expand_request_id)
            on_expand_request_id(id);
        model_expanded_items_.insert(id);
    } else {
        model_expanded_items_.erase(id);
    }
    rebuild_model_expansion_index();
    refresh_provider_selection_snapshot();
    on_resized();
    invalidate();
}

bool TreeView::set_item_expanded(TreeItemId id, bool expanded) {
    const auto item = model_item(id);
    if (!item || (expanded && !model_item_might_have_children(id, *item))) return false;
    set_model_item_expanded(id, expanded);
    return true;
}

void TreeView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
}

void TreeView::set_connector_style(TreeConnectorStyle style) {
    if (connector_style_ == style) return;
    connector_style_ = style;
    invalidate();
}

void TreeView::set_roots(std::vector<TreeNode> roots) {
    clear_model();
    roots_ = std::move(roots);
    invalidate_visible_entries();
    const auto& entries = visible_entries();
    cursor_node_ = entries.empty() ? nullptr : entries.front().node;
    on_resized();
    invalidate();
    // Fires only when there IS a selection — an empty forest has no
    // TreeNode to pass a reference to. A non-empty forest always
    // fires, even if a same-labeled node ends up selected again: the
    // OLD TreeNode this pointed to is gone the moment roots_ was
    // replaced, so a master-detail pane must re-resolve against the
    // new tree regardless of whether the label looks unchanged.
    if (cursor_node_ != nullptr && on_selection_changed) on_selection_changed(*cursor_node_);
}

bool TreeView::reveal_and_select(std::uint64_t id) {
    if (model_ != nullptr) {
        if (!model_item(id)) return false;

        std::vector<TreeItemId> path;
        std::set<TreeItemId> seen;
        TreeItemId current = id;
        while (true) {
            if (!seen.insert(current).second) return false;
            path.push_back(current);
            const auto parent = model_->parent_id_of(current);
            if (!parent) break;
            if (*parent == kInvalidTreeItemId || !model_item(*parent)) return false;
            current = *parent;
        }
        if (!model_->root_index_of(path.back())) return false;

        for (std::size_t index = 1; index < path.size(); ++index)
            model_expanded_items_.insert(path[index]);
        rebuild_model_expansion_index();

        const auto row = model_row_of(id);
        const auto entry = row ? model_entry_at(*row) : std::nullopt;
        if (!entry || entry->id != id) return false;
        select_provider_entry(*entry, *row, true);
        return true;
    }

    std::vector<TreeNode*> path;
    if (!find_path_by_id(roots_, id, path)) return false;

    bool revealed = false;
    // The final entry is the node being selected; only its ancestors must
    // open. Nodes in this materialized tree already exist, so this is not a
    // lazy-population request and does not invoke on_expand_request.
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        if (!path[i]->expanded) {
            path[i]->expanded = true;
            revealed = true;
        }
    }
    if (revealed) invalidate_visible_entries();

    TreeNode* const target = path.back();
    if (target != cursor_node_) {
        select_node(target);
    } else if (revealed) {
        on_resized();
        invalidate();
    }
    return true;
}

void TreeView::flatten_into(std::vector<TreeNode>& nodes, int depth, TreeNode* parent,
                             std::vector<VisibleEntry>& out, std::uint32_t stem_mask) {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        TreeNode& n = nodes[i];
        const bool last = i + 1 == nodes.size();
        out.push_back(VisibleEntry{&n, depth, parent, last, stem_mask});
        if (!n.expanded || n.children.empty()) continue;
        // A stem passes under this node only while it still has siblings
        // below it. Depths past the mask's width keep their parent's
        // stems rather than inventing one.
        const std::uint32_t child_mask =
            last || depth >= 31 ? stem_mask : (stem_mask | (std::uint32_t{1} << depth));
        flatten_into(n.children, depth + 1, &n, out, child_mask);
    }
}

int TreeView::branch_columns() const noexcept {
    return connector_style_ == TreeConnectorStyle::Outline ? 3 : 2;
}

const std::vector<TreeView::VisibleEntry>& TreeView::visible_entries() {
    if (!visible_entries_valid_) {
        visible_entries_.clear();
        flatten_into(roots_, 0, nullptr, visible_entries_, 0);
        visible_entries_valid_ = true;
    }
    return visible_entries_;
}

void TreeView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    const std::size_t count = model_ != nullptr ? model_visible_count() : visible_entries().size();
    scrollbar_->set_range(scrollbar_range(count), std::max(1, bounds().height));
}

void TreeView::ensure_cursor_visible(int cursor_index) {
    if (scrollbar_ == nullptr) return;
    if (cursor_index < scrollbar_->position()) {
        scrollbar_->set_position(cursor_index);
    } else if (cursor_index >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_index - scrollbar_->viewport_size() + 1);
    }
}

void TreeView::move_cursor(int delta) {
    if (model_ != nullptr) {
        const std::size_t count = model_visible_count();
        if (count == 0) {
            model_cursor_id_ = kInvalidTreeItemId;
            model_selected_node_.reset();
            return;
        }
        const auto current_row = model_row_of(model_cursor_id_).value_or(0);
        const int last = scrollbar_range(count) - 1;
        const int current = static_cast<int>(std::min(current_row,
                                                       static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int target = std::clamp(current + delta, 0, last);
        if (const auto entry = model_entry_at(static_cast<std::size_t>(target)))
            select_provider_entry(*entry, static_cast<std::size_t>(target), true);
        return;
    }

    const auto& entries = visible_entries();
    if (entries.empty()) {
        cursor_node_ = nullptr;
        return;
    }
    int index = 0;
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (entries[i].node == cursor_node_) {
            index = static_cast<int>(i);
            break;
        }
    index = std::clamp(index + delta, 0, static_cast<int>(entries.size()) - 1);
    TreeNode* const previous = cursor_node_;
    cursor_node_ = entries[static_cast<std::size_t>(index)].node;
    on_resized();  // the visible set may have changed size (expand/collapse) since the last layout
    ensure_cursor_visible(index);
    invalidate();
    // A no-op move (Up at the top entry, Down at the bottom) must NOT
    // fire — a master-detail pane subscribed to this would otherwise
    // reload identical detail content on every extra keypress at an
    // edge.
    if (cursor_node_ != previous && on_selection_changed) on_selection_changed(*cursor_node_);
}

void TreeView::select_node(TreeNode* node) {
    if (node == nullptr || node == cursor_node_) return;
    TreeNode* const previous = cursor_node_;
    cursor_node_ = node;
    on_resized();
    const auto& entries = visible_entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].node == node) {
            ensure_cursor_visible(static_cast<int>(i));
            break;
        }
    }
    invalidate();
    if (cursor_node_ != previous && on_selection_changed) on_selection_changed(*cursor_node_);
}

void TreeView::set_expanded(TreeNode& node, bool expanded) {
    if (expanded && !node.children_known) {
        // The callback may add children before it returns. Do not allow any
        // re-entrant layout or draw to consume the old flattened view.
        invalidate_visible_entries();
        if (on_expand_request) on_expand_request(node);
        node.children_known = true;
    }
    node.expanded = expanded;
    invalidate_visible_entries();
    on_resized();
    invalidate();
}

bool TreeView::on_key(const KeyEvent& event) {
    if (model_ != nullptr) {
        switch (event.chord.key) {
            case Key::Up:
                move_cursor(-1);
                return true;
            case Key::Down:
                move_cursor(+1);
                return true;
            case Key::Left:
                if (model_cursor_id_ == kInvalidTreeItemId) return true;
                if (item_expanded(model_cursor_id_)) {
                    set_model_item_expanded(model_cursor_id_, false);
                } else if (const auto parent = model_->parent_id_of(model_cursor_id_)) {
                    if (const auto row = model_row_of(*parent)) {
                        if (const auto entry = model_entry_at(*row)) select_provider_entry(*entry, *row, true);
                    }
                }
                return true;
            case Key::Right: {
                const auto row = model_row_of(model_cursor_id_);
                const auto entry = row ? model_entry_at(*row) : std::nullopt;
                if (!entry || !entry->might_have_children) return true;
                if (!entry->expanded)
                    set_model_item_expanded(entry->id, true);
                else
                    move_cursor(+1);
                return true;
            }
            case Key::Enter:
            case Key::Char: {
                if (event.chord.key == Key::Char && event.chord.text != " ") return false;
                const auto row = model_row_of(model_cursor_id_);
                const auto entry = row ? model_entry_at(*row) : std::nullopt;
                if (!entry) return true;
                if (entry->might_have_children) set_model_item_expanded(entry->id, !entry->expanded);
                notify_provider_activation();
                return true;
            }
            default:
                return false;
        }
    }

    switch (event.chord.key) {
        case Key::Up:
            move_cursor(-1);
            return true;
        case Key::Down:
            move_cursor(+1);
            return true;
        case Key::Left:
            if (cursor_node_ != nullptr) {
                if (cursor_node_->expanded) {
                    set_expanded(*cursor_node_, false);
                } else {
                    // Already collapsed (or a leaf): jump to the
                    // parent instead, if there is one (M10/WP-22).
                    for (const VisibleEntry& e : visible_entries()) {
                        if (e.node == cursor_node_) {
                            select_node(e.parent);
                            break;
                        }
                    }
                }
            }
            return true;
        case Key::Right:
            if (cursor_node_ != nullptr && cursor_node_->might_have_children()) {
                if (!cursor_node_->expanded) {
                    set_expanded(*cursor_node_, true);
                } else {
                    move_cursor(+1);  // already expanded: step into the first child
                }
            }
            return true;
        case Key::Enter:
        case Key::Char:
            if (event.chord.key == Key::Char && event.chord.text != " ") return false;
            if (cursor_node_ != nullptr && cursor_node_->might_have_children()) {
                set_expanded(*cursor_node_, !cursor_node_->expanded);
            }
            // Enter/Space is "act on the current node" regardless of
            // whether it also happened to have children to toggle — a
            // leaf (e.g. a file, in a file-browser TreeView) has no
            // expand/collapse state at all, and Enter on it must still
            // reach the application.
            if (cursor_node_ != nullptr && on_activate) on_activate(*cursor_node_);
            return true;
        default:
            return false;
    }
}

bool TreeView::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || scrollbar_ == nullptr) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || row >= bounds().height) return false;
    const int index = scrollbar_->position() + row;

    if (model_ != nullptr) {
        if (index < 0) return false;
        const auto entry = model_entry_at(static_cast<std::size_t>(index));
        if (!entry) return false;

        const bool clicking_already_selected = entry->id == model_cursor_id_;
        select_provider_entry(*entry, static_cast<std::size_t>(index), false);
        const int local_x = event.cell.x - abs.x;
        const int columns = branch_columns();
        const int twisty_x = entry->depth * columns;
        const bool clicked_twisty = entry->might_have_children && local_x >= twisty_x && local_x < twisty_x + columns;
        if (clicked_twisty) set_model_item_expanded(entry->id, !entry->expanded);

        if (!clicked_twisty && clicking_already_selected)
            notify_provider_activation();
        else if (!clicking_already_selected)
            notify_provider_selection();
        return true;
    }

    const auto& entries = visible_entries();
    if (index < 0 || static_cast<std::size_t>(index) >= entries.size()) return false;

    VisibleEntry entry = entries[static_cast<std::size_t>(index)];
    const bool clicking_already_selected = entry.node == cursor_node_;
    cursor_node_ = entry.node;
    const int local_x = event.cell.x - abs.x;
    const int columns = branch_columns();
    const int twisty_x = entry.depth * columns;
    const bool clicked_twisty =
        entry.node->might_have_children() && local_x >= twisty_x && local_x < twisty_x + columns;
    if (clicked_twisty) set_expanded(*entry.node, !entry.node->expanded);
    invalidate();
    // A second click on the ALREADY-selected node (outside the twisty)
    // is "activate", mirroring ListView::on_mouse's identical
    // convention — the first click only ever selects.
    if (!clicked_twisty && clicking_already_selected) {
        if (on_activate) on_activate(*entry.node);
    } else if (!clicking_already_selected && on_selection_changed) {
        on_selection_changed(*entry.node);
    }
    return true;
}

void TreeView::draw(scene::Painter& painter) {
    if (model_ != nullptr) {
        const int visible_width = std::max(0, bounds().width - 1);
        const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
        for (int row = 0; row < bounds().height; ++row) {
            const int index = top + row;
            const auto entry = index < 0 ? std::nullopt : model_entry_at(static_cast<std::size_t>(index));
            const bool selected = entry && entry->id == model_cursor_id_;
            const Style style = selected ? context().theme->resolve(selected_role_)
                                         : context().theme->resolve(normal_role_);
            painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", style));
            if (!entry) continue;

            const int columns = branch_columns();
            const int indent = entry->depth * columns;
            const std::string twisty = [&] {
                if (connector_style_ == TreeConnectorStyle::Minimal)
                    return !entry->might_have_children ? std::string("  ")
                                                       : (entry->expanded ? std::string("- ") : std::string("+ "));
                if (connector_style_ == TreeConnectorStyle::Ascii)
                    return !entry->might_have_children ? std::string("`-")
                                                       : (entry->expanded ? std::string("--") : std::string("+-"));
                if (connector_style_ == TreeConnectorStyle::Outline) {
                    const std::string junction = entry->last_sibling ? "└" : "├";
                    const bool closed = entry->might_have_children && !entry->expanded;
                    return junction + (closed ? "─+" : "──");
                }
                return !entry->might_have_children ? std::string("└─")
                                                    : (entry->expanded ? std::string("├▼") : std::string("├▶"));
            }();
            if (connector_style_ == TreeConnectorStyle::Outline) {
                for (int ancestor = 0; ancestor < entry->depth; ++ancestor) {
                    const bool stem = ancestor < 32 && (entry->stem_mask & (std::uint32_t{1} << ancestor)) != 0;
                    painter.draw_text(Point{ancestor * columns, row}, stem ? "│  " : "   ", style);
                }
            }
            painter.draw_text(Point{indent, row}, twisty, style);
            painter.draw_text(Point{indent + columns, row}, entry->item.label, style);
        }
        return;
    }

    const auto& entries = visible_entries();
    const int visible_width = std::max(0, bounds().width - 1);
    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;

    for (int row = 0; row < bounds().height; ++row) {
        const int index = top + row;
        const Style style = (index >= 0 && static_cast<std::size_t>(index) < entries.size() &&
                              entries[static_cast<std::size_t>(index)].node == cursor_node_)
                                 ? context().theme->resolve(selected_role_)
                                 : context().theme->resolve(normal_role_);
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", style));
        if (index < 0 || static_cast<std::size_t>(index) >= entries.size()) continue;

        const VisibleEntry& entry = entries[static_cast<std::size_t>(index)];
        const int columns = branch_columns();
        const int indent = entry.depth * columns;
        const std::string twisty = [&] {
            if (connector_style_ == TreeConnectorStyle::Minimal)
                return !entry.node->might_have_children() ? std::string("  ")
                                                          : (entry.node->expanded ? std::string("- ")
                                                                                  : std::string("+ "));
            if (connector_style_ == TreeConnectorStyle::Ascii)
                return !entry.node->might_have_children() ? std::string("`-")
                                                          : (entry.node->expanded ? std::string("--")
                                                                                  : std::string("+-"));
            if (connector_style_ == TreeConnectorStyle::Outline) {
                // The junction states the node's place among its siblings —
                // a tee while more follow, an elbow at the last — and the
                // marker states whether it opens.
                const std::string junction = entry.last_sibling ? "└" : "├";
                // The marker says what activating the row would do: a
                // closed group offers to open, while an open group and a
                // leaf offer nothing, so the branch simply runs on.
                const bool closed = entry.node->might_have_children() && !entry.node->expanded;
                return junction + (closed ? "─+" : "──");
            }
            return !entry.node->might_have_children() ? std::string("└─")
                                                      : (entry.node->expanded ? std::string("├▼")
                                                                              : std::string("├▶"));
        }();
        if (connector_style_ == TreeConnectorStyle::Outline) {
            // An ancestry stem passes through this row only where that
            // ancestor still has a sibling below; under a last child the
            // branch has ended and the column is blank.
            for (int ancestor = 0; ancestor < entry.depth; ++ancestor) {
                const bool stem = ancestor < 32 &&
                                  (entry.stem_mask & (std::uint32_t{1} << ancestor)) != 0;
                painter.draw_text(Point{ancestor * columns, row}, stem ? "│  " : "   ", style);
            }
        }
        painter.draw_text(Point{indent, row}, twisty, style);
        painter.draw_text(Point{indent + columns, row}, entry.node->label, style);
    }
}

}  // namespace ckv::widgets
