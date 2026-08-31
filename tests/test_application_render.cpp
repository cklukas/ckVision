// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The render pipeline (View tree -> off-screen Surface -> Presenter ->
// Terminal bytes) that Application::step() now drives — before this,
// step() only dispatched input; nothing anywhere ever painted a View
// into terminal output. These tests prove bytes actually reach the
// terminal, that they reflect real content changes, that resize is
// handled, and that an invisible/unchanged tree costs nothing.
#include "cvision/ui/application.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/window.hpp"

#include <optional>
#include <string_view>

using ckv::ManualClock;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Label;
using ckv::widgets::Desktop;
using ckv::widgets::DropdownMenu;
using ckv::widgets::MenuItem;
using ckv::widgets::Window;

namespace {
class ColorProbe final : public ckv::ui::View {
public:
    void draw(ckv::scene::Painter& painter) override {
        painter.draw_text(ckv::Point{0, 0}, "C",
                          ckv::Style{ckv::Color::rgb(17, 34, 51), ckv::Color::rgb(68, 85, 102), ckv::Attr{}});
    }
};

class CountingPopup final : public ckv::ui::View {
public:
    void draw(ckv::scene::Painter& painter) override {
        ++draw_count;
        painter.draw_text(ckv::Point{0, 0}, "P", ckv::Style{});
    }

    void change_content() { invalidate(); }

    int draw_count = 0;
};

// Alternating glyphs and colours make a one-column translation change every
// visible content cell. It is the deterministic text analogue of a process
// monitor: many independently styled fields move together with the window,
// so each intermediate position is a genuinely large terminal frame.
class DenseTextProbe final : public ckv::ui::View {
public:
    void draw(ckv::scene::Painter& painter) override {
        constexpr int kWidth = 68;
        constexpr int kHeight = 18;
        for (int row = 0; row < kHeight; ++row) {
            for (int column = 0; column < kWidth; ++column) {
                const bool alternate = ((row + column) & 1) != 0;
                const ckv::Style style{
                    alternate ? ckv::Color::rgb(40, 210, 120)
                              : ckv::Color::rgb(230, 170, 30),
                    ckv::Color::rgb(0, 0, 0), ckv::Attr{}};
                painter.draw_text(ckv::Point{column, row}, alternate ? "A" : "B", style);
            }
        }
    }
};

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 12}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    Fixture() { app.theme() = make_classic_theme(app.roles(), roles); }
};
}  // namespace

CK_TEST(the_very_first_step_paints_and_presents_even_with_no_input) {
    // dirty_ starts true specifically so an application that builds its
    // whole UI before the first step() still gets an initial frame.
    Fixture f;
    CK_CHECK(f.term.written_bytes().empty());
    f.app.step(0);
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(a_second_step_with_nothing_invalidated_writes_no_further_bytes) {
    Fixture f;
    f.app.step(0);
    f.term.clear_written();
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().empty());
}

CK_TEST(a_live_color_scheme_capability_change_forces_a_full_represent) {
    Fixture f;
    f.app.step(0);
    f.term.clear_written();

    ckv::term::Capabilities updated = f.term.capabilities();
    updated.color_scheme = ckv::term::ColorScheme::Dark;
    updated.color_scheme_notifications = true;
    f.term.inject_capability_change(updated);

    CK_CHECK(f.app.step(0));
    // A scheme change can alter every resolved role. Application must reset
    // the presenter rather than trusting an unchanged cell diff, so the
    // terminal receives a complete current frame even for an otherwise idle
    // view tree.
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(a_runtime_color_depth_downgrade_reencodes_the_entire_application_frame) {
    ckv::term::Capabilities truecolor = ckv::term::baseline_capabilities();
    truecolor.color_depth = ckv::term::ColorDepth::TrueColor;
    ckv::term::HeadlessTerminal term{ckv::Size{40, 12}, truecolor};
    ManualClock clock;
    Application app{term, clock};
    auto probe = std::make_unique<ColorProbe>();
    probe->set_bounds(Rect{0, 0, 1, 1});
    app.root().add_child(std::move(probe));

    app.step(0);
    CK_CHECK(term.written_bytes().find("38;2;17;34;51") != std::string::npos);
    CK_CHECK(term.written_bytes().find("48;2;68;85;102") != std::string::npos);
    term.clear_written();

    ckv::term::Capabilities mono = truecolor;
    mono.color_depth = ckv::term::ColorDepth::Mono16;
    term.inject_capability_change(mono);
    CK_CHECK(app.step(0));
    // The application's capability-change route invalidates Presenter before
    // it paints. An unchanged view must therefore be re-encoded under the
    // downgraded palette instead of retaining truecolor bytes in the host.
    CK_CHECK(term.written_bytes().find("38;2;") == std::string::npos);
    CK_CHECK(term.written_bytes().find("48;2;") == std::string::npos);
}

CK_TEST(a_label_actually_renders_its_text_into_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Hello");
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Hello") != std::string::npos);
}

CK_TEST(setting_new_text_repaints_so_the_old_text_no_longer_appears_in_the_next_frame) {
    Fixture f;
    auto owned = std::make_unique<Label>("Old");
    Label* label = owned.get();
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(owned));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Old") != std::string::npos);

    label->set_text("New");
    f.app.step(0);
    // The Presenter diffs cell-by-cell, so "New" must appear even
    // though nothing ever calls a bespoke "clear" — the diff against
    // the previously presented frame is what erases "Old"'s cells.
    CK_CHECK(f.term.written_bytes().find("New") != std::string::npos);
}

