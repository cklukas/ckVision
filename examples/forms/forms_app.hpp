// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/message_box.hpp"

namespace ckv::widgets {
class Button;
class DatePicker;
class CheckGroup;
class ComboBox;
class InputLine;
class RadioGroup;
class Slider;
class SpinBox;
class StatusLine;
class TimePicker;
class Wizard;
class Window;
}  // namespace ckv::widgets

namespace ckv::forms {

class FormsApp {
public:
    explicit FormsApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* window() const noexcept { return window_; }
    widgets::InputLine* name_input() const noexcept { return name_input_; }
    widgets::CheckGroup* options() const noexcept { return options_; }
    widgets::RadioGroup* mode() const noexcept { return mode_; }
    widgets::ComboBox* country() const noexcept { return country_; }
    widgets::DatePicker* date_picker() const noexcept { return date_picker_; }
    widgets::TimePicker* time_picker() const noexcept { return time_picker_; }
    widgets::SpinBox* spin_box() const noexcept { return spin_box_; }
    widgets::Slider* slider() const noexcept { return slider_; }
    widgets::Wizard* wizard() const noexcept { return wizard_; }
    widgets::Button* dialog_button() const noexcept { return dialog_button_; }
    widgets::Button* message_button() const noexcept { return message_button_; }
    widgets::Button* help_button() const noexcept { return help_button_; }

    void present_profile_dialog();
    void present_info_message();
    void present_help();

    int validation_attempts() const noexcept { return validation_attempts_; }
    const std::optional<widgets::DialogResult>& last_dialog_result() const noexcept { return last_dialog_result_; }
    const std::optional<widgets::MessageBoxResult>& last_message_result() const noexcept {
        return last_message_result_;
    }

    void set_close_allowed(bool allowed) noexcept { close_allowed_ = allowed; }
    bool close_allowed() const noexcept { return close_allowed_; }

private:
    void build_chrome();
    void build_window();
    widgets::DialogDescriptor make_profile_dialog_descriptor();

    ui::Application& app_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::InputLine* name_input_ = nullptr;
    widgets::CheckGroup* options_ = nullptr;
    widgets::RadioGroup* mode_ = nullptr;
    widgets::ComboBox* country_ = nullptr;
    widgets::DatePicker* date_picker_ = nullptr;
    widgets::TimePicker* time_picker_ = nullptr;
    widgets::SpinBox* spin_box_ = nullptr;
    widgets::Slider* slider_ = nullptr;
    widgets::Wizard* wizard_ = nullptr;
    widgets::Button* dialog_button_ = nullptr;
    widgets::Button* message_button_ = nullptr;
    widgets::Button* help_button_ = nullptr;

    widgets::MemoryHelpProvider help_provider_;
    std::optional<widgets::DescriptorDialogPresentation> profile_dialog_;
    std::optional<widgets::MessageBoxPresentation> message_box_;
    std::optional<widgets::HelpViewerPresentation> help_viewer_;
    std::optional<widgets::DialogResult> last_dialog_result_;
    std::optional<widgets::MessageBoxResult> last_message_result_;
    int validation_attempts_ = 0;
    bool close_allowed_ = false;
};

}  // namespace ckv::forms
