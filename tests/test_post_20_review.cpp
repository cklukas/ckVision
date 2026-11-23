// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <memory>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/key.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/sixel_decoder.hpp"
#include "cvision/term/sixel_encoder.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/dock.hpp"
#include "cvision/ui/grid.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/overlay.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/check_box.hpp"
#include "cvision/widgets/context_menu.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/expander.hpp"
#include "cvision/widgets/group_box.hpp"
#include "cvision/widgets/info_bar.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/search_box.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/link.hpp"
#include "cvision/widgets/property_inspector.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/separator.hpp"
#include "cvision/widgets/slider.hpp"
#include "cvision/widgets/spin_box.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/toast.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/window.hpp"

namespace {

CK_TEST(attr_and_modifier_bitwise_operators_work) {
    ckv::Attr attrs = ckv::Attr::None;
    attrs |= ckv::Attr::Bold | ckv::Attr::Italic;
    CK_CHECK(ckv::has_attr(attrs, ckv::Attr::Bold));
    CK_CHECK(ckv::has_attr(attrs, ckv::Attr::Italic));
    attrs &= ~ckv::Attr::Bold;
    CK_CHECK(!ckv::has_attr(attrs, ckv::Attr::Bold));
    CK_CHECK(ckv::has_attr(attrs, ckv::Attr::Italic));
    attrs ^= ckv::Attr::Underline;
    CK_CHECK(ckv::has_attr(attrs, ckv::Attr::Underline));

    ckv::Modifier mods = ckv::Modifier::None;
    mods |= ckv::Modifier::Ctrl | ckv::Modifier::Shift;
    CK_CHECK(ckv::has_modifier(mods, ckv::Modifier::Ctrl));
    mods &= ~ckv::Modifier::Shift;
    CK_CHECK(!ckv::has_modifier(mods, ckv::Modifier::Shift));
}

CK_TEST(key_chord_escape_alias_and_shift_letter_matching) {
    const auto esc = ckv::KeyChord::parse("Escape");
    CK_CHECK(esc.has_value());
    CK_CHECK(esc->key == ckv::Key::Escape);

    ckv::ui::CommandRegistry registry;
    const auto id_global = registry.declare(
        {.key = "test.global", .title = "Global", .category = "Edit", .chord = "Shift+A"});
    const auto id_scoped = registry.declare(
        {.key = "test.scoped", .title = "Scoped", .category = "Edit", .chord = ""});
    const ckv::KeyChord kitty_shift_a{ckv::Key::Char, ckv::Modifier::Shift, "A"};
    CK_CHECK(registry.command_for_key(kitty_shift_a) == id_global);

    const auto ctrl_f = *ckv::KeyChord::parse("Ctrl+F");
    registry.bind_key(ctrl_f, id_scoped, "editor");
    CK_CHECK(!registry.command_for_key(ctrl_f).has_value());
    CK_CHECK(registry.command_for_key(ctrl_f, {"editor.text"}) == id_scoped);
}

CK_TEST(image_content_id_is_deterministic_across_equal_images) {
    ckv::Image a(ckv::PixelSize{3, 2});
    ckv::Image b(ckv::PixelSize{3, 2});
    CK_CHECK(a.content_id() != 0);
    CK_CHECK(a.content_id() == b.content_id());
    a.set_pixel(1, 1, ckv::Image::Rgba{10, 20, 30, 255});
    CK_CHECK(a.content_id() != b.content_id());
    b.set_pixel(1, 1, ckv::Image::Rgba{10, 20, 30, 255});
    CK_CHECK(a.content_id() == b.content_id());
}

CK_TEST(compositor_repairs_wide_glyph_when_occluder_splits_it) {
    ckv::scene::Surface bg(ckv::Size{6, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    bg.set_cell(ckv::Point{1, 0}, ckv::Cell::from_grapheme("中", ckv::Style{}));
    bg.set_cell(ckv::Point{2, 0}, ckv::Cell::continuation(ckv::Style{}));

    ckv::scene::Surface overlay(ckv::Size{2, 1}, ckv::Cell::from_grapheme("X", ckv::Style{}));
    ckv::scene::Compositor comp(ckv::Size{6, 2});
    comp.compose({}, bg, {});
    CK_CHECK(comp.frame().at(ckv::Point{1, 0}).grapheme() == "中");

    // Cover only the continuation cell at x=2: lead cell x=1 must become space.
    comp.compose({ckv::scene::Layer{1, &overlay, ckv::Point{2, 0}, false, std::nullopt}}, bg, {});
    CK_CHECK(comp.frame().at(ckv::Point{1, 0}).grapheme() == " ");
    CK_CHECK(comp.frame().at(ckv::Point{2, 0}).grapheme() == "X");

    // Move overlay away: wide glyph at x=1..2 must restore cleanly.
    comp.compose({}, bg, {});
    CK_CHECK(comp.frame().at(ckv::Point{1, 0}).grapheme() == "中");
    CK_CHECK(comp.frame().at(ckv::Point{2, 0}).is_continuation());
}

CK_TEST(input_decoder_bounds_unterminated_osc_and_aborts_on_can) {
    ckv::term::InputDecoder decoder(ckv::term::baseline_capabilities());
    // Unterminated OSC aborted by CAN (0x18) followed by 'a' must still decode 'a'.
    auto ev1 = decoder.feed("\x1B]0;unterminated\x18" "a", 0);
    CK_CHECK(!ev1.empty());
    // Overlong unterminated OSC (>4096 bytes) must not stall subsequent input.
    std::string junk = "\x1B]0;" + std::string(4200, 'x') + "b";
    auto ev2 = decoder.feed(junk, 0);
    CK_CHECK(!ev2.empty());
}

CK_TEST(sixel_decoder_supports_p2_2_and_hls_colors) {
    ckv::term::SixelPalette palette;
    std::string error;
    const auto res = ckv::term::decode_sixel("0;2;0q#1;1;120;50;100#1~", ckv::PixelSize{16, 16},
                                             1024, palette, error);
    CK_CHECK(res.has_value());
    CK_CHECK(!res->erases_background);
    CK_CHECK(res->image.width() >= 1 && res->image.height() >= 6);
    const auto px = res->image.pixel(0, 0);
    CK_CHECK(px.g == 255 && px.r == 0 && px.b == 0);
}

CK_TEST(presenter_respects_decstbm_scroll_capability_gate) {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.decstbm_scroll = false;
    ckv::term::HeadlessTerminal term(ckv::Size{10, 4}, caps);
    ckv::term::Presenter presenter(term);
    ckv::scene::Surface s(ckv::Size{10, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    presenter.present(s.view(), {}, 0);
    const std::size_t before = term.written_bytes().size();
    s.set_cell(ckv::Point{0, 3}, ckv::Cell::from_grapheme("Z", ckv::Style{}));
    presenter.present(s.view(), {}, 0, {}, ckv::ScrollIntent{ckv::Rect{0, 0, 10, 4}, 1});
    const std::string_view out = term.written_bytes().substr(before);
    CK_CHECK(out.find("\x1B[1;4r") == std::string::npos);
}

CK_TEST(layout_padding_spec_mutation_and_shrink_remainder) {
    ckv::ui::Row row(ckv::Rect{0, 0, 12, 5});
    row.set_padding(1, 1);
    auto* a = row.add_item(std::make_unique<ckv::ui::View>(),
                           ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Expanding, 1, ckv::ui::Alignment::Fill, 0, 0});
    auto* b = row.add_item(std::make_unique<ckv::ui::View>(),
                           ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Expanding, 1, ckv::ui::Alignment::Fill, 0, 0});
    CK_CHECK(a->bounds().x == 1 && a->bounds().y == 1);
    CK_CHECK(a->bounds().width + b->bounds().width == 10);

    std::vector<ckv::ui::LayoutChild> items = {
        {1, 10, 20, ckv::ui::SizePolicy::Expanding, 1},
        {8, 10, 20, ckv::ui::SizePolicy::Expanding, 1},
    };
    const auto distributed = ckv::ui::distribute_main_axis(items, 15, 0);
    CK_CHECK(distributed[0].second + distributed[1].second == 15);
    CK_CHECK(distributed[1].second >= 8);
}

CK_TEST(scroll_viewport_follows_focus_and_horizontal_wheel) {
    ckv::term::HeadlessTerminal term(ckv::Size{30, 10});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* vp = app.root().add(std::make_unique<ckv::widgets::ScrollViewport>());
    vp->set_bounds(ckv::Rect{0, 0, 12, 4});
    auto col = std::make_unique<ckv::ui::Column>();
    ckv::widgets::Button* last_btn = nullptr;
    for (int i = 0; i < 6; ++i) {
        last_btn = col->add_item(std::make_unique<ckv::widgets::Button>(
            "WideButton_" + std::to_string(i) + "_Overflowing_12_Cols"));
    }
    vp->set_content(std::move(col));
    app.step(0);
    CK_CHECK(vp->scroll_y() == 0);
    app.set_focus(last_btn);
    app.step(0);
    CK_CHECK(vp->scroll_y() > 0);
    CK_CHECK(vp->on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelRight,
                                          ckv::Point{2, 2}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(vp->scroll_x() > 0);
}

CK_TEST(theme_fallback_and_new_widgets_behave_cleanly) {
    ckv::ui::RoleRegistry reg;
    const auto roles = ckv::ui::intern_standard_roles(reg);
    const auto custom = reg.intern("custom.role", ckv::Style{});
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(reg, roles);
    theme.set_fallback(custom, roles.button_normal);
    CK_CHECK(theme.resolve(custom) == theme.resolve(roles.button_normal));

    ckv::term::HeadlessTerminal term(ckv::Size{30, 10});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* group = app.root().add(std::make_unique<ckv::widgets::GroupBox>("Options"));
    auto* col = group->set_content(std::make_unique<ckv::ui::Column>());
    auto* cb = col->add_item(std::make_unique<ckv::widgets::CheckBox>("&Enable"));
    col->add_item(std::make_unique<ckv::widgets::Separator>());
    col->add_item(std::make_unique<ckv::widgets::Spacer>(1, 1));
    app.step(0);
    CK_CHECK(!cb->is_checked());
    cb->toggle();
    CK_CHECK(cb->is_checked());
}

CK_TEST(dialog_result_keyed_accessors_work) {
    ckv::widgets::DialogResult r;
    r.accepted = true;
    r.values = {"alice", ""};
    r.checked = {false, true};
    r.selected = {-1, -1};
    r.numbers = {std::nullopt, std::nullopt};
    r.field_keys = {"name", "agree"};
    CK_CHECK(r.text("name") == "alice");
    CK_CHECK(r.is_checked("agree"));
    CK_CHECK(!r.index_of("missing").has_value());
}

CK_TEST(wp106_posix_filesystem_and_input_decoder_fixes) {
    ckv::term::PosixFileSystem pfs;
    CK_CHECK(pfs.normalize_path(".") == ".");
    CK_CHECK(pfs.join(".", "foo") == "./foo");
    CK_CHECK(pfs.join("subdir/", "bar") == "subdir/bar");

    ckv::term::Capabilities paste_caps;
    paste_caps.bracketed_paste = true;
    ckv::term::InputDecoder dec(paste_caps);
    // DCS payload must not leak as Alt+P key events.
    const auto dcs_events = dec.feed("\x1BP1$r0m\x1B\\", 0);
    CK_CHECK(dcs_events.empty());

    // Bracketed paste sanitizes CR per D-040 while preserving LF.
    auto paste_events = dec.feed("\x1B[200~hello\r\nworld\x1B[201~", 0);
    auto flushed = dec.poll_timeout(1'000'000'000);
    paste_events.insert(paste_events.end(), flushed.begin(), flushed.end());
    CK_CHECK(paste_events.size() == 1);
    const auto* pe = std::get_if<ckv::TextEvent>(&paste_events[0]);
    CK_CHECK(pe != nullptr && pe->from_paste && pe->text == "hello\xEF\xBF\xBD\nworld");
}

CK_TEST(wp108_wp109_layout_focus_and_controls) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 14});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);

    // Hidden view cannot receive key events even if focused.
    bool btn_pressed = false;
    auto* btn = app.root().add(std::make_unique<ckv::widgets::Button>("OK"));
    btn->on_press = [&] { btn_pressed = true; };
    btn->set_bounds(ckv::Rect{0, 0, 8, 1});
    app.set_focus(btn);
    CK_CHECK(btn->has_focus());
    btn->set_visible(false);
    term.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, {}}});
    app.step(0);
    CK_CHECK(!btn_pressed);

    // Grid weighted/fixed tracks + padding.
    auto* grid = app.root().add(std::make_unique<ckv::ui::Grid>(ckv::Rect{0, 0, 22, 6}, 1, 2));
    grid->set_padding(1, 0);
    grid->set_column_specs({{ckv::ui::TrackKind::Fixed, 5}, {ckv::ui::TrackKind::Weight, 1}});
    auto* left_lbl = grid->make_item<ckv::widgets::Label>(ckv::ui::GridSpec{0, 0}, "L");
    auto* right_lbl = grid->make_item<ckv::widgets::Label>(ckv::ui::GridSpec{0, 1}, "R");
    CK_CHECK(left_lbl->bounds().x == 1);
    CK_CHECK(left_lbl->bounds().width <= 5);
    CK_CHECK(right_lbl->bounds().x == 6);

    // Dock & Overlay size hints.
    ckv::ui::Dock dock;
    dock.make_item<ckv::widgets::Label>(ckv::ui::DockEdge::Top, "Header");
    CK_CHECK(dock.vertical_size_hint().preferred >= 1);
    ckv::ui::Overlay overlay;
    overlay.make_item<ckv::widgets::Label>(ckv::ui::OverlayMode::Fill, "Layer");
    CK_CHECK(overlay.horizontal_size_hint().preferred >= 5);

    // Slider & SpinBox new getters and hints.
    ckv::widgets::Slider slider;
    slider.set_orientation(ckv::widgets::Orientation::Vertical);
    slider.set_range(10, 50);
    slider.set_step(5);
    CK_CHECK(slider.minimum() == 10 && slider.maximum() == 50 && slider.step() == 5);
    CK_CHECK(slider.horizontal_size_hint().preferred == 1);

    ckv::widgets::SpinBox spin;
    spin.set_step(2);
    CK_CHECK(spin.step() == 2);
    CK_CHECK(spin.horizontal_size_hint().preferred >= 5);

    // TabControl auto-advances when disabling active tab.
    ckv::widgets::TabControl tabs;
    tabs.add_tab("One", std::make_unique<ckv::widgets::Label>("1"));
    tabs.add_tab("Two", std::make_unique<ckv::widgets::Label>("2"));
    CK_CHECK(tabs.active_index() == 0);
    tabs.set_tab_enabled(0, false);
    CK_CHECK(tabs.active_index() == 1);

    // PropertyInspector vertical scroll.
    ckv::widgets::PropertyInspector insp;
    insp.set_bounds(ckv::Rect{0, 0, 20, 2});
    std::vector<ckv::widgets::PropertyItem> items;
    for (int i = 0; i < 6; ++i) items.push_back({"K" + std::to_string(i), "V"});
    insp.set_items(std::move(items));
    insp.set_scroll_y(3);
    CK_CHECK(insp.scroll_y() == 3);
}

