// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tree_view.hpp"

#include <map>
#include <string_view>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::TreeNode;
using ckv::widgets::TreeConnectorStyle;
using ckv::widgets::TreeItem;
using ckv::widgets::TreeItemId;
using ckv::widgets::TreeModel;
using ckv::widgets::TreeView;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

TreeView make_tree(Fixture&) { return TreeView(); }

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

std::string row_text(const Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}

std::vector<TreeNode> sample_forest() {
    TreeNode child1{.label = "child1"};
    TreeNode child2{.label = "child2"};
    TreeNode parent{.label = "parent", .children = {child1, child2}};
    TreeNode leaf{.label = "leaf"};
    std::vector<TreeNode> roots;
    roots.push_back(std::move(parent));
    roots.push_back(std::move(leaf));
    return roots;
}

struct ProviderNode {
    TreeItemId id = 0;
    std::optional<TreeItemId> parent;
    std::string label;
    std::vector<TreeItemId> children;
    bool children_known = true;
    bool present = true;
};

class ProviderTree final : public TreeModel {
public:
    ProviderTree() {
        roots_ = {1};
        nodes_.emplace(1, ProviderNode{1, std::nullopt, "root", {2, 3}, true, true});
        nodes_.emplace(2, ProviderNode{2, 1, "branch", {4}, true, true});
        nodes_.emplace(3, ProviderNode{3, 1, "sibling", {}, true, true});
        nodes_.emplace(4, ProviderNode{4, 2, "match", {}, true, true});
    }

    std::size_t root_count() const override { return roots_.size(); }
    TreeItemId root_id_at(std::size_t index) const override { return index < roots_.size() ? roots_[index] : 0; }
    std::optional<std::size_t> root_index_of(TreeItemId id) const override {
        for (std::size_t index = 0; index < roots_.size(); ++index)
            if (roots_[index] == id && lookup(id) != nullptr) return index;
        return std::nullopt;
    }
    std::optional<TreeItemId> parent_id_of(TreeItemId id) const override {
        const ProviderNode* node = lookup(id);
        return node == nullptr ? std::nullopt : node->parent;
    }
    std::size_t child_count(TreeItemId parent) const override {
        const ProviderNode* node = lookup(parent);
        return node == nullptr ? 0 : node->children.size();
    }
    TreeItemId child_id_at(TreeItemId parent, std::size_t index) const override {
        const ProviderNode* node = lookup(parent);
        return node != nullptr && index < node->children.size() ? node->children[index] : 0;
    }
    std::optional<std::size_t> child_index_of(TreeItemId parent, TreeItemId child) const override {
        const ProviderNode* node = lookup(parent);
        if (node == nullptr) return std::nullopt;
        for (std::size_t index = 0; index < node->children.size(); ++index)
            if (node->children[index] == child && lookup(child) != nullptr) return index;
        return std::nullopt;
    }
    std::optional<TreeItem> item(TreeItemId id) const override {
        ++item_queries;
        const ProviderNode* node = lookup(id);
        if (node == nullptr) return std::nullopt;
        return TreeItem{node->label, node->children_known, id};
    }

    void reorder_root_children() { nodes_.at(1).children = {3, 2}; }
    void remove(TreeItemId id) { nodes_.at(id).present = false; }
    void mark_unknown(TreeItemId id) { nodes_.at(id).children_known = false; }

    mutable std::size_t item_queries = 0;

private:
    const ProviderNode* lookup(TreeItemId id) const {
        const auto found = nodes_.find(id);
        return found != nodes_.end() && found->second.present ? &found->second : nullptr;
    }

    std::vector<TreeItemId> roots_;
    std::map<TreeItemId, ProviderNode> nodes_;
};

class MillionRoots final : public TreeModel {
public:
    static constexpr std::size_t kCount = 1'000'000;

