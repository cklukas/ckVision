// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::HistoryRegistry;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
namespace ui = ckv::ui;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Button;
using ckv::widgets::InputLine;
using ckv::widgets::Label;
using ckv::widgets::StaticText;
using ckv::widgets::Window;

namespace {

struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ui::Context ctx() { return ui::Context{&theme, &registry, nullptr}; }
};

Surface make_surface(int w, int h) { return Surface(ckv::Size{w, h}, ckv::Cell::from_grapheme(" ", ckv::Style{})); }

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}

ckv::KeyEvent ctrl_char(std::string text) {
    return ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, std::move(text)}};
}

// Size-hint-change propagation (M9/WP-16): a bare View parent that
// just counts how many times its own on_child_size_hint_changed()
// fires, standing in for a real container (Row/Column/Window/Desktop),
// all of which already have their own dedicated coverage.
class SpyParent : public ui::View {
public:
    int notifications = 0;
    void on_child_size_hint_changed(ui::View&) override { ++notifications; }
};

}  // namespace

// --- Label -------------------------------------------------------------

CK_TEST(label_strips_the_mnemonic_marker_from_its_displayed_text) {
    Label label("&Name:");
    CK_CHECK(label.text() == "&Name:");
    CK_CHECK(label.mnemonic() == "N");
}

CK_TEST(label_preferred_width_matches_the_displayed_not_raw_text) {
    Label label("&Name:");
    CK_CHECK(label.horizontal_size_hint().preferred == 5);  // "Name:" is 5 columns
}

CK_TEST(label_draws_its_display_text_into_the_surface) {
    Fixture f;
    Surface s = make_surface(20, 3);
    Label label("&Name:");
    label.set_context(f.ctx());
    label.set_bounds(Rect{2, 1, 10, 1});
    Painter root(s, Rect{0, 0, 20, 3});
    Painter child = root.translated(Point{2, 1}, Rect{0, 0, 10, 1});
    label.draw(child);
    CK_CHECK(row_text(s, 1).substr(2, 5) == "Name:");
}

CK_TEST(label_set_text_updates_mnemonic_and_display_and_invalidates) {
    Label label("&Name:");
    int calls = 0;
    label.set_dirty_rect_sink([&](Rect) { ++calls; });
    label.set_text("&Address:");
    CK_CHECK(label.mnemonic() == "A");
    CK_CHECK(calls >= 1);
}

CK_TEST(label_buddy_defaults_to_null_and_round_trips) {
    Label label("Plain");
    CK_CHECK(label.buddy() == nullptr);
    ckv::ui::View buddy_view;
    label.set_buddy(&buddy_view);
    CK_CHECK(label.buddy() == &buddy_view);
}

CK_TEST(label_buddy_returns_null_after_the_buddy_is_destroyed) {
    Label label("Plain");
    {
        auto buddy = std::make_unique<ckv::ui::View>();
        label.set_buddy(buddy.get());
        CK_CHECK(label.buddy() == buddy.get());
    }
    CK_CHECK(label.buddy() == nullptr);
}

CK_TEST(alt_mnemonic_on_a_window_focuses_the_labels_buddy) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);

    auto* window = app.root().add(std::make_unique<Window>("Form"));
    window->set_bounds(Rect{0, 0, 40, 10});
    auto& pane = window->content_pane();
    auto* label = pane.add(std::make_unique<Label>("&Name:"));
    auto* input = pane.add(std::make_unique<InputLine>());
    auto* other = pane.add(std::make_unique<InputLine>());
    label->set_buddy(input);
    app.set_focus(other);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "n"}}));
    CK_CHECK(app.focused() == input);
}

// --- Button --------------------------------------------------------------

CK_TEST(button_fires_on_press_on_enter_key) {
    Fixture f;
    Button button("OK");
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(presses == 1);
}

CK_TEST(button_fires_on_press_on_enter_key_repeat_but_not_release) {
    Button button("OK");
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""},
                                         ckv::KeyAction::Repeat}));
    CK_CHECK(!button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""},
                                          ckv::KeyAction::Release}));
    CK_CHECK(presses == 1);
}

CK_TEST(button_fires_on_press_on_space_char) {
    Fixture f;
    Button button("OK");
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " "}}));
    CK_CHECK(presses == 1);
}

