// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/tree_view.hpp"

#include <algorithm>

namespace ckv::widgets {

TreeView::TreeView() {
    auto scrollbar = std::make_unique<Scrollbar>(Orientation::Vertical);
    scrollbar_ = static_cast<Scrollbar*>(add_child(std::move(scrollbar)));
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void TreeView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
}

void TreeView::set_roots(std::vector<TreeNode> roots) {
    roots_ = std::move(roots);
    auto entries = visible_entries();
    cursor_node_ = entries.empty() ? nullptr : entries.front().node;
    on_resized();
    invalidate();
    // Always fires (even if a same-labeled node ends up selected again)
    // — the OLD TreeNode this pointed to is gone the moment roots_ was
    // replaced, so a master-detail pane must re-resolve against the
    // new tree regardless of whether the label looks unchanged.
    if (on_selection_changed) on_selection_changed();
}

void TreeView::flatten_into(std::vector<TreeNode>& nodes, int depth, std::vector<VisibleEntry>& out) {
    for (auto& n : nodes) {
        out.push_back(VisibleEntry{&n, depth});
        if (n.expanded && !n.children.empty()) flatten_into(n.children, depth + 1, out);
    }
}

std::vector<TreeView::VisibleEntry> TreeView::visible_entries() {
    std::vector<VisibleEntry> out;
    flatten_into(roots_, 0, out);
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
    if (cursor_node_ != previous && on_selection_changed) on_selection_changed();
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
            if (cursor_node_ != nullptr && cursor_node_->expanded) {
                cursor_node_->expanded = false;
                on_resized();
                invalidate();
            }
            return true;
        case Key::Right:
            if (cursor_node_ != nullptr && !cursor_node_->children.empty()) {
                if (!cursor_node_->expanded) {
                    cursor_node_->expanded = true;
                    on_resized();
                    invalidate();
                } else {
                    move_cursor(+1);  // already expanded: step into the first child
                }
            }
            return true;
        case Key::Enter:
        case Key::Char:
            if (event.chord.key == Key::Char && event.chord.text != " ") return false;
            if (cursor_node_ != nullptr && !cursor_node_->children.empty()) {
                cursor_node_->expanded = !cursor_node_->expanded;
                on_resized();
                invalidate();
            }
            // Enter/Space is "act on the current node" regardless of
            // whether it also happened to have children to toggle — a
            // leaf (e.g. a file, in a file-browser TreeView) has no
            // expand/collapse state at all, and Enter on it must still
            // reach the application.
            if (cursor_node_ != nullptr && on_activate) on_activate();
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
    const int twisty_x = entry.depth * 2;
    const bool clicked_twisty = !entry.node->children.empty() && local_x >= twisty_x && local_x < twisty_x + 2;
    if (clicked_twisty) {
        entry.node->expanded = !entry.node->expanded;
        on_resized();
    }
    invalidate();
    // A second click on the ALREADY-selected node (outside the twisty)
    // is "activate", mirroring ListView::on_mouse's identical
    // convention — the first click only ever selects.
    if (!clicked_twisty && clicking_already_selected) {
        if (on_activate) on_activate();
    } else if (!clicking_already_selected && on_selection_changed) {
        on_selection_changed();
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
        const int indent = entry.depth * 2;
        const std::string twisty = entry.node->children.empty() ? "  " : (entry.node->expanded ? "- " : "+ ");
        painter.draw_text(Point{indent, row}, twisty, style);
        painter.draw_text(Point{indent + 2, row}, entry.node->label, style);
    }
}

}  // namespace ckv::widgets
