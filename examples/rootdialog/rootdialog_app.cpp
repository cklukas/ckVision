// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "rootdialog_app.hpp"

#include "../example_about.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "cvision/scene/painter.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::rootdialog {
namespace {

constexpr const char* kIntroText =
    "A form that is the whole application: no desktop, no window, no frame. "
    "Tab and Shift-Tab move between the controls, Alt with an underlined letter "
    "jumps to one, Enter accepts and Esc cancels.";
constexpr const char* kReadyText = "Enter accepts the form; Esc leaves it unchanged.";

// The undecorated root surface. It paints the dialog background edge to
// edge, holds its content one cell in from every edge, and answers the three
// keys a Window answers for a dialog it hosts: Enter, Esc and Alt+mnemonic.
// Every other key has already been offered to the focused control, because
// the surface is the last view on the focus path before the root.
class FormSurface final : public ui::View {
public:
    FormSurface(ui::RoleId background_role, std::function<void()> accept,
                std::function<void()> cancel)
        : background_role_(background_role),
          accept_(std::move(accept)),
          cancel_(std::move(cancel)) {
        content_ = make<ui::Column>();
    }

    ui::Column& content() noexcept { return *content_; }

    void on_resized() override {
        const Rect local = bounds();
        content_->set_bounds(
            Rect{1, 1, std::max(0, local.width - 2), std::max(0, local.height - 2)});
    }

    void draw(scene::Painter& painter) override {
        painter.fill(Rect{0, 0, bounds().width, bounds().height},
                     Cell::from_grapheme(" ", context().theme->resolve(background_role_)));
    }

    bool on_key(const KeyEvent& event) override {
        if (event.chord.key == Key::Enter) {
            accept_();
            return true;
        }
        if (event.chord.key == Key::Escape) {
            cancel_();
            return true;
        }
        return context().app != nullptr &&
               widgets::activate_control_mnemonic(*this, event, *context().app);
    }

private:
    ui::RoleId background_role_;
    std::function<void()> accept_;
    std::function<void()> cancel_;
    ui::Column* content_ = nullptr;
};

}  // namespace

RootDialogApp::RootDialogApp(ui::Application& app)
    : app_(app), roles_(ui::intern_standard_roles(app.roles())), descriptor_(make_descriptor()) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto surface = std::make_unique<FormSurface>(roles_.dialog_background, [this] { accept(); },
                                                 [this] { cancel(); });
    ui::Column& column = surface->content();
    column.set_spacing(1);

    auto intro = std::make_unique<widgets::StaticText>(kIntroText);
    intro_ = intro.get();
    column.add_item(std::move(intro));

    dialog_ = widgets::materialize_dialog(descriptor_);
    column.add_item(std::move(dialog_.root), ui::LayoutSpec{ui::SizePolicy::Expanding});

    auto status = std::make_unique<widgets::StaticText>(kReadyText);
    status_ = status.get();
    column.add_item(std::move(status), ui::LayoutSpec{ui::SizePolicy::Fixed});

    app_.root().add_child(std::move(surface));

    // A veto marks what was wrong when the reader accepted. Editing a field
    // is the reader answering that, so the edit supersedes the mark and the
    // reason; the next accept judges the field again.
    for (widgets::InputLine* input : {name_input(), email_input()}) {
        input->on_edited = [this, input] {
            input->set_valid(true);
            status_->set_text(kReadyText);
        };
    }

    // Alt+X leaves the form the way Esc does. There is no Desktop here to
    // give the standard quit command its window-sweeping default.
    app_.commands().set_handler(app_.commands().standard().quit, [this] { cancel(); });
    app_.set_focus(dialog_.initial_focus);
}

widgets::DialogDescriptor RootDialogApp::make_descriptor() {
    widgets::DialogDescriptor descriptor;
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", "", [](const std::string& value) { return !value.empty(); }});
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Email:", "",
        [](const std::string& value) { return value.find('@') != std::string::npos; }});
    // Each button's press is the request its role names. materialize_dialog
    // copies these callbacks unchanged; nothing else wires them.
    descriptor.buttons.push_back(
        widgets::ButtonDescriptor{"&OK", widgets::ButtonRole::Accept, [this] { accept(); }});
    descriptor.buttons.push_back(
        widgets::ButtonDescriptor{"&Cancel", widgets::ButtonRole::Dismiss, [this] { cancel(); }});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{
        "&About", widgets::ButtonRole::Neutral, [this] { show_about(); }});
    return descriptor;
}

void RootDialogApp::accept() {
    if (outcome_) return;
    // The veto is validate_dialog's: every field is marked valid or invalid,
    // and the first invalid one takes the focus.
    if (!widgets::validate_dialog(dialog_, descriptor_, app_)) {
        ++vetoes_;
        status_->set_text(!name_input()->valid() ? "A name is required."
                                                 : "An email address needs an @.");
        return;
    }
    outcome_ = Outcome{true, name_input()->text(), email_input()->text()};
    status_->set_text("Accepted.");
    app_.request_quit();
}

void RootDialogApp::cancel() {
    if (outcome_) return;
    outcome_ = Outcome{};
    status_->set_text("Cancelled.");
    app_.request_quit();
}

void RootDialogApp::show_about() {
    intro_->set_text(
        ckv::examples::about_text("ckVision Root Dialog example: the M4 form, undecorated."));
}

}  // namespace ckv::rootdialog
