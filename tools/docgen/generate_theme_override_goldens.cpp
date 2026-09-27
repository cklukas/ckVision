// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Fixture generator for the per-window theme override script (M9): two
// overlapping windows on one Desktop, the front one carrying
// View::set_theme_override, under each of the four built-in application
// schemes. The override is always a scheme other than the application's --
// the next one in classic, dark, light, mono order -- so every frame shows
// the two side by side. tests/test_theme_override_golden.cpp replays the
// same script and compares against these bytes; generated_golden_bytes
// regenerates them on every host.
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace {

using SchemeFactory = ckv::ui::Theme (*)(const ckv::ui::RoleRegistry&, const ckv::ui::StandardRoles&);

struct Scheme {
    const char* name;
    SchemeFactory make;
};

constexpr std::array<Scheme, 4> kSchemes{{
    {"classic", ckv::ui::make_classic_theme},
    {"dark", ckv::ui::make_dark_theme},
    {"light", ckv::ui::make_light_theme},
    {"mono", ckv::ui::make_mono_theme},
}};

std::string frame_under(std::size_t scheme) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.set_theme(kSchemes[scheme].make(app.roles(), roles));
    auto desktop_owner = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* const desktop = desktop_owner.get();
    app.root().add_child(std::move(desktop_owner));

    auto document = std::make_unique<ckv::widgets::Window>("Document");
    document->set_bounds(ckv::Rect{2, 1, 44, 14});
    auto body = std::make_unique<ckv::ui::View>();
    auto text = std::make_unique<ckv::widgets::StaticText>("Drawn in the application's scheme.");
    text->set_bounds(ckv::Rect{1, 1, 40, 1});
    body->add_child(std::move(text));
    auto input = std::make_unique<ckv::widgets::InputLine>();
    input->set_bounds(ckv::Rect{1, 3, 30, 1});
    input->set_text("an input line");
    body->add_child(std::move(input));
    document->set_content(std::move(body));
    desktop->add_window(std::move(document));

    auto console = std::make_unique<ckv::widgets::Window>("Console");
    console->set_bounds(ckv::Rect{30, 8, 46, 13});
    console->set_theme_override(kSchemes[(scheme + 1) % kSchemes.size()].make(app.roles(), roles));
    auto console_body = std::make_unique<ckv::ui::View>();
    auto console_text = std::make_unique<ckv::widgets::StaticText>("Drawn in its own override.");
    console_text->set_bounds(ckv::Rect{1, 1, 40, 1});
    console_body->add_child(std::move(console_text));
    auto console_input = std::make_unique<ckv::widgets::InputLine>();
    console_input->set_bounds(ckv::Rect{1, 3, 30, 1});
    console_input->set_text("its input line");
    console_body->add_child(std::move(console_input));
    console->set_content(std::move(console_body));
    desktop->add_window(std::move(console));

    app.step(0);
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    for (std::size_t scheme = 0; scheme < kSchemes.size(); ++scheme) {
        const std::filesystem::path path =
            directory / (std::string("theme_override_") + kSchemes[scheme].name + ".dump");
        std::ofstream output(path, std::ios::binary);
        output << frame_under(scheme);
        std::fprintf(stderr, "wrote %s\n", path.string().c_str());
    }
    return 0;
}
