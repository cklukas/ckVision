// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Directory picker: tree-based selection (the widget catalog M6c
// baseline "Directory picker": "Tree-based selection"), built on
// TreeView + the injected FileSystem.
//
// Scope note (documented, not an oversight): the tree is materialized
// EAGERLY and in full from `root_path` down at construction time —
// TreeView has no lazy/on-demand expansion hook in v1 (same "no
// virtualized model" precedent as ListView/TreeView/Table), so this is
// only appropriate for directory trees of a size a TUI dialog would
// reasonably show, not an entire real filesystem root. Files are
// excluded from the tree entirely; this picks directories only.
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

struct DirectoryPickerResult {
    bool accepted = false;
    std::string path;  // full path; meaningful only when accepted
};

using DirectoryPickerPresentation = DialogPresentation<DirectoryPickerResult>;

// `root_path` must be an existing directory in `fs` (degrades to a
// picker with just the root node and no children otherwise, same
// "empty rather than an error" contract as FileSystem::list_directory
// itself). `on_result` fires exactly once: true and the chosen path on
// OK, false and an empty path on Cancel/Esc. It may detach or destroy the
// dialog; no factory-owned work touches the Window after it returns. `fs` must outlive the
// returned Window (the tree is built once from it at construction —
// no closure keeps a live reference afterward, unlike file_dialog.hpp).
// Desktop::present_modeless attaches the returned handle and focuses
// its initial_focus in one call; modal presentation is explicit through
// Desktop::present_modal. The returned standard dialog window is
// non-resizable by default.
WindowHandle make_directory_picker(const FileSystem& fs, std::string root_path, const ui::StandardRoles& roles,
                                    ui::Application& app, ui::View* restore_focus_to,
                                    std::function<void(bool accepted, std::string path)> on_result,
                                    const StandardStrings& strings = english_standard_strings());

// Presents the picker modally without a nested loop. Completion occurs
// after detachment; selecting a directory wins, while close, external
// detach, and quit resolve to {false, ""}.
[[nodiscard]] DirectoryPickerPresentation present_directory_picker(const FileSystem& fs, std::string root_path,
                                                                    ui::Application& app, Desktop& desktop,
                                                                    const ui::StandardRoles& roles,
                                                                    const StandardStrings& strings = english_standard_strings());

// Blocking convenience for an application that owns the outer loop. It uses
// Desktop::exec_modal and therefore rejects calls from handlers, posts, and
// timers where a nested dispatch pump would be unsafe. The non-blocking
// present_directory_picker is the handler-safe alternative. A quit request
// that ends the outer pump resolves to the same cancelled result as Esc.
DirectoryPickerResult exec_directory_picker(const FileSystem& fs, std::string root_path,
                                            ui::Application& app, Desktop& desktop,
                                            const ui::StandardRoles& roles,
                                            const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
