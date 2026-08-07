// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/file_dialog.hpp"

#include <algorithm>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;
}  // namespace

FileDialogHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                   const ui::Theme& theme, const ui::StandardRoles& roles, ui::Application& app,
                                   ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result) {
    auto window = std::make_unique<Window>(mode == FileDialogMode::Open ? "Open File" : "Save File", theme,
                                            roles.window_frame_active, roles.window_frame_inactive,
                                            roles.window_title_active, roles.window_title_inactive);
    Window* window_ptr = window.get();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto path_field = std::make_unique<InputLine>(theme, roles.input_normal, roles.input_focused,
                                                    roles.input_invalid);
    auto* path_field_ptr =
        static_cast<InputLine*>(column->add_item(std::move(path_field), LayoutSpec{SizePolicy::Fixed, 1}));

    auto list = std::make_unique<ListView>(theme, roles.input_normal, roles.input_focused, roles.dialog_frame,
                                            roles.dialog_background, /*multi_select=*/false);
    auto* list_ptr =
        static_cast<ListView*>(column->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto ok_button = std::make_unique<Button>(mode == FileDialogMode::Open ? "Open" : "Save", theme,
                                               roles.button_normal, roles.button_focused, roles.button_default);
    ok_button->set_default(true);
    auto* ok_ptr =
        static_cast<Button*>(button_row->add_item(std::move(ok_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto cancel_button =
        std::make_unique<Button>("Cancel", theme, roles.button_normal, roles.button_focused, roles.button_default);
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

    ok_ptr->on_press = [&fs, current_dir, path_field_ptr, window_ptr, on_result]() {
        std::string chosen = path_field_ptr->text();
        if (chosen.empty()) return;  // nothing to accept
        if (chosen.front() != '/') chosen = fs.join(*current_dir, chosen);
        if (on_result) on_result(FileDialogResult{true, chosen});
        window_ptr->close();
    };
    cancel_ptr->on_press = [window_ptr, on_result]() {
        if (on_result) on_result(FileDialogResult{false, ""});
        window_ptr->close();
    };
    window_ptr->accept_request = [ok_ptr]() {
        if (ok_ptr->on_press) ok_ptr->on_press();
    };
    window_ptr->cancel_request = [cancel_ptr]() {
        if (cancel_ptr->on_press) cancel_ptr->on_press();
    };

    window_ptr->on_closed = [&app, restore_focus_to, window_ptr]() {
        if (restore_focus_to != nullptr && restore_focus_to->focusable()) app.set_focus(restore_focus_to);
        schedule_self_detach(*window_ptr, app);
    };

    return FileDialogHandle{std::move(window), list_ptr};
}

}  // namespace ckv::widgets
