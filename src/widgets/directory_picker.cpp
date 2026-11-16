// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/directory_picker.hpp"
#include "widgets/path_dialog_internal.hpp"

#include <optional>
#include <utility>

#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

WindowHandle make_directory_picker(DirectoryService& directories, std::string root_path,
                                   ui::Application& app, ui::View* restore_focus_to,
                                   std::function<void(DirectoryPickerResult)> on_result) {
    return detail::make_path_dialog(
        FileDialogMode::Open, true, std::move(root_path), directories, {}, app,
        restore_focus_to,
        [callback = std::move(on_result)](FileDialogResult result) {
            if (callback) callback(DirectoryPickerResult{result.accepted, std::move(result.path)});
        });
}

DirectoryPickerPresentation present_modal_directory_picker(DirectoryService& directories,
                                                           std::string root_path,
                                                           ui::Application& app, Desktop& desktop) {
    using Access = detail::DialogPresentationAccess<DirectoryPickerResult>;
    auto parts = Access::make();
    auto handle = make_directory_picker(
        directories, std::move(root_path), app, app.focused(),
        [state = parts.state](DirectoryPickerResult result) {
            Access::record(state, std::move(result));
        });
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state] {
        if (previous) previous();
        Access::finish(state, {});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

DirectoryPickerResult exec_modal_directory_picker(DirectoryService& directories,
                                                  std::string root_path,
                                                  ui::Application& app, Desktop& desktop) {
    std::optional<DirectoryPickerResult> result;
    auto handle = make_directory_picker(
        directories, std::move(root_path), app, app.focused(),
        [&result](DirectoryPickerResult r) { result = std::move(r); });
    desktop.exec_modal(app, std::move(handle));
    return result.value_or(DirectoryPickerResult{});
}

}  // namespace ckv::widgets