CK_TEST(a_hidden_view_never_reaches_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Secret");
    label->set_bounds(Rect{2, 2, 10, 1});
    label->set_visible(false);
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Secret") == std::string::npos);
}

CK_TEST(a_resize_event_grows_the_backing_surface_and_forces_a_full_repaint) {
    Fixture f;
    auto label = std::make_unique<Label>("Grow");
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));
    f.app.step(0);

    f.term.resize(ckv::Size{60, 20});
    f.app.step(0);
    // The label is still visible after growing the terminal — proves
    // the off-screen Surface was actually resized to match, not left
    // at the old (now too-small) size where painting it would be UB.
    CK_CHECK(f.term.written_bytes().find("Grow") != std::string::npos);
    CK_CHECK(f.app.root().bounds().width == 60);
    CK_CHECK(f.app.root().bounds().height == 20);
}

CK_TEST(moving_a_window_recomposes_its_retained_backing_without_content_repaint) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto window_owned = std::make_unique<Window>("Retained");
    window_owned->set_bounds(Rect{2, 2, 14, 6});
    Window* window = desktop->add_window(std::move(window_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    const std::size_t original_repaints = window->content_repaint_count();
    CK_CHECK(original_repaints == 1);

    window->set_bounds(Rect{18, 2, 14, 6});  // same local size: composition-only movement
    f.term.clear_written();
    f.app.step(0);

    CK_CHECK(window->content_repaint_count() == original_repaints);
    CK_CHECK(desktop->last_content_repaints() == 0);
    // Two 14x6 positions plus their non-overlapping 2x1 shadow footprints
    // are the whole possible structural damage: this is deliberately far
    // below the 40x12 desktop and catches a hidden base/full-frame repaint.
    CK_CHECK(f.app.last_compose_cells_touched() <= 216);
    CK_CHECK(f.app.last_bytes_emitted() > 0);
    // A full 40x12 repaint is far larger. This captures the Presenter side
    // of the move budget as well as the compositor cells-touched bound.
    // The separately colored close and maximize/restore glyphs each add a
    // truecolor transition to a title row, and the resize grips add two more
    // to the bottom border. 2180 preserves the same "far below a full
    // repaint" guard while accounting for those deliberately visible
    // controls -- note the cells-touched bound above is unchanged, so the
    // extra bytes are colour transitions and not a wider repaint.
    CK_CHECK(f.app.last_bytes_emitted() <= 2180);
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(dragging_a_window_recomposes_without_repainting_its_retained_content) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto window_owned = std::make_unique<Window>("Dragged");
    window_owned->set_bounds(Rect{2, 2, 14, 6});
    Window* window = desktop->add_window(std::move(window_owned));
    f.app.root().add_child(std::move(desktop_owned));
    f.app.step(0);
    const std::size_t original_repaints = window->content_repaint_count();

    // Grab the title bar on a column that carries no frame control: the
    // close control occupies local columns 2-4 and the maximize/restore
    // control local columns width-5..width-3, so local column 6 (absolute
    // 8, for a window whose left edge is at 2) is title, not chrome. A
    // press on a control activates it instead of starting a drag.
    f.term.inject_event(ckv::MouseEvent{.action = ckv::MouseAction::Down,
                                        .button = ckv::MouseButton::Left,
                                        .cell = ckv::Point{8, 2},
                                        .pixel = std::nullopt,
                                        .modifiers = ckv::Modifier::None});
    f.app.step(0);

    f.term.inject_event(ckv::MouseEvent{.action = ckv::MouseAction::Move,
                                        .button = ckv::MouseButton::Left,
                                        .cell = ckv::Point{20, 2},
                                        .pixel = std::nullopt,
                                        .modifiers = ckv::Modifier::None});
    f.term.clear_written();
    f.app.step(0);

    CK_CHECK(window->bounds() == (Rect{14, 2, 14, 6}));
    CK_CHECK(window->content_repaint_count() == original_repaints);
    CK_CHECK(desktop->last_content_repaints() == 0);
    CK_CHECK(f.app.last_compose_cells_touched() <= 216);
    CK_CHECK(f.app.last_bytes_emitted() > 0);
    CK_CHECK(f.app.last_bytes_emitted() <= 2048);
}

CK_TEST(rapid_large_text_window_dragging_keeps_only_the_latest_position) {
    ckv::term::HeadlessTerminal term{ckv::Size{100, 30}};
    ManualClock clock;
    Application app{term, clock};
    const ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    app.set_frame_completion_tracking(true);

    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    auto window_owned = std::make_unique<Window>("Dense text");
    window_owned->set_bounds(Rect{2, 3, 72, 22});
    Window* const window = desktop_owned->add_window(std::move(window_owned));
    auto content = std::make_unique<DenseTextProbe>();
    content->set_bounds(Rect{1, 1, 68, 18});
    window->add_child(std::move(content));
    app.root().add_child(std::move(desktop_owned));

    app.step(clock.now_nanos());
    CK_CHECK(app.last_bytes_emitted() >= ckv::term::kLargeTextFrameBytes);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);

    // Start a real title-bar drag through Application's event route. The
    // first move is allowed through; it establishes the expensive frame the
    // host has not yet acknowledged.
    term.inject_event(ckv::MouseEvent{.action = ckv::MouseAction::Down,
                                      .button = ckv::MouseButton::Left,
                                      .cell = ckv::Point{12, 3},
                                      .pixel = std::nullopt,
                                      .modifiers = ckv::Modifier::None});
    app.step(clock.now_nanos());
    term.clear_written();

    term.inject_event(ckv::MouseEvent{.action = ckv::MouseAction::Move,
                                      .button = ckv::MouseButton::Left,
                                      .cell = ckv::Point{13, 3},
                                      .pixel = std::nullopt,
                                      .modifiers = ckv::Modifier::None});
    app.step(clock.now_nanos());
    const std::size_t first_move_bytes = term.written_bytes().size();
    CK_CHECK(first_move_bytes >= ckv::term::kLargeTextFrameBytes);
    CK_CHECK(app.frames_awaiting_terminal() == 1U);

    // Nineteen more pointer-rate positions arrive while the host still owes
    // that answer. The retained scene follows every event, but not one stale
    // intermediate frame is appended to the terminal stream.
    for (int step = 2; step <= 20; ++step) {
        term.inject_event(ckv::MouseEvent{.action = ckv::MouseAction::Move,
                                          .button = ckv::MouseButton::Left,
                                          .cell = ckv::Point{12 + step, 3},
                                          .pixel = std::nullopt,
                                          .modifiers = ckv::Modifier::None});
        app.step(clock.now_nanos());
    }
    CK_CHECK(window->bounds() == (Rect{22, 3, 72, 22}));
    CK_CHECK(term.written_bytes().size() == first_move_bytes);

    // Once the host catches up, exactly the current position is presented.
    // The byte ceiling is deliberately a frame-class budget: without
    // coalescing this deterministic gesture emits roughly twenty large
    // frames and exceeds both the ratio and the absolute bound.
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    const std::size_t drag_bytes = term.written_bytes().size();
    CK_CHECK(drag_bytes > first_move_bytes);
    CK_CHECK(drag_bytes <= first_move_bytes * 3U);
    CK_CHECK(drag_bytes <= 96U * 1024U);

    std::size_t completion_marks = 0;
    for (std::size_t at = term.written_bytes().find("\x1B[5n");
         at != std::string_view::npos;
         at = term.written_bytes().find("\x1B[5n", at + 1))
        ++completion_marks;
    CK_CHECK(completion_marks == 2U);
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
}