CK_TEST(button_ignores_unrelated_keys) {
    Fixture f;
    Button button("OK");
    int presses = 0;
    button.on_press = [&] { ++presses; };
    CK_CHECK(!button.on_key(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(presses == 0);
}

CK_TEST(button_press_fires_only_when_mouse_up_lands_inside_its_bounds) {
    Fixture f;
    Button button("OK");
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
    Button button("OK");
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
    Button button("OK");
    CK_CHECK(button.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
}

CK_TEST(button_escape_cancels_a_key_press_in_flight_and_is_consumed) {
    Fixture f;
    Button button("OK");
    int presses = 0;
    button.on_press = [&] { ++presses; };
    button.on_focus(ckv::FocusEvent{true});

    const KeyChord space{Key::Char, Modifier::None, " "};
    const KeyChord escape{Key::Escape, Modifier::None, ""};
    CK_CHECK(button.on_key(ckv::KeyEvent{space, ckv::KeyAction::Press, true}));
    CK_CHECK(button.pressed());

    // Escape takes the press back — and is consumed by that cancellation,
    // so it cannot also close the dialog underneath.
    CK_CHECK(button.on_key(ckv::KeyEvent{escape, ckv::KeyAction::Press, true}));
    CK_CHECK(!button.pressed());
    CK_CHECK(!button.on_key_release(ckv::KeyEvent{space, ckv::KeyAction::Release, true}));
    CK_CHECK(presses == 0);

    // With no press in flight, Escape is not the button's to consume.
    CK_CHECK(!button.on_key(ckv::KeyEvent{escape, ckv::KeyAction::Press, true}));
}

CK_TEST(button_is_a_tab_stop_by_construction) {
    Fixture f;
    Button button("OK");
    CK_CHECK(button.focusable());
}

// --- Button rendering (classic face + composited drop shadow) -------------

CK_TEST(button_draws_a_face_with_half_block_shadow_on_the_right_edge_and_bottom_row) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);

    CK_CHECK(s.at(ckv::Point{11, 0}).grapheme() == "▄");  // right edge, first face row
    for (int x = 2; x <= 11; ++x) CK_CHECK(s.at(ckv::Point{x, 1}).grapheme() == "▀");  // bottom shadow run
    CK_CHECK(s.at(ckv::Point{0, 1}).grapheme() == " ");  // bottom row spacer cells
    CK_CHECK(s.at(ckv::Point{1, 1}).grapheme() == " ");

    // Label centered on the face: "OK" in a 12-wide button -> cols 5-6.
    CK_CHECK(s.at(ckv::Point{5, 0}).grapheme() == "O");
    CK_CHECK(s.at(ckv::Point{6, 0}).grapheme() == "K");

    // Face cells carry the button style; shadow glyphs carry the shadow style.
    CK_CHECK(s.at(ckv::Point{5, 0}).style() == f.theme.resolve(f.roles.button_normal));
    CK_CHECK(s.at(ckv::Point{11, 0}).style() == f.theme.resolve(f.roles.button_shadow));
    CK_CHECK(s.at(ckv::Point{5, 1}).style() == f.theme.resolve(f.roles.button_shadow));
}

CK_TEST(button_mnemonics_use_the_dialog_mnemonic_accent) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("&OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);

    // "OK" is centered at columns 5-6; only the '&'-marked O is yellow.
    CK_CHECK(s.at(ckv::Point{5, 0}).style().fg == f.theme.resolve(f.roles.label_mnemonic).fg);
    CK_CHECK(s.at(ckv::Point{5, 0}).style().bg == f.theme.resolve(f.roles.button_normal).bg);
    CK_CHECK(s.at(ckv::Point{6, 0}).style() == f.theme.resolve(f.roles.button_normal));
}

CK_TEST(button_clipping_keeps_combining_graphemes_intact) {
    Fixture f;
    Surface s = make_surface(7, 3);
    Button button("A\x65\xCC\x81\xE4\xB8\xAD");  // A, e + combining acute, then a wide ideograph
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 5, 2});
    Painter painter(s, Rect{0, 0, 7, 3});
    button.draw(painter);

    // Three face columns are available. The final two-column ideograph cannot
    // fit, while the preceding combining sequence remains one complete Cell.
    CK_CHECK(s.at(ckv::Point{1, 0}).grapheme() == "A");
    CK_CHECK(s.at(ckv::Point{2, 0}).grapheme() == "e\xCC\x81");
    CK_CHECK(s.at(ckv::Point{3, 0}).grapheme() == " ");
}

CK_TEST(a_taller_button_uses_full_block_shadow_below_the_first_face_row) {
    Fixture f;
    Surface s = make_surface(14, 4);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 3});  // two face rows + shadow row
    Painter painter(s, Rect{0, 0, 14, 4});
    button.draw(painter);
    CK_CHECK(s.at(ckv::Point{11, 0}).grapheme() == "▄");
    CK_CHECK(s.at(ckv::Point{11, 1}).grapheme() == "█");
}

CK_TEST(a_pressed_button_shifts_its_face_right_and_loses_its_shadow) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 0},
                                     std::nullopt, Modifier::None});  // press and hold
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);

    CK_CHECK(s.at(ckv::Point{11, 0}).grapheme() != "▄");                       // no right-edge shadow
    for (int x = 2; x <= 11; ++x) CK_CHECK(s.at(ckv::Point{x, 1}).grapheme() == " ");  // no bottom shadow
    CK_CHECK(s.at(ckv::Point{6, 0}).grapheme() == "O");  // label shifted one cell right (was col 5)
    CK_CHECK(s.at(ckv::Point{7, 0}).grapheme() == "K");
}

