// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Directory picker: tree-based selection (plans/widgets.md M6c
// baseline "Directory picker": "Tree-based selection"), built on
// TreeView + the injected FileSystem.
//
// Scope note (documented, not an oversight): the tree is materialized
// EAGERLY and in full from `root_path` down at construction time —
// it uses neither TreeView's lazy population (on_expand_request) nor a
// TreeModel provider, so this is
// only appropriate for directory trees of a size a TUI dialog would
// reasonably show, not an entire real filesystem root. Files are
// excluded from the tree entirely; this picks directories only.
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <memory>
#include <string>

#include "cvision/core/filesystem.hpp"
#include "cvision/core/directory_service.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/ui/ui_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

// The answer of a directory picker.
struct CVISION_API DirectoryPickerResult {
    // Whether the reader pressed Select (or Enter). When true, `path` is the
    // selected node's path: `root_path` as given for the root, and
    // FileSystem::join of it with each directory name below. When false,
    // `path` is empty.
    bool accepted = false;
    std::string path;  // full path; meaningful only when accepted
};

// The handle present_modal_directory_picker returns; see DialogPresentation.
using DirectoryPickerPresentation = DialogPresentation<DirectoryPickerResult>;

// `root_path` must be an existing directory in `fs` (degrades to a
// picker with just the root node and no children otherwise, same
// "empty rather than an error" contract as FileSystem::list_directory
// itself). `on_result` fires exactly once: true and the chosen path on
// OK, false and an empty path on Cancel/Esc. It may detach or destroy the
// dialog; no factory-owned work touches the Window after it returns. `fs` is
// read only while this call builds the tree — no closure keeps a reference
// to it afterward, unlike file_dialog.hpp — so it need not outlive the
// Responsive folder workflow with an editable path and bounded paged list.
// The service and Application outlive the window. Enter in the field or on a
// directory row navigates; Select resolves and accepts an existing directory.
// Absolute/relative paths, '.' and '..' share the file dialog's completion and
// navigation contract. Files are excluded. Closing never waits for discovery.
CVISION_API WindowHandle make_directory_picker(DirectoryService& directories, std::string root_path,
                                   ui::Application& app, ui::View* restore_focus_to,
                                   std::function<void(DirectoryPickerResult)> on_result);

// Modal responsive picker; selecting resolves after detachment.
[[nodiscard]] CVISION_API DirectoryPickerPresentation present_modal_directory_picker(
    DirectoryService& directories, std::string root_path, ui::Application& app,
    Desktop& desktop);

// Blocking convenience for an application that owns the outer loop. It uses
// Desktop::exec_modal and therefore rejects calls from handlers, posts, and
// timers where a nested dispatch pump would be unsafe. The non-blocking
// present_modal_directory_picker is the handler-safe alternative. A quit request
// that ends the outer pump resolves to the same cancelled result as Esc.
CVISION_API DirectoryPickerResult exec_modal_directory_picker(
    DirectoryService& directories, std::string root_path, ui::Application& app,
    Desktop& desktop);

}  // namespace ckv::widgets