    std::size_t root_count() const override { return kCount; }
    TreeItemId root_id_at(std::size_t index) const override { return index < kCount ? index + 1 : 0; }
    std::optional<std::size_t> root_index_of(TreeItemId id) const override {
        return id != 0 && id <= kCount ? std::optional<std::size_t>(id - 1) : std::nullopt;
    }
    std::optional<TreeItemId> parent_id_of(TreeItemId) const override { return std::nullopt; }
    std::size_t child_count(TreeItemId) const override { return 0; }
    TreeItemId child_id_at(TreeItemId, std::size_t) const override { return 0; }
    std::optional<std::size_t> child_index_of(TreeItemId, TreeItemId) const override { return std::nullopt; }
    std::optional<TreeItem> item(TreeItemId id) const override {
        ++item_queries;
        if (id == 0 || id > kCount) return std::nullopt;
        return TreeItem{"row " + std::to_string(id), true, id};
    }

    mutable std::size_t item_queries = 0;
};
}  // namespace

// --- Basics --------------------------------------------------------------

CK_TEST(a_tree_with_no_roots_has_no_selection) {
    Fixture f;
    auto tree = make_tree(f);
    CK_CHECK(tree.selected() == nullptr);
}

CK_TEST(setting_roots_selects_the_first_top_level_node) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    CK_CHECK(tree.selected() != nullptr);
    CK_CHECK(tree.selected()->label == "parent");
}

CK_TEST(reveal_and_select_opens_ancestors_before_selecting_a_hidden_node) {
    Fixture f;
    TreeNode match{.label = "match", .id = 17};
    TreeNode group{.label = "group", .children = {std::move(match)}, .id = 11};
    auto tree = make_tree(f);
    tree.set_roots({std::move(group)});

    int selection_changes = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++selection_changes; };

    CK_CHECK(tree.reveal_and_select(17));
    CK_CHECK(tree.selected() != nullptr);
    CK_CHECK(tree.selected()->label == "match");
    CK_CHECK(tree.selected()->id == 17);
    CK_CHECK(selection_changes == 1);
}

CK_TEST(reveal_and_select_reports_a_missing_id_without_disturbing_selection) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());

    CK_CHECK(!tree.reveal_and_select(999));
    CK_CHECK(tree.selected() != nullptr);
    CK_CHECK(tree.selected()->label == "parent");
}

CK_TEST(collapsed_children_are_not_reachable_by_down_arrow) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());  // "parent" starts collapsed
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected()->label == "leaf");  // skipped straight past parent's hidden children
}

CK_TEST(provider_tree_reveals_stable_id_and_preserves_it_through_a_refresh) {
    Fixture f;
    ProviderTree model;
    auto tree = make_tree(f);
    tree.set_model(model);

    CK_CHECK(tree.reveal_and_select(4));
    CK_CHECK(tree.selected_id() == 4);
    CK_CHECK(tree.selected()->label == "match");
    CK_CHECK(tree.item_expanded(1));
    CK_CHECK(tree.item_expanded(2));

    model.reorder_root_children();
    tree.model_changed();
    CK_CHECK(tree.selected_id() == 4);
    CK_CHECK(tree.item_expanded(1));
    CK_CHECK(tree.item_expanded(2));

    tree.on_key(key(Key::Left));
    CK_CHECK(tree.selected_id() == 2);
    tree.on_key(key(Key::Left));
    CK_CHECK(!tree.item_expanded(2));
    tree.on_key(key(Key::Left));
    CK_CHECK(tree.selected_id() == 1);

    CK_CHECK(tree.reveal_and_select(4));
    model.remove(4);
    tree.model_changed();
    CK_CHECK(!tree.selected_id());
}

CK_TEST(provider_tree_requests_unknown_children_once_and_keeps_expansion_in_the_view) {
    Fixture f;
    ProviderTree model;
    model.mark_unknown(2);
    auto tree = make_tree(f);
    tree.set_model(model);
    CK_CHECK(tree.set_item_expanded(1, true));
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected_id() == 2);

    int requests = 0;
    tree.on_expand_request_id = [&](TreeItemId id) {
        CK_CHECK(id == 2);
        ++requests;
    };
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Left));
    tree.on_key(key(Key::Right));

    CK_CHECK(requests == 1);
    CK_CHECK(tree.item_expanded(2));
}

