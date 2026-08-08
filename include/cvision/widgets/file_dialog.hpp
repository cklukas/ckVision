// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// File open/save dialog: path input with completion, file list,
// directory navigation (plans/widgets.md M6c baseline "File open/
// save"), enumerating through the injected FileSystem (D-025) so it
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
#include <string>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

enum class FileDialogMode { Open, Save };

struct FileDialogResult {
    bool accepted = false;
    std::string path;  // full path; meaningful only when accepted
};

// `initial_directory` must exist as a directory in `fs` (a dialog
// opened on a stale/nonexistent path degrades to an empty listing
// rather than crashing — see FileSystem::list_directory's own
// contract). `on_result` fires exactly once: with accepted=true and
// the chosen path on OK, or accepted=false on Cancel/Esc. `fs` must
// outlive the returned Window (the installed closures capture it by
// reference). Desktop::present_modeless attaches the returned handle
// and focuses its initial_focus in one call; modal presentation is
// explicit through Desktop::present_modal.
WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(FileDialogResult)> on_result);

}  // namespace ckv::widgets