CK_TEST(small_text_frames_remain_immediate_while_an_answer_is_outstanding) {
    Fixture f;
    f.app.set_frame_completion_tracking(true);
    auto label_owned = std::make_unique<Label>("A");
    Label* const label = label_owned.get();
    label->set_bounds(Rect{2, 2, 4, 1});
    f.app.root().add_child(std::move(label_owned));

    f.app.step(f.clock.now_nanos());
    f.term.inject_bytes("\x1B[0n", f.clock.now_nanos());
    f.app.step(f.clock.now_nanos());
    CK_CHECK(f.app.frames_awaiting_terminal() == 0U);

    label->set_text("B");
    f.term.clear_written();
    f.app.step(f.clock.now_nanos());
    CK_CHECK(f.app.last_bytes_emitted() > 0U);
    CK_CHECK(f.app.last_bytes_emitted() < ckv::term::kLargeTextFrameBytes);
    CK_CHECK(f.app.frames_awaiting_terminal() == 1U);

    // No acknowledgement for B. C still goes immediately because this frame
    // class carries latency-sensitive interaction, not enough terminal work
    // to justify a host round trip.
    label->set_text("C");
    f.term.clear_written();
    f.app.step(f.clock.now_nanos());
    CK_CHECK(!f.term.written_bytes().empty());
    CK_CHECK(f.app.last_bytes_emitted() < ckv::term::kLargeTextFrameBytes);
    CK_CHECK(f.app.frames_awaiting_terminal() == 2U);
}

