// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "completion_app.hpp"
#include "../example_about.hpp"

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::completion_demo {
CompletionApp::CompletionApp(ui::Application& app, DirectoryService& directories, std::string initial_directory)
    : app_(app), directories_(directories), roles_(ui::intern_standard_roles(app.roles())),
      initial_directory_(std::move(initial_directory)) {
    const auto& commands = app_.commands().standard();
    app_.commands().set_handler(commands.quit, [&app] { app.request_quit(); });
    widgets::ApplicationShell shell(app_, {.theme = ui::make_classic_theme(app_.roles(), roles_),
        .menus = {{"&File", {
            widgets::MenuItem::command(widgets::CommandPresentation{commands.help, "&About..."}),
            widgets::MenuItem::command(widgets::CommandPresentation{commands.quit})}}},
        .status_items = {widgets::StatusLineItem{widgets::CommandPresentation{commands.help}},
                         widgets::StatusLineItem{widgets::CommandPresentation{commands.quit}}}});
    desktop_ = &shell.desktop();
    widgets::install_about_help(app_, *desktop_, roles_, "ckVision Completion Lab",
        examples::about_text("Right accepts a gray completion suffix; Tab moves focus. "
            "The file and folder dialogs share filtered filename suggestions and their visible list. "
            "Enter on docs, .., ../images, or an absolute directory navigates without closing the dialog."));
    auto window = std::make_unique<widgets::Window>("Completion Lab");
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background,
                              roles_.dialog_frame, roles_.dialog_background);
    auto column = std::make_unique<ui::Column>(); column->set_spacing(0);
    column->add_item(std::make_unique<widgets::StaticText>(
        "Type a prefix. Right accepts the gray suffix; Tab moves focus.\n"
        "In file and folder dialogs, Enter on a directory navigates.\n"
        "Try docs, ../images, .., or an absolute path; filters also apply to suggestions."), {ui::SizePolicy::Fixed, 1});
    column->add_item(std::make_unique<widgets::StaticText>("Command (open-file or open-folder):"), {ui::SizePolicy::Fixed, 1});
    auto command = std::make_unique<widgets::InputLine>(); command_ = command.get();
    column->add_item(std::move(command), {ui::SizePolicy::Fixed, 1});
    command_->set_completions({"open-file", "open-folder", "refresh", "search"});
    command_->set_text("open-f");
    command_->on_accept = [this] {
        if (command_->text() == "open-file") open_file();
        else if (command_->text() == "open-folder") open_folder();
    };
    column->add_item(std::make_unique<widgets::StaticText>("SearchBox: completion without submitting a search"), {ui::SizePolicy::Fixed, 1});
    auto search = std::make_unique<widgets::SearchBox>();
    search->field().set_completions({"author:", "filename:", "modified:", "type:"});
    search->field().set_text("fi");
    column->add_item(std::move(search), {ui::SizePolicy::Fixed, 1});
    column->add_item(std::make_unique<widgets::StaticText>("Editable ComboBox: caller-supplied vocabulary"), {ui::SizePolicy::Fixed, 1});
    auto combo = std::make_unique<widgets::ComboBox>(widgets::ComboBoxMode::Editable);
    combo->set_items({"Markdown", "Plain text", "Source code"});
    combo->field().set_completions(combo->items()); combo->set_text("Mar");
    column->add_item(std::move(combo), {ui::SizePolicy::Fixed, 1});
    auto buttons = std::make_unique<ui::Row>(); buttons->set_spacing(2);
    auto file_button = std::make_unique<widgets::Button>("Open File"); auto* file = file_button.get();
    buttons->add_item(std::move(file_button), {ui::SizePolicy::Fixed, 1});
    file->on_press = [this] { open_file(); };
    auto folder_button = std::make_unique<widgets::Button>("Select Folder"); auto* folder = folder_button.get();
    buttons->add_item(std::move(folder_button), {ui::SizePolicy::Fixed, 1});
    folder->on_press = [this] { open_folder(); };
    auto quit_button = std::make_unique<widgets::Button>("Quit"); auto* quit = quit_button.get();
    buttons->add_item(std::move(quit_button), {ui::SizePolicy::Fixed, 1});
    quit->on_press = [this] { app_.request_quit(); };
    column->add_item(std::move(buttons), {ui::SizePolicy::Fixed, 1});
    auto result = std::make_unique<widgets::StaticText>("No file or folder selected."); result_ = result.get();
    column->add_item(std::move(result), {ui::SizePolicy::Fixed, 1});
    window->set_content(std::move(column));
    window->set_bounds({5, 2, 70, 20});
    window->cancel_request = [this] { app_.request_quit(); };
    desktop_->present_modeless({std::move(window), command_}, app_);
    command_->set_cursor(command_->cursor());
}
CompletionApp::~CompletionApp() {
    lifetime_.reset();
    app_.commands().set_handler(app_.commands().standard().help, {});
    app_.commands().set_handler(app_.commands().standard().quit, {});
    app_.root().remove_child(desktop_);
}
void CompletionApp::open_file() {
    widgets::FileDialogOptions options;
    options.filters = {{"Text", {"txt", "md"}}, {"Images", {"png", "jpg", "svg"}}, {"All files", {}}};
    options.recent_locations_key = "completion-demo.locations";
    auto picker = widgets::present_modal_file_dialog(widgets::FileDialogMode::Open, initial_directory_, directories_,
                                                    std::move(options), app_, *desktop_, roles_);
    pending_.await(std::move(picker), [this, alive = std::weak_ptr<void>(lifetime_)](widgets::FileDialogResult result) {
        if (alive.expired()) return;
        result_->set_text(result.accepted ? "File: " + result.path : "File selection cancelled.");
    });
}
void CompletionApp::open_folder() {
    auto picker = widgets::present_modal_directory_picker(directories_, initial_directory_, app_, *desktop_, roles_);
    pending_.await(std::move(picker), [this, alive = std::weak_ptr<void>(lifetime_)](widgets::DirectoryPickerResult result) {
        if (alive.expired()) return;
        result_->set_text(result.accepted ? "Folder: " + result.path : "Folder selection cancelled.");
    });
}
} // namespace ckv::completion_demo
