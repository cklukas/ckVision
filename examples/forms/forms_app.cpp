// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "forms_app.hpp"

#include "../example_about.hpp"

#include <memory>
#include <string>
#include <vector>

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
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

    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_focus(name_input_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Forms example",
                                ckv::examples::about_text(
                                    "Field controls, validation, context help and a wizard flow."));
}

void FormsApp::build_chrome() {
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit}));

    widgets::MenuBarItem help_menu{"&Help", {}};
    help_menu.items.push_back(widgets::MenuItem::action("&Forms help", [this] { present_help(); }));
    help_menu.items.push_back(widgets::MenuItem::separator());
    help_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{
        app_.commands().standard().help, "&About..."}));

    desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file_menu), std::move(help_menu)}));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                       widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().help}},
                       widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().quit}}});
    status->set_hint_provider([](const std::string& key) {
        if (key == "forms") return std::string{"F1 opens the forms help topic"};
        return std::string{};
    });
    desktop_->dock_bottom(std::move(status));
}

void FormsApp::build_window() {
    auto window = std::make_unique<widgets::Window>("Forms");
    window->set_bounds(Rect{4, 3, 68, 19});
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
    options->set_group_label("Options");
    options->set_bounds(Rect{1, 6, 26, 4});
    options->set_tristate(true);
    options->set_check_state(1, widgets::CheckState::Mixed);
    options_ = options.get();
    content->add_child(std::move(options));

    auto mode = std::make_unique<widgets::RadioGroup>(std::vector<std::string>{"&Modal", "Mode&less"});
    mode->set_group_label("Presentation mode");
    mode->set_bounds(Rect{30, 6, 14, 3});
    mode->set_selected(0);
    mode_ = mode.get();
    content->add_child(std::move(mode));

    auto country = std::make_unique<widgets::ComboBox>(widgets::ComboBoxMode::Editable);
    country->set_bounds(Rect{30, 9, 20, 4});
    country->set_items({"US", "DE", "FR", "JP"});
    country->set_text("DE");
    country_ = country.get();
    content->add_child(std::move(country));

    auto date = std::make_unique<widgets::DatePicker>();
    date->set_bounds(Rect{1, 10, 13, 1});
    date->set_value(widgets::DateValue{2026, 8, 9});
    date_picker_ = date.get();
    content->add_child(std::move(date));

    auto time = std::make_unique<widgets::TimePicker>();
    time->set_bounds(Rect{16, 10, 10, 1});
    time->set_value(widgets::TimeValue{14, 30, 0});
    time_picker_ = time.get();
    content->add_child(std::move(time));

    auto spin = std::make_unique<widgets::SpinBox>();
    spin->set_bounds(Rect{30, 13, 10, 1});
    spin->set_range(0, 10);
    spin->set_value(3);
    spin_box_ = spin.get();
    content->add_child(std::move(spin));

    auto slider = std::make_unique<widgets::Slider>();
    slider->set_bounds(Rect{42, 13, 18, 1});
    slider->set_value(40);
    slider_ = slider.get();
    content->add_child(std::move(slider));

    auto wizard = std::make_unique<widgets::Wizard>();
    wizard->set_bounds(Rect{47, 4, 16, 5});
    wizard->set_pages({widgets::WizardPage{"Step 1", [this] { return !name_input_->text().empty(); }},
                       widgets::WizardPage{"Step 2", [] { return true; }}});
    wizard_ = wizard.get();
    content->add_child(std::move(wizard));

    auto dialog = std::make_unique<widgets::Button>("&Profile...");
    dialog->set_bounds(Rect{1, 14, 14, 2});
    dialog->set_default(true);
    dialog->on_press = [this] { present_profile_dialog(); };
    dialog_button_ = dialog.get();
    content->add_child(std::move(dialog));

    auto message = std::make_unique<widgets::Button>("&Info");
    message->set_bounds(Rect{18, 14, 12, 2});
    message->on_press = [this] { present_info_message(); };
    message_button_ = message.get();
    content->add_child(std::move(message));

    auto help = std::make_unique<widgets::Button>("&Help");
    help->set_bounds(Rect{33, 14, 12, 2});
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
    descriptor.buttons.push_back(widgets::ButtonDescriptor{"&OK", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
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