CK_TEST(resizing_one_window_repaints_only_its_retained_subtree) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto first_owned = std::make_unique<Window>("First");
    first_owned->set_bounds(Rect{2, 2, 12, 6});
    Window* first = desktop->add_window(std::move(first_owned));
    auto second_owned = std::make_unique<Window>("Second");
    second_owned->set_bounds(Rect{18, 2, 12, 6});
    Window* second = desktop->add_window(std::move(second_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    const std::size_t first_repaints = first->content_repaint_count();
    const std::size_t second_repaints = second->content_repaint_count();

    first->set_bounds(Rect{2, 2, 15, 7});
    f.app.step(0);

    CK_CHECK(first->content_repaint_count() == first_repaints + 1);
    CK_CHECK(second->content_repaint_count() == second_repaints);
    CK_CHECK(desktop->last_content_repaints() == 1);
}

CK_TEST(retained_window_layers_preserve_foreground_frame_topology_through_move_and_resize) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto background_owned = std::make_unique<Window>("Back");
    background_owned->set_bounds(Rect{2, 2, 14, 4});
    desktop->add_window(std::move(background_owned));
    auto foreground_owned = std::make_unique<Window>("Front");
    foreground_owned->set_bounds(Rect{12, 3, 12, 7});
    Window* foreground = desktop->add_window(std::move(foreground_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    // The foreground Window's isolated backing provides its exact corner;
    // it must never merge with the lower Window's vertical border.
    CK_CHECK(f.app.current_frame().at(ckv::Point{12, 3}).grapheme() == "╔");

    foreground->set_bounds(Rect{11, 3, 12, 7});
    f.app.step(0);
    CK_CHECK(f.app.current_frame().at(ckv::Point{11, 3}).grapheme() == "╔");

    foreground->set_bounds(Rect{11, 3, 14, 8});
    f.app.step(0);
    CK_CHECK(f.app.current_frame().at(ckv::Point{11, 3}).grapheme() == "╔");
}

CK_TEST(moving_a_popup_recomposes_its_retained_backing_without_content_repaint) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto popup_owned = std::make_unique<CountingPopup>();
    popup_owned->set_bounds(Rect{2, 2, 8, 2});
    ckv::ui::View* popup_child = popup_owned->add_child(std::make_unique<ckv::ui::View>());
    CountingPopup* popup = desktop->add_popup(std::move(popup_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    CK_CHECK(popup->draw_count == 1);

    popup->set_bounds(Rect{18, 2, 8, 2});
    f.app.step(0);
    CK_CHECK(popup->draw_count == 1);
    CK_CHECK(desktop->last_content_repaints() == 0);

    popup->change_content();
    f.app.step(0);
    CK_CHECK(popup->draw_count == 2);
    CK_CHECK(desktop->last_content_repaints() == 1);

    popup_child->invalidate();
    f.app.step(0);
    CK_CHECK(popup->draw_count == 3);
    CK_CHECK(desktop->last_content_repaints() == 1);

    popup->set_bounds(Rect{18, 2, 9, 3});
    f.app.step(0);
    CK_CHECK(popup->draw_count == 4);
    CK_CHECK(desktop->last_content_repaints() == 1);
}

CK_TEST(runtime_popup_attachment_and_removal_recompose_the_retained_scene) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    f.app.root().add_child(std::move(desktop_owned));
    f.app.step(0);

    auto popup_owned = std::make_unique<CountingPopup>();
    popup_owned->set_bounds(Rect{7, 4, 8, 2});
    CountingPopup* popup = desktop->add_popup(std::move(popup_owned));
    f.app.step(0);
    CK_CHECK(popup->draw_count == 1);
    CK_CHECK(f.app.current_frame().at(ckv::Point{7, 4}).grapheme() == "P");

    std::unique_ptr<ckv::ui::View> detached = desktop->remove_popup(popup);
    CK_CHECK(detached != nullptr);
    f.app.step(0);
    CK_CHECK(desktop->last_content_repaints() == 0);
    CK_CHECK(f.app.current_frame().at(ckv::Point{7, 4}).grapheme() == "░");
}

CK_TEST(dropdown_menu_shadows_follow_the_retained_popup_layer) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto dropdown_owned = std::make_unique<DropdownMenu>(
        std::vector<MenuItem>{MenuItem::action("&Open", {})});
    dropdown_owned->set_bounds(Rect{2, 2, 6, 2});
    DropdownMenu* dropdown = desktop->add_popup(std::move(dropdown_owned));
    f.app.root().add_child(std::move(desktop_owned));

    const ckv::Style background = f.app.theme().resolve(f.roles.desktop_background);
    const ckv::Style shadow = ckv::scene::default_dim(background);
    f.app.step(0);
    CK_CHECK(f.app.current_frame().at(ckv::Point{8, 3}).style() == shadow);

    dropdown->set_bounds(Rect{18, 2, 6, 2});
    f.app.step(0);
    CK_CHECK(desktop->last_content_repaints() == 0);
    CK_CHECK(f.app.current_frame().at(ckv::Point{8, 3}).style() == background);
    CK_CHECK(f.app.current_frame().at(ckv::Point{24, 3}).style() == shadow);
}

CK_TEST(window_and_dropdown_shadows_form_one_retained_binary_union) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    auto window_owned = std::make_unique<Window>("Lower");
    window_owned->set_bounds(Rect{2, 2, 10, 6});
    desktop->add_window(std::move(window_owned));
    auto dropdown_owned = std::make_unique<DropdownMenu>(
        std::vector<MenuItem>{MenuItem::action("&Open", {})});
    dropdown_owned->set_bounds(Rect{6, 3, 6, 2});
    desktop->add_popup(std::move(dropdown_owned));
    f.app.root().add_child(std::move(desktop_owned));

    const ckv::Style background = f.app.theme().resolve(f.roles.desktop_background);
    f.app.step(0);
    // (12,4) belongs to both right-hand shadow strips, while remaining
    // outside both layer rectangles. The composed background must be dimmed
    // once, not once per caster.
    CK_CHECK(f.app.current_frame().at(ckv::Point{12, 4}).style() == ckv::scene::default_dim(background));
}

