// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/window.hpp"
#include "cvision/widgets/terminal_view.hpp"

#include "cktest.hpp"

#include <algorithm>

namespace {

struct Fixture {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    ckv::ui::Context context() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

}  // namespace

CK_TEST(terminal_view_paints_private_snapshot_cells_and_routes_input) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 8, 2});
    session.feed_output("ok");

    ckv::scene::Surface surface(ckv::Size{8, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 8, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "o");
    CK_CHECK(view.on_text(ckv::TextEvent{"input"}));
    CK_CHECK(session.take_pending_input() == "input");
}

CK_TEST(terminal_view_invalidates_when_its_private_session_changes) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{10, 2};
    ckv::term::TerminalEmulator session(profile);
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 10, 2});
    app.root().add_child(std::move(view));
    app.step(0);
    terminal.clear_written();

    session.feed_output("wake");
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    CK_CHECK(terminal.written_bytes().find("wake") != std::string_view::npos);
}

CK_TEST(terminal_view_sizes_its_private_child_from_the_outer_terminal_metrics) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6}, ckv::term::headless_sixel_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cell_pixels = ckv::Size{1, 1};
    ckv::term::TerminalEmulator session(profile);
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 8, 2});
    app.root().add_child(std::move(view));

    const ckv::term::TerminalSnapshot snapshot = session.snapshot();
    CK_CHECK(snapshot.cells == (ckv::Size{8, 2}));
    CK_CHECK(snapshot.cells != terminal.size());
    CK_CHECK(session.profile().cell_pixels == terminal.capabilities().cell_pixels);
}

CK_TEST(terminal_view_keeps_parent_escape_out_of_child_input) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    bool returned_to_parent = false;
    view.on_parent_escape = [&returned_to_parent] { returned_to_parent = true; };
    const ckv::KeyEvent escape{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl | ckv::Modifier::Alt, " "}};
    CK_CHECK(view.on_key(escape));
    CK_CHECK(returned_to_parent);
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(terminal_view_encodes_control_meta_and_modified_navigation_for_the_private_child) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "c"}}));
    CK_CHECK(session.take_pending_input() == "\x03");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Alt, "x"}}));
    CK_CHECK(session.take_pending_input() == "\x1bx");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Up, ckv::Modifier::Shift, ""}}));
    CK_CHECK(session.take_pending_input() == "\x1b[1;2A");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F5, ckv::Modifier::None, ""}}));
    CK_CHECK(session.take_pending_input() == "\x1b[15~");

    session.feed_output("\x1b[?1h");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Up, ckv::Modifier::None, ""}}));
    CK_CHECK(session.take_pending_input() == "\x1bOA");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Up, ckv::Modifier::Shift, ""}}));
    CK_CHECK(session.take_pending_input() == "\x1b[1;2A");
}

CK_TEST(terminal_view_wraps_paste_only_when_the_child_requests_bracketed_paste) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    CK_CHECK(view.on_text(ckv::TextEvent{"plain", true}));
    CK_CHECK(session.take_pending_input() == "plain");

    session.feed_output("\x1b[?2004h");
    CK_CHECK(view.on_text(ckv::TextEvent{"pasted", true}));
    CK_CHECK(session.take_pending_input() == "\x1b[200~pasted\x1b[201~");
}

CK_TEST(terminal_view_sends_focus_reports_only_when_the_private_child_requests_them) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    view.on_focus(ckv::FocusEvent{true});
    CK_CHECK(session.take_pending_input().empty());

    session.feed_output("\x1b[?1004h");
    view.on_focus(ckv::FocusEvent{true});
    CK_CHECK(session.take_pending_input() == "\x1b[I");
    view.on_focus(ckv::FocusEvent{false});
    CK_CHECK(session.take_pending_input() == "\x1b[O");
}

CK_TEST(terminal_view_publishes_its_focused_private_cursor_in_absolute_coordinates) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    view.set_bounds(ckv::Rect{3, 4, 8, 2});
    view.on_focus(ckv::FocusEvent{true});
    const std::optional<ckv::CursorState> cursor = view.cursor_state();
    CK_CHECK(cursor.has_value());
    CK_CHECK(cursor->position == (ckv::Point{3, 4}));
    CK_CHECK(cursor->blink);
    view.on_focus(ckv::FocusEvent{false});
    CK_CHECK(!view.cursor_state().has_value());
}

