// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/key.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::format;
using ckv::Key;
using ckv::key_from_name;
using ckv::key_name;
using ckv::KeyChord;
using ckv::Modifier;

// --- key_name / key_from_name --------------------------------------------

CK_TEST(key_name_round_trips_through_key_from_name_for_every_named_key) {
    const Key keys[] = {Key::Enter,    Key::Escape,   Key::Tab,  Key::Backspace, Key::Delete,
                         Key::Insert,  Key::Up,       Key::Down, Key::Left,      Key::Right,
                         Key::Home,    Key::End,      Key::PageUp, Key::PageDown,
                         Key::F1,      Key::F2,       Key::F3,   Key::F4,        Key::F5,
                         Key::F6,      Key::F7,       Key::F8,   Key::F9,        Key::F10,
                         Key::F11,     Key::F12};
    for (Key k : keys) {
        const auto looked_up = key_from_name(key_name(k));
        CK_CHECK(looked_up.has_value());
        CK_CHECK(*looked_up == k);
    }
}

CK_TEST(escape_is_named_esc_not_escape) {
    // Every existing comment/test name in this codebase already
    // spells it "Esc" — the canonical spelling matches that, not the
    // enumerator's own name.
    CK_CHECK(key_name(Key::Escape) == "Esc");
}

CK_TEST(key_from_name_is_case_insensitive) {
    CK_CHECK(key_from_name("f10") == Key::F10);
    CK_CHECK(key_from_name("F10") == Key::F10);
    CK_CHECK(key_from_name("ESC") == Key::Escape);
    CK_CHECK(key_from_name("enter") == Key::Enter);
}

CK_TEST(key_from_name_returns_nullopt_for_an_unknown_name) {
    CK_CHECK(!key_from_name("Nonsense").has_value());
    CK_CHECK(!key_from_name("").has_value());
}

CK_TEST(key_name_on_char_aborts) {
    CK_EXPECT_ABORT({
        key_name(Key::Char);  // must abort: Key::Char has no fixed name
    });
}

CK_TEST(key_name_on_none_aborts) {
    CK_EXPECT_ABORT({
        key_name(Key::None);  // must abort: not a real key
    });
}

// --- KeyChord::parse -------------------------------------------------------

CK_TEST(parse_a_bare_named_key) {
    const auto chord = KeyChord::parse("F10");
    CK_CHECK(chord.has_value());
    CK_CHECK(chord->key == Key::F10);
    CK_CHECK(chord->modifiers == Modifier::None);
}

CK_TEST(parse_is_case_insensitive_for_both_modifier_and_key_names) {
    const auto chord = KeyChord::parse("alt+esc");
    CK_CHECK(chord.has_value());
    CK_CHECK(chord->key == Key::Escape);
    CK_CHECK(chord->modifiers == Modifier::Alt);
}

CK_TEST(parse_a_single_modifier_plus_a_printable_character) {
    const auto chord = KeyChord::parse("Alt+G");
    CK_CHECK(chord.has_value());
    CK_CHECK(chord->key == Key::Char);
    CK_CHECK(chord->modifiers == Modifier::Alt);
    CK_CHECK(chord->text == "g");  // lowercased, matching the library-wide convention
}

CK_TEST(parse_stacks_multiple_modifiers_in_any_order) {
    const auto chord = KeyChord::parse("Shift+Ctrl+F10");
    CK_CHECK(chord.has_value());
    CK_CHECK(chord->key == Key::F10);
    CK_CHECK(ckv::has_modifier(chord->modifiers, Modifier::Shift));
    CK_CHECK(ckv::has_modifier(chord->modifiers, Modifier::Ctrl));
    CK_CHECK(!ckv::has_modifier(chord->modifiers, Modifier::Alt));
}

CK_TEST(parse_rejects_an_empty_string) {
    CK_CHECK(!KeyChord::parse("").has_value());
}

CK_TEST(parse_rejects_an_unknown_modifier_token) {
    CK_CHECK(!KeyChord::parse("Meta+G").has_value());
}

CK_TEST(parse_rejects_a_trailing_multi_character_token_that_is_not_a_known_key_name) {
    CK_CHECK(!KeyChord::parse("Alt+Nonsense").has_value());
}

CK_TEST(parse_rejects_a_bare_modifier_with_nothing_after_it) {
    CK_CHECK(!KeyChord::parse("Alt+").has_value());
}

// --- format ----------------------------------------------------------------

CK_TEST(format_a_named_key_with_no_modifiers) {
    CK_CHECK(format(KeyChord{Key::F10, Modifier::None, ""}) == "F10");
}

CK_TEST(format_renders_modifiers_in_a_fixed_ctrl_alt_shift_super_order_regardless_of_bit_order) {
    CK_CHECK(format(KeyChord{Key::F10, Modifier::Shift | Modifier::Ctrl, ""}) == "Ctrl+Shift+F10");
}

CK_TEST(format_upper_cases_a_single_letter_char_chord_for_display) {
    CK_CHECK(format(KeyChord{Key::Char, Modifier::Alt, "g"}) == "Alt+G");
}

CK_TEST(format_and_parse_round_trip_for_every_modifier_combination) {
    const Modifier combos[] = {
        Modifier::None,  Modifier::Ctrl, Modifier::Alt, Modifier::Shift, Modifier::Super,
        Modifier::Ctrl | Modifier::Alt, Modifier::Ctrl | Modifier::Alt | Modifier::Shift,
    };
    for (Modifier m : combos) {
        const KeyChord original{Key::F5, m, ""};
        const std::string spelling = format(original);
        const auto parsed = KeyChord::parse(spelling);
        CK_CHECK(parsed.has_value());
        CK_CHECK(*parsed == original);
    }
}

CK_TEST(format_and_parse_round_trip_for_a_char_chord) {
    const KeyChord original{Key::Char, Modifier::Ctrl, "x"};
    const std::string spelling = format(original);
    CK_CHECK(spelling == "Ctrl+X");
    const auto parsed = KeyChord::parse(spelling);
    CK_CHECK(parsed.has_value());
    CK_CHECK(*parsed == original);
}

CK_TEST(format_on_key_none_aborts) {
    CK_EXPECT_ABORT({
        format(KeyChord{Key::None, Modifier::None, ""});  // must abort
    });
}
