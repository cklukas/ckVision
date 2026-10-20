// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/widgets/file_dialog.hpp"

namespace ckv::widgets::detail {
// Shared implementation of the DirectoryService file/folder factories.
// Clients use the documented overloads in file_dialog/directory_picker.hpp.
WindowHandle make_path_dialog(FileDialogMode mode, bool directories_only, std::string initial_directory,
                              DirectoryService& directories, FileDialogOptions options,
                              const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                              std::function<void(FileDialogResult)> on_result, const StandardStrings& strings);
} // namespace ckv::widgets::detail
