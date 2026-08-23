// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/directory_picker.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
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

// Completion state must outlive the dialog because user result callbacks may
// detach and destroy it. The early single-shot mark also rejects a callback's
// reentrant attempt to accept or cancel the same picker.
struct DirectoryPickerCompletion {
    std::weak_ptr<void> window_liveness;
    std::function<void(bool, std::string)> on_result;
    bool delivered = false;

    void report(bool accepted, std::string path, Window* window) {
        if (delivered) return;
        delivered = true;
        if (on_result) on_result(accepted, std::move(path));
        if (!window_liveness.expired()) window->close();
    }
};

}  // namespace

WindowHandle make_directory_picker(const FileSystem& fs, std::string root_path, const ui::StandardRoles& roles,
                                    ui::Application& app, ui::View* restore_focus_to,
                                    std::function<void(bool, std::string)> on_result,
                                    const StandardStrings& strings) {
    auto window = std::make_unique<Window>(strings.select_directory_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    window->set_resizable(false);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<DirectoryPickerCompletion>(
        DirectoryPickerCompletion{window_ptr->lifetime_token(), std::move(on_result)});

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

    auto tree = std::make_unique<TreeView>();
    tree->set_roots(std::move(roots));
    auto* tree_ptr = static_cast<TreeView*>(column->add_item(std::move(tree), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto select_button = std::make_unique<Button>(strings.select);
    select_button->set_default(true);
    auto* select_ptr =
        static_cast<Button*>(button_row->add_item(std::move(select_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto cancel_button = std::make_unique<Button>(strings.cancel);
    auto* cancel_ptr =
        static_cast<Button*>(button_row->add_item(std::move(cancel_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    select_ptr->on_press = [tree_ptr, path_of, root_path, window_ptr, completion]() {
        std::string chosen = root_path;
        if (const TreeNode* selected = tree_ptr->selected()) {
            auto it = path_of->find(selected);
            if (it != path_of->end()) chosen = it->second;
        }
        Window* const report_window = window_ptr;
        const std::shared_ptr<DirectoryPickerCompletion> held_completion = completion;
        held_completion->report(true, std::move(chosen), report_window);
    };
    cancel_ptr->on_press = [window_ptr, completion]() {
        Window* const report_window = window_ptr;
        const std::shared_ptr<DirectoryPickerCompletion> held_completion = completion;
        held_completion->report(false, "", report_window);
    };
    window_ptr->accept_request = [select_ptr]() {
        if (select_ptr->on_press) select_ptr->on_press();
    };
    window_ptr->cancel_request = [cancel_ptr]() {
        if (cancel_ptr->on_press) cancel_ptr->on_press();
    };
    window_ptr->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    return WindowHandle{std::move(window), tree_ptr};
}

DirectoryPickerPresentation present_directory_picker(const FileSystem& fs, std::string root_path,
                                                      ui::Application& app, Desktop& desktop,
                                                      const ui::StandardRoles& roles,
                                                      const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<DirectoryPickerResult>;
    auto parts = Access::make();
    auto handle = make_directory_picker(
        fs, std::move(root_path), roles, app, app.focused(),
        [state = parts.state](bool accepted, std::string path) {
            Access::record(state, DirectoryPickerResult{accepted, std::move(path)});
        },
        strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, DirectoryPickerResult{});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

DirectoryPickerResult exec_directory_picker(const FileSystem& fs, std::string root_path,
                                            ui::Application& app, Desktop& desktop,
                                            const ui::StandardRoles& roles,
                                            const StandardStrings& strings) {
    std::optional<DirectoryPickerResult> result;
    auto handle = make_directory_picker(
        fs, std::move(root_path), roles, app, app.focused(),
        [&result](bool accepted, std::string path) { result = DirectoryPickerResult{accepted, std::move(path)}; },
        strings);
    desktop.exec_modal(app, std::move(handle));
    // Every normal selection/cancellation path records a result before
    // closing. If the host quits while the modal is still attached,
    // exec_modal ends its pump and cancellation is the documented fallback.
    return result.value_or(DirectoryPickerResult{});
}

}  // namespace ckv::widgets