CK_TEST(releasing_a_pressed_button_restores_the_shadow) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 0},
                                     std::nullopt, Modifier::None});
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{5, 0},
                                     std::nullopt, Modifier::None});
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);
    CK_CHECK(s.at(ckv::Point{11, 0}).grapheme() == "▄");
    CK_CHECK(s.at(ckv::Point{5, 1}).grapheme() == "▀");
}

CK_TEST(a_focused_buttons_face_uses_the_focused_style) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    button.on_focus(ckv::FocusEvent{true});
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);
    CK_CHECK(s.at(ckv::Point{5, 0}).style() == f.theme.resolve(f.roles.button_focused));
}

CK_TEST(a_default_buttons_face_uses_the_default_style_when_unfocused) {
    Fixture f;
    Surface s = make_surface(14, 3);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 12, 2});
    button.set_default(true);
    Painter painter(s, Rect{0, 0, 14, 3});
    button.draw(painter);
    CK_CHECK(s.at(ckv::Point{5, 0}).style() == f.theme.resolve(f.roles.button_default));
}

CK_TEST(button_prefers_a_two_row_height_so_layout_reserves_its_shadow_row) {
    Fixture f;
    Button button("OK");
    CK_CHECK(button.vertical_size_hint().preferred == 2);
}

CK_TEST(a_degenerate_tiny_button_draws_without_crashing) {
    Fixture f;
    Surface s = make_surface(4, 2);
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 2, 1});  // below the 3-cell minimum: draw() must bail, not crash
    Painter painter(s, Rect{0, 0, 4, 2});
    button.draw(painter);
    CK_CHECK(true);
}

// --- StaticText ------------------------------------------------------------

CK_TEST(static_text_with_ample_width_stays_on_one_line) {
    StaticText text("Hello world");
    CK_CHECK(text.height_for_width(80) == 1);
}

CK_TEST(static_text_wraps_at_word_boundaries_when_width_is_tight) {
    StaticText text("one two three");
    CK_CHECK(text.height_for_width(7) == 2);  // "one two" / "three"
}

CK_TEST(static_text_hard_breaks_a_single_word_wider_than_the_available_width) {
    StaticText text("supercalifragilistic");
    const int height = text.height_for_width(5);
    CK_CHECK(height == 4);  // 20 columns / 5 columns per line
}

CK_TEST(static_text_explicit_newlines_force_paragraph_breaks) {
    StaticText text("first\nsecond\nthird");
    CK_CHECK(text.height_for_width(80) == 3);
}

CK_TEST(static_text_empty_string_reports_one_line) {
    StaticText text("");
    CK_CHECK(text.height_for_width(80) == 1);
}

CK_TEST(static_text_zero_width_does_not_crash) {
    StaticText text("hello");
    CK_CHECK(text.height_for_width(0) >= 1);
}

CK_TEST(static_text_draws_wrapped_lines_into_the_surface) {
    Fixture f;
    Surface s = make_surface(10, 4);
    StaticText text("one two three");
    text.set_context(f.ctx());
    text.set_bounds(Rect{0, 0, 7, 4});
    Painter root(s, Rect{0, 0, 10, 4});
    text.draw(root);
    CK_CHECK(row_text(s, 0).substr(0, 7) == "one two");
    CK_CHECK(row_text(s, 1).substr(0, 5) == "three");
}

CK_TEST(static_text_center_alignment_offsets_each_wrapped_line) {
    Fixture f;
    Surface s = make_surface(10, 3);
    StaticText text("hi");
    text.set_context(f.ctx());
    text.set_bounds(Rect{0, 0, 10, 1});
    text.set_alignment(ui::Alignment::Center);
    Painter root(s, Rect{0, 0, 10, 3});
    text.draw(root);
    CK_CHECK(row_text(s, 0) == "    hi    ");
}

CK_TEST(static_text_end_alignment_offsets_each_wrapped_line) {
    Fixture f;
    Surface s = make_surface(10, 3);
    StaticText text("hi");
    text.set_context(f.ctx());
    text.set_bounds(Rect{0, 0, 10, 1});
    text.set_alignment(ui::Alignment::End);
    Painter root(s, Rect{0, 0, 10, 3});
    text.draw(root);
    CK_CHECK(row_text(s, 0) == "        hi");
}

CK_TEST(static_text_asks_for_a_readable_measure_rather_than_its_unwrapped_length) {
    // The width a paragraph asks for is the width it is read at. Asking for
    // its unwrapped length instead made every self-sizing container built
    // around prose as wide as its longest sentence.
    const std::string sentence =
        "A paragraph of ordinary prose, long enough that written out on one "
        "line it would run well past any width a reader is comfortable with.";
    StaticText text(sentence);
    CK_CHECK(static_cast<int>(sentence.size()) > ckv::widgets::kProseMeasureCells);
    CK_CHECK(text.horizontal_size_hint().preferred == ckv::widgets::kProseMeasureCells);
    // A request, not a ceiling: given more columns, the text still uses them.
    CK_CHECK(text.horizontal_size_hint().max == ui::kUnboundedExtent);
    CK_CHECK(text.height_for_width(200) == 1);
}

