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

    app_.commands().register_command(kCmdQuit, "Quit", "File", KeyChord{Key::Char, Modifier::Alt, "x"});
    app_.set_command_handler(kCmdQuit, [this] { quit_ = true; });

    // Tab isn't wired to focus traversal anywhere in the library by
    // default (Application::focus_next()/previous() are callable
    // directly, but nothing binds a key to them automatically — every
    // application does this itself, the same way the M4 exit-criteria
    // integration test drives focus_next() directly rather than
    // through a Tab keystroke). Two panes with no OTHER use for Tab
    // (unlike, say, a text editor that wants Tab to insert a literal
    // tab) makes this the obvious default here.
    app_.commands().register_command(kCmdFocusNextPane, "Next Pane", "Window",
                                      KeyChord{Key::Tab, Modifier::None, ""});
    app_.set_command_handler(kCmdFocusNextPane, [this] { app_.focus_next(); });

    app_.set_focus(tree_);
}

void FileBrowserApp::build_menu_bar() {
    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem{"E&xit", kCmdQuit, nullptr, false});
    menus.push_back(std::move(file_menu));

    auto menu = std::make_unique<widgets::MenuBar>(std::move(menus), app_.theme(), roles_.menu_bar_normal,
                                                     roles_.menu_bar_active, roles_.menu_dropdown_normal,
                                                     roles_.menu_dropdown_highlighted,
                                                     roles_.menu_dropdown_disabled, app_, *desktop_);
    menu_ = static_cast<widgets::MenuBar*>(desktop_->dock_top(std::move(menu)));

    auto status = std::make_unique<widgets::StatusLine>(app_.theme(), roles_.menu_bar_normal, app_);
    status->set_items({{"Tab Switch Pane", ui::kInvalidCommand}, {"Alt+X Quit", kCmdQuit}});
    desktop_->dock_bottom(std::move(status));
}

void FileBrowserApp::build_window() {
    auto window = std::make_unique<widgets::Window>("File Browser", app_.theme(), roles_.window_frame_active,
                                                      roles_.window_frame_inactive, roles_.window_title_active,
                                                      roles_.window_title_inactive);
    window->set_bounds(desktop_->content_area());
    window->set_min_size(Size{40, 10});

    auto content = std::make_unique<ui::Row>();

    auto tree = std::make_unique<widgets::TreeView>(app_.theme(), roles_.input_normal, roles_.input_focused,
                                                      roles_.dialog_frame, roles_.dialog_background);
    tree_ = tree.get();
    tree_->set_focus_policy(ui::FocusPolicy::TabStop);
    std::vector<widgets::TreeNode> roots;
    roots.push_back(build_subtree(fs_, root_path_, root_path_, /*depth=*/8));
    roots.front().expanded = true;  // show the root's own subdirectories immediately, like a file
                                     // manager's initial view — a collapsed root would hide
                                     // everything behind one extra Right/Enter keypress.
    record_paths(fs_, roots.front(), root_path_, path_of_);
    tree_->set_roots(std::move(roots));

    auto file_list = std::make_unique<widgets::ListView>(app_.theme(), roles_.input_normal, roles_.input_focused,
                                                           roles_.dialog_frame, roles_.dialog_background);
    file_list_ = file_list.get();
    file_list_->set_focus_policy(ui::FocusPolicy::TabStop);

    tree_->on_selection_changed = [this] { refresh_file_list_for_selected_node(); };

    content->add_item(std::move(tree), ui::LayoutSpec{ui::SizePolicy::Expanding, 1});
    content->add_item(std::move(file_list), ui::LayoutSpec{ui::SizePolicy::Expanding, 1});

    window->set_content(std::move(content));

    // The selected directory's full path, shown live on the window's
    // own bottom border via Window::set_frame_overlay — exactly the
    // "current line in a text input window" pattern the overlay slot
    // exists for, applied here to "current directory" instead.
    auto path_label = std::make_unique<widgets::Label>(root_path_, app_.theme(), roles_.static_text,
                                                         roles_.label_mnemonic);
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
