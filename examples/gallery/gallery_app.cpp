// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "gallery_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::gallery {

// Every command this app needs is already in the standard set (M9/
// WP-12) — activating the menu, quitting, tiling, and cascading are
// all framework concepts with no gallery-specific wording to pin, so
// this app declares no commands of its own at all; it just attaches
// handlers to commands().standard().

GalleryApp::GalleryApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    // Items referencing a command carry no label text of their own
    // (M9/WP-11) — title (with mnemonic), chord hint, and enablement
    // all render live from the registration below.
    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{
                           widgets::CommandPresentation{app_.commands().standard().menu}},
                       widgets::StatusLineItem{
                           widgets::CommandPresentation{app_.commands().standard().quit}}});
    // dock_bottom (not add_child + a one-time set_bounds) keeps the
    // status line pinned to the last row across every future terminal
    // resize — Desktop::on_resized() re-derives its position from
    // content_area() automatically; see tests/test_m8_integration.cpp.
    status_ = desktop_->dock_bottom(std::move(status));

    build_menu_bar();
    build_controls_window();
    build_image_window();

    // Every one of these ids is already registered by Application's
    // constructor (M9/WP-12) — only kQuit needs a handler attached
    // here. kMenu/kTile/kCascade already have one each:
    // MenuBar::on_attached() installs itself as kMenu's default the
    // moment it attaches (M9/WP-13, D-029), and Desktop::on_attached()
    // does the same for kTile/kCascade (M10/WP-13 completion) — both
    // already fired at desktop_'s own attachment above, before this
    // point.
    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });

    app_.set_focus(name_input_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Gallery example",
                                "An ordinary application shell carrying a form, a window and an image.");
}

void GalleryApp::build_menu_bar() {
    std::vector<widgets::MenuBarItem> menus;

    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::action("&About...",
                                                  [this] {
                                                      widgets::MessageBoxDescriptor descriptor{
                                                          widgets::MessageBoxKind::Info, "About Gallery",
                                                          "ckVision Gallery — a demonstration application.",
                                                          widgets::MessageBoxButtons::Ok};
                                                      auto presentation =
                                                          widgets::present_message_box(app_, *desktop_, roles_, descriptor);
                                                      presentation.set_completion_handler([](widgets::MessageBoxResult) {});
                                                  }));
    file_menu.items.push_back(widgets::MenuItem::separator());
    file_menu.items.push_back(
        widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit}));
    menus.push_back(std::move(file_menu));

    widgets::MenuBarItem window_menu{"&Window", {}};
    window_menu.items.push_back(
        widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().tile}));
    window_menu.items.push_back(
        widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().cascade}));
    menus.push_back(std::move(window_menu));

    auto menu = std::make_unique<widgets::MenuBar>(std::move(menus));
    desktop_->dock_top(std::move(menu));
}

void GalleryApp::build_controls_window() {
    // Dialog-gray chrome: the Controls window is a form (a dialog),
    // and dialogs are the gray family — the Sixel Demo window below
    // keeps the blue document-window chrome so both looks appear.
    auto window = std::make_unique<widgets::Window>("Controls");
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                               roles_.dialog_background);
    window->set_bounds(Rect{2, 2, 40, 10});

    auto content = std::make_unique<ui::View>();
    auto label = std::make_unique<widgets::Label>("&Name:");
    label->set_bounds(Rect{1, 1, 8, 1});
    content->add_child(std::move(label));

    auto input = std::make_unique<widgets::InputLine>();
    input->set_bounds(Rect{10, 1, 20, 1});
    name_input_ = input.get();
    content->add_child(std::move(input));

    auto greet = std::make_unique<widgets::Button>("&Greet");
    greet->set_bounds(Rect{1, 3, 12, 2});  // two rows: face + drop shadow
    greet->set_default(true);
    widgets::InputLine* name_input = name_input_;
    ui::Application* app_ptr = &app_;
    widgets::Desktop* desktop = desktop_;
    greet->on_press = [name_input, app_ptr, desktop, this] {
        widgets::MessageBoxDescriptor descriptor{
            widgets::MessageBoxKind::Info, "Greeting",
            name_input->text().empty() ? "Hello, stranger!" : "Hello, " + name_input->text() + "!",
            widgets::MessageBoxButtons::Ok};
        auto presentation = widgets::present_message_box(*app_ptr, *desktop, roles_, descriptor);
        presentation.set_completion_handler([](widgets::MessageBoxResult) {});
    };
    content->add_child(std::move(greet));

    window->set_content(std::move(content));
    controls_window_ = desktop_->add_window(std::move(window));
}

std::shared_ptr<const Image> GalleryApp::make_demo_gradient() const {
    auto image = std::make_shared<Image>(64, 32);
    for (int y = 0; y < image->height(); ++y) {
        for (int x = 0; x < image->width(); ++x) {
            const auto r = static_cast<std::uint8_t>(x * 255 / (image->width() - 1));
            const auto g = static_cast<std::uint8_t>(y * 255 / (image->height() - 1));
            image->set_pixel(x, y, Image::Rgba{r, g, static_cast<std::uint8_t>(255 - r), 255});
        }
    }
    return image;
}

void GalleryApp::build_image_window() {
    auto window = std::make_unique<widgets::Window>("Sixel Demo");  // default document-window roles
    window->set_bounds(Rect{20, 6, 34, 16});

    auto view = std::make_unique<widgets::ImageView>();
    view->set_role_override(roles_.dialog_background);
    view->set_bounds(Rect{0, 0, 32, 14});
    view->set_image(make_demo_gradient());
    image_view_ = view.get();
    window->set_content(std::move(view));

    image_window_ = desktop_->add_window(std::move(window));
}

}  // namespace ckv::gallery