CK_TEST(static_text_shorter_than_the_measure_asks_only_for_what_it_occupies) {
    StaticText text("Saved.");
    CK_CHECK(text.horizontal_size_hint().preferred == 6);
}

CK_TEST(preformatted_static_text_asks_for_every_column_it_was_written_with) {
    // Preformatted text cannot wrap into a narrower view -- it would be
    // clipped instead -- so it asks for its full width however wide that is.
    const std::string table =
        "name                                      value                     units\n"
        "short                                     1                         cells";
    StaticText text(table);
    text.set_preformatted(true);
    CK_CHECK(text.horizontal_size_hint().preferred == 73);
    CK_CHECK(text.horizontal_size_hint().preferred > ckv::widgets::kProseMeasureCells);
}

// --- Size-hint-change propagation (M9/WP-16, E10) ---------------------

CK_TEST(label_set_text_notifies_its_parent_of_the_changed_size_hint) {
    SpyParent parent;
    parent.add_child(std::make_unique<Label>("&Name:"));
    CK_CHECK(parent.notifications == 0);  // construction, before attachment, notified nobody

    static_cast<Label*>(parent.children().front().get())->set_text("&A much longer label:");
    CK_CHECK(parent.notifications == 1);
}

CK_TEST(button_set_text_notifies_its_parent_of_the_changed_size_hint) {
    SpyParent parent;
    parent.add_child(std::make_unique<Button>("OK"));
    CK_CHECK(parent.notifications == 0);

    static_cast<Button*>(parent.children().front().get())->set_text("Cancel Everything");
    CK_CHECK(parent.notifications == 1);
}

CK_TEST(static_text_set_text_notifies_its_parent_of_the_changed_size_hint) {
    SpyParent parent;
    parent.add_child(std::make_unique<StaticText>("short"));
    CK_CHECK(parent.notifications == 0);

    auto* text = static_cast<StaticText*>(parent.children().front().get());
    text->set_text("a much longer piece of text");
    CK_CHECK(parent.notifications == 1);
}

// --- InputLine ---------------------------------------------------------

CK_TEST(input_line_starts_empty_with_cursor_at_zero) {
    Fixture f;
    InputLine input;
    CK_CHECK(input.text().empty());
    CK_CHECK(input.cursor() == 0);
}

CK_TEST(set_text_places_the_cursor_at_the_end) {
    Fixture f;
    InputLine input;
    input.set_text("hello");
    CK_CHECK(input.text() == "hello");
    CK_CHECK(input.cursor() == 5);
}

CK_TEST(typed_text_inserts_at_the_cursor) {
    Fixture f;
    InputLine input;
    input.set_text("helloworld");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    for (int i = 0; i < 5; ++i) input.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    input.on_text(ckv::TextEvent{" ", false});
    CK_CHECK(input.text() == "hello world");
}

CK_TEST(character_key_events_use_the_same_insertion_path_as_text_events) {
    Fixture f;
    InputLine input;
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "a"}}));
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Shift, "B"}}));
    CK_CHECK(input.text() == "aB");

    CK_CHECK(!input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}}));
    CK_CHECK(!input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "c"}}));
    CK_CHECK(!input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Super, "v"}}));
    CK_CHECK(input.text() == "aB");
}

CK_TEST(backspace_at_start_of_field_is_a_no_op) {
    Fixture f;
    InputLine input;
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(input.text() == "abc");
    CK_CHECK(input.cursor() == 0);
}

CK_TEST(delete_at_end_of_field_is_a_no_op) {
    Fixture f;
    InputLine input;
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Delete, Modifier::None, ""}});
    CK_CHECK(input.text() == "abc");
}

CK_TEST(backspace_removes_the_grapheme_before_the_cursor) {
    Fixture f;
    InputLine input;
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(input.text() == "ab");
    CK_CHECK(input.cursor() == 2);
}

CK_TEST(shift_left_extends_a_selection_and_plain_left_collapses_it) {
    Fixture f;
    InputLine input;
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
    InputLine input;
    input.set_text("hello");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});  // selects "lo"
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "helX");
}

CK_TEST(overwrite_mode_replaces_the_grapheme_under_the_cursor_instead_of_inserting) {
    Fixture f;
    InputLine input;
    input.set_text("abc");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}});
    CK_CHECK(input.overwrite_mode());
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "Xbc");
}

CK_TEST(overwrite_mode_past_the_end_of_text_appends_instead_of_indexing_out_of_range) {
    Fixture f;
    InputLine input;
    input.set_text("ab");  // cursor already at the end
    input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}});
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "abX");
}

CK_TEST(grapheme_correct_editing_never_splits_a_multi_byte_cluster) {
    Fixture f;
    InputLine input;
    input.set_text("a\xC3\xA9z");  // "aéz"
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});   // cursor between é and z
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});  // removes the whole é, not a byte of it
    CK_CHECK(input.text() == "az");
}

