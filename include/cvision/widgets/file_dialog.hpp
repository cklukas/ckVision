// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// File open/save dialog: path input with completion, file list,
// directory navigation (the widget catalog M6c baseline "File open/
// save"), enumerating through the injected FileSystem (D-039) so it
// golden-tests headlessly against a scripted MemoryFileSystem.
//
// Scope note: "completion" here means selecting/activating a list
// entry fills the path field, not inline as-you-type autocomplete;
// recent locations via the history registry, the hidden-file toggle,
// and cross-platform path semantics are explicitly beyond-baseline.
// Directory picker (a Tree-based variant) is a separate, later
// increment sharing this dialog's FileSystem plumbing.
#pragma once

#include <functional>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

enum class FileDialogMode { Open, Save };

struct FileDialogFilter {
    std::string label;
    // Case-insensitive file suffixes. ".txt" and "txt" both match
    // "report.txt". Directories are never removed by file filters.
    std::vector<std::string> extensions;
};

struct FileDialogOptions {
    std::vector<FileDialogFilter> filters;
    std::size_t active_filter = 0;
    bool show_hidden = false;

    // Optional recent-location registry. The dialog records the current
    // directory on accept and shows still-existing directories as navigable
    // "Recent: ..." rows at the top of the list.
    ui::HistoryRegistry* recent_locations = nullptr;
    std::string recent_locations_key = "ckv.file_dialog.recent_locations";
};

struct FileDialogResult {
    bool accepted = false;
    std::string path;  // full path; meaningful only when accepted
};

using FileDialogPresentation = DialogPresentation<FileDialogResult>;

// `initial_directory` must exist as a directory in `fs` (a dialog
// opened on a stale/nonexistent path degrades to an empty listing
// rather than crashing — see FileSystem::list_directory's own
// contract). `on_result` fires exactly once: with accepted=true and
// the chosen path on OK, or accepted=false on Cancel/Esc. It may detach or
// destroy the dialog; no factory-owned work touches the Window after it
// returns. `fs` must outlive the returned Window (the installed closures
// capture it by reference). Desktop::present_modeless attaches the returned handle
// and focuses its initial_focus in one call; modal presentation is
// explicit through Desktop::present_modal. The returned standard dialog window
// is non-resizable by default.
WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               FileDialogOptions options, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

// Presents a file dialog modally without a nested loop. Completion
// occurs only after detachment; an accepted result wins, while close,
// external detach, and quit resolve to {false, ""}.
[[nodiscard]] FileDialogPresentation present_file_dialog(FileDialogMode mode, std::string initial_directory,
                                                          const FileSystem& fs, ui::Application& app,
                                                          Desktop& desktop, const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

[[nodiscard]] FileDialogPresentation present_file_dialog(FileDialogMode mode, std::string initial_directory,
                                                          const FileSystem& fs, FileDialogOptions options,
                                                          ui::Application& app, Desktop& desktop,
                                                          const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

// Blocking convenience for an application that owns the outer loop. It uses
// Desktop::exec_modal and therefore rejects calls from handlers, posts, and
// timers where a nested dispatch pump would be unsafe. The non-blocking
// present_file_dialog is the handler-safe alternative. A quit request that
// ends the outer pump resolves to the same cancelled result as Esc.
FileDialogResult exec_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                  ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                                  const StandardStrings& strings = english_standard_strings());

FileDialogResult exec_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                  FileDialogOptions options, ui::Application& app, Desktop& desktop,
                                  const ui::StandardRoles& roles,
                                  const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
