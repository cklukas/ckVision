// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "gallery_app.hpp"

#include "../example_about.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::gallery {

// Activating the menu, quitting, tiling, and cascading are framework
// concepts already in the standard command set (M9/WP-12), so the app
// only attaches handlers to commands().standard(). The four colour
// schemes are the one choice that belongs to this app, and they are
// declared as its own commands so the menu, the keymap and a test all
// reach them the same way.

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

    // The name field starts with the focus, and focusing it activates the
    // Controls window that holds it (the architecture §5): the keyboard's
    // window is the one drawn active. The two windows stand side by side,
    // so the active form covers none of the picture.
    app_.set_focus(name_input_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Gallery example",
                                ckv::examples::about_text(
                                    "An ordinary application shell carrying a form, a window and an image."));
}

void GalleryApp::build_menu_bar() {
    std::vector<widgets::MenuBarItem> menus;

    // A scheme is one ordinary command: the whole application repaints in
    // it, pictures included, and the menu marks which one is showing.
    const auto scheme_command = [this](ui::CommandDescriptor descriptor, auto factory, int index) {
        descriptor.handler = [this, factory, index] {
            app_.set_theme(factory(app_.roles(), roles_));
            active_scheme_ = index;
        };
        return app_.commands().declare(std::move(descriptor));
    };
    const ui::CommandId classic_scheme = scheme_command(
        {.key = "gallery.scheme.classic", .title = "&Classic", .category = "View"},
        ui::make_classic_theme, 0);
    const ui::CommandId dark_scheme = scheme_command(
        {.key = "gallery.scheme.dark", .title = "&Dark", .category = "View"},
        ui::make_dark_theme, 1);
    const ui::CommandId light_scheme = scheme_command(
        {.key = "gallery.scheme.light", .title = "&Light", .category = "View"},
        ui::make_light_theme, 2);
    const ui::CommandId mono_scheme = scheme_command(
        {.key = "gallery.scheme.mono", .title = "&Mono", .category = "View"},
        ui::make_mono_theme, 3);
    const auto scheme_item = [this](ui::CommandId command, int index) {
        return widgets::MenuItem::command(widgets::CommandPresentation{command})
            .with_mark_provider([this, index] {
                return active_scheme_ == index ? widgets::MenuMark::RadioOn : widgets::MenuMark::RadioOff;
            });
    };

    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{
        app_.commands().standard().help, "&About..."}));
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{
        app_.commands().standard().terminal_report}));
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

    widgets::MenuBarItem view_menu{"&View", {}};
    view_menu.items.push_back(widgets::MenuItem::submenu(
        "&Scheme", {scheme_item(classic_scheme, 0), scheme_item(dark_scheme, 1),
                    scheme_item(light_scheme, 2), scheme_item(mono_scheme, 3)}));
    menus.push_back(std::move(view_menu));

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
    // Right of the Sixel Demo window and its shadow, and narrow enough that
    // the pair fits a terminal nested in another application's window.
    window->set_bounds(Rect{38, 2, 32, 10});

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
        auto presentation = widgets::present_modal_message_box(*app_ptr, *desktop, roles_, descriptor);
        presentation.set_completion_handler([](widgets::MessageBoxResult) {});
    };
    content->add_child(std::move(greet));

    window->set_content(std::move(content));
    controls_window_ = desktop_->add_window(std::move(window));
}

std::shared_ptr<const Image> GalleryApp::make_demo_gradient() const {
    // Twice as tall as the window shows, so there is always somewhere to
    // scroll: red runs across, green runs down, and blue answers red.
    auto image = std::make_shared<Image>(PixelSize{62, 112});
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
    window->set_bounds(Rect{2, 2, 34, 16});  // under the View menu, so an open menu crosses the picture

    // ckvision-doc: gallery-scrolled-picture
    // The picture is taller than the window, so it lives in a viewport: the
    // wheel over it, or Up, Down, PageUp and PageDown once the viewport has
    // the focus, move it a row or a page at a time. Nothing inside a picture
    // can take the focus, so the viewport itself is the tab stop, and it
    // scrolls only vertically, since a picture cut off at the side is not
    // one a reader can scroll back to.
    auto viewport = std::make_unique<widgets::ScrollViewport>(ui::FocusPolicy::TabStop);
    viewport->set_horizontal_scrollbar_policy(widgets::ScrollbarPolicy::Hidden);
    viewport->set_bounds(Rect{0, 0, 32, 14});

    // 62 x 112 pixels over 31 columns is 28 rows on a terminal whose cells
    // are twice as tall as they are wide; ImageView keeps the proportions
    // on any other cell and centres the picture in the rows it was given.
    auto view = std::make_unique<widgets::ImageView>();
    view->set_role_override(roles_.dialog_background);
    view->set_preferred_size(Size{31, 28});
    view->set_image(make_demo_gradient());
    image_view_ = view.get();
    viewport->set_content(std::move(view));
    picture_viewport_ = viewport.get();
    window->set_content(std::move(viewport));
    // ckvision-doc-end: gallery-scrolled-picture

    image_window_ = desktop_->add_window(std::move(window));
}

}  // namespace ckv::gallery
