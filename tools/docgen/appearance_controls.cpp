// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Appearance specimens: the form controls — the things a dialog is made of.
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "appearance_matrix.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/key_chord_capture.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/popup_list.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/tab_control.hpp"

namespace ckv::docgen::appearance {

namespace {

// Every control specimen stands in one dialog of this size, on a screen with
// room for its shadow.
constexpr Size kScreen{30, 7};
constexpr Rect kDialog{1, 1, 27, 5};
// A specimen that needs more than one row — a group, a memo, a strip of tabs,
// a list dropped below its field — stands in this taller window instead. Its
// content area is 29 x 6.
constexpr Size kTallScreen{34, 10};
constexpr Rect kTallWindow{1, 1, 31, 8};

KeyEvent key(Key which, Modifier modifiers = Modifier::None) { return KeyEvent{KeyChord{which, modifiers, ""}}; }

MouseEvent pointer(MouseAction action, Point cell) {
    return MouseEvent{action, action == MouseAction::Move ? MouseButton::None : MouseButton::Left, cell, std::nullopt};
}

// --- Button ---------------------------------------------------------------

widgets::Button& button(Stage& stage, std::string text = "&Save", Rect bounds = Rect{7, 1, 12, 2}) {
    ui::View& body = stage.dialog(kDialog, "Button");
    return stage.place(body, bounds, std::make_unique<widgets::Button>(std::move(text)));
}

void add_button(Catalog& catalog) {
    Element& e = catalog.element("Button", "include/cvision/widgets/button.hpp", Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { button(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(button(s)); });
    state(e, "default", kScreen, [](Stage& s) { button(s).set_default(true); });
    state(e, "hovered", kScreen, [](Stage& s) {
        widgets::Button& b = button(s);
        s.step();
        const Rect at = b.absolute_bounds();
        s.app().dispatch(pointer(MouseAction::Move, Point{at.x + 3, at.y}));
    });
    state(e, "pressed", kScreen, [](Stage& s) {
        widgets::Button& b = button(s);
        s.step();
        const Rect at = b.absolute_bounds();
        s.app().dispatch(pointer(MouseAction::Down, Point{at.x + 3, at.y}));
    });
    state(e, "disabled", kScreen, [](Stage& s) { button(s).set_enabled(false); });
    state(e, "wide", kScreen, [](Stage& s) { button(s, std::string(kWideText), Rect{3, 1, 20, 2}); });
    state(e, "narrow", kScreen, [](Stage& s) { button(s, "&Synchronize", Rect{7, 1, 8, 2}); });
}

// --- Label ----------------------------------------------------------------

widgets::Label& label(Stage& stage, std::string text = "&Name:", Rect bounds = Rect{2, 1, 12, 1}) {
    ui::View& body = stage.dialog(kDialog, "Label");
    auto& placed = stage.place(body, bounds, std::make_unique<widgets::Label>(std::move(text)));
    auto& field = stage.place(body, Rect{9, 1, 14, 1}, std::make_unique<widgets::InputLine>());
    placed.set_buddy(&field);
    return placed;
}

void add_label(Catalog& catalog) {
    Element& e = catalog.element("Label", "include/cvision/widgets/label.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { label(s); });
    state(e, "disabled", kScreen, [](Stage& s) { label(s).set_enabled(false); });
    state(e, "wide", kScreen, [](Stage& s) { label(s, std::string(kWideText), Rect{2, 1, 20, 1}); });
    state(e, "narrow", kScreen, [](Stage& s) { label(s, "&Description:", Rect{2, 1, 6, 1}); });
}

// --- StaticText -----------------------------------------------------------

void static_text(Stage& stage, std::string text, Rect bounds = Rect{1, 0, 23, 3}) {
    ui::View& body = stage.dialog(kDialog, "Static text");
    stage.place(body, bounds, std::make_unique<widgets::StaticText>(std::move(text)));
}

void add_static_text(Catalog& catalog) {
    Element& e = catalog.element("StaticText", "include/cvision/widgets/static_text.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { static_text(s, "Changes are saved when the dialog closes."); });
    state(e, "emphasized", kScreen, [](Stage& s) {
        ui::View& body = s.dialog(kDialog, "Static text");
        auto& text = s.place(body, Rect{1, 0, 23, 3},
                             std::make_unique<widgets::StaticText>("Unsaved changes\nThey are lost on quit."));
        text.set_emphasized_leading_lines(1);
    });
    state(e, "wide", kScreen, [](Stage& s) { static_text(s, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { static_text(s, "Changes are saved when the dialog closes.", Rect{1, 0, 9, 3}); });
}

// --- InputLine ------------------------------------------------------------

widgets::InputLine& input(Stage& stage, std::string text = "report.txt", Rect bounds = Rect{2, 1, 20, 1}) {
    ui::View& body = stage.dialog(kDialog, "Input line");
    auto& field = stage.place(body, bounds, std::make_unique<widgets::InputLine>());
    field.set_text(std::move(text));
    return field;
}

void add_input_line(Catalog& catalog) {
    Element& e = catalog.element("InputLine", "include/cvision/widgets/input_line.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { input(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(input(s)); });
    state(e, "selected", kScreen, [](Stage& s) {
        widgets::InputLine& field = input(s);
        s.focus(field);
        s.app().dispatch(key(Key::End));
        for (int i = 0; i < 3; ++i) s.app().dispatch(key(Key::Left, Modifier::Shift));
    });
    state(e, "invalid", kScreen, [](Stage& s) { input(s, "12x").set_valid(false); });
    state(e, "password", kScreen, [](Stage& s) {
        widgets::InputLine& field = input(s, "secret");
        field.set_password_echo(true);
    });
    state(e, "disabled", kScreen, [](Stage& s) { input(s).set_enabled(false); });
    state(e, "wide", kScreen, [](Stage& s) { s.focus(input(s, std::string(kWideText))); });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(input(s, "a-long-file-name.txt", Rect{2, 1, 8, 1})); });
}

// --- KeyChordCapture ------------------------------------------------------

constexpr KeyChord kPalette{Key::Char, Modifier::Ctrl | Modifier::Shift, "p"};

widgets::KeyChordCapture& shortcut(Stage& stage, std::optional<KeyChord> chord = kPalette,
                                   Rect bounds = Rect{2, 1, 21, 1}) {
    ui::View& body = stage.dialog(kDialog, "Shortcut");
    auto& field = stage.place(body, bounds, std::make_unique<widgets::KeyChordCapture>());
    field.set_chord(std::move(chord));
    return field;
}

void add_key_chord_capture(Catalog& catalog) {
    Element& e = catalog.element("KeyChordCapture", "include/cvision/widgets/key_chord_capture.hpp",
                                 Traits{.focusable = true, .control = true});
    state(e, "normal", kScreen, [](Stage& s) { shortcut(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(shortcut(s)); });
    state(e, "capturing", kScreen, [](Stage& s) {
        widgets::KeyChordCapture& field = shortcut(s);
        s.focus(field);
        field.begin_capture();
    });
    state(e, "unbound", kScreen, [](Stage& s) { shortcut(s, std::nullopt); });
    state(e, "disabled", kScreen, [](Stage& s) { shortcut(s).set_enabled(false); });
    state(e, "narrow", kScreen, [](Stage& s) {
        shortcut(s, KeyChord{Key::PageDown, Modifier::Ctrl | Modifier::Alt | Modifier::Shift, ""}, Rect{2, 1, 8, 1});
    });
}

// --- Memo -----------------------------------------------------------------

constexpr std::string_view kProse =
    "A memo holds prose the reader is writing, and wraps it between words.\nA second paragraph.";

widgets::Memo& memo(Stage& stage, std::string_view text = kProse, Rect bounds = Rect{1, 1, 27, 4}) {
    ui::View& body = stage.dialog(kTallWindow, "Memo");
    auto& field = stage.place(body, bounds, std::make_unique<widgets::Memo>());
    field.set_text(std::string(text));
    return field;
}

// Enough lines that four rows show only part of them.
std::string numbered_lines(int count) {
    std::string text;
    for (int i = 1; i <= count; ++i) {
        if (i > 1) text += '\n';
        text += "Line " + std::to_string(i) + " of the note";
    }
    return text;
}

void add_memo(Catalog& catalog) {
    Element& e = catalog.element("Memo", "include/cvision/widgets/memo.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { memo(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(memo(s)); });
    state(e, "selected", kTallScreen, [](Stage& s) {
        s.focus(memo(s));
        s.app().dispatch(key(Key::Down, Modifier::Shift));
        for (int i = 0; i < 4; ++i) s.app().dispatch(key(Key::Right, Modifier::Shift));
    });
    state(e, "invalid", kTallScreen, [](Stage& s) { memo(s).set_valid(false); });
    state(e, "scrolled", kTallScreen, [](Stage& s) {
        s.focus(memo(s, numbered_lines(9)));
        s.app().dispatch(key(Key::End, Modifier::Ctrl));
    });
    state(e, "unwrapped", kTallScreen, [](Stage& s) { memo(s).set_wrap_mode(widgets::WrapMode::None); });
    state(e, "disabled", kTallScreen, [](Stage& s) { memo(s).set_enabled(false); });
    state(e, "wide", kTallScreen, [](Stage& s) { s.focus(memo(s, kWideText)); });
    state(e, "narrow", kTallScreen, [](Stage& s) { memo(s, kProse, Rect{1, 1, 9, 4}); });
}

// --- CheckGroup -----------------------------------------------------------

widgets::CheckGroup& checks(Stage& stage,
                            std::vector<std::string> labels = {"&Optimize", "&Debug info", "&Warnings as errors"},
                            Rect bounds = Rect{1, 1, 27, 4}) {
    ui::View& body = stage.dialog(kTallWindow, "Check group");
    auto& group = stage.place(body, bounds, std::make_unique<widgets::CheckGroup>(std::move(labels)));
    group.set_group_label("&Compilation");
    group.set_checked(0, true);
    return group;
}

void add_check_group(Catalog& catalog) {
    Element& e = catalog.element("CheckGroup", "include/cvision/widgets/option_group.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { checks(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(checks(s)); });
    state(e, "mixed", kTallScreen, [](Stage& s) {
        widgets::CheckGroup& group = checks(s);
        group.set_tristate(true);
        group.set_check_state(2, widgets::CheckState::Mixed);
    });
    state(e, "columns", kTallScreen, [](Stage& s) {
        widgets::CheckGroup& group = checks(s, {"&Optimize", "&Debug", "&Strip", "&Lint"}, Rect{1, 1, 27, 3});
        group.set_columns(2);
        group.set_checked(2, true);
    });
    state(e, "disabled", kTallScreen, [](Stage& s) { checks(s).set_enabled(false); });
    state(e, "wide", kTallScreen, [](Stage& s) { checks(s, {std::string(kWideText), "&Debug info"}); });
    state(e, "narrow", kTallScreen, [](Stage& s) {
        checks(s, {"&Optimize", "&Debug info", "&Warnings as errors"}, Rect{1, 1, 11, 4});
    });
}

// --- RadioGroup -----------------------------------------------------------

widgets::RadioGroup& radios(Stage& stage, std::vector<std::string> labels = {"&Static", "S&hared", "&Module"},
                            Rect bounds = Rect{1, 1, 27, 4}) {
    ui::View& body = stage.dialog(kTallWindow, "Radio group");
    auto& group = stage.place(body, bounds, std::make_unique<widgets::RadioGroup>(std::move(labels)));
    group.set_group_label("&Library");
    group.set_selected(1);
    return group;
}

void add_radio_group(Catalog& catalog) {
    Element& e = catalog.element("RadioGroup", "include/cvision/widgets/option_group.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { radios(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(radios(s)); });
    state(e, "unselected", kTallScreen, [](Stage& s) { radios(s).set_selected(-1); });
    state(e, "columns", kTallScreen, [](Stage& s) {
        widgets::RadioGroup& group = radios(s, {"None", "Size", "Speed", "Full"}, Rect{1, 1, 27, 3});
        group.set_columns(2);
        group.set_selected(2);
    });
    state(e, "disabled", kTallScreen, [](Stage& s) { radios(s).set_enabled(false); });
    state(e, "wide", kTallScreen, [](Stage& s) { radios(s, {"&Static", std::string(kWideText)}); });
    state(e, "narrow", kTallScreen, [](Stage& s) {
        radios(s, {"&Static library", "S&hared library"}, Rect{1, 1, 10, 3});
    });
}

// --- ComboBox and the PopupList it drops ----------------------------------

std::vector<std::string> countries() { return {"Germany", "France", "Japan", "United States"}; }

widgets::ComboBox& combo(Stage& stage, std::vector<std::string> items = countries(), Rect bounds = Rect{2, 1, 20, 1},
                         widgets::ComboBoxMode mode = widgets::ComboBoxMode::PickOnly,
                         Size screen = kTallScreen) {
    const Rect dialog{1, 1, screen.width - 3, screen.height - 2};
    ui::View& body = stage.dialog(dialog, "Combo box");
    auto& box = stage.place(body, bounds, std::make_unique<widgets::ComboBox>(mode));
    box.set_items(std::move(items));
    box.set_selected_index(0);
    return box;
}

// Drops the list the way a reader does: from the focused field.
void drop_down(Stage& stage, widgets::ComboBox& box) {
    stage.focus(box);
    stage.step();
    box.open_dropdown();
}

void add_combo_box(Catalog& catalog) {
    Element& e = catalog.element("ComboBox", "include/cvision/widgets/combo_box.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { combo(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(combo(s)); });
    state(e, "editable", kTallScreen, [](Stage& s) {
        s.focus(combo(s, {"Europe/Berlin", "Europe/Paris", "Asia/Tokyo"}, Rect{2, 1, 20, 1},
                      widgets::ComboBoxMode::Editable));
    });
    state(e, "open", kTallScreen, [](Stage& s) { drop_down(s, combo(s)); });
    state(e, "disabled", kTallScreen, [](Stage& s) { combo(s).set_enabled(false); });
    state(e, "wide", kTallScreen, [](Stage& s) { combo(s, {std::string(kWideText), "Germany"}); });
    state(e, "narrow", kTallScreen, [](Stage& s) {
        combo(s, {"Switzerland", "Germany"}, Rect{2, 1, 8, 1});
    });
}

// A PopupList has no face of its own apart from the control that drops it,
// so each of its states is an open ComboBox.
void add_popup_list(Catalog& catalog) {
    Element& e = catalog.element("PopupList", "include/cvision/widgets/popup_list.hpp", Traits{.text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { drop_down(s, combo(s)); });
    state(e, "chosen", kTallScreen, [](Stage& s) {
        widgets::ComboBox& box = combo(s);
        box.set_selected_index(2);
        drop_down(s, box);
    });
    // Twelve items in a list with room for five rows: it scrolls, and opens on
    // the chosen one.
    state(e, "scrolled", kTallScreen, [](Stage& s) {
        widgets::ComboBox& box = combo(s, {"January", "February", "March", "April", "May", "June", "July", "August",
                                           "September", "October", "November", "December"},
                                       Rect{2, 1, 14, 1});
        box.set_selected_index(9);
        drop_down(s, box);
    });
    // No room below the field: the list hangs above it.
    state(e, "above", kTallScreen, [](Stage& s) { drop_down(s, combo(s, countries(), Rect{2, 5, 20, 1})); });
    state(e, "wide", kTallScreen, [](Stage& s) {
        drop_down(s, combo(s, {std::string(kWideText), "Germany", "Japan"}));
    });
    // A screen narrower than the longest item: the list is as wide as the
    // screen and its items clip.
    state(e, "narrow", Size{20, 10}, [](Stage& s) {
        drop_down(s, combo(s, {"United States of America", "Germany"}, Rect{1, 1, 12, 1},
                           widgets::ComboBoxMode::PickOnly, Size{20, 10}));
    });
}

// --- Scrollbar ------------------------------------------------------------

// A bar stands at the edge of a plain document window, where a scrolled
// view would put it; nothing else in the window draws.
constexpr Size kBarScreen{30, 10};
constexpr Rect kBarWindow{1, 1, 27, 8};

widgets::Scrollbar& scrollbar(Stage& stage, widgets::Orientation orientation, int content, int viewport, int position) {
    ui::View& body = stage.document(kBarWindow, "Scrollbar");
    const Rect bounds = orientation == widgets::Orientation::Vertical ? Rect{24, 0, 1, 6} : Rect{0, 5, 24, 1};
    auto& bar = stage.place(body, bounds, std::make_unique<widgets::Scrollbar>(orientation));
    bar.set_range(content, viewport);
    bar.set_position(position);
    return bar;
}

void add_scrollbar(Catalog& catalog) {
    Element& e = catalog.element("Scrollbar", "include/cvision/widgets/scrollbar.hpp", Traits{});
    state(e, "normal", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Vertical, 24, 6, 6); });
    state(e, "top", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Vertical, 24, 6, 0); });
    state(e, "bottom", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Vertical, 24, 6, 18); });
    // A thumb whose ends fall mid-cell, drawn in half blocks.
    state(e, "half-cell", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Vertical, 16, 6, 3); });
    state(e, "horizontal", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Horizontal, 96, 24, 40); });
    // Everything already on screen: the thumb fills the track.
    state(e, "full", kBarScreen, [](Stage& s) { scrollbar(s, widgets::Orientation::Vertical, 4, 6, 0); });
}

// --- ScrollViewport -------------------------------------------------------

// Numbered rows, each running on as a ruler to the content's full width, so
// either axis shows how far it has moved.
std::unique_ptr<ui::View> scroll_content(Size extent) {
    constexpr std::string_view kRuler = "....:....|";
    auto page = std::make_unique<ui::View>();
    page->set_preferred_size(extent);
    std::string text;
    for (int row = 1; row <= extent.height; ++row) {
        if (row > 1) text += '\n';
        std::string line = (row < 10 ? "Row 0" : "Row ") + std::to_string(row) + ' ';
        while (static_cast<int>(line.size()) < extent.width) line += kRuler[line.size() % kRuler.size()];
        text += line;
    }
    auto& body = *page->make<widgets::StaticText>(std::move(text));
    body.set_bounds(Rect{0, 0, extent.width, extent.height});
    return page;
}

widgets::ScrollViewport& viewport(Stage& stage, Size extent = Size{44, 14}) {
    ui::View& body = stage.document(kTallWindow, "Scroll viewport");
    auto region = std::make_unique<widgets::ScrollViewport>();
    region->set_content(scroll_content(extent));
    return stage.place(body, Rect{0, 0, 29, 6}, std::move(region));
}

void add_scroll_viewport(Catalog& catalog) {
    Element& e = catalog.element("ScrollViewport", "include/cvision/widgets/scroll_viewport.hpp", Traits{});
    state(e, "normal", kTallScreen, [](Stage& s) { viewport(s); });
    state(e, "scrolled", kTallScreen, [](Stage& s) { viewport(s).set_scroll(9, 5); });
    state(e, "bottom", kTallScreen, [](Stage& s) { viewport(s).scroll_to_bottom(); });
    state(e, "vertical-only", kTallScreen, [](Stage& s) { viewport(s, Size{27, 14}); });
    state(e, "fits", kTallScreen, [](Stage& s) { viewport(s, Size{27, 4}); });
    state(e, "bars-always", kTallScreen, [](Stage& s) {
        viewport(s, Size{27, 4}).set_scrollbars_always_visible(true);
    });
}

// --- Splitter -------------------------------------------------------------

widgets::Splitter& splitter(Stage& stage, widgets::Orientation orientation = widgets::Orientation::Horizontal,
                            int position = 12, bool second_visible = true) {
    ui::View& body = stage.document(kTallWindow, "Splitter");
    auto first = std::make_unique<widgets::StaticText>("First pane, the one that keeps its size.");
    auto second = std::make_unique<widgets::StaticText>("Second pane, which takes what is left.");
    second->set_visible(second_visible);
    const Rect bounds{0, 0, 29, 6};
    auto split = std::make_unique<widgets::Splitter>(bounds, std::move(first), std::move(second), orientation);
    widgets::Splitter& placed = stage.place(body, bounds, std::move(split));
    placed.set_split_position(position);
    return placed;
}

void add_splitter(Catalog& catalog) {
    Element& e = catalog.element("Splitter", "include/cvision/widgets/splitter.hpp", Traits{.focusable = true});
    state(e, "normal", kTallScreen, [](Stage& s) { splitter(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(splitter(s)); });
    state(e, "moved", kTallScreen, [](Stage& s) {
        s.focus(splitter(s));
        for (int i = 0; i < 5; ++i) s.app().dispatch(key(Key::Right));
    });
    state(e, "stacked", kTallScreen, [](Stage& s) { splitter(s, widgets::Orientation::Vertical, 2); });
    state(e, "collapsed", kTallScreen, [](Stage& s) { splitter(s, widgets::Orientation::Horizontal, 12, false); });
}

// --- TabControl -----------------------------------------------------------

widgets::TabControl& tabs(Stage& stage, std::vector<std::string> labels = {"&General", "&Editor", "&Keys"},
                          Rect bounds = Rect{0, 0, 29, 6}) {
    ui::View& body = stage.dialog(kTallWindow, "Tab control");
    auto& strip = stage.place(body, bounds, std::make_unique<widgets::TabControl>());
    const std::string count = std::to_string(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        auto page = std::make_unique<ui::View>();
        const std::string text = "Page " + std::to_string(i + 1) + " of " + count + ".";
        page->make<widgets::StaticText>(text)->set_bounds(Rect{1, 1, 26, 3});
        strip.add_tab(std::move(labels[i]), std::move(page));
    }
    return strip;
}

std::vector<std::string> many_tabs() { return {"&General", "&Editor", "&Keys", "&Display", "&Advanced"}; }

void add_tab_control(Catalog& catalog) {
    Element& e = catalog.element("TabControl", "include/cvision/widgets/tab_control.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kTallScreen, [](Stage& s) { tabs(s); });
    state(e, "focused", kTallScreen, [](Stage& s) { s.focus(tabs(s)); });
    state(e, "second-page", kTallScreen, [](Stage& s) { tabs(s).set_active_index(1); });
    state(e, "disabled", kTallScreen, [](Stage& s) { tabs(s).set_enabled(false); });
    state(e, "wide", kTallScreen, [](Stage& s) { tabs(s, {std::string(kWideText), "&Keys"}); });
    state(e, "narrow", kTallScreen, [](Stage& s) { tabs(s, {"&General", "&Editor", "&Keys"}, Rect{0, 0, 14, 6}); });
    // More captions than the strip holds: it scrolls to keep the active one
    // shown, and marks the side that hides the rest -- the right with the
    // first tab active, both from the middle, the left at the far end.
    state(e, "overflow", kTallScreen, [](Stage& s) { tabs(s, many_tabs()); });
    state(e, "overflow-middle", kTallScreen, [](Stage& s) { tabs(s, many_tabs()).set_active_index(3); });
    // Left from the first caption wraps to the last, and the strip follows.
    state(e, "overflow-end", kTallScreen, [](Stage& s) {
        s.focus(tabs(s, many_tabs()));
        s.app().dispatch(key(Key::Left));
    });
}

// --- Progress -------------------------------------------------------------

widgets::Progress& progress(Stage& stage, double fraction, std::string label = {}, Rect bounds = Rect{1, 1, 23, 1}) {
    ui::View& body = stage.dialog(kDialog, "Progress");
    auto& bar = stage.place(body, bounds, std::make_unique<widgets::Progress>());
    bar.set_fraction(fraction);
    bar.set_label(std::move(label));
    return bar;
}

void add_progress(Catalog& catalog) {
    Element& e = catalog.element("Progress", "include/cvision/widgets/progress.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { progress(s, 0.62, "13 of 21 files"); });
    state(e, "unlabelled", kScreen, [](Stage& s) { progress(s, 0.62); });
    state(e, "empty", kScreen, [](Stage& s) { progress(s, 0.0, "Waiting"); });
    state(e, "complete", kScreen, [](Stage& s) { progress(s, 1.0, "Done"); });
    state(e, "indeterminate", kScreen, [](Stage& s) {
        widgets::Progress& bar = progress(s, 0.0);
        bar.set_indeterminate(true);
        bar.set_pulse(7);
    });
    state(e, "wide", kScreen, [](Stage& s) { progress(s, 0.5, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { progress(s, 0.62, "13 of 21 files", Rect{1, 1, 9, 1}); });
}

}  // namespace

void add_control_specimens(Catalog& catalog) {
    add_label(catalog);
    add_static_text(catalog);
    add_button(catalog);
    add_input_line(catalog);
    add_key_chord_capture(catalog);
    add_memo(catalog);
    add_check_group(catalog);
    add_radio_group(catalog);
    add_combo_box(catalog);
    add_popup_list(catalog);
    add_scrollbar(catalog);
    add_scroll_viewport(catalog);
    add_splitter(catalog);
    add_tab_control(catalog);
    add_progress(catalog);
}

}  // namespace ckv::docgen::appearance
