// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The object graph behind the ckVision File Browser example — a
// master-detail pattern (a folder-hierarchy TreeView driving a
// ListView showing the selected folder's contents in a second pane),
// built entirely on the public ui::View / ui::Application / widgets::*
// surface plus the injected core::FileSystem, exactly the way an
// application is meant to. Factored out of main.cpp so
// tests/test_filebrowser_smoke.cpp can build and drive the exact same
// app headlessly against a MemoryFileSystem.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/tree_view.hpp"

namespace ckv::widgets {
class MenuBar;
class Window;
class Label;
}  // namespace ckv::widgets

namespace ckv::filebrowser {

class FileBrowserApp {
public:
    // `root_path` is where the folder tree starts; the tree is built
    // eagerly (directories only, like widgets::make_directory_picker's
    // own build_subtree — this is the documented, generally-useful
    // idiom for a "small enough to browse interactively" tree, not a
    // virtualized/lazy model) with a depth cap guarding against
    // pathological symlink cycles on a real filesystem.
    FileBrowserApp(ui::Application& app, FileSystem& fs, std::string root_path);

    bool should_quit() const noexcept { return quit_; }

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::TreeView* tree() const noexcept { return tree_; }
    widgets::ListView* file_list() const noexcept { return file_list_; }
    const std::string& selected_directory() const noexcept { return selected_directory_; }

private:
    void build_menu_bar();
    void build_window();
    void refresh_file_list_for_selected_node();

    ui::Application& app_;
    FileSystem& fs_;
    std::string root_path_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::MenuBar* menu_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::TreeView* tree_ = nullptr;
    widgets::ListView* file_list_ = nullptr;
    widgets::Label* path_label_ = nullptr;

    // TreeNode pointers are stable once built (see build_subtree's own
    // discipline in directory_picker.cpp: never record addresses until
    // the whole subtree is in its final resting place) — this maps
    // each node back to its full path, the same pattern
    // make_directory_picker uses, duplicated here deliberately so this
    // example is self-contained and copy-paste-usable rather than
    // depending on that dialog's private helpers.
    std::unordered_map<const widgets::TreeNode*, std::string> path_of_;

    std::string selected_directory_;
    bool quit_ = false;
};

}  // namespace ckv::filebrowser