CK_TEST(set_valid_false_is_observable_and_toggling_back_is_observable_too) {
    Fixture f;
    InputLine input;
    CK_CHECK(input.valid());
    input.set_valid(false);
    CK_CHECK(!input.valid());
    input.set_valid(true);
    CK_CHECK(input.valid());
}

CK_TEST(mouse_click_places_the_cursor_at_the_clicked_grapheme) {
    Fixture f;
    InputLine input;
    input.set_bounds(Rect{0, 0, 20, 1});
    input.set_text("abcdef");
    input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                    std::nullopt, Modifier::None});
    CK_CHECK(input.cursor() == 3);
}

CK_TEST(mouse_drag_extends_the_input_line_selection) {
    Fixture f;
    InputLine input;
    input.set_bounds(Rect{0, 0, 20, 1});
    input.set_text("abcdef");
    input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 0},
                                    std::nullopt, Modifier::None});
    input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{4, 0},
                                    std::nullopt, Modifier::None});
    input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{4, 0},
                                    std::nullopt, Modifier::None});
    CK_CHECK(input.has_selection());
    const auto [begin, end] = input.selection_range();
    CK_CHECK(begin == 1);
    CK_CHECK(end == 4);
}

CK_TEST(click_outside_the_widgets_bounds_is_ignored) {
    Fixture f;
    InputLine input;
    input.set_bounds(Rect{0, 0, 5, 1});
    input.set_text("abc");
    CK_CHECK(!input.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{50, 50}, std::nullopt, Modifier::None}));
    CK_CHECK(input.cursor() == 3);  // unchanged
}

CK_TEST(draw_does_not_crash_when_the_field_is_empty_and_focused) {
    Fixture f;
    Surface s = make_surface(20, 3);
    InputLine input;
    input.set_context(f.ctx());
    input.set_bounds(Rect{0, 0, 10, 1});
    input.on_focus(ckv::FocusEvent{true});
    Painter root(s, Rect{0, 0, 20, 3});
    input.draw(root);
    CK_CHECK(true);
}

CK_TEST(long_text_scrolls_so_the_cursor_stays_visible) {
    Fixture f;
    Surface s = make_surface(20, 3);
    InputLine input;
    input.set_context(f.ctx());
    input.set_bounds(Rect{0, 0, 5, 1});
    input.set_text("abcdefghij");  // 10 chars, only 5 columns visible, cursor at end (10)
    Painter root(s, Rect{0, 0, 20, 3});
    input.draw(root);
    // The tail of the text (near the cursor) must be visible, not the head.
    CK_CHECK(row_text(s, 0).find('j') != std::string::npos);
}

CK_TEST(input_line_copy_cut_and_paste_use_the_application_clipboard) {
    Fixture f;
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    InputLine input;
    input.set_context(ui::Context{&f.theme, &f.registry, &app});
    input.set_text("hello");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});

    CK_CHECK(input.on_key(ctrl_char("c")));
    CK_CHECK(app.clipboard_text() == "lo");
    CK_CHECK(input.text() == "hello");

    CK_CHECK(input.on_key(ctrl_char("x")));
    CK_CHECK(app.clipboard_text() == "lo");
    CK_CHECK(input.text() == "hel");

    CK_CHECK(input.on_key(ctrl_char("v")));
    CK_CHECK(input.text() == "hello");
}

CK_TEST(input_line_uses_word_navigation_and_legacy_insert_clipboard_bindings) {
    Fixture f;
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    InputLine input;
    input.set_context(ui::Context{&app.theme(), &app.roles(), &app});
    input.set_text("one two three");

    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Ctrl, ""}}));
    CK_CHECK(input.cursor() == 8U);
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Ctrl | Modifier::Shift, ""}}));
    const std::pair<std::size_t, std::size_t> expected_selection{4U, 8U};
    CK_CHECK(input.selection_range() == expected_selection);
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::Ctrl, ""}}));
    CK_CHECK(app.clipboard_text() == "two ");
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Delete, Modifier::Shift, ""}}));
    CK_CHECK(input.text() == "one three");
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Insert, Modifier::Shift, ""}}));
    CK_CHECK(input.text() == "one two three");
}

CK_TEST(input_line_undo_restores_the_previous_text_and_selection_state) {
    Fixture f;
    InputLine input;
    input.set_text("ab");
    input.on_text(ckv::TextEvent{"c", false});
    CK_CHECK(input.text() == "abc");
    CK_CHECK(input.on_key(ctrl_char("z")));
    CK_CHECK(input.text() == "ab");

    input.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::Shift, ""}});
    CK_CHECK(input.has_selection());
    input.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(input.text() == "aX");
    CK_CHECK(input.on_key(ctrl_char("z")));
    CK_CHECK(input.text() == "ab");
    CK_CHECK(input.has_selection());
    const auto [begin, end] = input.selection_range();
    CK_CHECK(begin == 1);
    CK_CHECK(end == 2);
}

