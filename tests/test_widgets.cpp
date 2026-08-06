// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/static_text.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Button;
using ckv::widgets::InputLine;
using ckv::widgets::Label;
using ckv::widgets::StaticText;

namespace {

struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

Surface make_surface(int w, int h) { return Surface(ckv::Size{w, h}, ckv::Cell::from_grapheme(" ", ckv::Style{})); }

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}

}  // namespace

// --- Label -------------------------------------------------------------

CK_TEST(label_strips_the_mnemonic_marker_from_its_displayed_text) {
    Fixture f;
    Label label("&Name:", f.theme, f.roles.label_text, f.roles.label_mnemonic);
    CK_CHECK(label.text() == "&Name:");
    CK_CHECK(label.mnemonic() == "N");
}

CK_TEST(label_preferred_width_matches_the_displayed_not_raw_text) {
    Fixture f;
    Label label("&Name:", f.theme, f.roles.label_text, f.roles.label_mnemonic);
    CK_CHECK(label.horizontal_size_hint().preferred == 5);  // "Name:" is 5 columns
}

CK_TEST(label_draws_its_display_text_into_the_surface) {
    Fixture f;
    Surface s = make_surface(20, 3);
    Label label("&Name:", f.theme, f.roles.label_text, f.roles.label_mnemonic);
    label.set_bounds(Rect{2, 1, 10, 1});
    Painter root(s, Rect{0, 0, 20, 3});
    Painter child = root.translated(Point{2, 1}, Rect{0, 0, 10, 1});
    label.draw(child);
    CK_CHECK(row_text(s, 1).substr(2, 5) == "Name:");
}

CK_TEST(label_set_text_updates_mnemonic_and_display_and_invalidates) {
    Fixture f;
    Label label("&Name:", f.theme, f.roles.label_text, f.roles.label_mnemonic);
    int calls = 0;
    label.set_dirty_rect_sink([&](Rect) { ++calls; });
    label.set_text("&Address:");
    CK_CHECK(label.mnemonic() == "A");
    CK_CHECK(calls >= 1);
}

CK_TEST(label_buddy_defaults_to_null_and_round_trips) {
    Fixture f;
    Label label("Plain", f.theme, f.roles.label_text, f.roles.label_mnemonic);
    CK_CHECK(label.buddy() == nullptr);
    ckv::ui::View buddy_view;
    label.set_buddy(&buddy_view);
    CK_CHECK(label.buddy() == &buddy_view);
}

// --- Button --------------------------------------------------------------

CK_TEST(button_fires_on_press_on_enter_key) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(presses == 1);
}

CK_TEST(button_fires_on_press_on_space_char) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " "}}));
    CK_CHECK(presses == 1);
}

CK_TEST(button_ignores_unrelated_keys) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(!button.on_key(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(presses == 0);
}

CK_TEST(button_press_fires_only_when_mouse_up_lands_inside_its_bounds) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    button.set_bounds(Rect{5, 5, 10, 1});
    int presses = 0;
    button.on_press = [&] { ++presses; };

    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5},
                                     std::nullopt, Modifier::None});
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{6, 5},
                                     std::nullopt, Modifier::None});
    CK_CHECK(presses == 1);
}

CK_TEST(button_press_does_not_fire_when_mouse_up_lands_outside_its_bounds_a_dragged_out_release) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    button.set_bounds(Rect{5, 5, 10, 1});
    int presses = 0;
    button.on_press = [&] { ++presses; };

    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5},
                                     std::nullopt, Modifier::None});
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{100, 100},
                                     std::nullopt, Modifier::None});
    CK_CHECK(presses == 0);
}

CK_TEST(button_with_no_on_press_handler_does_not_crash_when_pressed) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
}

CK_TEST(button_is_a_tab_stop_by_construction) {
    Fixture f;
    Button button("OK", f.theme, f.roles.button_normal, f.roles.button_focused, f.roles.button_default);
    CK_CHECK(button.focusable());
}

// --- StaticText ------------------------------------------------------------

CK_TEST(static_text_with_ample_width_stays_on_one_line) {
    Fixture f;
    StaticText text("Hello world", f.theme, f.roles.static_text);
    CK_CHECK(text.height_for_width(80) == 1);
}

CK_TEST(static_text_wraps_at_word_boundaries_when_width_is_tight) {
    Fixture f;
    StaticText text("one two three", f.theme, f.roles.static_text);
    CK_CHECK(text.height_for_width(7) == 2);  // "one two" / "three"
}

CK_TEST(static_text_hard_breaks_a_single_word_wider_than_the_available_width) {
    Fixture f;
    StaticText text("supercalifragilistic", f.theme, f.roles.static_text);
    const int height = text.height_for_width(5);
    CK_CHECK(height == 4);  // 20 columns / 5 columns per line
}

CK_TEST(static_text_explicit_newlines_force_paragraph_breaks) {
    Fixture f;
    StaticText text("first\nsecond\nthird", f.theme, f.roles.static_text);
    CK_CHECK(text.height_for_width(80) == 3);
}

