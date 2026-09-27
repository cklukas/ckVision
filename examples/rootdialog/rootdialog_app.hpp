// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Root Dialog: a form that is the whole application (the roadmap M4 exit).
// A materialized descriptor dialog is mounted directly under
// Application::root() -- no Desktop, no Window, no frame -- beside a
// StaticText that says what the form is for. Without a Window there is no
// accept_request or cancel_request to wire, so the surface around the
// dialog takes those two requests itself: Enter accepts through
// widgets::validate_dialog (a failing field vetoes and takes the focus),
// Esc cancels without validating, and Alt+letter reaches a field's label
// buddy or a button through widgets::activate_control_mnemonic. Tab and
// Shift-Tab need nothing: the Application's default keymap traverses the
// root tree in declaration order. See docs/example-apps.md.
#pragma once

#include <optional>
#include <string>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/dialog.hpp"

namespace ckv::widgets {
class Button;
class InputLine;
class Label;
class StaticText;
}  // namespace ckv::widgets

namespace ckv::rootdialog {

// How the form ended. Accepted carries the answers; a cancelled form carries
// none, exactly like a cancelled DialogResult.
struct Outcome {
    bool accepted = false;
    std::string name;
    std::string email;

    friend bool operator==(const Outcome&, const Outcome&) = default;
};

class RootDialogApp {
public:
    explicit RootDialogApp(ui::Application& app);

    widgets::StaticText* intro() const noexcept { return intro_; }
    widgets::StaticText* status() const noexcept { return status_; }
    widgets::Label* name_label() const noexcept { return dialog_.labels[kNameField]; }
    widgets::Label* email_label() const noexcept { return dialog_.labels[kEmailField]; }
    widgets::InputLine* name_input() const noexcept { return dialog_.inputs[kNameField]; }
    widgets::InputLine* email_input() const noexcept { return dialog_.inputs[kEmailField]; }
    widgets::Button* ok_button() const noexcept { return dialog_.buttons[kOkButton]; }
    widgets::Button* cancel_button() const noexcept { return dialog_.buttons[kCancelButton]; }
    widgets::Button* about_button() const noexcept { return dialog_.buttons[kAboutButton]; }

    // Empty while the form is open; set once by the accept or the cancel that
    // ends it, which also asks the Application to quit.
    const std::optional<Outcome>& outcome() const noexcept { return outcome_; }
    // How many accepts a field's validator has refused.
    int vetoes() const noexcept { return vetoes_; }

    // The two requests a Window would otherwise make of a hosted dialog.
    // Accept validates every field first; cancel never does.
    void accept();
    void cancel();

private:
    static constexpr std::size_t kNameField = 0;
    static constexpr std::size_t kEmailField = 1;
    static constexpr std::size_t kOkButton = 0;
    static constexpr std::size_t kCancelButton = 1;
    static constexpr std::size_t kAboutButton = 2;

    widgets::DialogDescriptor make_descriptor();
    void show_about();

    ui::Application& app_;
    ui::StandardRoles roles_;
    widgets::DialogDescriptor descriptor_;
    widgets::MaterializedDialog dialog_;
    widgets::StaticText* intro_ = nullptr;
    widgets::StaticText* status_ = nullptr;
    std::optional<Outcome> outcome_;
    int vetoes_ = 0;
};

}  // namespace ckv::rootdialog
