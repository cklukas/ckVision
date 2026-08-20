// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-36 acceptance closes the gap between isolated container math and visible
// application behavior: wrapping height participates in hosted dialog content,
// every standard dialog factory inherits the fixed-size dialog default, accept
// validation/Esc are exercised on a real Window hook, and resize storms are
// driven through Application frames rather than direct layout calls only.
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/filesystem.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"
#include "cvision/widgets/window_list_dialog.hpp"

using ckv::ManualClock;
using ckv::MemoryFileSystem;
using ckv::Rect;
using ckv::Size;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::StandardRoles;
using ckv::widgets::ButtonDescriptor;
using ckv::widgets::ButtonRole;
using ckv::widgets::Desktop;
using ckv::widgets::DialogDescriptor;
using ckv::widgets::FieldDescriptor;
using ckv::widgets::FileDialogMode;
using ckv::widgets::HelpTopic;
using ckv::widgets::MemoryHelpProvider;
using ckv::widgets::MessageBoxButtons;
using ckv::widgets::MessageBoxDescriptor;
using ckv::widgets::MessageBoxKind;
using ckv::widgets::Window;

namespace {

struct AppFixture {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    StandardRoles roles = intern_standard_roles(app.roles());

    AppFixture() { app.theme() = make_classic_theme(app.roles(), roles); }
};

MemoryFileSystem dialog_filesystem() {
    MemoryFileSystem fs;
    fs.add_directory("/root");
    fs.add_directory("/root/docs");
    fs.add_file("/root/docs/readme.txt");
    return fs;
}

MemoryHelpProvider help_provider() {
    MemoryHelpProvider provider;
    provider.add_topic("intro", HelpTopic{"Intro", "A short help topic.", {}});
    return provider;
}

std::string capture_frame(const Application& app) {
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

std::vector<std::string> run_shrink_storm_script() {
    AppFixture f;
    auto desktop = std::make_unique<Desktop>(f.app.root().bounds());
    Desktop* desktop_ptr = desktop.get();
    f.app.root().add_child(std::move(desktop));

    auto window = std::make_unique<Window>("Shrink");
    window->set_bounds(desktop_ptr->content_area());
    window->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);
    auto input = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* input_ptr = input.get();
    window->set_content(std::move(input));
    Window* window_ptr = desktop_ptr->add_window(std::move(window));
    f.app.set_focus(input_ptr);

    std::vector<std::string> frames;
    for (const Size size : {Size{80, 24}, Size{30, 8}, Size{8, 4}, Size{80, 24}}) {
        f.terminal.resize(size);
        f.app.step(0);
        CK_CHECK(f.app.current_frame().size() == size);
        CK_CHECK(f.terminal.display().size() == size);
        CK_CHECK(f.app.focused() == input_ptr);
        CK_CHECK(desktop_ptr->active_window() == window_ptr);
        CK_CHECK(window_ptr->bounds().x >= 0);
        CK_CHECK(window_ptr->bounds().y >= 0);
        CK_CHECK(window_ptr->content_rect().width >= 0);
        CK_CHECK(window_ptr->content_rect().height >= 0);
        frames.push_back(capture_frame(f.app));
    }
    return frames;
}

}  // namespace

CK_TEST(wrapped_static_text_reallocates_height_through_a_hosted_dialog_content_tree) {
    AppFixture f;
    auto desktop = std::make_unique<Desktop>(f.app.root().bounds());
    Desktop* desktop_ptr = desktop.get();
    f.app.root().add_child(std::move(desktop));

    auto window = std::make_unique<Window>("Wrapped");
    window->set_bounds(Rect{2, 2, 7, 8});
    window->set_resizable(true);  // this acceptance explicitly drives a resized dialog.

    auto column = std::make_unique<ckv::ui::Column>();
    auto text = std::make_unique<ckv::widgets::StaticText>("one two three four five");
    ckv::widgets::StaticText* text_ptr = text.get();
    column->add_item(std::move(text), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    window->set_content(std::move(column));

    Window* window_ptr = desktop_ptr->add_window(std::move(window));
    f.app.step(0);
    CK_CHECK(text_ptr->bounds() == (Rect{0, 0, 5, 5}));
    CK_CHECK(f.app.current_frame().at(ckv::Point{3, 3}).grapheme() == "o");

    window_ptr->set_bounds(Rect{2, 2, 20, 8});
    f.app.step(0);

    CK_CHECK(text_ptr->bounds() == (Rect{0, 0, 18, 2}));
    CK_CHECK(f.app.current_frame().at(ckv::Point{3, 3}).grapheme() == "o");
}

CK_TEST(alert_shaped_dialog_factories_build_non_resizable_windows_by_default) {
    AppFixture f;
    MemoryFileSystem fs = dialog_filesystem();
    MemoryHelpProvider help = help_provider();
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(std::make_unique<Window>("Document"));

    const MessageBoxDescriptor message{MessageBoxKind::Info, "Info", "Saved.", MessageBoxButtons::Ok};
    auto message_box = ckv::widgets::make_message_box(message, f.roles, f.app, nullptr, nullptr);
    auto file_dialog = ckv::widgets::make_file_dialog(FileDialogMode::Open, "/root", fs, f.roles, f.app,
                                                       nullptr, nullptr);
    auto directory_picker =
        ckv::widgets::make_directory_picker(fs, "/root", f.roles, f.app, nullptr, nullptr);
    auto help_viewer = ckv::widgets::make_help_viewer(help, "intro", f.roles, f.app, nullptr);
    auto window_list = ckv::widgets::make_window_list_dialog(desktop, f.roles, f.app, nullptr);

    // A dialog that asks one question is sized by that question: there is
    // nothing a reader could usefully do with a bigger one.
    CK_CHECK(!message_box.window->resizable());
    CK_CHECK(!file_dialog.window->resizable());
    CK_CHECK(!directory_picker.window->resizable());
    CK_CHECK(!window_list.window->resizable());
    // The help viewer is the exception, and deliberately: it shows a document
    // beside an index of documents, and how much of either to have on screen
    // is the reader's call, not the factory's.
    CK_CHECK(help_viewer.window->resizable());
}

CK_TEST(descriptor_dialog_validation_veto_and_escape_cancel_run_on_window_hooks) {
    AppFixture f;
    auto* invoker = f.app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    f.app.set_focus(invoker);

    DialogDescriptor descriptor;
    descriptor.title = "Validate";
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "", [](const std::string& value) {
                                                    return value == "valid";
                                                }});
    bool ok_pressed = false;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { ok_pressed = true; }});

    auto dialog = ckv::widgets::materialize_dialog(descriptor);
    ckv::widgets::InputLine* invalid_input = dialog.inputs[0];
    Window window("Validate");
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    ckv::widgets::wire_dialog_window(window, std::move(dialog), descriptor, f.app, invoker);

    window.accept_request();
    CK_CHECK(!ok_pressed);
    CK_CHECK(!closed);
    CK_CHECK(!invalid_input->valid());
    CK_CHECK(f.app.focused() == invalid_input);

    window.cancel_request();
    CK_CHECK(closed);
    CK_CHECK(f.app.focused() == invoker);
}

CK_TEST(shrink_storm_frames_are_deterministic_and_recover_focus_chrome_and_damage) {
    const std::vector<std::string> first = run_shrink_storm_script();
    const std::vector<std::string> second = run_shrink_storm_script();

    CK_CHECK(first.size() == 4);
    CK_CHECK(first == second);
    CK_CHECK(first.front() == first.back());
}