CK_TEST(wp110_new_widgets_behave_cleanly) {
    ckv::term::HeadlessTerminal term(ckv::Size{48, 16});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);

    bool action_fired = false;
    bool link_fired = false;
    auto* desk = app.root().add(std::make_unique<ckv::widgets::Desktop>());
    desk->set_bounds(ckv::Rect{0, 0, 48, 16});

    auto* info = desk->make<ckv::widgets::InfoBar>("Warn", ckv::widgets::NoticeSeverity::Warning);
    info->set_action_label("Fix");
    info->on_action = [&] { action_fired = true; };
    info->set_bounds(ckv::Rect{0, 0, 40, 1});

    auto* exp = desk->make<ckv::widgets::Expander>("Details", false);
    exp->set_content(std::make_unique<ckv::widgets::Label>("Inner"));
    exp->set_bounds(ckv::Rect{0, 2, 30, 3});
    CK_CHECK(!exp->expanded());
    exp->toggle();
    CK_CHECK(exp->expanded());

    auto* link = desk->make<ckv::widgets::Link>("Docs", "https://example.invalid");
    link->on_activate = [&] { link_fired = true; };
    link->set_bounds(ckv::Rect{0, 6, 10, 1});

    ckv::widgets::Toast* toast = ckv::widgets::show_toast(
        app, *desk, "Done", ckv::widgets::NoticeSeverity::Success, 1'000'000'000LL);
    CK_CHECK(toast != nullptr);

    ckv::widgets::ContextMenu standalone_menu({ckv::widgets::MenuItem::action("Run", [&] { action_fired = true; })});
    standalone_menu.show(app, *desk, ckv::Point{2, 2});
    CK_CHECK(standalone_menu.is_open());

    app.step(0);
    app.set_focus(info);
    term.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, {}}});
    app.step(0);
    CK_CHECK(action_fired);
    app.set_focus(link);
    term.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, {}}});
    app.step(0);
    CK_CHECK(link_fired);
    clock.advance(2'000'000'000LL);
    app.step(0);

    // Dialog FieldKind::Password, Slider, SpinBox materialization.
    ckv::widgets::DialogDescriptor desc;
    desc.title = "Settings";
    ckv::widgets::FieldDescriptor f0;
    f0.kind = ckv::widgets::FieldKind::Password;
    f0.label = "Secret";
    f0.key = "secret";
    ckv::widgets::FieldDescriptor f1;
    f1.kind = ckv::widgets::FieldKind::Slider;
    f1.label = "Level";
    f1.minimum = 0.0;
    f1.maximum = 10.0;
    f1.initial_text = "4";
    f1.key = "level";
    ckv::widgets::FieldDescriptor f2;
    f2.kind = ckv::widgets::FieldKind::SpinBox;
    f2.label = "Count";
    f2.minimum = 1.0;
    f2.maximum = 9.0;
    f2.initial_text = "3";
    f2.key = "count";
    desc.fields = {std::move(f0), std::move(f1), std::move(f2)};
    desc.buttons = {{.label = "OK", .role = ckv::widgets::ButtonRole::Accept}};
    auto mat = ckv::widgets::materialize_dialog(desc);
    CK_CHECK(mat.inputs[0] != nullptr);
    CK_CHECK(mat.sliders[1] != nullptr && mat.sliders[1]->value() == 4);
    CK_CHECK(mat.spin_boxes[2] != nullptr && mat.spin_boxes[2]->value() == 3);
}