// --- InputLine completion: validator, mask, password echo, history ---

CK_TEST(validator_runs_automatically_after_every_edit) {
    Fixture f;
    InputLine input;
    input.set_validator([](const std::string& s) { return s.size() >= 3; });
    CK_CHECK(!input.valid());  // empty text fails immediately (validator runs on install)
    input.on_text(ckv::TextEvent{"abc", false});
    CK_CHECK(input.valid());
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(!input.valid());  // "ab" — edit re-runs the validator, overriding the prior verdict
}

CK_TEST(grapheme_filter_applies_to_typed_and_pasted_text_but_not_programmatic_values) {
    Fixture f;
    InputLine input;
    input.set_grapheme_filter([](std::string_view grapheme) {
        return grapheme.size() == 1 && grapheme.front() >= '0' && grapheme.front() <= '9';
    });

    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "4"}}));
    CK_CHECK(input.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "x"}}));
    CK_CHECK(input.on_text(ckv::TextEvent{"5y6", true}));
    CK_CHECK(input.text() == "456");

    input.set_text("legacy-value");
    CK_CHECK(input.text() == "legacy-value");
}

CK_TEST(external_set_valid_is_overridden_by_the_next_edit_when_a_validator_is_installed) {
    Fixture f;
    InputLine input;
    input.set_validator([](const std::string& s) { return !s.empty(); });
    input.set_text("x");
    input.set_valid(false);  // external override (e.g. a dialog-accept veto)
    CK_CHECK(!input.valid());
    input.on_text(ckv::TextEvent{"y", false});
    CK_CHECK(input.valid());  // the edit re-validates and supersedes the stale external verdict
}

CK_TEST(with_no_validator_installed_set_valid_behaves_exactly_as_before) {
    Fixture f;
    InputLine input;
    input.set_valid(false);
    input.on_text(ckv::TextEvent{"x", false});
    CK_CHECK(!input.valid());  // no validator installed: editing does not touch valid()
}

CK_TEST(mask_prefills_literals_and_placeholders) {
    Fixture f;
    InputLine input;
    input.set_mask("999-999");
    CK_CHECK(input.text() == "___-___");
    CK_CHECK(input.cursor() == 0);  // the first editable position
}

CK_TEST(typing_into_a_mask_only_accepts_matching_characters_and_auto_advances) {
    Fixture f;
    InputLine input;
    input.set_mask("999-999");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"a", false});  // rejected: '9' requires a digit
    input.on_text(ckv::TextEvent{"2", false});
    CK_CHECK(input.text() == "12_-___");
    CK_CHECK(input.cursor() == 2);  // still waiting on the third digit
}

CK_TEST(typing_skips_over_literal_positions_automatically) {
    Fixture f;
    InputLine input;
    input.set_mask("999-999");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"2", false});
    input.on_text(ckv::TextEvent{"3", false});  // fills the last digit before the literal '-'
    CK_CHECK(input.text() == "123-___");
    CK_CHECK(input.cursor() == 4);  // skipped straight past the literal to the next editable slot
}

CK_TEST(mask_letter_class_rejects_digits) {
    Fixture f;
    InputLine input;
    input.set_mask("AAA");
    input.on_text(ckv::TextEvent{"5", false});
    CK_CHECK(input.text() == "___");  // rejected
    input.on_text(ckv::TextEvent{"x", false});
    CK_CHECK(input.text() == "x__");
}

CK_TEST(mask_backspace_clears_the_previous_editable_position_and_moves_there) {
    Fixture f;
    InputLine input;
    input.set_mask("999");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"2", false});
    input.on_key(ckv::KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
    CK_CHECK(input.text() == "1__");
    CK_CHECK(input.cursor() == 1);
}

CK_TEST(mask_fully_filled_stops_accepting_further_input) {
    Fixture f;
    InputLine input;
    input.set_mask("99");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"2", false});
    input.on_text(ckv::TextEvent{"3", false});  // no editable slot left
    CK_CHECK(input.text() == "12");
}

CK_TEST(disabling_the_mask_preserves_the_current_masked_text_as_free_form) {
    Fixture f;
    InputLine input;
    input.set_mask("99");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"2", false});
    input.set_mask("");
    CK_CHECK(!input.has_mask());
    CK_CHECK(input.text() == "12");
    input.on_text(ckv::TextEvent{"x", false});  // free-form editing now applies
    CK_CHECK(input.text() == "12x");
}

CK_TEST(password_echo_hides_content_but_editing_still_affects_the_real_text) {
    Fixture f;
    InputLine input;
    input.set_context(f.ctx());
    input.set_password_echo(true);
    input.set_text("secret");
    CK_CHECK(input.text() == "secret");  // the real underlying text is untouched

    Surface s = make_surface(20, 1);
    Painter painter(s, Rect{0, 0, 20, 1});
    input.set_bounds(Rect{0, 0, 20, 1});
    input.draw(painter);
    CK_CHECK(row_text(s, 0).substr(0, 6) == "******");
    CK_CHECK(row_text(s, 0).find('s') == std::string::npos);  // no real character leaks into the display
}