CK_TEST(terminal_view_forwards_the_child_selected_mouse_encoding_only_to_private_session) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    view.set_bounds(ckv::Rect{3, 4, 8, 2});
    const ckv::MouseEvent click{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 5}, {}, ckv::Modifier::None};
    session.feed_output("\x1b[?1000h");
    CK_CHECK(view.on_mouse(click));
    CK_CHECK(session.take_pending_input() == std::string("\x1b[M") + char{32} + char{35} + char{34});

    session.feed_output("\x1b[?1006h");
    CK_CHECK(view.on_mouse(click));
    CK_CHECK(session.take_pending_input() == "\x1b[<0;3;2M");
}

CK_TEST(terminal_view_shift_drag_selects_locally_without_child_mouse_bytes) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 1};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    view.set_bounds(ckv::Rect{0, 0, 4, 1});
    session.feed_output("test");
    std::string copied;
    view.on_selection_copy = [&copied](std::string text) { copied = std::move(text); };
    const ckv::Modifier shift = ckv::Modifier::Shift;
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 0}, {}, shift}));
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{3, 0}, {}, shift}));
    CK_CHECK(copied == "est");
    CK_CHECK(session.take_pending_input().empty());

    Fixture fixture;
    view.set_context(fixture.context());
    ckv::scene::Surface surface(ckv::Size{4, 1}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 1});
    view.draw(painter);
    CK_CHECK(ckv::has_attr(surface.at(ckv::Point{1, 0}).style().attrs, ckv::Attr::Reverse));
    CK_CHECK(!ckv::has_attr(surface.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Reverse));
}

CK_TEST(terminal_view_copies_the_scrollback_rows_currently_visible_to_the_user) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    CK_CHECK(view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::PageUp, ckv::Modifier::None, ""}}));

    std::string copied;
    view.on_selection_copy = [&copied](std::string text) { copied = std::move(text); };
    const ckv::Modifier shift = ckv::Modifier::Shift;
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 0}, {}, shift}));
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{1, 1}, {}, shift}));
    CK_CHECK(copied == "ne \ntw");
}

CK_TEST(terminal_view_places_child_sixel_only_through_scene_raster_path) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{2, 2};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 2, 2});
    session.set_raster_identity(99);
    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");

    ckv::scene::Surface surface(ckv::Size{2, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 2, 2});
    view.draw(painter);
    CK_CHECK(surface.raster_regions().size() == 1);
    CK_CHECK(surface.raster_regions()[0].id == 99);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "[");
}

CK_TEST(terminal_view_uses_its_text_fallback_when_the_outer_terminal_has_no_graphics) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{2, 2};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator session(profile);
    session.set_raster_identity(77);

    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 2, 2});
    app.root().add_child(std::move(view));
    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    app.step(0);

    CK_CHECK(!terminal.display().has_raster_pixels());
    CK_CHECK(terminal.written_bytes().find("\x1bPq") == std::string_view::npos);
    CK_CHECK(terminal.display().frame().at(ckv::Point{1, 1}).grapheme() == "[");
}

CK_TEST(terminal_view_reencodes_private_child_sixel_only_through_the_outer_presenter) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6}, ckv::term::headless_sixel_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{2, 2};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator session(profile);
    session.set_raster_identity(78);

    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 2, 2});
    app.root().add_child(std::move(view));
    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    app.step(0);

    CK_CHECK(terminal.display().has_raster_pixels());
    CK_CHECK(terminal.written_bytes().find("\x1bP") != std::string_view::npos);
    CK_CHECK(terminal.display().frame().at(ckv::Point{1, 1}).grapheme() == " ");
}