CK_TEST(static_text_empty_string_reports_one_line) {
    Fixture f;
    StaticText text("", f.theme, f.roles.static_text);
    CK_CHECK(text.height_for_width(80) == 1);
}

CK_TEST(static_text_zero_width_does_not_crash) {
    Fixture f;
    StaticText text("hello", f.theme, f.roles.static_text);
    CK_CHECK(text.height_for_width(0) >= 1);
}

CK_TEST(static_text_draws_wrapped_lines_into_the_surface) {
    Fixture f;
    Surface s = make_surface(10, 4);
    StaticText text("one two three", f.theme, f.roles.static_text);
    text.set_bounds(Rect{0, 0, 7, 4});
    Painter root(s, Rect{0, 0, 10, 4});
    text.draw(root);
    CK_CHECK(row_text(s, 0).substr(0, 7) == "one two");
    CK_CHECK(row_text(s, 1).substr(0, 5) == "three");
}

// --- InputLine ---------------------------------------------------------

CK_TEST(input_line_starts_empty_with_cursor_at_zero) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    CK_CHECK(input.text().empty());
    CK_CHECK(input.cursor() == 0);
}

CK_TEST(set_text_places_the_cursor_at_the_end) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("hello");
    CK_CHECK(input.text() == "hello");
    CK_CHECK(input.cursor() == 5);
}

CK_TEST(typed_text_inserts_at_the_cursor) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("helloworld");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    for (int i = 0; i < 5; ++i) input.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    input.on_text(ckv::TextEvent{" ", false});
    CK_CHECK(input.text() == "hello world");
}

CK_TEST(backspace_at_start_of_field_is_a_no_op) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(input.text() == "abc");
    CK_CHECK(input.cursor() == 0);
}

CK_TEST(delete_at_end_of_field_is_a_no_op) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Delete, Modifier::None, ""}});
    CK_CHECK(input.text() == "abc");
}

CK_TEST(backspace_removes_the_grapheme_before_the_cursor) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(input.text() == "ab");
    CK_CHECK(input.cursor() == 2);
}

CK_TEST(shift_left_extends_a_selection_and_plain_left_collapses_it) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("hello");  // cursor at 5
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    CK_CHECK(input.has_selection());
    auto [b, e] = input.selection_range();
    CK_CHECK(b == 3 && e == 5);

    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});
    CK_CHECK(!input.has_selection());
}

CK_TEST(typing_over_a_selection_replaces_it) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("hello");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});  // selects "lo"
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "helX");
}

CK_TEST(overwrite_mode_replaces_the_grapheme_under_the_cursor_instead_of_inserting) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}});
    CK_CHECK(input.overwrite_mode());
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "Xbc");
}

CK_TEST(overwrite_mode_past_the_end_of_text_appends_instead_of_indexing_out_of_range) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("ab");  // cursor already at the end
    input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}});
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "abX");
}

CK_TEST(grapheme_correct_editing_never_splits_a_multi_byte_cluster) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_text("a\xC3\xA9z");  // "aéz"
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});   // cursor between é and z
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});  // removes the whole é, not a byte of it
    CK_CHECK(input.text() == "az");
}

CK_TEST(set_valid_false_is_observable_and_toggling_back_is_observable_too) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    CK_CHECK(input.valid());
    input.set_valid(false);
    CK_CHECK(!input.valid());
    input.set_valid(true);
    CK_CHECK(input.valid());
}

CK_TEST(mouse_click_places_the_cursor_at_the_clicked_grapheme) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_bounds(Rect{0, 0, 20, 1});
    input.set_text("abcdef");
    input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                    std::nullopt, Modifier::None});
    CK_CHECK(input.cursor() == 3);
}

CK_TEST(click_outside_the_widgets_bounds_is_ignored) {
    Fixture f;
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_bounds(Rect{0, 0, 5, 1});
    input.set_text("abc");
    CK_CHECK(!input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{50, 50}, std::nullopt, Modifier::None}));
    CK_CHECK(input.cursor() == 3);  // unchanged
}

CK_TEST(draw_does_not_crash_when_the_field_is_empty_and_focused) {
    Fixture f;
    Surface s = make_surface(20, 3);
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_bounds(Rect{0, 0, 10, 1});
    input.on_focus(ckv::FocusEvent{true});
    Painter root(s, Rect{0, 0, 20, 3});
    input.draw(root);
    CK_CHECK(true);
}

CK_TEST(long_text_scrolls_so_the_cursor_stays_visible) {
    Fixture f;
    Surface s = make_surface(20, 3);
    InputLine input(f.theme, f.roles.input_normal, f.roles.input_focused, f.roles.input_invalid);
    input.set_bounds(Rect{0, 0, 5, 1});
    input.set_text("abcdefghij");  // 10 chars, only 5 columns visible, cursor at end (10)
    Painter root(s, Rect{0, 0, 20, 3});
    input.draw(root);
    // The tail of the text (near the cursor) must be visible, not the head.
    CK_CHECK(row_text(s, 0).find('j') != std::string::npos);
}
