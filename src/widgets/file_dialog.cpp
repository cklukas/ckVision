// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/file_dialog.hpp"

#include <algorithm>
#include <optional>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// Completion survives a result callback that detaches and destroys its
// dialog. Marking it delivered before entering application code makes the
// documented result callback contract robust against reentrant dismissal.
struct FileDialogCompletion {
    std::weak_ptr<void> window_liveness;
    std::function<void(FileDialogResult)> on_result;
    bool delivered = false;

    void report(FileDialogResult result, Window* window) {
        if (delivered) return;
        delivered = true;
        if (on_result) on_result(std::move(result));
        if (!window_liveness.expired()) window->close();
    }
};
}  // namespace

WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(FileDialogResult)> on_result) {
    auto window = std::make_unique<Window>(mode == FileDialogMode::Open ? "Open File" : "Save File");
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<FileDialogCompletion>(
        FileDialogCompletion{window_ptr->lifetime_token(), std::move(on_result)});

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto path_field = std::make_unique<InputLine>();
    auto* path_field_ptr =
        static_cast<InputLine*>(column->add_item(std::move(path_field), LayoutSpec{SizePolicy::Fixed, 1}));

    auto list = std::make_unique<ListView>(/*multi_select=*/false);
    auto* list_ptr =
        static_cast<ListView*>(column->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto ok_button = std::make_unique<Button>(mode == FileDialogMode::Open ? "Open" : "Save");
    ok_button->set_default(true);
    auto* ok_ptr =
        static_cast<Button*>(button_row->add_item(std::move(ok_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto cancel_button = std::make_unique<Button>("Cancel");
    auto* cancel_ptr =
        static_cast<Button*>(button_row->add_item(std::move(cancel_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    auto current_dir = std::make_shared<std::string>(std::move(initial_directory));
    auto current_entries = std::make_shared<std::vector<FileEntry>>();
    auto refresh = std::make_shared<std::function<void()>>();
    *refresh = [&fs, current_dir, current_entries, list_ptr, path_field_ptr]() {
        auto entries = fs.list_directory(*current_dir);
        std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) return a.is_directory && !b.is_directory;
            return a.name < b.name;
        });
        *current_entries = entries;
        std::vector<std::string> labels;
        const bool has_parent = *current_dir != "/";
        if (has_parent) labels.push_back("..");
        for (const auto& e : entries) labels.push_back(e.is_directory ? e.name + "/" : e.name);
        list_ptr->set_items(std::move(labels));
        path_field_ptr->set_text(*current_dir);
    };
    (*refresh)();

    list_ptr->on_activate = [&fs, current_dir, current_entries, refresh, path_field_ptr](std::size_t index) {
        const bool has_parent = *current_dir != "/";
        if (has_parent && index == 0) {
            *current_dir = fs.parent(*current_dir);
            (*refresh)();
            return;
        }
        const std::size_t entry_index = has_parent ? index - 1 : index;
        if (entry_index >= current_entries->size()) return;
        const FileEntry& entry = (*current_entries)[entry_index];
        if (entry.is_directory) {
            *current_dir = fs.join(*current_dir, entry.name);
            (*refresh)();
        } else {
            path_field_ptr->set_text(fs.join(*current_dir, entry.name));
        }
    };

    ok_ptr->on_press = [&fs, current_dir, path_field_ptr, window_ptr, completion]() {
        std::string chosen = path_field_ptr->text();
        if (chosen.empty()) return;  // nothing to accept
        if (chosen.front() != '/') chosen = fs.join(*current_dir, chosen);
        Window* const window = window_ptr;
        const std::shared_ptr<FileDialogCompletion> held_completion = completion;
        held_completion->report(FileDialogResult{true, std::move(chosen)}, window);
    };
    cancel_ptr->on_press = [window_ptr, completion]() {
        Window* const window = window_ptr;
        const std::shared_ptr<FileDialogCompletion> held_completion = completion;
        held_completion->report(FileDialogResult{}, window);
    };
    window_ptr->accept_request = [ok_ptr]() {
        if (ok_ptr->on_press) ok_ptr->on_press();
    };
    window_ptr->cancel_request = [cancel_ptr]() {
        if (cancel_ptr->on_press) cancel_ptr->on_press();
    };

    window_ptr->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    return WindowHandle{std::move(window), list_ptr};
}

FileDialogPresentation present_file_dialog(FileDialogMode mode, std::string initial_directory,
                                            const FileSystem& fs, ui::Application& app, Desktop& desktop,
                                            const ui::StandardRoles& roles) {
    using Access = detail::DialogPresentationAccess<FileDialogResult>;
    auto parts = Access::make();
    auto handle = make_file_dialog(mode, std::move(initial_directory), fs, roles, app, app.focused(),
                                   [state = parts.state](FileDialogResult result) {
                                       Access::record(state, std::move(result));
                                   });
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, FileDialogResult{});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

FileDialogResult exec_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                  ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles) {
    std::optional<FileDialogResult> result;
    auto handle = make_file_dialog(mode, std::move(initial_directory), fs, roles, app, app.focused(),
                                   [&result](FileDialogResult value) { result = std::move(value); });
    desktop.exec_modal(app, std::move(handle));
    // Every normal acceptance/cancellation path records a result before
    // closing. If the host quits while the modal is still attached,
    // exec_modal ends its pump and cancellation is the documented fallback.
    return result.value_or(FileDialogResult{});
}

}  // namespace ckv::widgets
