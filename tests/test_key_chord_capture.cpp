// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/key_chord_capture.hpp"

#include <string>
#include <utility>

#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

namespace {

ckv::KeyEvent press(ckv::Key key, ckv::Modifier modifiers = ckv::Modifier::None, std::string text = {}) {
    return ckv::KeyEvent{ckv::KeyChord{key, modifiers, std::move(text)}};
}

struct Fixture {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::ui::Context context() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

}  // namespace

CK_TEST(key_chord_capture_consumes_the_captured_command_chord) {
    ckv::widgets::KeyChordCapture capture;
    std::optional<ckv::KeyChord> observed;
    capture.on_chord_changed = [&observed](const std::optional<ckv::KeyChord>& chord) { observed = chord; };

    CK_CHECK(capture.on_key(press(ckv::Key::Enter)));
    CK_CHECK(capture.capturing());
    CK_CHECK(capture.on_key(press(ckv::Key::Char, ckv::Modifier::Ctrl, "s")));
    CK_CHECK(!capture.capturing());
    CK_CHECK(observed.has_value());
    if (observed) {
        const ckv::KeyChord expected{ckv::Key::Char, ckv::Modifier::Ctrl, "s"};
        CK_CHECK(*observed == expected);
    }
}

CK_TEST(key_chord_capture_esc_cancels_capture_without_changing_the_binding) {
    ckv::widgets::KeyChordCapture capture;
    capture.set_chord(ckv::KeyChord{ckv::Key::F4, ckv::Modifier::Alt, {}});
    int changes = 0;
    capture.on_chord_changed = [&changes](const std::optional<ckv::KeyChord>&) { ++changes; };

    capture.begin_capture();
    CK_CHECK(capture.on_key(press(ckv::Key::Escape)));
    CK_CHECK(!capture.capturing());
    CK_CHECK(capture.chord().has_value());
    if (capture.chord()) {
        const ckv::KeyChord expected{ckv::Key::F4, ckv::Modifier::Alt, {}};
        CK_CHECK(*capture.chord() == expected);
    }
    CK_CHECK(changes == 0);
}

CK_TEST(key_chord_capture_clear_is_explicit_and_typed) {
    ckv::widgets::KeyChordCapture capture;
    capture.set_chord(ckv::KeyChord{ckv::Key::F1, ckv::Modifier::None, {}});
    bool saw_clear = false;
    capture.on_chord_changed = [&saw_clear](const std::optional<ckv::KeyChord>& chord) { saw_clear = !chord.has_value(); };

    CK_CHECK(capture.on_key(press(ckv::Key::Backspace)));
    CK_CHECK(!capture.chord().has_value());
    CK_CHECK(saw_clear);
}

CK_TEST(key_chord_capture_has_a_deterministic_unbound_golden) {
    Fixture fixture;
    ckv::widgets::KeyChordCapture control;
    control.set_context(fixture.context());
    control.on_attached();
    control.set_bounds(ckv::Rect{0, 0, 18, 1});

    ckv::scene::Surface surface(ckv::Size{18, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 18, 1});
    control.draw(painter);

    const std::string expected =
        "ckvision-golden 1\n"
        "frame 18 1\n"
        "cursor hidden\n"
        "styles 1\n"
        "0 fg #FFFFFF bg #0000AA attrs -\n"
        "grid\n"
        "|Unbound           |\n"
        "stylemap\n"
        "|000000000000000000|\n"
        "end\n";
    CK_CHECK(ckv::golden::serialize(ckv::scene::capture(surface)) == expected);
}
