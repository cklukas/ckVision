// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// M9's per-window theme override, as a script under every built-in scheme:
// two overlapping windows on one Desktop, the front one overriding the
// application scheme with the next scheme in classic, dark, light, mono
// order. Each frame is pinned byte for byte, and the override is checked to
// draw exactly what that scheme draws when it is the application's own.
//
// To update the fixtures after a deliberate rendering change, rebuild and run
//   build/tools/docgen/generate_theme_override_goldens tests/golden
// and review the diff like any other source change.
#include <array>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
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

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// The generator's script, step for step.
struct OverrideScript {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::widgets::Window* document = nullptr;
    ckv::widgets::Window* console = nullptr;

    explicit OverrideScript(std::size_t scheme) {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.set_theme(kSchemes[scheme].make(app.roles(), roles));
        auto desktop_owner = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
        ckv::widgets::Desktop* const desktop = desktop_owner.get();
        app.root().add_child(std::move(desktop_owner));

        auto document_owner = std::make_unique<ckv::widgets::Window>("Document");
        document_owner->set_bounds(ckv::Rect{2, 1, 44, 14});
        auto body = std::make_unique<ckv::ui::View>();
        auto text = std::make_unique<ckv::widgets::StaticText>("Drawn in the application's scheme.");
        text->set_bounds(ckv::Rect{1, 1, 40, 1});
        body->add_child(std::move(text));
        auto input = std::make_unique<ckv::widgets::InputLine>();
        input->set_bounds(ckv::Rect{1, 3, 30, 1});
        input->set_text("an input line");
        body->add_child(std::move(input));
        document_owner->set_content(std::move(body));
        document = desktop->add_window(std::move(document_owner));

        auto console_owner = std::make_unique<ckv::widgets::Window>("Console");
        console_owner->set_bounds(ckv::Rect{30, 8, 46, 13});
        console_owner->set_theme_override(kSchemes[(scheme + 1) % kSchemes.size()].make(app.roles(), roles));
        auto console_body = std::make_unique<ckv::ui::View>();
        auto console_text = std::make_unique<ckv::widgets::StaticText>("Drawn in its own override.");
        console_text->set_bounds(ckv::Rect{1, 1, 40, 1});
        console_body->add_child(std::move(console_text));
        auto console_input = std::make_unique<ckv::widgets::InputLine>();
        console_input->set_bounds(ckv::Rect{1, 3, 30, 1});
        console_input->set_text("its input line");
        console_body->add_child(std::move(console_input));
        console_owner->set_content(std::move(console_body));
        console = desktop->add_window(std::move(console_owner));

        app.step(0);
    }

    std::string frame() const {
        return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    }
    // The style of the first cell of a window's static text line.
    ckv::Style text_style(const ckv::widgets::Window& window) const {
        const ckv::Rect content = window.content_rect();
        const ckv::Rect bounds = window.bounds();
        return terminal.display().frame().at(ckv::Point{bounds.x + content.x + 1, bounds.y + content.y + 1}).style();
    }
};

}  // namespace

CK_TEST(an_overridden_window_matches_its_pinned_golden_under_every_application_scheme) {
    for (std::size_t scheme = 0; scheme < kSchemes.size(); ++scheme) {
        const OverrideScript script(scheme);
        const std::string expected =
            read_file(std::string("golden/theme_override_") + kSchemes[scheme].name + ".dump");
        CK_CHECK(!expected.empty());
        CK_CHECK(script.frame() == expected);
    }
}

CK_TEST(the_override_draws_what_its_scheme_draws_as_the_application_scheme_and_nothing_else_changes) {
    for (std::size_t scheme = 0; scheme < kSchemes.size(); ++scheme) {
        const std::size_t next = (scheme + 1) % kSchemes.size();
        const OverrideScript script(scheme);
        const OverrideScript as_application(next);
        // The document window keeps the application's scheme...
        CK_CHECK(script.text_style(*script.document) != script.text_style(*script.console));
        // ...and the console draws exactly what its override scheme draws for
        // the same text when that scheme is the application's.
        CK_CHECK(script.text_style(*script.console) == as_application.text_style(*as_application.document));
    }
}

CK_TEST(an_override_set_after_the_first_frame_repaints_every_window_inside_its_subtree) {
    // A theme override is a shared visual dependency of everything under
    // it. Set on a desktop that has already drawn its windows, it has to
    // reach the windows' retained surfaces too, as Application::set_theme
    // reaches every surface in the tree -- not only the desktop's own fill.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 12}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 40, 12}));
    auto window = std::make_unique<ckv::widgets::Window>("Notes");
    window->set_bounds(ckv::Rect{2, 2, 20, 6});
    window->set_content(std::make_unique<ckv::widgets::StaticText>("override"));
    desktop->add_window(std::move(window));
    app.step(0);

    const ckv::ui::Theme dark = ckv::ui::make_dark_theme(app.roles(), roles);
    desktop->set_theme_override(dark);
    app.step(0);
    // The window's text, inside its frame, in the dark scheme's text colours.
    const ckv::FrameView frame = app.current_frame();
    const ckv::Cell& text = frame.at(ckv::Point{3, 3});
    CK_CHECK(text.grapheme() == "o");
    CK_CHECK(text.style().fg == dark.resolve(roles.static_text).fg);

    desktop->clear_theme_override();
    app.step(0);
    CK_CHECK(app.current_frame().at(ckv::Point{3, 3}).style().fg == app.theme().resolve(roles.static_text).fg);
}
