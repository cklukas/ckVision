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
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

// Which question the dialog asks. The mode sets the window title and the
// accept button's caption (the StandardStrings open/save entries) and nothing
// else: Open does not require the path to exist, and Save does not confirm
// overwriting one. Such checks are the caller's.
enum class FileDialogMode { Open, Save };

// A named set of file suffixes the listing can be narrowed to.
struct FileDialogFilter {
    // Shown on the filter button as "<filter caption>: <label>" while this
    // filter is active.
    std::string label;
    // Case-insensitive file suffixes. ".txt" and "txt" both match
    // "report.txt". Directories are never removed by file filters.
    // An empty list, or an empty suffix in it, matches every file.
    std::vector<std::string> extensions;
};

// The optional parts of a file dialog. A default-constructed value is the
// plain dialog the overloads without options present.
struct FileDialogOptions {
    // The filters the filter button cycles through, in order, starting at
    // `active_filter` (clamped to the last one). With no filters every file is
    // listed and the button does nothing.
    std::vector<FileDialogFilter> filters{};
    std::size_t active_filter = 0;
    // Whether names beginning with '.' are listed at first; the reader toggles
    // it with the show/hide-hidden button.
    bool show_hidden = false;

    // A name to offer, for a dialog that is saving something the application
    // can already name: Save As a document called "notes.md", an export beside
    // it as "notes.html". The path field then reads the shown directory with
    // this name, in whichever directory the reader browses to, and has the
    // focus, so Enter accepts the suggestion and typing replaces it. A file
    // chosen from the list replaces the name. Empty offers none.
    std::string suggested_name{};

    // The history list of recent locations: the list under this key in the
    // Application::history() of the application the dialog is built for.
    // The dialog shows the still-existing directories on it as navigable
    // "Recent: ..." rows at the top of the listing and records the current
    // directory on accept. Every dialog naming the same key shares the one
    // list, so an application normally names one key for all its file
    // dialogs. Empty (the default) shows and records none.
    std::string recent_locations_key{};
};

// The answer of a file dialog.
struct FileDialogResult {
    // Whether the reader accepted a non-empty path. When true, `path` is the
    // path field's text, normalized when absolute and otherwise joined to the
    // directory being shown. It may name a directory or a file that does not
    // exist; nothing is checked. When false, `path` is empty.
    bool accepted = false;
    std::string path;  // full path; meaningful only when accepted
};

// The handle present_modal_file_dialog returns; see DialogPresentation.
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
//
// The window holds the path field, the listing (directories first, then files,
// each sorted by name; ".." except at "/"), and the filter, hidden-files, accept
// and Cancel buttons. Activating a directory row enters it; activating a file
// row puts its path in the field and focuses the field. Tab in the field
// completes its last path segment when exactly one name in the directory it
// names (filtered as the listing is) starts with it, ignoring case. The field
// opens holding the shown directory, and the handle's initial_focus is the
// listing. The labels it keeps from `strings` are copied, so `strings` need only
// live for the call.
WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

// The same dialog shaped by `options`: filters, hidden files, a recent-location
// list, and a suggested name. With a suggested name the handle's initial_focus
// is the path field instead of the listing.
WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               FileDialogOptions options, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

// Presents a file dialog modally without a nested loop. Completion
// occurs only after detachment; an accepted result wins, while close,
// external detach, and quit resolve to {false, ""}.
[[nodiscard]] FileDialogPresentation present_modal_file_dialog(FileDialogMode mode, std::string initial_directory,
                                                          const FileSystem& fs, ui::Application& app,
                                                          Desktop& desktop, const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

// present_modal_file_dialog shaped by `options` (see the options overload of
// make_file_dialog). `fs` must outlive the dialog; `strings` is copied.
[[nodiscard]] FileDialogPresentation present_modal_file_dialog(FileDialogMode mode, std::string initial_directory,
                                                          const FileSystem& fs, FileDialogOptions options,
                                                          ui::Application& app, Desktop& desktop,
                                                          const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

// Blocking convenience for an application that owns the outer loop. It uses
// Desktop::exec_modal and therefore rejects calls from handlers, posts, and
// timers where a nested dispatch pump would be unsafe. The non-blocking
// present_modal_file_dialog is the handler-safe alternative. A quit request that
// ends the outer pump resolves to the same cancelled result as Esc.
FileDialogResult exec_modal_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                        ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                                        const StandardStrings& strings = english_standard_strings());

// exec_modal_file_dialog shaped by `options` (see the options overload of
// make_file_dialog).
FileDialogResult exec_modal_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                        FileDialogOptions options, ui::Application& app, Desktop& desktop,
                                        const ui::StandardRoles& roles,
                                        const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