CK_TEST(provider_tree_paints_only_the_visible_rows_of_a_million_item_forest) {
    Fixture f;
    MillionRoots model;
    auto tree = make_tree(f);
    tree.set_context(f.ctx());
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_model(model);
    model.item_queries = 0;

    Surface surface(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 5});
    tree.draw(painter);

    CK_CHECK(model.item_queries == 5);
    CK_CHECK(row_text(surface, 0).find("row 1") != std::string::npos);
    CK_CHECK(row_text(surface, 4).find("row 5") != std::string::npos);
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected_id() == 2);
}

CK_TEST(connector_style_defaults_to_minimal) {
    Fixture f;
    auto tree = make_tree(f);
    CK_CHECK(tree.connector_style() == TreeConnectorStyle::Minimal);
}

CK_TEST(ascii_connector_style_draws_printable_branch_prefixes) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_context(f.ctx());
    tree.set_bounds(Rect{0, 0, 20, 4});
    tree.set_connector_style(TreeConnectorStyle::Ascii);
    tree.set_roots(sample_forest());

    Surface surface(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 4});
    tree.draw(painter);

    CK_CHECK(row_text(surface, 0).substr(0, 2) == "+-");
    CK_CHECK(row_text(surface, 1).substr(0, 2) == "`-");
}

CK_TEST(box_drawing_connector_style_draws_box_branch_prefixes) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_context(f.ctx());
    tree.set_bounds(Rect{0, 0, 20, 4});
    tree.set_connector_style(TreeConnectorStyle::BoxDrawing);
    tree.set_roots(sample_forest());

    Surface surface(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 4});
    tree.draw(painter);

    CK_CHECK(row_text(surface, 0).substr(0, 6) == "├▶");
    CK_CHECK(row_text(surface, 1).substr(0, 6) == "└─");
}

CK_TEST(outline_connector_style_draws_junctioned_group_and_leaf_branches) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_context(f.ctx());
    tree.set_bounds(Rect{0, 0, 20, 4});
    tree.set_connector_style(TreeConnectorStyle::Outline);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));

    Surface surface(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 4});
    tree.draw(painter);

    // A tee while siblings follow, and a stem only under a parent that
    // still has one. "parent" was just expanded, so it shows no "+": the
    // marker offers to open a group, and this one is already open.
    constexpr std::string_view group_prefix = "├──";
    constexpr std::string_view nested_leaf_prefix = "│  ├──";
    CK_CHECK(row_text(surface, 0).substr(0, group_prefix.size()) == group_prefix);
    CK_CHECK(row_text(surface, 1).substr(0, nested_leaf_prefix.size()) == nested_leaf_prefix);
}

// --- Expand / collapse -----------------------------------------------

CK_TEST(right_arrow_expands_a_collapsed_node_with_children_without_moving_the_cursor) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));
    CK_CHECK(tree.selected()->label == "parent");  // still on parent, now expanded
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(right_arrow_on_an_already_expanded_node_steps_into_the_first_child) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));  // expand
    tree.on_key(key(Key::Right));  // step in
    CK_CHECK(tree.selected()->label == "child1");
}

CK_TEST(right_arrow_on_a_leaf_does_nothing) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));  // move to "leaf"
    tree.on_key(key(Key::Right));
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(expanded_children_become_reachable_by_down_arrow) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));  // expand "parent"
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected()->label == "child1");
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected()->label == "child2");
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(left_arrow_collapses_an_expanded_node) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));  // expand
    tree.on_key(key(Key::Left));
    CK_CHECK(!tree.selected()->expanded);
}

CK_TEST(left_arrow_on_a_collapsed_root_node_does_nothing_since_it_has_no_parent) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Left));
    CK_CHECK(tree.selected()->label == "parent");
    CK_CHECK(!tree.selected()->expanded);
}

