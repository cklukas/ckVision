// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/directory_picker.hpp"

#include <algorithm>
#include <unordered_map>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/tree_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

TreeNode build_subtree(const FileSystem& fs, const std::string& path, std::string label) {
    TreeNode node;
    node.label = std::move(label);
    auto entries = fs.list_directory(path);
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) { return a.name < b.name; });
    for (const auto& e : entries) {
        if (!e.is_directory) continue;
        node.children.push_back(build_subtree(fs, fs.join(path, e.name), e.name));
    }
    return node;
}

// Records each node's full path AFTER the whole subtree is fully
// constructed — walking the tree's OWN addresses one last time before
// it's moved into TreeView. A vector move transfers its heap buffer
// wholesale (no per-element relocation), so these addresses stay valid
// once inside TreeView; recording them any earlier, mid-construction,
// would be unsound (an inner build_subtree() call's local TreeNode is
// itself relocated the moment it's push_back'd into its parent's
// children vector).
void record_paths(const FileSystem& fs, TreeNode& node, const std::string& path,
                   std::unordered_map<const TreeNode*, std::string>& out) {
    out[&node] = path;
    for (auto& child : node.children) record_paths(fs, child, fs.join(path, child.label), out);
}

}  // namespace

WindowHandle make_directory_picker(const FileSystem& fs, std::string root_path, const ui::Theme& theme,
                                    const ui::StandardRoles& roles, ui::Application& app,
                                    ui::View* restore_focus_to,
                                    std::function<void(bool, std::string)> on_result) {
    auto window = std::make_unique<Window>("Select Directory", theme, roles.dialog_frame,
                                            roles.dialog_background, roles.dialog_frame,
                                            roles.dialog_background);
    Window* window_ptr = window.get();

    // record_paths() must run AFTER root is in its final resting place
    // (inside `roots`, the vector that gets moved into TreeView) — its
    // OWN address changes the moment it's push_back'd; recording paths
    // against the pre-move local would key the map with an address
    // TreeView's tree can never actually contain.
    std::vector<TreeNode> roots;
    roots.push_back(build_subtree(fs, root_path, root_path));
    auto path_of = std::make_shared<std::unordered_map<const TreeNode*, std::string>>();
    record_paths(fs, roots.front(), root_path, *path_of);

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto tree = std::make_unique<TreeView>(theme, roles.input_normal, roles.input_focused, roles.dialog_frame,
                                            roles.dialog_background);
    tree->set_roots(std::move(roots));
    auto* tree_ptr = static_cast<TreeView*>(column->add_item(std::move(tree), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto select_button = std::make_unique<Button>("Select");
    select_button->set_default(true);
    auto* select_ptr =
        static_cast<Button*>(button_row->add_item(std::move(select_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto cancel_button = std::make_unique<Button>("Cancel");
    auto* cancel_ptr =
        static_cast<Button*>(button_row->add_item(std::move(cancel_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    select_ptr->on_press = [tree_ptr, path_of, root_path, window_ptr, on_result]() {
        std::string chosen = root_path;
        if (const TreeNode* selected = tree_ptr->selected()) {
            auto it = path_of->find(selected);
            if (it != path_of->end()) chosen = it->second;
        }
        if (on_result) on_result(true, chosen);
        window_ptr->close();
    };
    cancel_ptr->on_press = [window_ptr, on_result]() {
        if (on_result) on_result(false, "");
        window_ptr->close();
    };
    window_ptr->accept_request = [select_ptr]() {
        if (select_ptr->on_press) select_ptr->on_press();
    };
    window_ptr->cancel_request = [cancel_ptr]() {
        if (cancel_ptr->on_press) cancel_ptr->on_press();
    };
    window_ptr->on_closed = [&app, restore_focus_to, window_ptr]() {
        if (restore_focus_to != nullptr && restore_focus_to->focusable()) app.set_focus(restore_focus_to);
        schedule_self_detach(*window_ptr, app);
    };

    return WindowHandle{std::move(window), tree_ptr};
}

}  // namespace ckv::widgets
