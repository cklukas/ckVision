// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/file_dialog.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

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

class CompletingInputLine final : public InputLine {
public:
    std::function<bool()> complete_request;

    bool on_key(const KeyEvent& event) override {
        if (event.action != KeyAction::Release && event.chord.key == Key::Tab &&
            event.chord.modifiers == Modifier::None && complete_request) {
            return complete_request();
        }
        return InputLine::on_key(event);
    }
};

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

enum class FileListItemKind { Parent, Recent, Entry };

struct FileListItem {
    FileListItemKind kind = FileListItemKind::Entry;
    std::string path;
    std::size_t entry_index = 0;
};

bool hidden_name(std::string_view name) noexcept { return !name.empty() && name.front() == '.'; }

std::string lowercase(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) out.push_back(static_cast<char>(std::tolower(ch)));
    return out;
}

bool has_suffix_case_insensitive(std::string_view name, std::string_view suffix) {
    if (suffix.empty()) return true;
    std::string normalized_suffix(suffix);
    if (!normalized_suffix.empty() && normalized_suffix.front() != '.') normalized_suffix = "." + normalized_suffix;
    const std::string lower_name = lowercase(name);
    const std::string lower_suffix = lowercase(normalized_suffix);
    return lower_name.size() >= lower_suffix.size() &&
           lower_name.compare(lower_name.size() - lower_suffix.size(), lower_suffix.size(), lower_suffix) == 0;
}

bool matches_filter(const FileEntry& entry, const FileDialogOptions& options) {
    if (entry.is_directory || options.filters.empty()) return true;
    const std::size_t active = std::min(options.active_filter, options.filters.size() - 1);
    const auto& extensions = options.filters[active].extensions;
    if (extensions.empty()) return true;
    for (const auto& extension : extensions)
        if (has_suffix_case_insensitive(entry.name, extension)) return true;
    return false;
}

std::string base_name(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos) return std::string(path);
    return std::string(path.substr(slash + 1));
}

std::string parent_fragment(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos) return {};
    return std::string(path.substr(0, slash));
}

bool starts_with_case_insensitive(std::string_view text, std::string_view prefix) {
    if (prefix.size() > text.size()) return false;
    return lowercase(text.substr(0, prefix.size())) == lowercase(prefix);
}
}  // namespace

WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings) {
    return make_file_dialog(mode, std::move(initial_directory), fs, FileDialogOptions{}, roles, app,
                            restore_focus_to, std::move(on_result), strings);
}

WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                               FileDialogOptions options, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result,
                               const StandardStrings& strings) {
    if (!options.filters.empty()) options.active_filter = std::min(options.active_filter, options.filters.size() - 1);
    auto window = std::make_unique<Window>(
        mode == FileDialogMode::Open ? strings.open_file_title : strings.save_file_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    window->set_resizable(false);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<FileDialogCompletion>(
        FileDialogCompletion{window_ptr->lifetime_token(), std::move(on_result)});

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto path_field = std::make_unique<CompletingInputLine>();
    auto* path_field_ptr = static_cast<CompletingInputLine*>(
        column->add_item(std::move(path_field), LayoutSpec{SizePolicy::Fixed, 1}));

    auto list = std::make_unique<ListView>(/*multi_select=*/false);
    auto* list_ptr =
        static_cast<ListView*>(column->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto filter_button = std::make_unique<Button>(strings.filter);
    auto* filter_ptr =
        static_cast<Button*>(button_row->add_item(std::move(filter_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto hidden_button = std::make_unique<Button>(options.show_hidden ? strings.hide_hidden : strings.show_hidden);
    auto* hidden_ptr =
        static_cast<Button*>(button_row->add_item(std::move(hidden_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto ok_button = std::make_unique<Button>(mode == FileDialogMode::Open ? strings.open : strings.save);
    ok_button->set_default(true);
    auto* ok_ptr =
        static_cast<Button*>(button_row->add_item(std::move(ok_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto cancel_button = std::make_unique<Button>(strings.cancel);
    auto* cancel_ptr =
        static_cast<Button*>(button_row->add_item(std::move(cancel_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    auto current_dir = std::make_shared<std::string>(fs.normalize_path(initial_directory));
    auto current_entries = std::make_shared<std::vector<FileEntry>>();
    auto visible_items = std::make_shared<std::vector<FileListItem>>();
    auto options_state = std::make_shared<FileDialogOptions>(std::move(options));
    auto refresh = std::make_shared<std::function<void()>>();
    *refresh = [&fs, current_dir, current_entries, visible_items, list_ptr, path_field_ptr, filter_ptr, hidden_ptr,
                options_state, &strings]() {
        auto entries = fs.list_directory(*current_dir);
        entries.erase(std::remove_if(entries.begin(), entries.end(), [options_state](const FileEntry& entry) {
                          return (!options_state->show_hidden && hidden_name(entry.name)) ||
                                 !matches_filter(entry, *options_state);
                      }),
                      entries.end());
        std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) return a.is_directory && !b.is_directory;
            return a.name < b.name;
        });
        *current_entries = entries;
        std::vector<std::string> labels;
        visible_items->clear();
        if (options_state->recent_locations != nullptr) {
            for (const std::string& recent :
                 options_state->recent_locations->entries(options_state->recent_locations_key)) {
                const std::string normalized = fs.normalize_path(recent);
                if (!fs.is_directory(normalized)) continue;
                labels.push_back("Recent: " + normalized);
                visible_items->push_back(FileListItem{FileListItemKind::Recent, normalized, 0});
            }
        }
        const bool has_parent = *current_dir != "/";
        if (has_parent) {
            labels.push_back("..");
            visible_items->push_back(FileListItem{FileListItemKind::Parent, {}, 0});
        }
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            labels.push_back(e.is_directory ? e.name + "/" : e.name);
            visible_items->push_back(FileListItem{FileListItemKind::Entry, {}, i});
        }
        list_ptr->set_items(std::move(labels));
        path_field_ptr->set_text(*current_dir);
        if (!options_state->filters.empty()) {
            filter_ptr->set_text(strings.filter + ": " + options_state->filters[options_state->active_filter].label);
        } else {
            filter_ptr->set_text(strings.filter);
        }
        hidden_ptr->set_text(options_state->show_hidden ? strings.hide_hidden : strings.show_hidden);
    };
    (*refresh)();

    auto complete_path = [current_dir, path_field_ptr, options_state, &fs]() {
        const std::string text = path_field_ptr->text();
        const std::string normalized_text = fs.normalize_path(text);
        const bool absolute = fs.is_absolute_path(text);
        const std::string typed_parent = parent_fragment(text);
        const std::string directory =
            absolute ? fs.parent(normalized_text)
                     : (typed_parent.empty() ? *current_dir : fs.join(*current_dir, typed_parent));
        const std::string prefix = base_name(text);
        auto entries = fs.list_directory(directory);
        entries.erase(std::remove_if(entries.begin(), entries.end(), [options_state](const FileEntry& entry) {
                          return (!options_state->show_hidden && hidden_name(entry.name)) ||
                                 !matches_filter(entry, *options_state);
                      }),
                      entries.end());
        std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) return a.is_directory && !b.is_directory;
            return a.name < b.name;
        });
        std::optional<FileEntry> match;
        for (const auto& entry : entries) {
            if (!starts_with_case_insensitive(entry.name, prefix)) continue;
            if (match) return false;
            match = entry;
        }
        if (!match) return false;
        std::string completed = fs.join(directory, match->name);
        if (match->is_directory) completed += "/";
        path_field_ptr->set_text(std::move(completed));
        return true;
    };
    path_field_ptr->complete_request = complete_path;

    list_ptr->on_activate = [&fs, current_dir, current_entries, visible_items, refresh, path_field_ptr](
                                std::size_t index) {
        if (index >= visible_items->size()) return;
        const FileListItem& item = (*visible_items)[index];
        if (item.kind == FileListItemKind::Parent) {
            *current_dir = fs.parent(*current_dir);
            (*refresh)();
            return;
        }
        if (item.kind == FileListItemKind::Recent) {
            *current_dir = item.path;
            (*refresh)();
            return;
        }
        const std::size_t entry_index = item.entry_index;
        if (entry_index >= current_entries->size()) return;
        const FileEntry& entry = (*current_entries)[entry_index];
        if (entry.is_directory) {
            *current_dir = fs.join(*current_dir, entry.name);
            (*refresh)();
        } else {
            path_field_ptr->set_text(fs.join(*current_dir, entry.name));
        }
    };

    filter_ptr->on_press = [options_state, refresh]() {
        if (options_state->filters.empty()) return;
        options_state->active_filter = (options_state->active_filter + 1) % options_state->filters.size();
        (*refresh)();
    };
    hidden_ptr->on_press = [options_state, refresh]() {
        options_state->show_hidden = !options_state->show_hidden;
        (*refresh)();
    };

    ok_ptr->on_press = [&fs, current_dir, path_field_ptr, window_ptr, completion, options_state]() {
        std::string chosen = path_field_ptr->text();
        if (chosen.empty()) return;  // nothing to accept
        chosen = fs.is_absolute_path(chosen) ? fs.normalize_path(chosen) : fs.join(*current_dir, chosen);
        if (options_state->recent_locations != nullptr)
            options_state->recent_locations->record(options_state->recent_locations_key, *current_dir);
        Window* const report_window = window_ptr;
        const std::shared_ptr<FileDialogCompletion> held_completion = completion;
        held_completion->report(FileDialogResult{true, std::move(chosen)}, report_window);
    };
    cancel_ptr->on_press = [window_ptr, completion]() {
        Window* const report_window = window_ptr;
        const std::shared_ptr<FileDialogCompletion> held_completion = completion;
        held_completion->report(FileDialogResult{}, report_window);
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
                                            const ui::StandardRoles& roles,
                                            const StandardStrings& strings) {
    return present_file_dialog(mode, std::move(initial_directory), fs, FileDialogOptions{}, app, desktop, roles,
                               strings);
}

FileDialogPresentation present_file_dialog(FileDialogMode mode, std::string initial_directory,
                                            const FileSystem& fs, FileDialogOptions options,
                                            ui::Application& app, Desktop& desktop,
                                            const ui::StandardRoles& roles,
                                            const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<FileDialogResult>;
    auto parts = Access::make();
    auto handle = make_file_dialog(mode, std::move(initial_directory), fs, std::move(options), roles, app, app.focused(),
                                   [state = parts.state](FileDialogResult result) {
                                       Access::record(state, std::move(result));
                                   },
                                   strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, FileDialogResult{});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

FileDialogResult exec_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                  ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                                  const StandardStrings& strings) {
    return exec_file_dialog(mode, std::move(initial_directory), fs, FileDialogOptions{}, app, desktop, roles,
                            strings);
}

FileDialogResult exec_file_dialog(FileDialogMode mode, std::string initial_directory, const FileSystem& fs,
                                  FileDialogOptions options, ui::Application& app, Desktop& desktop,
                                  const ui::StandardRoles& roles, const StandardStrings& strings) {
    std::optional<FileDialogResult> result;
    auto handle = make_file_dialog(mode, std::move(initial_directory), fs, std::move(options), roles, app, app.focused(),
                                   [&result](FileDialogResult value) { result = std::move(value); }, strings);
    desktop.exec_modal(app, std::move(handle));
    // Every normal acceptance/cancellation path records a result before
    // closing. If the host quits while the modal is still attached,
    // exec_modal ends its pump and cancellation is the documented fallback.
    return result.value_or(FileDialogResult{});
}

}  // namespace ckv::widgets