CK_TEST(wp111_i18n_rtl_and_accessibility) {
    ckv::term::HeadlessTerminal term(ckv::Size{30, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    struct StringsObserver final : ckv::ui::View {
        int notified = 0;
        void on_strings_changed() override { ++notified; }
    };
    auto* obs = app.root().add(std::make_unique<StringsObserver>());
    obs->set_accessible_role(ckv::ui::AccessibleRole::Panel);
    obs->set_accessible_name("MainPanel");
    CK_CHECK(obs->accessible_info().role == ckv::ui::AccessibleRole::Panel);
    CK_CHECK(obs->accessible_info().name == "MainPanel");

    ckv::ui::UiStrings custom;
    custom.search_box_prompt = "Find ";
    app.set_strings(custom);
    CK_CHECK(obs->notified == 1);

    app.set_layout_direction(ckv::ui::LayoutDirection::Rtl);
    CK_CHECK(app.layout_direction() == ckv::ui::LayoutDirection::Rtl);
    app.announce("Ready");
    CK_CHECK(app.last_announcement() == "Ready");
}

CK_TEST(wp116_notice_roles_context_menu_and_password_security) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 16});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto* desktop = app.root().make<ckv::widgets::Desktop>();
    desktop->set_bounds(ckv::Rect{0, 0, 60, 16});

    // 1. NoticeSeverity roles are interned and visually distinct in themes.
    const auto& roles = app.standard_roles();
    CK_CHECK(roles.notice_info != ckv::ui::kInvalidRole);
    CK_CHECK(roles.notice_success != ckv::ui::kInvalidRole);
    CK_CHECK(roles.notice_warning != ckv::ui::kInvalidRole);
    CK_CHECK(roles.notice_error != ckv::ui::kInvalidRole);
    CK_CHECK(app.theme().resolve(roles.notice_info) != app.theme().resolve(roles.notice_error));
    CK_CHECK(app.theme().resolve(roles.notice_success) != app.theme().resolve(roles.notice_warning));

    // 2. Toasts stack vertically and cast shadow.
    auto* t1 = ckv::widgets::show_toast(app, *desktop, "First", ckv::widgets::NoticeSeverity::Info, 0);
    auto* t2 = ckv::widgets::show_toast(app, *desktop, "Second", ckv::widgets::NoticeSeverity::Error, 0);
    CK_CHECK(t1 != nullptr && t2 != nullptr);
    CK_CHECK(t1->casts_shadow());
    CK_CHECK(t2->bounds().y < t1->bounds().y);

    // 3. ContextMenu opens with capture, restores focus, and runs item action.
    auto* btn = desktop->make<ckv::widgets::Button>("Target");
    btn->set_bounds(ckv::Rect{1, 1, 12, 2});
    app.set_focus(btn);
    bool copied = false;
    bool dismissed = false;
    ckv::widgets::ContextMenu menu({ckv::widgets::MenuItem::action("Copy", [&] { copied = true; })});
    menu.on_dismiss = [&] { dismissed = true; };
    menu.show(app, *desktop, ckv::Point{4, 4});
    CK_CHECK(menu.is_open());
    CK_CHECK(app.input_capture() != nullptr);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    CK_CHECK(copied);
    CK_CHECK(dismissed);
    CK_CHECK(!menu.is_open());
    CK_CHECK(app.focused() == btn);

    // 4. Password InputLine never commits to history or copies to clipboard.
    auto* pw = desktop->make<ckv::widgets::InputLine>();
    pw->set_bounds(ckv::Rect{1, 4, 20, 1});
    pw->set_password_echo(true);
    pw->set_history_key("pw_hist");
    pw->set_text("s3cr3t");
    pw->commit_to_history();
    CK_CHECK(app.history().entries("pw_hist").empty());
    pw->on_focus(ckv::FocusEvent{true});
    CK_CHECK(!pw->copy_selection_to_clipboard());
    CK_CHECK(app.clipboard_text().empty());
}

