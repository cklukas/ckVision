// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Directory picker: tree-based selection (plans/widgets.md M6c
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
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// `root_path` must be an existing directory in `fs` (degrades to a
// picker with just the root node and no children otherwise, same
// "empty rather than an error" contract as FileSystem::list_directory
// itself). `on_result` fires exactly once: true and the chosen path on
// OK, false and an empty path on Cancel/Esc. `fs` must outlive the
// returned Window (the tree is built once from it at construction —
// no closure keeps a live reference afterward, unlike file_dialog.hpp).
// Desktop::present_modeless attaches the returned handle and focuses
// its initial_focus in one call; modal presentation is explicit through
// Desktop::present_modal.
WindowHandle make_directory_picker(const FileSystem& fs, std::string root_path, const ui::StandardRoles& roles,
                                    ui::Application& app, ui::View* restore_focus_to,
                                    std::function<void(bool accepted, std::string path)> on_result);

}  // namespace ckv::widgets
