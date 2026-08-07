// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "gallery_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::gallery {

namespace {
constexpr ui::CommandId kCmdActivateMenu = 1;
constexpr ui::CommandId kCmdQuit = 2;
constexpr ui::CommandId kCmdTile = 3;
constexpr ui::CommandId kCmdCascade = 4;
}  // namespace

GalleryApp::GalleryApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background);
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    auto status = std::make_unique<widgets::StatusLine>(app_.theme(), roles_.menu_bar_normal, app_);
    status->set_items({{"F10 Menu", ui::kInvalidCommand}, {"Alt+X Quit", kCmdQuit}});
    // dock_bottom (not add_child + a one-time set_bounds) keeps the
    // status line pinned to the last row across every future terminal
    // resize — Desktop::on_resized() re-derives its position from
    // content_area() automatically; see tests/test_m8_integration.cpp.
    status_ = static_cast<widgets::StatusLine*>(desktop_->dock_bottom(std::move(status)));

    build_menu_bar();
    build_controls_window();
    build_image_window();

    app_.commands().register_command(kCmdActivateMenu, "Activate Menu Bar", "Window",
                                      KeyChord{Key::F10, Modifier::None, ""});
    app_.commands().register_command(kCmdQuit, "Quit", "File",
                                      KeyChord{Key::Char, Modifier::Alt, "x"});
    app_.commands().register_command(kCmdTile, "Tile Windows", "Window");
    app_.commands().register_command(kCmdCascade, "Cascade Windows", "Window");

    app_.set_command_handler(kCmdActivateMenu, [this] { menu_->activate(); });
    app_.set_command_handler(kCmdQuit, [this] { quit_ = true; });
    app_.set_command_handler(kCmdTile, [this] { desktop_->tile(); });
    app_.set_command_handler(kCmdCascade, [this] { desktop_->cascade(); });

    app_.set_focus(name_input_);
}

void GalleryApp::build_menu_bar() {
    std::vector<widgets::MenuBarItem> menus;

    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem{"&About...", ui::kInvalidCommand,
                                                  [this] {
                                                      widgets::MessageBoxDescriptor descriptor{
                                                          widgets::MessageBoxKind::Info, "About Gallery",
                                                          "ckVision Gallery — a demonstration application.",
                                                          widgets::MessageBoxButtons::Ok};
                                                      auto handle = widgets::make_message_box(
                                                          descriptor, app_.theme(), roles_, app_, app_.focused(),
                                                          nullptr);
                                                      desktop_->show_dialog(std::move(handle), app_);
                                                  },
                                                  false});
    file_menu.items.push_back(widgets::MenuItem{"", ui::kInvalidCommand, nullptr, true});
    file_menu.items.push_back(widgets::MenuItem{"E&xit", kCmdQuit, nullptr, false});
    menus.push_back(std::move(file_menu));

    widgets::MenuBarItem window_menu{"&Window", {}};
    window_menu.items.push_back(widgets::MenuItem{"&Tile", kCmdTile, nullptr, false});
    window_menu.items.push_back(widgets::MenuItem{"&Cascade", kCmdCascade, nullptr, false});
    menus.push_back(std::move(window_menu));

    auto menu = std::make_unique<widgets::MenuBar>(std::move(menus), app_.theme(), roles_.menu_bar_normal,
                                                     roles_.menu_bar_active, roles_.menu_dropdown_normal,
                                                     roles_.menu_dropdown_highlighted,
                                                     roles_.menu_dropdown_disabled, app_, *desktop_);
    menu_ = static_cast<widgets::MenuBar*>(desktop_->dock_top(std::move(menu)));
}

void GalleryApp::build_controls_window() {
    // Dialog-gray chrome: the Controls window is a form (a dialog),
    // and dialogs are the gray family — the Sixel Demo window below
    // keeps the blue document-window chrome so both looks appear.
    auto window = std::make_unique<widgets::Window>("Controls", app_.theme(), roles_.dialog_frame,
                                                      roles_.dialog_background, roles_.dialog_frame,
                                                      roles_.dialog_background);
    window->set_bounds(Rect{2, 2, 40, 10});

    auto content = std::make_unique<ui::View>();
    auto label = std::make_unique<widgets::Label>("&Name:", app_.theme(), roles_.label_text, roles_.label_mnemonic);
    label->set_bounds(Rect{1, 1, 8, 1});
    content->add_child(std::move(label));

    auto input = std::make_unique<widgets::InputLine>(app_.theme(), roles_.input_normal, roles_.input_focused,
                                                        roles_.input_invalid);
    input->set_bounds(Rect{10, 1, 20, 1});
    name_input_ = input.get();
    content->add_child(std::move(input));

    auto greet = std::make_unique<widgets::Button>("&Greet", app_.theme(), roles_.button_normal,
                                                     roles_.button_focused, roles_.button_default, roles_.button_shadow);
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
        auto handle = widgets::make_message_box(descriptor, app_ptr->theme(), roles_, *app_ptr, app_ptr->focused(),
                                                  nullptr);
        desktop->show_dialog(std::move(handle), *app_ptr);
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
    auto window = std::make_unique<widgets::Window>("Sixel Demo", app_.theme(), roles_.window_frame_active,
                                                      roles_.window_frame_inactive, roles_.window_title_active,
                                                      roles_.window_title_inactive);
    window->set_bounds(Rect{20, 6, 34, 16});

    auto view = std::make_unique<widgets::ImageView>(app_.theme(), roles_.dialog_background);
    view->set_bounds(Rect{0, 0, 32, 14});
    view->set_image(make_demo_gradient());
    window->set_content(std::move(view));

    image_window_ = desktop_->add_window(std::move(window));
}

}  // namespace ckv::gallery
