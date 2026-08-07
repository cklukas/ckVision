// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "filebrowser_app.hpp"

#include <algorithm>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::filebrowser {

namespace {
constexpr ui::CommandId kCmdQuit = 1;
constexpr ui::CommandId kCmdFocusNextPane = 2;

// Directories only, sorted, depth-capped against a real filesystem's
// symlink cycles (a real, bounded disk still could be arbitrarily
// deep; a demo browser has no business hanging on one).
widgets::TreeNode build_subtree(const FileSystem& fs, const std::string& path, std::string label, int depth) {
    widgets::TreeNode node;
    node.label = std::move(label);
    if (depth <= 0) return node;
    auto entries = fs.list_directory(path);
    std::sort(entries.begin(), entries.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.name < b.name; });
    for (const auto& e : entries) {
        if (!e.is_directory) continue;
        node.children.push_back(build_subtree(fs, fs.join(path, e.name), e.name, depth - 1));
    }
    return node;
}

// Recorded AFTER the whole subtree is in its final resting place —
// see filebrowser_app.hpp's own doc comment on path_of_ for why.
void record_paths(const FileSystem& fs, widgets::TreeNode& node, const std::string& path,
                   std::unordered_map<const widgets::TreeNode*, std::string>& out) {
    out[&node] = path;
    for (auto& child : node.children) record_paths(fs, child, fs.join(path, child.label), out);
}
}  // namespace

FileBrowserApp::FileBrowserApp(ui::Application& app, FileSystem& fs, std::string root_path)
    : app_(app), fs_(fs), root_path_(std::move(root_path)), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background);
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_menu_bar();
    build_window();

    // One call each (M9/WP-10) — metadata, chord, and handler together,
    // instead of a register_command()+set_command_handler() pair.
    auto& commands = app_.commands();
    commands.add({.id = kCmdQuit,
                  .title = "&Quit",
                  .category = "File",
                  .chord = "Alt+X",
                  .handler = [this] { quit_ = true; }});

    // Tab isn't wired to focus traversal anywhere in the library by
    // default (Application::focus_next()/previous() are callable
    // directly, but nothing binds a key to them automatically — every
    // application does this itself, the same way the M4 exit-criteria
    // integration test drives focus_next() directly rather than
    // through a Tab keystroke). Two panes with no OTHER use for Tab
    // (unlike, say, a text editor that wants Tab to insert a literal
    // tab) makes this the obvious default here.
    commands.add({.id = kCmdFocusNextPane,
                  .title = "Next Pane",
                  .category = "Window",
                  .chord = "Tab",
                  .handler = [this] { app_.focus_next(); }});

    app_.set_focus(tree_);
}

void FileBrowserApp::build_menu_bar() {
    // Items referencing a command carry no label text of their own
    // (M9/WP-11) — title (with mnemonic), chord hint, and enablement
    // all render live from the registration above.
    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem{"", kCmdQuit, nullptr, false});
    menus.push_back(std::move(file_menu));

    auto menu = std::make_unique<widgets::MenuBar>(std::move(menus));
    menu_ = desktop_->dock_top(std::move(menu));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_role_override(roles_.menu_bar_normal);
    status->set_items({{"", kCmdFocusNextPane}, {"", kCmdQuit}});
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

    auto content = std::make_unique<ui::Row>();

    auto tree = std::make_unique<widgets::TreeView>();
    tree_ = tree.get();
    std::vector<widgets::TreeNode> roots;
    roots.push_back(build_subtree(fs_, root_path_, root_path_, /*depth=*/8));
    roots.front().expanded = true;  // show the root's own subdirectories immediately, like a file
                                     // manager's initial view — a collapsed root would hide
                                     // everything behind one extra Right/Enter keypress.
    record_paths(fs_, roots.front(), root_path_, path_of_);
    tree_->set_roots(std::move(roots));

    auto file_list = std::make_unique<widgets::ListView>();
    file_list_ = file_list.get();

    tree_->on_selection_changed = [this] { refresh_file_list_for_selected_node(); };

    content->add_item(std::move(tree), ui::LayoutSpec{ui::SizePolicy::Expanding, 1});
    content->add_item(std::move(file_list), ui::LayoutSpec{ui::SizePolicy::Expanding, 1});

    window->set_content(std::move(content));

    // The selected directory's full path, shown live on the window's
    // own bottom border via Window::set_frame_overlay — exactly the
    // "current line in a text input window" pattern the overlay slot
    // exists for, applied here to "current directory" instead.
    auto path_label = std::make_unique<widgets::Label>(root_path_);
    path_label->set_role_override(roles_.static_text, roles_.label_mnemonic);  // static-text styling, not a form label
    window->set_frame_overlay(std::move(path_label));
    path_label_ = static_cast<widgets::Label*>(window->frame_overlay());

    window_ = desktop_->add_window(std::move(window));
    refresh_file_list_for_selected_node();
}

void FileBrowserApp::refresh_file_list_for_selected_node() {
    widgets::TreeNode* selected = tree_->selected();
    if (selected == nullptr) return;
    auto it = path_of_.find(selected);
    selected_directory_ = it != path_of_.end() ? it->second : root_path_;

    if (path_label_ != nullptr) {
        path_label_->set_text(selected_directory_);
        // set_text() changes the label's own preferred width, which
        // nothing propagates upward automatically — see
        // Window::relayout_frame_overlay's own doc comment.
        window_->relayout_frame_overlay();
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
