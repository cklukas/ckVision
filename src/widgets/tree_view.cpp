// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tree_view.hpp"

#include <algorithm>

namespace ckv::widgets {

TreeView::TreeView() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    set_focus_policy(ui::FocusPolicy::TabStop);
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
    roots_ = std::move(roots);
    auto entries = visible_entries();
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

std::vector<TreeView::VisibleEntry> TreeView::visible_entries() {
    std::vector<VisibleEntry> out;
    flatten_into(roots_, 0, nullptr, out, 0);
    return out;
}

void TreeView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    scrollbar_->set_range(static_cast<int>(visible_entries().size()), std::max(1, bounds().height));
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
    auto entries = visible_entries();
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
    auto entries = visible_entries();
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
        if (on_expand_request) on_expand_request(node);
        node.children_known = true;
    }
    node.expanded = expanded;
    on_resized();
    invalidate();
}

bool TreeView::on_key(const KeyEvent& event) {
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
    auto entries = visible_entries();
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
    auto entries = visible_entries();
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