CK_TEST(left_arrow_on_a_collapsed_child_jumps_to_its_parent) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));  // expand "parent"
    tree.on_key(key(Key::Down));   // move to "child1"
    CK_CHECK(tree.selected()->label == "child1");

    tree.on_key(key(Key::Left));  // "child1" is a leaf, already collapsed: jump to parent

    CK_CHECK(tree.selected()->label == "parent");
}

CK_TEST(left_arrow_on_a_leaf_root_does_nothing_since_it_has_no_parent) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));  // move to "leaf", a root with no children
    tree.on_key(key(Key::Left));
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(enter_toggles_expand_state) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Enter));
    CK_CHECK(tree.selected()->expanded);
    tree.on_key(key(Key::Enter));
    CK_CHECK(!tree.selected()->expanded);
}

CK_TEST(enter_on_a_leaf_does_not_crash_and_leaves_it_unexpanded) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));  // "leaf"
    tree.on_key(key(Key::Enter));
    CK_CHECK(!tree.selected()->expanded);
}

CK_TEST(collapsing_the_selected_node_itself_still_leaves_a_valid_selection) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Right));  // expand "parent"
    tree.on_key(key(Key::Left));   // collapse it again — cursor stays on "parent" throughout
    CK_CHECK(tree.selected() != nullptr);
    CK_CHECK(tree.selected()->label == "parent");
}

// --- Mouse ------------------------------------------------------------

CK_TEST(clicking_a_row_selects_it) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(clicking_the_twisty_toggles_expand_without_requiring_a_separate_press) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 0}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(tree.selected()->label == "parent");
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(clicking_past_the_last_row_is_unhandled) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 10});
    tree.set_roots(sample_forest());
    CK_CHECK(!tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 5},
                                             std::nullopt, Modifier::None}));
}

// --- Degenerate cases ----------------------------------------------------

CK_TEST(empty_tree_does_not_crash_on_key_or_mouse) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.on_key(key(Key::Down));
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Enter));
    CK_CHECK(!tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 0},
                                             std::nullopt, Modifier::None}));
}

CK_TEST(navigation_clamps_at_the_top_and_bottom_of_the_visible_list) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Up));  // already at the top
    CK_CHECK(tree.selected()->label == "parent");
    tree.on_key(key(Key::Down));
    tree.on_key(key(Key::Down));  // one past "leaf", the last visible entry
    CK_CHECK(tree.selected()->label == "leaf");
}

// --- on_selection_changed / on_activate (master-detail hooks) ------------

CK_TEST(down_arrow_to_a_new_entry_fires_selection_changed_exactly_once) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.on_key(key(Key::Down));
    CK_CHECK(fires == 1);
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(a_no_op_move_at_the_top_of_the_list_does_not_fire_selection_changed) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.on_key(key(Key::Up));  // already at the top: no-op move
    CK_CHECK(fires == 0);
}

CK_TEST(a_no_op_move_at_the_bottom_of_the_list_does_not_fire_selection_changed) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));  // now on "leaf", the last entry
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.on_key(key(Key::Down));  // no further entry to move to
    CK_CHECK(fires == 0);
}

CK_TEST(expanding_a_node_without_moving_the_cursor_does_not_fire_selection_changed) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());  // cursor on "parent", collapsed
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.on_key(key(Key::Right));  // expands "parent" in place, cursor stays put
    CK_CHECK(fires == 0);
    CK_CHECK(tree.selected()->label == "parent");
}

CK_TEST(set_roots_fires_selection_changed_even_though_the_first_entrys_label_is_unchanged) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.set_roots(sample_forest());  // a fresh forest: the OLD TreeNode* is gone regardless
    CK_CHECK(fires == 1);
}

CK_TEST(mouse_click_on_a_different_entry_fires_selection_changed_not_activate) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());  // cursor starts on "parent" (row 0)
    int selection_fires = 0;
    int activate_fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++selection_fires; };
    tree.on_activate = [&](TreeNode&) { ++activate_fires; };
    // Row 1 is "leaf" (parent is collapsed) — clicking it selects, does not activate.
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(selection_fires == 1);
    CK_CHECK(activate_fires == 0);
    CK_CHECK(tree.selected()->label == "leaf");
}

