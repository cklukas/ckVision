// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "filebrowser_app.hpp"

#include "../example_about.hpp"

#include <algorithm>
#include <any>

#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::filebrowser {

namespace {
// Every command this app needs is already in the standard set (M9/
// WP-12/WP-13) — Quit's wording ("&Quit", Alt+X) already matches the
// framework default exactly, and Tab-switches-pane is just the standard
// focus_next command's own default behavior (D-029) — so this app
// declares no commands of its own at all.

// Lists `node`'s own one level of direct subdirectories (M10/WP-22),
// sorted, storing each child's full path in TreeNode::user_data — the
// lazy-population body TreeView::on_expand_request calls the first
// time a node is expanded, also reused directly (once) for the root
// itself so it can start expanded. Files are skipped: this tree is a
// folder hierarchy only, the same "directories only" scope
// make_directory_picker's own build_subtree already documents.
// children_known is set true unconditionally at the end, whether or
// not any subdirectory was found — a genuinely empty directory is
// thereafter indistinguishable from any other leaf.
void populate_children(const FileSystem& fs, widgets::TreeNode& node) {
    const auto& path = std::any_cast<const std::string&>(node.user_data);
    auto entries = fs.list_directory(path);
    std::sort(entries.begin(), entries.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.name < b.name; });
    for (const auto& e : entries) {
        if (!e.is_directory) continue;
        widgets::TreeNode child;
        child.label = e.name;
        child.user_data = fs.join(path, e.name);
        child.children_known = false;
        node.children.push_back(std::move(child));
    }
    node.children_known = true;
}
}  // namespace

FileBrowserApp::FileBrowserApp(ui::Application& app, FileSystem& fs, std::string root_path)
    : app_(app), fs_(fs), root_path_(std::move(root_path)), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_menu_bar();
    build_window();

    // kQuit is already registered by Application's constructor (M9/
    // WP-12) — only its handler needs attaching here. Tab already
    // cycles focus through the tree, the Splitter (M10/WP-19 — Left/
    // Right there adjusts the pane split), and the file-list pane out
    // of the box (M9/WP-13, D-029) — nothing to wire for that at all.
    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });

    app_.set_focus(tree_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision File Browser example",
                                ckv::examples::about_text(
                                    "Master and detail panes over an injected filesystem."));
}

void FileBrowserApp::build_menu_bar() {
    // Items referencing a command carry no label text of their own
    // (M9/WP-11) — title (with mnemonic), chord hint, and enablement
    // all render live from the registration above.
    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{
        app_.commands().standard().help, "&About..."}));
    file_menu.items.push_back(widgets::MenuItem::separator());
    file_menu.items.push_back(
        widgets::MenuItem::command(app_.commands().standard().quit));
    menus.push_back(std::move(file_menu));

    auto menu = std::make_unique<widgets::MenuBar>(std::move(menus));
    menu_ = desktop_->dock_top(std::move(menu));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{
                           widgets::CommandPresentation{app_.commands().standard().focus_next}},
                       widgets::StatusLineItem{
                           widgets::CommandPresentation{app_.commands().standard().quit}}});
    desktop_->dock_bottom(std::move(status));
}

void FileBrowserApp::build_window() {
    auto window = std::make_unique<widgets::Window>("File Browser");  // default document-window roles
    window->set_bounds(desktop_->content_area());
    window->set_min_size(Size{40, 10});
    // Keeps filling the content area as the terminal grows, rather
    // than staying pinned at whatever size it happened to be created
    // at (M8 WP-4) — the natural policy for a single-window,
    // fills-the-desktop application like this one.
    window->set_grow_policy(widgets::DesktopGrowPolicy::KeepFilling);

    auto tree = std::make_unique<widgets::TreeView>();
    tree_ = tree.get();

    widgets::TreeNode root;
    root.label = root_path_;
    root.user_data = root_path_;
    populate_children(fs_, root);  // eager for the root only, so it can start expanded
    root.expanded = true;
    std::vector<widgets::TreeNode> roots;
    roots.push_back(std::move(root));
    tree_->set_roots(std::move(roots));

    auto file_list = std::make_unique<widgets::ListView>();
    file_list_ = file_list.get();

    tree_->on_selection_changed = [this](widgets::TreeNode&) {
        refresh_file_list_for_selected_node();
    };
    // Lazy population (M10/WP-22): fires the first time the user
    // expands a node whose children were never listed.
    tree_->on_expand_request = [this](widgets::TreeNode& node) { populate_children(fs_, node); };

    // Splitter (M10/WP-19) replaces the old fixed 50/50 Row: the split
    // starts at the same exact 50/50 ratio (Splitter's own default),
    // and Left/Right now lets the user adjust it.
    auto content = std::make_unique<widgets::Splitter>(window->content_rect(), std::move(tree),
                                                         std::move(file_list));
    splitter_ = content.get();
    window->set_content(std::move(content));

    // The selected directory's full path, shown live on the window's
    // own bottom border via Window::add_frame_overlay — exactly the
    // "current line in a text input window" pattern the overlay slot
    // exists for, applied here to "current directory" instead.
    auto path_label = std::make_unique<widgets::Label>(root_path_);
    path_label->set_role_override(roles_.static_text, roles_.label_mnemonic);  // static-text styling, not a form label
    path_label_ = window->add_frame_overlay(std::move(path_label), widgets::FrameSlot{});

    window_ = desktop_->add_window(std::move(window));
    refresh_file_list_for_selected_node();
}

void FileBrowserApp::refresh_file_list_for_selected_node() {
    widgets::TreeNode* selected = tree_->selected();
    if (selected == nullptr) return;
    selected_directory_ = std::any_cast<std::string>(selected->user_data);

    if (path_label_ != nullptr) {
        // set_text() propagates its own changed preferred width up to
        // Window automatically (M9/WP-16) — no manual relayout call.
        path_label_->set_text(selected_directory_);
    }

    auto entries = fs_.list_directory(selected_directory_);
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) return a.is_directory > b.is_directory;
        return a.name < b.name;
    });
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto& e : entries) names.push_back(e.is_directory ? e.name + "/" : e.name);
    file_list_->set_items(std::move(names));
}

}  // namespace ckv::filebrowser
