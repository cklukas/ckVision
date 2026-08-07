// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/tree_view.hpp"

#include "cktest.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::TreeNode;
using ckv::widgets::TreeView;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

TreeView make_tree(Fixture& f) {
    return TreeView(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.dialog_frame,
                    f.roles.dialog_background);
}

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

std::vector<TreeNode> sample_forest() {
    TreeNode child1{"child1", {}, false};
    TreeNode child2{"child2", {}, false};
    TreeNode parent{"parent", {child1, child2}, false};
    TreeNode leaf{"leaf", {}, false};
    std::vector<TreeNode> roots;
    roots.push_back(std::move(parent));
    roots.push_back(std::move(leaf));
    return roots;
}
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

CK_TEST(collapsed_children_are_not_reachable_by_down_arrow) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());  // "parent" starts collapsed
    tree.on_key(key(Key::Down));
    CK_CHECK(tree.selected()->label == "leaf");  // skipped straight past parent's hidden children
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

CK_TEST(left_arrow_on_an_already_collapsed_node_does_nothing) {
    Fixture f;
    auto tree = make_tree(f);
    tree.set_roots(sample_forest());
    tree.on_key(key(Key::Left));
    CK_CHECK(tree.selected()->label == "parent");
    CK_CHECK(!tree.selected()->expanded);
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