CK_TEST(a_second_click_on_the_already_selected_entry_fires_activate_not_selection_changed) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                   Modifier::None});  // first click: selects "leaf"
    int selection_fires = 0;
    int activate_fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++selection_fires; };
    tree.on_activate = [&](TreeNode&) { ++activate_fires; };
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                   Modifier::None});  // second click: activates
    CK_CHECK(selection_fires == 0);
    CK_CHECK(activate_fires == 1);
}

CK_TEST(clicking_the_twisty_of_the_selected_node_toggles_expansion_and_does_not_activate) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());  // cursor already on "parent" at row 0
    int activate_fires = 0;
    tree.on_activate = [&](TreeNode&) { ++activate_fires; };
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 0}, std::nullopt,
                                   Modifier::None});  // clicks the twisty, not just the row
    CK_CHECK(activate_fires == 0);
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(enter_fires_activate_on_a_leaf_that_has_no_expand_state_at_all) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));  // move to "leaf" (no children)
    int activate_fires = 0;
    tree.on_activate = [&](TreeNode&) { ++activate_fires; };
    tree.on_key(key(Key::Enter));
    CK_CHECK(activate_fires == 1);
}

CK_TEST(enter_on_a_branch_node_both_toggles_expansion_and_fires_activate) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());  // cursor on "parent", collapsed
    int activate_fires = 0;
    tree.on_activate = [&](TreeNode&) { ++activate_fires; };
    tree.on_key(key(Key::Enter));
    CK_CHECK(activate_fires == 1);
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(no_callbacks_installed_is_a_harmless_no_op) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Down));
    tree.on_key(key(Key::Enter));
    tree.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(true);  // must not crash with unset std::function callbacks
}

CK_TEST(set_roots_with_an_empty_forest_does_not_fire_selection_changed) {
    // Unlike a non-empty forest (always fires — see the label-unchanged
    // test above), an empty one has no TreeNode to pass a reference to.
    Fixture f;
    auto tree = make_tree(f);
    int fires = 0;
    tree.on_selection_changed = [&](TreeNode&) { ++fires; };
    tree.set_roots({});
    CK_CHECK(fires == 0);
    CK_CHECK(tree.selected() == nullptr);
}

// --- Lazy population (M10/WP-22) ------------------------------------------

namespace {
std::vector<TreeNode> forest_with_an_unknown_child() {
    TreeNode lazy_child{.label = "lazy", .children_known = false};
    TreeNode root{.label = "root", .children = {lazy_child}};
    std::vector<TreeNode> roots;
    roots.push_back(std::move(root));
    return roots;
}
}  // namespace

CK_TEST(a_node_with_unknown_children_reports_might_have_children_even_though_empty) {
    TreeNode node{.label = "dir", .children_known = false};
    CK_CHECK(node.children.empty());
    CK_CHECK(node.might_have_children());
}

CK_TEST(a_confirmed_leaf_reports_it_might_not_have_children) {
    TreeNode node{.label = "file"};  // children_known defaults true, children defaults empty
    CK_CHECK(!node.might_have_children());
}

CK_TEST(expanding_a_node_with_unknown_children_fires_on_expand_request_exactly_once) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));  // expand "root"
    tree.on_key(key(Key::Down));   // move to "lazy"

    int requests = 0;
    tree.on_expand_request = [&](TreeNode& node) {
        ++requests;
        node.children.push_back(TreeNode{.label = "discovered"});
    };
    tree.on_key(key(Key::Right));  // expand "lazy" for the first time

    CK_CHECK(requests == 1);
    CK_CHECK(tree.selected()->children_known);
    CK_CHECK(tree.selected()->children.size() == 1);
    CK_CHECK(tree.selected()->children[0].label == "discovered");
}

