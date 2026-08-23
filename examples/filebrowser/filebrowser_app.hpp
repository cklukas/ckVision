// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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
class Splitter;
}  // namespace ckv::widgets

namespace ckv::filebrowser {

class FileBrowserApp {
public:
    // `root_path` is where the folder tree starts. Populated lazily
    // (M10/WP-22): only the root's own direct children are listed
    // eagerly, so it can start expanded (a collapsed root would hide
    // everything behind one extra Right/Enter keypress); every deeper
    // level is listed on demand the first time the user expands it,
    // via TreeView::on_expand_request — no depth cap needed, since
    // nothing is ever recursed into ahead of the user actually asking
    // to see it.
    FileBrowserApp(ui::Application& app, FileSystem& fs, std::string root_path);

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::TreeView* tree() const noexcept { return tree_; }
    widgets::ListView* file_list() const noexcept { return file_list_; }
    widgets::Splitter* splitter() const noexcept { return splitter_; }
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
    widgets::Splitter* splitter_ = nullptr;
    widgets::Label* path_label_ = nullptr;

    std::string selected_directory_;
};

}  // namespace ckv::filebrowser