CK_TEST(history_up_cycles_to_the_most_recent_entry_and_down_restores_the_live_text) {
    Fixture f;
    ui::HistoryRegistry history;
    history.record("k", "first");
    history.record("k", "second");  // "second" is now most-recent (front)
    InputLine input;
    input.set_history(&history, "k");
    input.set_text("typing...");

    input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});
    CK_CHECK(input.text() == "second");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});
    CK_CHECK(input.text() == "first");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});
    CK_CHECK(input.text() == "first");  // already at the oldest entry — stays put

    input.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(input.text() == "second");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(input.text() == "typing...");  // restored the pre-browsing live text
}

CK_TEST(history_navigation_is_disabled_entirely_on_a_masked_field) {
    // Regression guard: history_show() calls set_text() directly, which
    // would splice arbitrary unmasked text (e.g. "second") into a
    // masked field's fixed-length, class-constrained buffer — Up/Down
    // must fall through to on_key_masked() instead of ever reaching
    // history_show() when a mask is active.
    Fixture f;
    HistoryRegistry history;
    history.record("k", "second");
    history.record("k", "first");
    InputLine input;
    input.set_history(&history, "k");
    input.set_mask("999");
    input.on_text(ckv::TextEvent{"1", false});
    input.on_text(ckv::TextEvent{"2", false});
    const std::string before = input.text();

    input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});
    CK_CHECK(input.text() == before);  // untouched — Up did NOT pull in an unmasked history entry
    CK_CHECK(input.has_mask());        // and the mask itself is still active
}

CK_TEST(history_up_with_an_empty_registry_is_a_harmless_no_op) {
    Fixture f;
    ui::HistoryRegistry history;
    InputLine input;
    input.set_history(&history, "unused-key");
    input.set_text("still here");
    input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});
    CK_CHECK(input.text() == "still here");
}

CK_TEST(commit_to_history_records_the_current_text_under_the_configured_key) {
    Fixture f;
    ui::HistoryRegistry history;
    InputLine input;
    input.set_history(&history, "k");
    input.set_text("committed value");
    input.commit_to_history();
    CK_CHECK(history.entries("k").size() == 1);
    CK_CHECK(history.entries("k")[0] == "committed value");
}

CK_TEST(without_a_history_registry_up_and_down_are_unhandled_as_before) {
    Fixture f;
    InputLine input;
    CK_CHECK(!input.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}}));
    CK_CHECK(!input.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
}

// --- Button press lifecycle ----------------------------------------------
//
// A press is something that can be taken back. It begins on the way down,
// shows itself while it lasts, and only acts on the way up — and only if it
// was never revoked in between by moving off the control or by focus leaving
// it. Terminals that cannot report a key release get the closest honest
// substitute instead, which is asserted separately below.

namespace {

// A terminal profile that reports key releases (the kitty keyboard
// protocol); the legacy default never does.
ckv::term::Capabilities release_reporting_capabilities() {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.keyboard_protocol = ckv::term::KeyboardProtocol::Kitty;
    return caps;
}

}  // namespace

CK_TEST(a_held_enter_keeps_the_button_down_and_acts_only_on_release) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 10}, release_reporting_capabilities());
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* button = static_cast<Button*>(app.root().add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{0, 0, 10, 1});
    app.set_focus(button);
    int presses = 0;
    button->on_press = [&] { ++presses; };

    CK_CHECK(button->on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Press, true}));
    CK_CHECK(button->pressed());  // visibly down while the key is held
    CK_CHECK(presses == 0);       // and nothing has happened yet
    CK_CHECK(button->on_key(
        ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Repeat, true}));
    CK_CHECK(presses == 0);  // one press held, not many
    // The release arrives on its dedicated route, as Application routes it.
    CK_CHECK(button->on_key_release(
        ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Release, true}));
    CK_CHECK(!button->pressed());
    CK_CHECK(presses == 1);
    // A stray release with no press in flight is not this button's.
    CK_CHECK(!button->on_key_release(
        ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Release, true}));
    CK_CHECK(presses == 1);
}

CK_TEST(a_held_space_behaves_exactly_as_a_held_enter) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 10}, release_reporting_capabilities());
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* button = static_cast<Button*>(app.root().add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{0, 0, 10, 1});
    app.set_focus(button);
    int presses = 0;
    button->on_press = [&] { ++presses; };

    CK_CHECK(button->on_key(
        ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " "}, ckv::KeyAction::Press, true}));
    CK_CHECK(button->pressed());
    CK_CHECK(presses == 0);
    CK_CHECK(button->on_key_release(
        ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " "}, ckv::KeyAction::Release, true}));
    CK_CHECK(presses == 1);
}