CK_TEST(wp117_sixel_alpha_input_apc_and_compositor_reshow) {
    // 1. SixelEncoder alpha-composites against background when preserve_transparent_holes == true.
    ckv::Image img(ckv::PixelSize{2, 6});
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 2; ++x) img.set_pixel(x, y, ckv::Image::Rgba{255, 0, 0, 128});
    }
    ckv::term::SixelEncodeOptions opts;
    opts.max_color_registers = 16;
    opts.preserve_transparent_holes = true;
    opts.background = ckv::Color::rgb(0, 0, 255);
    const std::string sixel = ckv::term::encode_sixel(img, opts);
    CK_CHECK(sixel.find(";0;0;100") == std::string::npos);

    // 2. InputDecoder::drain() consumes aborted APC payloads without leaking text.
    ckv::term::InputDecoder decoder;
    const auto evs = decoder.feed("\x1B_Gabc\x18xyz", 1000);
    CK_CHECK(evs.size() == 3);  // 'x', 'y', 'z' only — 'abc' was consumed with CAN

    // 3. Compositor::compute_damage() damages newly shown clean layers.
    ckv::scene::Compositor comp(ckv::Size{20, 10});
    ckv::scene::Surface bg(ckv::Size{20, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Surface layer_surf(ckv::Size{6, 3}, ckv::Cell::from_grapheme("X", ckv::Style{}));
    layer_surf.clear_damage();  // Simulate clean surface re-shown later
    comp.compose({{1, &layer_surf, ckv::Point{2, 2}, false, std::nullopt}}, bg, ckv::scene::ShadowSpec{});
    CK_CHECK(!comp.frame().row_damage(3).empty());
    CK_CHECK(comp.frame().at(ckv::Point{3, 3}).grapheme() == "X");
}

CK_TEST(wp118_row_rtl_grid_auto_table_treeview_multiselect_and_i18n) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 12});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    app.root().set_bounds(ckv::Rect{0, 0, 40, 12});

    // 1. Live Row RTL layout propagation.
    auto* row = app.root().make<ckv::ui::Row>();
    auto* left_lbl = row->make_item<ckv::widgets::Label>(
        ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 4}, "A");
    auto* right_lbl = row->make_item<ckv::widgets::Label>(
        ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 4}, "B");
    row->set_bounds(ckv::Rect{0, 0, 20, 2});
    CK_CHECK(left_lbl->bounds().x < right_lbl->bounds().x);
    app.set_layout_direction(ckv::ui::LayoutDirection::Rtl);
    CK_CHECK(left_lbl->bounds().x > right_lbl->bounds().x);

    // 2. Grid TrackKind::Auto.
    auto* grid = app.root().make<ckv::ui::Grid>(ckv::Rect{0, 2, 20, 2}, 1, 2);
    auto* g0 = grid->make_item<ckv::widgets::Label>(ckv::ui::GridSpec{0, 0}, "Auto6!");
    auto* g1 = grid->make_item<ckv::widgets::Label>(ckv::ui::GridSpec{0, 1}, "Fill");
    (void)g1;
    grid->set_column_specs({{ckv::ui::TrackKind::Auto, 0}, {ckv::ui::TrackKind::Weight, 1}});
    CK_CHECK(g0->bounds().width == 6);

    // 3. TreeView::set_roots() clears selected_nodes_ safely; Table Shift+Down multi-selects.
    auto* tree = app.root().make<ckv::widgets::TreeView>();
    tree->set_multi_select(true);
    tree->set_bounds(ckv::Rect{0, 4, 20, 4});
    ckv::widgets::TreeNode r1; r1.label = "R1";
    ckv::widgets::TreeNode r2; r2.label = "R2";
    tree->set_roots({std::move(r1), std::move(r2)});
    app.set_focus(tree);
    ckv::MouseEvent click;
    click.action = ckv::MouseAction::Down;
    click.button = ckv::MouseButton::Left;
    click.cell = ckv::Point{1, 4};
    app.dispatch(click);
    CK_CHECK(tree->selected_nodes().size() == 1);
    ckv::widgets::TreeNode r_new; r_new.label = "New";
    tree->set_roots({std::move(r_new)});
    CK_CHECK(tree->selected_nodes().empty());

    auto* table = app.root().make<ckv::widgets::Table>();
    table->set_columns({{"Name", 10}});
    table->set_rows({{"r0"}, {"r1"}, {"r2"}});
    table->set_multi_select(true);
    table->set_bounds(ckv::Rect{20, 4, 20, 5});
    app.set_focus(table);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Down, ckv::Modifier::Shift, ""}});
    CK_CHECK(table->selected_rows().size() == 2);

    // 4. SearchBox updates prompt_columns_ on set_strings() and Button gets AccessibleRole::Button.
    auto* sb = app.root().make<ckv::widgets::SearchBox>();
    sb->set_bounds(ckv::Rect{0, 8, 20, 1});
    ckv::ui::UiStrings s;
    s.search_box_prompt = "Find: ";
    app.set_strings(s);
    CK_CHECK(sb->prompt() == "Find: ");
    auto* b = app.root().make<ckv::widgets::Button>("Go");
    CK_CHECK(b->accessible_info().role == ckv::ui::AccessibleRole::Button);
    CK_CHECK(b->accessible_info().name == "Go");
}

}  // namespace
