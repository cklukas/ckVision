// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Editable and clickable controls: the things a form is made of.
#include "widget_shots.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/popup_list.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/static_text.hpp"
#include "widget_stage.hpp"

namespace ckv::docgen {
namespace {

void shot_button(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Confirm", Rect{20, 6, 38, 8});

    // ckvision-doc: button
    auto* save = content.make<widgets::Button>("&Save");
    save->set_bounds(Rect{2, 2, 12, 2});
    save->set_default(true);
    save->on_press = [] { /* run the save command */ };

    auto* cancel = content.make<widgets::Button>("&Cancel");
    cancel->set_bounds(Rect{16, 2, 12, 2});
    cancel->on_press = [] { /* dismiss */ };

    auto* step = content.make<widgets::Button>("+");
    step->set_flat(true);  // one row, no shadow, as wide as its label
    step->set_bounds(Rect{30, 2, 3, 1});
    // ckvision-doc-end: button

    stage.focus(save);
    stage.step();
    stage.save_window(dir, "widget-button");
}

void shot_label(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Connection", Rect{20, 7, 40, 7});

    // ckvision-doc: label
    auto* host = content.make<widgets::InputLine>();
    host->set_bounds(Rect{12, 1, 22, 1});
    host->set_text("db.internal");

    auto* label = content.make<widgets::Label>("&Host name");
    label->set_bounds(Rect{1, 1, 11, 1});
    label->set_buddy(host);  // Alt+H now focuses the field, not the label
    // ckvision-doc-end: label

    stage.focus(host);
    stage.step();
    stage.save_window(dir, "widget-label");
}

void shot_static_text(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("About", Rect{18, 6, 44, 9});

    // ckvision-doc: statictext
    auto* body = content.make<widgets::StaticText>(
        "StaticText wraps a paragraph to its own width and never takes focus. "
        "It is the right view for explanatory copy inside a dialog.");
    body->set_bounds(Rect{1, 1, 40, 4});

    auto* footer = content.make<widgets::StaticText>("Centered footing");
    footer->set_alignment(ui::Alignment::Center);
    footer->set_bounds(Rect{1, 5, 40, 1});
    // ckvision-doc-end: statictext

    stage.step();
    stage.save_window(dir, "widget-statictext");
}

void shot_input_line(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Account", Rect{16, 5, 48, 11});

    // ckvision-doc: inputline
    auto* name = content.make<widgets::InputLine>();
    name->set_bounds(Rect{14, 1, 30, 1});
    name->set_text("Ada Lovelace");

    auto* secret = content.make<widgets::InputLine>();
    secret->set_bounds(Rect{14, 3, 30, 1});
    secret->set_password_echo(true);
    secret->set_text("analytical");

    auto* serial = content.make<widgets::InputLine>();
    serial->set_bounds(Rect{14, 5, 30, 1});
    // '9' a digit, 'A' a letter, '*' anything; everything else is a
    // literal the reader cannot edit and the cursor skips. Setting a
    // mask resets the field to placeholders, so set it before the value.
    serial->set_mask("AAAA-9999-9999");

    auto* port = content.make<widgets::InputLine>();
    port->set_bounds(Rect{14, 7, 30, 1});
    port->set_validator([](const std::string& text) { return text.find_first_not_of("0123456789") == std::string::npos; });
    port->set_text("80a");
    port->set_valid(false);  // draws in the invalid role until it validates
    // ckvision-doc-end: inputline

    // Typed rather than assigned: set_text() bypasses the mask by
    // design, so only real input shows the mask filling up.
    stage.focus(serial);
    for (const char* glyph : {"C", "K", "V", "A", "2", "0", "2", "6", "0", "1"})
        stage.app().dispatch(TextEvent{glyph, false});

    content.make<widgets::Label>("Name")->set_bounds(Rect{1, 1, 12, 1});
    content.make<widgets::Label>("Passphrase")->set_bounds(Rect{1, 3, 12, 1});
    content.make<widgets::Label>("Licence")->set_bounds(Rect{1, 5, 12, 1});
    content.make<widgets::Label>("Port")->set_bounds(Rect{1, 7, 12, 1});
    stage.focus(name);
    stage.step();
    stage.save_window(dir, "widget-inputline");
}

void shot_option_groups(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Build options", Rect{18, 5, 44, 11});

    // ckvision-doc: checkgroup
    auto* flags = content.make<widgets::CheckGroup>(
        std::vector<std::string>{"&Optimize", "&Debug info", "Warnings as errors"});
    flags->set_group_label("Compilation");
    flags->set_bounds(Rect{1, 1, 24, 4});
    flags->set_checked(0, true);
    flags->set_tristate(true);  // admits the third, Mixed state
    flags->set_check_state(2, widgets::CheckState::Mixed);
    flags->on_changed = [](std::size_t index, bool value) { (void)index; (void)value; };
    // ckvision-doc-end: checkgroup

    // ckvision-doc: radiogroup
    auto* target = content.make<widgets::RadioGroup>(
        std::vector<std::string>{"&Static", "S&hared"});
    target->set_group_label("Library");
    target->set_bounds(Rect{26, 1, 14, 3});
    target->set_selected(1);
    target->on_changed = [](int index) { (void)index; };
    // ckvision-doc-end: radiogroup

    stage.focus(flags);
    stage.step();
    stage.save_window(dir, "widget-checkgroup");
    // The radio group's own figure, cut from the same composed screen, so
    // the two are photographed under identical conditions.
    stage.focus(target);
    stage.step();
    stage.save_content(dir, "widget-radiogroup", Rect{26, 0, 15, 5});
}

void shot_combo_box(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Locale", Rect{18, 4, 42, 8});

    // ckvision-doc: combobox
    auto* country = content.make<widgets::ComboBox>(widgets::ComboBoxMode::PickOnly);
    country->set_bounds(Rect{12, 1, 20, 1});
    country->set_items({"Germany", "France", "Japan", "United States"});
    country->set_selected_index(0);

    auto* zone = content.make<widgets::ComboBox>(widgets::ComboBoxMode::Editable);
    zone->set_bounds(Rect{12, 3, 20, 1});
    zone->set_items({"Europe/Berlin", "Europe/Paris", "Asia/Tokyo"});
    zone->set_text("Europe/Berlin");
    // ckvision-doc-end: combobox

    content.make<widgets::Label>("Country")->set_bounds(Rect{1, 1, 10, 1});
    content.make<widgets::Label>("Time zone")->set_bounds(Rect{1, 3, 10, 1});
    stage.focus(country);
    stage.step();
    stage.save_window(dir, "widget-combobox");

    // The open state is the one worth showing: the list is a real popup
    // on the desktop, above the window, not a hole cut in the control.
    country->open_dropdown();
    stage.step();
    stage.save(dir, "widget-combobox-open", Rect{17, 3, 44, 12});
}

void shot_popup_list(const std::filesystem::path& dir) {
    WidgetStage stage;
    stage.window("Editor", Rect{14, 3, 50, 14});

    // ckvision-doc: popuplist
    auto popup = std::make_unique<widgets::PopupList>(
        std::vector<std::string>{"UTF-8", "UTF-16LE", "Latin-1", "Shift-JIS"},
        std::optional<std::size_t>{0});
    popup->set_bounds(Rect{24, 6, 16, 6});
    popup->on_choose = [](std::size_t index) { (void)index; /* apply the encoding */ };
    popup->on_dismiss = [] { /* nothing was chosen */ };
    widgets::PopupList* list = stage.desktop().add<widgets::PopupList>(std::move(popup));
    stage.app().set_focus(&list->list());  // the inner ListView is the focusable part
    // ckvision-doc-end: popuplist

    stage.step();
    stage.save(dir, "widget-popuplist", Rect{20, 4, 30, 10});
}

void shot_progress(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Indexing", Rect{20, 7, 40, 7});

    // ckvision-doc: progress
    auto* bar = content.make<widgets::Progress>();
    bar->set_bounds(Rect{1, 1, 36, 1});
    bar->set_fraction(0.62);
    bar->set_label("1 284 of 2 070 files");

    auto* scanning = content.make<widgets::Progress>();
    scanning->set_bounds(Rect{1, 3, 36, 1});
    scanning->set_indeterminate(true);  // no fraction is known yet
    scanning->set_pulse(7);             // the host advances this per tick
    // ckvision-doc-end: progress

    stage.step();
    stage.save_window(dir, "widget-progress");
}

void shot_slider_and_spin(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Playback", Rect{18, 6, 44, 8});

    // ckvision-doc: slider
    auto* volume = content.make<widgets::Slider>();
    volume->set_bounds(Rect{12, 1, 26, 1});
    volume->set_range(0, 100);
    volume->set_step(5);
    volume->set_value(65);
    volume->on_change = [](int value) { (void)value; };
    // ckvision-doc-end: slider

    // ckvision-doc: spinbox
    auto* speed = content.make<widgets::SpinBox>();
    speed->set_bounds(Rect{12, 3, 10, 1});
    speed->set_range(1, 16);  // set the range BEFORE the value
    speed->set_step(1);
    speed->set_value(4);
    speed->on_change = [](int value) { (void)value; };
    // ckvision-doc-end: spinbox

    content.make<widgets::Label>("Volume")->set_bounds(Rect{1, 1, 10, 1});
    content.make<widgets::Label>("Speed")->set_bounds(Rect{1, 3, 10, 1});
    stage.focus(volume);
    stage.step();
    stage.save_window(dir, "widget-slider");
    stage.save_content(dir, "widget-spinbox", Rect{1, 3, 22, 1});
}

void shot_pickers(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Appointment", Rect{18, 6, 44, 8});

    // ckvision-doc: datepicker
    auto* date = content.make<widgets::DatePicker>();
    date->set_bounds(Rect{12, 1, 13, 1});
    date->set_value(widgets::DateValue{2026, 8, 9});
    date->on_change = [](std::optional<widgets::DateValue> value) { (void)value; };
    // ckvision-doc-end: datepicker

    // ckvision-doc: timepicker
    auto* time = content.make<widgets::TimePicker>();
    time->set_bounds(Rect{12, 3, 12, 1});
    time->set_value(widgets::TimeValue{14, 30, 0});
    time->set_show_seconds(true);
    time->set_24_hour(true);
    time->on_change = [](widgets::TimeValue value) { (void)value; };
    // ckvision-doc-end: timepicker

    content.make<widgets::Label>("Date")->set_bounds(Rect{1, 1, 10, 1});
    content.make<widgets::Label>("Time")->set_bounds(Rect{1, 3, 10, 1});
    stage.focus(date);
    stage.step();
    stage.save_content(dir, "widget-datepicker", Rect{1, 1, 25, 1});
    stage.focus(time);
    stage.step();
    stage.save_content(dir, "widget-timepicker", Rect{1, 3, 25, 1});
}

void shot_search_box(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Contacts", Rect{20, 7, 40, 6});

    // ckvision-doc: searchbox
    auto* search = content.make<widgets::SearchBox>();
    search->set_bounds(Rect{1, 1, 34, 1});
    search->set_query("lovelace");
    search->on_change = [](const std::string& query) { (void)query; /* filter the model */ };
    search->on_clear = [] { /* show everything again */ };
    // ckvision-doc-end: searchbox

    stage.focus(search);
    stage.step();
    stage.save_window(dir, "widget-searchbox");
}

}  // namespace

void capture_control_shots(const std::filesystem::path& dir) {
    shot_button(dir);
    shot_label(dir);
    shot_static_text(dir);
    shot_input_line(dir);
    shot_option_groups(dir);
    shot_combo_box(dir);
    shot_popup_list(dir);
    shot_progress(dir);
    shot_slider_and_spin(dir);
    shot_pickers(dir);
    shot_search_box(dir);
}

}  // namespace ckv::docgen