CK_TEST(collapsing_and_re_expanding_a_populated_node_does_not_re_request) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Down));

    int requests = 0;
    tree.on_expand_request = [&](TreeNode&) { ++requests; };
    tree.on_key(key(Key::Right));  // first expand: requests
    tree.on_key(key(Key::Left));   // collapse
    tree.on_key(key(Key::Right));  // re-expand: children_known is already true

    CK_CHECK(requests == 1);
}

CK_TEST(children_known_becomes_true_even_when_the_callback_adds_nothing) {
    // A genuinely empty directory: the callback runs but adds no
    // children. children_known must still flip true, so the node
    // reads as a confirmed leaf afterward rather than re-asking on
    // every later expand attempt.
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Down));

    tree.on_expand_request = [](TreeNode&) {};  // populates nothing
    tree.on_key(key(Key::Right));

    CK_CHECK(tree.selected()->children_known);
    CK_CHECK(tree.selected()->children.empty());
    CK_CHECK(!tree.selected()->might_have_children());
}

CK_TEST(a_node_with_no_expand_request_installed_still_marks_children_known_after_an_attempt) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Down));

    tree.on_key(key(Key::Right));  // no on_expand_request installed at all

    CK_CHECK(tree.selected()->children_known);
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(enter_on_an_unknown_children_node_also_triggers_lazy_population) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));
    tree.on_key(key(Key::Down));

    int requests = 0;
    tree.on_expand_request = [&](TreeNode&) { ++requests; };
    tree.on_key(key(Key::Enter));

    CK_CHECK(requests == 1);
    CK_CHECK(tree.selected()->expanded);
}

CK_TEST(clicking_the_twisty_of_an_unknown_children_node_also_triggers_lazy_population) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_bounds(Rect{0, 0, 20, 5});
    tree.set_roots(forest_with_an_unknown_child());
    tree.on_key(key(Key::Right));  // expand "root" — "lazy" is now row 1

    int requests = 0;
    tree.on_expand_request = [&](TreeNode&) { ++requests; };
    // depth 1: twisty at columns 2-3
    const ckv::MouseEvent click{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 1},
                                 std::nullopt, Modifier::None};
    tree.on_mouse(click);

    CK_CHECK(requests == 1);
}

// --- User payload stability (M10/WP-22) -----------------------------------

CK_TEST(id_and_user_data_survive_expand_and_collapse_of_the_node_itself) {
    Fixture f;
    auto roots = sample_forest();
    roots[0].id = 42;
    roots[0].user_data = std::string("payload");
    auto tree = make_tree(f);
    tree.set_roots(std::move(roots));

    tree.on_key(key(Key::Right));  // expand
    tree.on_key(key(Key::Left));   // collapse

    CK_CHECK(tree.selected()->id == 42);
    CK_CHECK(std::any_cast<std::string>(tree.selected()->user_data) == "payload");
}

CK_TEST(id_and_user_data_survive_a_sibling_being_lazily_populated) {
    // A sibling's children.push_back() (lazy population) could, in
    // principle, reallocate ITS OWN children vector — never this
    // node's own id/user_data, which live in a completely separate
    // TreeNode object untouched by that call.
    TreeNode a{.label = "a", .children_known = false, .id = 7, .user_data = std::string("a-data")};
    TreeNode b{.label = "b", .children_known = false};
    TreeNode root{.label = "root", .children = {a, b}};
    std::vector<TreeNode> roots;
    roots.push_back(std::move(root));

    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(std::move(roots));
    tree.on_key(key(Key::Right));  // expand "root"
    tree.on_key(key(Key::Down));   // move to "a"
    tree.on_key(key(Key::Down));   // move to "b"

    tree.on_expand_request = [](TreeNode& node) {
        node.children.push_back(TreeNode{.label = "b-child"});
    };
    tree.on_key(key(Key::Right));  // expand "b" — only "b" is populated

    tree.on_key(key(Key::Up));  // back to "a"
    CK_CHECK(tree.selected()->id == 7);
    CK_CHECK(std::any_cast<std::string>(tree.selected()->user_data) == "a-data");
}