CK_TEST(rapid_retained_window_and_dropdown_lifecycles_leave_no_stale_scene_content) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{});
    Desktop* desktop = desktop_owned.get();
    f.app.root().add_child(std::move(desktop_owned));
    f.app.step(0);

    for (int iteration = 0; iteration < 16; ++iteration) {
        auto window_owned = std::make_unique<Window>("Transient");
        window_owned->set_bounds(Rect{20, 2, 12, 6});
        Window* window = desktop->add_window(std::move(window_owned));
        f.app.step(0);

        auto dropdown_owned = std::make_unique<DropdownMenu>(
            std::vector<MenuItem>{MenuItem::action("&Open", {})});
        dropdown_owned->set_bounds(Rect{4, 3, 6, 2});
        ckv::ui::View* dropdown = desktop->add_popup(std::move(dropdown_owned));
        f.app.step(0);

        desktop->remove_popup(dropdown).reset();
        f.app.step(0);
        CK_CHECK(f.app.current_frame().at(ckv::Point{10, 4}).grapheme() == "░");

        desktop->remove_window(window).reset();
        f.app.step(0);
        CK_CHECK(desktop->windows().empty());
        CK_CHECK(desktop->popups().empty());
    }
}

// --- Terminal too small (the architecture §5 "Sizing policy", M10/WP-21) ---