CK_TEST(focus_leaving_during_a_held_key_takes_the_press_back) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 10}, release_reporting_capabilities());
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* button = static_cast<Button*>(app.root().add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{0, 0, 10, 1});
    auto* other = static_cast<Button*>(app.root().add_child(std::make_unique<Button>("Cancel")));
    other->set_bounds(Rect{0, 2, 10, 1});
    app.set_focus(button);
    int presses = 0;
    button->on_press = [&] { ++presses; };

    CK_CHECK(button->on_key(
        ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Press, true}));
    CK_CHECK(button->pressed());
    app.set_focus(other);  // Tab away mid-press
    CK_CHECK(!button->pressed());
    // The key finally comes up, but it is no longer this button's press.
    CK_CHECK(!button->on_key_release(
        ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}, ckv::KeyAction::Release, true}));
    CK_CHECK(presses == 0);
}

CK_TEST(a_key_that_promises_no_release_acts_immediately) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 10});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* button = static_cast<Button*>(app.root().add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{0, 0, 10, 1});
    app.set_focus(button);
    int presses = 0;
    button->on_press = [&] { ++presses; };

    CK_CHECK(button->on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(button->pressed());  // still acknowledged visibly
    CK_CHECK(presses == 1);       // but acts now: waiting would wait forever
}

CK_TEST(dragging_off_a_pressed_button_lifts_it_and_dragging_back_presses_it_again) {
    Fixture f;
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 10, 1});
    int presses = 0;
    button.on_press = [&] { ++presses; };

    const auto at = [](int x, int y, ckv::MouseAction action) {
        return ckv::MouseEvent{action, ckv::MouseButton::Left, Point{x, y}, std::nullopt,
                               Modifier::None};
    };
    CK_CHECK(button.on_mouse(at(2, 0, ckv::MouseAction::Down)));
    CK_CHECK(button.pressed());
    button.on_mouse(at(30, 5, ckv::MouseAction::Move));  // off the button
    CK_CHECK(!button.pressed());
    button.on_mouse(at(3, 0, ckv::MouseAction::Move));  // and back onto it
    CK_CHECK(button.pressed());
    button.on_mouse(at(3, 0, ckv::MouseAction::Up));
    CK_CHECK(presses == 1);
}

CK_TEST(releasing_the_pointer_away_from_a_button_takes_the_press_back) {
    Fixture f;
    Button button("OK");
    button.set_context(f.ctx());
    button.set_bounds(Rect{0, 0, 10, 1});
    int presses = 0;
    button.on_press = [&] { ++presses; };

    const auto at = [](int x, int y, ckv::MouseAction action) {
        return ckv::MouseEvent{action, ckv::MouseButton::Left, Point{x, y}, std::nullopt,
                               Modifier::None};
    };
    button.on_mouse(at(2, 0, ckv::MouseAction::Down));
    button.on_mouse(at(30, 5, ckv::MouseAction::Move));
    button.on_mouse(at(30, 5, ckv::MouseAction::Up));
    CK_CHECK(!button.pressed());
    CK_CHECK(presses == 0);  // moving away before releasing cancels it
}

CK_TEST(a_flat_button_is_one_row_of_label_and_shows_its_press_in_the_colours) {
    Fixture f;
    Button button("<<");
    button.set_flat(true);
    button.set_context(f.ctx());
    button.on_attached();
    // No shadow to stand off from, no ten-cell footprint, and no row below:
    // a stepper beside a field has room for its label and nothing else.
    CK_CHECK(button.horizontal_size_hint().preferred == 2);
    CK_CHECK(button.vertical_size_hint().preferred == 1);
    CK_CHECK(!button.trailing_row_is_shadow());

    button.set_bounds(ckv::Rect{0, 0, 2, 1});
    int presses = 0;
    button.on_press = [&] { ++presses; };
    const auto face_bg = [&] {
        Surface surface = make_surface(2, 1);
        Painter painter(surface, ckv::Rect{0, 0, 2, 1});
        button.draw(painter);
        return surface.at(ckv::Point{0, 0}).style().bg;
    };
    const auto idle = face_bg();
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 0},
                                    std::nullopt, ckv::Modifier::None});
    CK_CHECK(button.pressed());
    CK_CHECK(face_bg() != idle);  // the press is visible where the geometry cannot show it
    CK_CHECK(presses == 0);       // ...and it has not fired yet
    button.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{0, 0},
                                    std::nullopt, ckv::Modifier::None});
    CK_CHECK(presses == 1);
    CK_CHECK(face_bg() == idle);
}

CK_TEST(an_input_line_exactly_as_wide_as_its_text_shows_all_of_it) {
    Fixture f;
    InputLine line;
    line.set_context(f.ctx());
    line.on_attached();
    line.set_bounds(ckv::Rect{0, 0, 4, 1});
    line.set_text("2026");  // set_text leaves the cursor past the last digit

    Surface surface = make_surface(4, 1);
    Painter painter(surface, ckv::Rect{0, 0, 4, 1});
    line.draw(painter);
    // Scrolling to make room for a cursor past the last character would cost
    // the first one, which is not a trade a field that fits should make.
    CK_CHECK(row_text(surface, 0) == "2026");
}