CK_TEST(terminal_view_reencodes_private_child_sixel_from_a_retained_window_backing) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{30, 12}, ckv::term::headless_sixel_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::ApplicationShell shell(
        app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles)});
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 4};
    profile.cell_pixels = terminal.capabilities().cell_pixels;
    ckv::term::TerminalEmulator session(profile);
    session.set_raster_identity(79);
    auto window = std::make_unique<ckv::widgets::Window>("contained");
    window->set_bounds(ckv::Rect{2, 2, 12, 7});
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    window->set_content(std::move(view));
    ckv::widgets::Window* const window_observer = window.get();
    shell.desktop().add_window(std::move(window));

    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);

    CK_CHECK(terminal.display().has_raster_pixels());
    CK_CHECK(terminal.written_bytes().find("\x1bP") != std::string_view::npos);

    const ckv::Size cell_pixels = terminal.capabilities().cell_pixels;
    const auto raster_is_inside_content = [&]() {
        const ckv::Rect window_absolute = window_observer->absolute_bounds();
        const ckv::Rect content = window_observer->content_rect();
        const ckv::Rect content_absolute{window_absolute.x + content.x, window_absolute.y + content.y,
                                         content.width, content.height};
        const ckv::Image& raster_plane = terminal.display().raster_plane();
        for (int y = 0; y < raster_plane.height(); ++y) {
            for (int x = 0; x < raster_plane.width(); ++x) {
                if (raster_plane.pixel(x, y).a == 0) continue;
                const int cell_x = x / std::max(1, cell_pixels.width);
                const int cell_y = y / std::max(1, cell_pixels.height);
                const bool inside = cell_x >= content_absolute.x &&
                                    cell_x < content_absolute.x + content_absolute.width &&
                                    cell_y >= content_absolute.y &&
                                    cell_y < content_absolute.y + content_absolute.height;
                if (!inside) return false;
            }
        }
        return true;
    };
    CK_CHECK(raster_is_inside_content());

    // Parent geometry changes must recompose the retained window while the
    // private decoded image remains owned by the child session.
    window_observer->set_bounds(ckv::Rect{8, 3, 12, 7});
    app.step(0);
    CK_CHECK(terminal.display().has_raster_pixels());
    CK_CHECK(raster_is_inside_content());

    window_observer->set_bounds(ckv::Rect{8, 3, 18, 9});
    app.step(0);
    CK_CHECK(terminal.display().has_raster_pixels());
    CK_CHECK(raster_is_inside_content());

    // A higher retained window must occlude the child raster in the outer
    // decoded pixel plane, not merely overwrite its fallback cells.
    auto occluder = std::make_unique<ckv::widgets::Window>("occluder");
    occluder->set_bounds(ckv::Rect{10, 2, 8, 6});
    shell.desktop().add_window(std::move(occluder));
    app.step(0);
    const ckv::Rect occluder_absolute = shell.desktop().active_window()->absolute_bounds();
    const ckv::Image& occluded_plane = terminal.display().raster_plane();
    bool has_transparent_occluder_pixel = false;
    for (int y = std::max(0, occluder_absolute.y * cell_pixels.height);
         y < std::min(occluded_plane.height(), occluder_absolute.bottom() * cell_pixels.height); ++y) {
        for (int x = std::max(0, occluder_absolute.x * cell_pixels.width);
             x < std::min(occluded_plane.width(), occluder_absolute.right() * cell_pixels.width); ++x) {
            if (occluded_plane.pixel(x, y).a == 0) {
                has_transparent_occluder_pixel = true;
                break;
            }
        }
        if (has_transparent_occluder_pixel) break;
    }
    CK_CHECK(has_transparent_occluder_pixel);
    CK_CHECK(raster_is_inside_content());

    // A child clear removes the retained raster before the next outer frame.
    session.feed_output("\x1b[2J");
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    CK_CHECK(!terminal.display().has_raster_pixels());
}

CK_TEST(terminal_view_child_raster_cannot_paint_outside_its_scene_clip) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{2, 2};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 2, 2});
    session.set_raster_identity(101);
    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");

    ckv::scene::Surface surface(ckv::Size{4, 2}, ckv::Cell::from_grapheme("P", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 2, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{2, 0}).grapheme() == "P");
    CK_CHECK(surface.at(ckv::Point{3, 1}).grapheme() == "P");
}

CK_TEST(terminal_view_renders_private_diagnostic_without_raw_control_data) {
    ckv::term::TerminalSubsessionOptions options;
    options.max_control_bytes = 1;
    ckv::term::TerminalEmulator session(ckv::term::embedded_xterm_sixel_profile(), options);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 32, 2});
    session.feed_output("\x1b]oversize\x1b\\");
    ckv::scene::Surface surface(ckv::Size{32, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 32, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 1}).grapheme() == "[");
    CK_CHECK(surface.at(ckv::Point{10, 1}).grapheme() != "\x1b");
}