CK_TEST(terminal_too_small_is_false_at_a_normal_size) {
    Fixture f;
    CK_CHECK(!f.app.terminal_too_small());
}

CK_TEST(terminal_too_small_is_false_exactly_at_the_hard_floor) {
    Fixture f;
    f.term.resize(ckv::Size{20, 6});  // kHardFloorSize itself: still large enough to render
    f.app.step(0);
    CK_CHECK(!f.app.terminal_too_small());
}

CK_TEST(terminal_too_small_is_true_one_cell_below_the_floor_on_either_axis) {
    Fixture f;
    f.term.resize(ckv::Size{19, 6});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());

    f.term.resize(ckv::Size{20, 5});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());
}

CK_TEST(shrinking_below_the_floor_shows_the_too_small_message_instead_of_corrupted_output) {
    Fixture f;
    // 19 wide: below the hard floor (kHardFloorSize.width == 20) but
    // still wide enough for the full "Terminal too small" message
    // (18 chars) to render unclipped, so the exact text is checkable.
    f.term.resize(ckv::Size{19, 5});
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Terminal too small") != std::string::npos);
}

CK_TEST(a_terminal_narrower_than_the_message_still_shows_a_clipped_prefix_not_garbage) {
    Fixture f;
    f.term.resize(ckv::Size{8, 5});  // narrower than "Terminal too small" itself
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Terminal") != std::string::npos);
}

CK_TEST(the_too_small_state_hides_the_normal_view_tree_entirely) {
    Fixture f;
    auto label = std::make_unique<Label>("Hidden");
    label->set_bounds(Rect{0, 0, 6, 1});
    f.app.root().add_child(std::move(label));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Hidden") != std::string::npos);  // renders normally first
    f.term.clear_written();  // written_bytes() is cumulative — isolate the NEXT step's own output

    f.term.resize(ckv::Size{15, 5});
    f.app.step(0);

    CK_CHECK(f.term.written_bytes().find("Hidden") == std::string::npos);
}

CK_TEST(growing_back_above_the_floor_recovers_normal_rendering_automatically) {
    Fixture f;
    auto label = std::make_unique<Label>("Back");
    label->set_bounds(Rect{0, 0, 4, 1});
    f.app.root().add_child(std::move(label));

    f.term.resize(ckv::Size{15, 5});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());

    f.term.resize(ckv::Size{40, 12});
    f.app.step(0);

    CK_CHECK(!f.app.terminal_too_small());
    CK_CHECK(f.term.written_bytes().find("Back") != std::string::npos);
}

// --- Docked chrome stays above windows ---------------------------------------

CK_TEST(a_window_dragged_onto_the_menu_bar_passes_under_it) {
    // Docked chrome is the desktop's furniture: always there, and a window
    // that can cover it can hide the way out of whatever it is covering.
    Fixture f;
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        f.app.root().add_child(std::make_unique<ckv::widgets::Desktop>(f.app.root().bounds())));
    desktop->dock_top(std::make_unique<ckv::widgets::MenuBar>(
        std::vector<ckv::widgets::MenuBarItem>{{"&File", {}}}));
    auto* w = desktop->add_window(std::make_unique<ckv::widgets::Window>("Win"));
    w->set_bounds(ckv::Rect{2, 3, 20, 5});
    f.app.step(0);

    const auto row = [&f](int y) {
        std::string text;
        for (int x = 0; x < f.app.composed_surface().size().width; ++x)
            text += f.app.composed_surface().at(ckv::Point{x, y}).grapheme();
        return text;
    };
    const std::string menu_row = row(0);
    CK_CHECK(menu_row.find("File") != std::string::npos);

    // Drag it up over the menu bar. Its title row would land on row 0.
    w->set_bounds(ckv::Rect{2, 0, 20, 5});
    f.app.step(0);
    CK_CHECK(row(0) == menu_row);                      // menu bar untouched
    CK_CHECK(row(0).find("Win") == std::string::npos);  // and not the title
    CK_CHECK(row(1).find("║") != std::string::npos);    // the window is there, below
}
