// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <stdexcept>

#include "appearance_matrix.hpp"

namespace ckv::docgen::appearance {

std::string_view scheme_name(Scheme scheme) noexcept {
    switch (scheme) {
        case Scheme::Classic: return "classic";
        case Scheme::Dark: return "dark";
        case Scheme::Light: return "light";
        case Scheme::Mono: return "mono";
    }
    return "classic";
}

namespace {

ui::Theme make_theme(Scheme scheme, const ui::RoleRegistry& registry, const ui::StandardRoles& roles) {
    switch (scheme) {
        case Scheme::Classic: return ui::make_classic_theme(registry, roles);
        case Scheme::Dark: return ui::make_dark_theme(registry, roles);
        case Scheme::Light: return ui::make_light_theme(registry, roles);
        case Scheme::Mono: return ui::make_mono_theme(registry, roles);
    }
    return ui::make_classic_theme(registry, roles);
}

}  // namespace

Stage::Stage(Size screen, Scheme scheme, Graphics graphics)
    : terminal_(screen, graphics == Graphics::Sixel ? term::headless_sixel_profile()
                                                    : term::headless_no_graphics_profile()),
      app_(terminal_, clock_) {
    roles_ = ui::intern_standard_roles(app_.roles());
    app_.theme() = make_theme(scheme, app_.roles(), roles_);
    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));
}

widgets::Window& Stage::window(Rect bounds, std::string title, std::unique_ptr<ui::View> content,
                               bool dialog_roles) {
    auto frame = std::make_unique<widgets::Window>(std::move(title));
    frame->set_bounds(bounds);
    frame->set_content(std::move(content));
    if (dialog_roles)
        frame->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                                 roles_.dialog_background);
    return add_window(std::move(frame));
}

widgets::Window& Stage::add_window(std::unique_ptr<widgets::Window> window) {
    widgets::Window& added = *desktop_->add_window(std::move(window));
    // Adding a window activates it, and the activation carries the focus to
    // its first focus stop (D-107). A specimen's focus is its state's to give:
    // every window starts unfocused here, and a state that shows a focused
    // look asks for it through focus().
    app_.set_focus(nullptr);
    return added;
}

ui::View& Stage::dialog(Rect bounds, std::string title) {
    return *window(bounds, std::move(title), std::make_unique<ui::View>(), true).content();
}

ui::View& Stage::document(Rect bounds, std::string title) {
    return *window(bounds, std::move(title), std::make_unique<ui::View>(), false).content();
}

void Stage::focus(ui::View& view) {
    if (!view.focusable()) throw std::logic_error("appearance specimen focuses a view that cannot take focus");
    app_.set_focus(&view);
}

void Stage::step() { app_.step(0); }

}  // namespace ckv::docgen::appearance
