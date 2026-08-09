// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "forms_app.hpp"

#include <memory>
#include <string>
#include <vector>

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::forms {
namespace {
widgets::StandardStrings teaching_strings() {
    widgets::StandardStrings strings;
    strings.ok = "Accept";
    strings.cancel = "Dismiss";
    strings.help_title = "Forms Help";
    return strings;
}
}  // namespace

FormsApp::FormsApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    help_provider_.add_topic("forms", widgets::HelpTopic{"Forms",
                                                          "Forms demonstrates descriptor dialogs, validation, "
                                                          "localized standard strings, and close veto.",
                                                          {{"dialogs", "Descriptor dialogs"}}});
    help_provider_.add_topic("dialogs", widgets::HelpTopic{"Descriptor dialogs",
                                                            "Accept validates required fields; Escape cancels.",
                                                            {{"forms", "Back to forms"}}});

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_chrome();
    build_window();

    app_.commands().set_handler(ui::std_commands::kQuit, [this] { app_.request_quit(); });
    app_.set_focus(name_input_);
}

void FormsApp::build_chrome() {
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kQuit}});

    widgets::MenuBarItem help_menu{"&Help", {}};
    help_menu.items.push_back(widgets::MenuItem{"&Forms help", ui::kInvalidCommand, [this] { present_help(); }});

    desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file_menu), std::move(help_menu)}));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kMenu}},
                       widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kHelp}},
                       widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kQuit}}});
    status->set_hint_provider([](const std::string& key) {
        if (key == "forms") return std::string{"F1 opens the forms help topic"};
        return std::string{};
    });
    desktop_->dock_bottom(std::move(status));
}

void FormsApp::build_window() {
    auto window = std::make_unique<widgets::Window>("Forms");
    window->set_bounds(Rect{4, 3, 58, 16});
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                              roles_.dialog_background);
    window->close_request = [this] { return close_allowed_; };

    auto content = std::make_unique<ui::View>();

    auto intro = std::make_unique<widgets::StaticText>(
        "Public form widgets plus descriptor dialogs. This window initially vetoes Close.");
    intro->set_bounds(Rect{1, 1, 52, 2});
    content->add_child(std::move(intro));

    auto name_label = std::make_unique<widgets::Label>("&Name:");
    name_label->set_bounds(Rect{1, 4, 8, 1});
    content->add_child(std::move(name_label));

    auto name = std::make_unique<widgets::InputLine>();
    name->set_bounds(Rect{10, 4, 22, 1});
    name->set_help_context_key("forms");
    name_input_ = name.get();
    content->add_child(std::move(name));

    auto options = std::make_unique<widgets::CheckGroup>(
        std::vector<std::string>{"&Validate on accept", "&Tri-state option", "&Remember value"});
    options->set_bounds(Rect{1, 6, 26, 3});
    options->set_tristate(true);
    options->set_check_state(1, widgets::CheckState::Mixed);
    options_ = options.get();
    content->add_child(std::move(options));

    auto mode = std::make_unique<widgets::RadioGroup>(std::vector<std::string>{"&Modal", "Mode&less"});
    mode->set_bounds(Rect{30, 6, 14, 2});
    mode->set_selected(0);
    mode_ = mode.get();
    content->add_child(std::move(mode));

    auto country = std::make_unique<widgets::ComboBox>(widgets::ComboBoxMode::Editable);
    country->set_bounds(Rect{30, 9, 20, 4});
    country->set_items({"US", "DE", "FR", "JP"});
    country->set_text("DE");
    country_ = country.get();
    content->add_child(std::move(country));

    auto dialog = std::make_unique<widgets::Button>("&Profile...");
    dialog->set_bounds(Rect{1, 11, 14, 2});
    dialog->set_default(true);
    dialog->on_press = [this] { present_profile_dialog(); };
    dialog_button_ = dialog.get();
    content->add_child(std::move(dialog));

    auto message = std::make_unique<widgets::Button>("&Info");
    message->set_bounds(Rect{18, 11, 12, 2});
    message->on_press = [this] { present_info_message(); };
    message_button_ = message.get();
    content->add_child(std::move(message));

    auto help = std::make_unique<widgets::Button>("&Help");
    help->set_bounds(Rect{33, 11, 12, 2});
    help->on_press = [this] { present_help(); };
    help_button_ = help.get();
    content->add_child(std::move(help));

    window->set_content(std::move(content));
    window_ = desktop_->add_window(std::move(window));
}

widgets::DialogDescriptor FormsApp::make_profile_dialog_descriptor() {
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Profile";
    descriptor.resizable = true;
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", name_input_->text(),
        [this](const std::string& value) {
            ++validation_attempts_;
            return !value.empty();
        }});
    descriptor.fields.push_back(widgets::FieldDescriptor{"&Email:", "", [](const std::string& value) {
                                                             return value.find('@') != std::string::npos;
                                                         }});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{"&OK", true, [] {}});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{"&Cancel", false, [] {}});
    return descriptor;
}

void FormsApp::present_profile_dialog() {
    profile_dialog_ = widgets::present_dialog(make_profile_dialog_descriptor(), app_, *desktop_, roles_);
    profile_dialog_->set_completion_handler([this](widgets::DialogResult result) {
        last_dialog_result_ = std::move(result);
    });
}

void FormsApp::present_info_message() {
    message_box_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Info, "Forms",
                                      "Info dialog uses caller-supplied standard strings.",
                                      widgets::MessageBoxButtons::Ok},
        teaching_strings());
    message_box_->set_completion_handler([this](widgets::MessageBoxResult result) {
        last_message_result_ = result;
    });
}

void FormsApp::present_help() {
    help_viewer_ =
        widgets::present_help_viewer(help_provider_, "forms", app_, *desktop_, roles_, teaching_strings());
}

}  // namespace ckv::forms
