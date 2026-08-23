// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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

#include "cvision/testing/cktest.hpp"

#include <algorithm>

namespace {

struct Fixture {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    ckv::ui::Context context() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

// A session that counts the reads a view makes of it, forwarding everything to
// a real emulator.
//
// `snapshot()` copies the grid AND the history; the borrowed reads copy nothing.
// Which one a view reaches for is therefore not a style question: at ten
// thousand lines of history a snapshot is tens of megabytes, and a view that
// takes one per repaint — or worse, several per keystroke — makes a terminal
// slower the longer it has been alive.
class CountingSubsession final : public ckv::term::TerminalSubsession {
public:
    explicit CountingSubsession(const ckv::term::TerminalCapabilityProfile& profile)
        : emulator_(profile) {}

    ckv::core::TerminalSnapshot snapshot() const override {
        ++snapshots;
        return emulator_.snapshot();
    }
    ckv::core::TerminalStatus status() const override {
        ++statuses;
        ckv::core::TerminalStatus status = emulator_.status();
        // A session over a protocol may fill in only the older, coarser
        // `mouse_reporting_enabled` — see `names_tracking_level`.
        if (!names_tracking_level) status.mouse_tracking = ckv::core::TerminalMouseTracking::None;
        return status;
    }
    const ckv::core::TerminalDamage& damage() const noexcept override { return emulator_.damage(); }
    void clear_damage() noexcept override { emulator_.clear_damage(); }
    bool synchronized_output_active() const noexcept override {
        return emulator_.synchronized_output_active();
    }
    std::span<const ckv::Cell> cells() const noexcept override { return emulator_.cells(); }
    std::span<const ckv::Cell> scrollback() const noexcept override { return emulator_.scrollback(); }
    std::span<const ckv::core::TerminalRaster> rasters() const noexcept override {
        return emulator_.rasters();
    }
    std::span<const ckv::core::TerminalDiagnostic> diagnostics() const noexcept override {
        return emulator_.diagnostics();
    }
    const ckv::core::TerminalCapabilityProfile& profile() const noexcept override {
        return emulator_.profile();
    }
    void feed_output(std::string_view bytes) override { emulator_.feed_output(bytes); }
    void resize(ckv::Size cells, ckv::Size cell_pixels) override {
        emulator_.resize(cells, cell_pixels);
    }
    void send_input(std::string_view bytes) override { emulator_.send_input(bytes); }
    std::string take_pending_input() override { return emulator_.take_pending_input(); }
    ckv::core::TerminalSubsessionState state() const noexcept override { return emulator_.state(); }
    void set_raster_identity(int identity) noexcept override {
        emulator_.set_raster_identity(identity);
    }

    mutable int snapshots = 0;
    mutable int statuses = 0;
    // Whether this session answers the mouse-tracking level at all. A host that
    // implements the seam itself — a mirror over a protocol whose wire carries
    // one bit for the mouse — may not, and must still get its pointer events
    // through.
    bool names_tracking_level = true;

private:
    ckv::term::TerminalEmulator emulator_;
};

}  // namespace

CK_TEST(a_view_reads_a_terminal_without_copying_it) {
    // The claim is exact: repainting and typing take no snapshot at all. The
    // one snapshot a view may still take is for the clipboard TEXT, and only
    // after the serial says it changed — which is what the serial is for.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{40, 6};
    CountingSubsession session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 40, 6});
    session.feed_output("a line the child printed\r\n");

    ckv::scene::Surface surface(ckv::Size{40, 6}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 40, 6});
    for (int frame = 0; frame < 10; ++frame) view.draw(painter);
    CK_CHECK(view.on_text(ckv::TextEvent{"typed"}));
    ckv::KeyEvent up;
    up.chord.key = ckv::Key::Up;
    CK_CHECK(view.on_key(up));
    ckv::KeyEvent page_up;
    page_up.chord.key = ckv::Key::PageUp;
    CK_CHECK(view.on_key(page_up));

    CK_CHECK(session.snapshots == 0);
    // And it did read the cheap one, so this is not passing because the view
    // stopped looking at the terminal altogether.
    CK_CHECK(session.statuses > 0);
    // The content still arrived on screen and the input still reached the child.
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "a");
    CK_CHECK(session.take_pending_input().find("typed") != std::string::npos);
}

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

    const ckv::MouseEvent horizontal_wheel{ckv::MouseAction::Wheel, ckv::MouseButton::WheelLeft,
                                           ckv::Point{5, 5}, {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(horizontal_wheel));
    CK_CHECK(session.take_pending_input() == "\x1b[<66;3;2M");
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

CK_TEST(two_simultaneous_child_pictures_in_one_terminal_get_distinct_scene_ids) {
    // A program that draws a second Sixel without clearing the first is
    // ordinary — terminal_emulator.cpp keeps up to 64 rasters alive at
    // once — but every raster of one terminal used to carry that terminal's
    // bare identity verbatim, so a second one alive at the same time as the
    // first collided with it the moment both reached the same Surface:
    // Surface::add_raster_region's own uniqueness contract turned that into
    // a hard abort. This is the field crash's minimal reproduction.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 2};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 8, 2});
    session.set_raster_identity(99);
    session.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    session.feed_output("\x1b[1;5H\x1bPq#0;2;100;0;0~\x1b\\");
    CK_CHECK(session.snapshot().rasters.size() == 2U);

    ckv::scene::Surface surface(ckv::Size{8, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 8, 2});
    view.draw(painter);  // used to abort here
    CK_CHECK(surface.raster_regions().size() == 2);
    CK_CHECK(surface.raster_regions()[0].id == 99);
    CK_CHECK(surface.raster_regions()[1].id == 100);
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

CK_TEST(a_wheel_over_a_full_screen_child_scrolls_it_with_cursor_keys) {
    // `less` and `man` never ask for mouse reporting, so without this a wheel
    // over them does nothing at all — which reads as a broken wheel rather
    // than as a mode nobody enabled.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{10, 3};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[?1049h");  // the child goes full-screen
    session.take_pending_input();

    const ckv::MouseEvent wheel_down{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                     ckv::Point{2, 1}, {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(wheel_down));
    CK_CHECK(session.take_pending_input() == "\x1b[B\x1b[B\x1b[B");

    const ckv::MouseEvent wheel_up{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                   ckv::Point{2, 1}, {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(wheel_up));
    CK_CHECK(session.take_pending_input() == "\x1b[A\x1b[A\x1b[A");
}

CK_TEST(alternate_scroll_speaks_the_cursor_key_dialect_the_child_selected) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[?1049h\x1b[?1h");  // full-screen, application cursor keys
    session.take_pending_input();
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                           ckv::Point{2, 1}, {}, ckv::Modifier::None}));
    CK_CHECK(session.take_pending_input() == "\x1bOB\x1bOB\x1bOB");
}

CK_TEST(a_wheel_on_the_primary_buffer_walks_the_terminals_own_history) {
    // On the primary buffer the wheel belongs to the terminal's own history:
    // nothing goes to the child — arrow keys sent there would walk the
    // shell's command history instead — and the view itself moves.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");

    const ckv::MouseEvent wheel_up{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                   ckv::Point{1, 0}, {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(wheel_up));
    CK_CHECK(session.take_pending_input().empty());
    // A notch asks for three rows; one is all the history there is.
    CK_CHECK(view.scroll_state().offset == 1);

    ckv::scene::Surface surface(ckv::Size{4, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "o");  // "one", from history

    const ckv::MouseEvent wheel_down{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                     ckv::Point{1, 0}, {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(wheel_down));
    CK_CHECK(view.scroll_state().offset == 0);
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(typing_returns_a_scrolled_view_to_the_live_edge) {
    // "Any key exits to live": the keystroke lands at the prompt, and the
    // reader who sent it wants to watch it land.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    view.set_scrollback_offset(1);
    CK_CHECK(view.scroll_state().offset == 1);

    CK_CHECK(view.on_text(ckv::TextEvent{"x"}));
    CK_CHECK(view.scroll_state().offset == 0);
    CK_CHECK(session.take_pending_input() == "x");

    view.set_scrollback_offset(1);
    ckv::KeyEvent enter;
    enter.chord.key = ckv::Key::Enter;
    CK_CHECK(view.on_key(enter));
    CK_CHECK(view.scroll_state().offset == 0);
    CK_CHECK(session.take_pending_input() == "\r");
}

CK_TEST(history_growth_does_not_move_a_reading_view) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    view.notify_terminal_subsession_changed(session);

    // The reader walks up to the oldest line...
    view.set_scrollback_offset(1);
    ckv::scene::Surface surface(ckv::Size{4, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "o");

    // ...the child prints on; the rows under the reader's eyes stay put.
    session.feed_output("\r\nfor");
    view.notify_terminal_subsession_changed(session);
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "o");
    CK_CHECK(view.scroll_state().offset == 2);

    // The live edge, by contrast, follows the child.
    view.set_scrollback_offset(0);
    session.feed_output("\r\nfiv");
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(view.scroll_state().offset == 0);
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 1}).grapheme() == "f");  // "fiv" on the bottom row
}

CK_TEST(a_shift_wheel_is_the_hosts_even_over_a_mouse_tracking_child) {
    // A Shift-marked gesture is the host's, here as everywhere: with mouse
    // reporting on, Shift+wheel still walks the terminal's own history and
    // sends the child nothing.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    session.feed_output("\x1b[?1000h\x1b[?1006h");
    session.take_pending_input();

    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                           ckv::Point{1, 0}, {}, ckv::Modifier::Shift}));
    CK_CHECK(session.take_pending_input().empty());
    CK_CHECK(view.scroll_state().offset == 1);

    // The unshifted wheel stays the child's own mouse report.
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                           ckv::Point{1, 0}, {}, ckv::Modifier::None}));
    CK_CHECK(session.take_pending_input() == "\x1b[<64;2;1M");
}

CK_TEST(the_alternate_buffer_stands_on_no_history) {
    // A stale offset from the prompt must not paint the prompt's history
    // above a full-screen program's screen.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    view.set_scrollback_offset(1);

    session.feed_output("\x1b[?1049h");
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(view.scroll_state().offset == 0);
    CK_CHECK(!view.scroll_state().primary_screen);

    ckv::scene::Surface surface(ckv::Size{4, 2}, ckv::Cell{});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 2});
    view.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == " ");  // the fresh alternate screen
}

CK_TEST(the_scroll_seam_reports_moves_growth_and_buffer_flips_once_each) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    view.notify_terminal_subsession_changed(session);

    int changes = 0;
    view.on_scroll_state_changed = [&changes] { ++changes; };

    view.set_scrollback_offset(1);
    CK_CHECK(changes == 1);
    view.set_scrollback_offset(1);  // the same place is not a change
    CK_CHECK(changes == 1);

    session.feed_output("\r\nfor");  // history grows: total_rows moved
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(changes == 2);
    view.notify_terminal_subsession_changed(session);  // nothing new to say
    CK_CHECK(changes == 2);

    session.feed_output("\x1b[?1049h");  // the buffer flips
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(changes == 3);
}

CK_TEST(the_primary_screens_plain_paging_keys_stay_local_under_the_kitty_protocol) {
    // Consistency across keyboard protocols: plain PageUp pages the history
    // whether or not the child asked for kitty encodings — and its release is
    // claimed with it, so an event-reporting child never sees a release whose
    // press never arrived.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 2};
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 4, 2});
    session.feed_output("one\r\ntwo\r\ntri");
    session.feed_output("\x1b[>3u");  // disambiguate + report event types
    session.take_pending_input();

    ckv::KeyEvent page_up;
    page_up.chord.key = ckv::Key::PageUp;
    CK_CHECK(view.on_key(page_up));
    CK_CHECK(view.scroll_state().offset == 1);
    CK_CHECK(session.take_pending_input().empty());

    ckv::KeyEvent released = page_up;
    released.action = ckv::KeyAction::Release;
    CK_CHECK(view.on_key(released));
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(a_child_that_turned_alternate_scroll_off_gets_no_cursor_keys) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[?1049h\x1b[?1007l");
    CK_CHECK(!view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                            ckv::Point{2, 1}, {}, ckv::Modifier::None}));
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(a_child_tracking_the_mouse_itself_keeps_its_wheel_events) {
    // With mouse reporting on, the wheel is the child's own input and must
    // arrive as a mouse report rather than as keys it never asked for.
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[?1049h\x1b[?1000h\x1b[?1006h");
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                           ckv::Point{2, 1}, {}, ckv::Modifier::None}));
    CK_CHECK(session.take_pending_input() == "\x1b[<65;3;2M");
}

CK_TEST(a_child_clipboard_request_reaches_the_host_once_per_request) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.clipboard_policy = ckv::term::TerminalClipboardPolicy::AllowWrite;
    ckv::term::TerminalEmulator session(profile);
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});

    std::vector<std::string> exported;
    view.on_clipboard_write = [&exported](std::string text) { exported.push_back(std::move(text)); };

    session.feed_output("\x1b]52;c;aGVsbG8=\x07");
    view.notify_terminal_subsession_changed(session);
    // Another look at the same snapshot is not another request: the snapshot
    // is a value, and reading it must not be what causes the export.
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(exported.size() == 1);
    CK_CHECK(exported[0] == "hello");

    session.feed_output("\x1b]52;c;d29ybGQ=\x07");
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(exported.size() == 2);
    CK_CHECK(exported[1] == "world");
}

CK_TEST(a_denied_child_clipboard_request_never_reaches_the_host) {
    ckv::term::TerminalEmulator session;  // the default profile denies it
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    bool exported = false;
    view.on_clipboard_write = [&exported](std::string) { exported = true; };
    session.feed_output("\x1b]52;c;aGVsbG8=\x07");
    view.notify_terminal_subsession_changed(session);
    CK_CHECK(!exported);
}

namespace {

ckv::KeyEvent key_press(ckv::Key key, ckv::Modifier modifiers = ckv::Modifier::None,
                        std::string text = {}) {
    return ckv::KeyEvent{ckv::KeyChord{key, modifiers, std::move(text)}, ckv::KeyAction::Press, false};
}

}  // namespace

CK_TEST(a_child_that_asked_for_disambiguation_can_tell_ctrl_i_from_tab) {
    // The legacy encoding spells both as one byte, which is the whole reason
    // the protocol exists.
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[>1u");

    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::Ctrl, "i")));
    CK_CHECK(session.take_pending_input() == "\x1b[105;5u");
    CK_CHECK(view.on_key(key_press(ckv::Key::Tab)));
    CK_CHECK(session.take_pending_input() == "\t");
    CK_CHECK(view.on_key(key_press(ckv::Key::Escape)));
    CK_CHECK(session.take_pending_input() == "\x1b[27u");
}

CK_TEST(plain_typing_stays_plain_text_until_a_child_asks_for_every_key) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});

    session.feed_output("\x1b[>1u");
    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::None, "a")));
    CK_CHECK(session.take_pending_input() == "a");

    session.feed_output("\x1b[>8u");
    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::None, "a")));
    CK_CHECK(session.take_pending_input() == "\x1b[97u");
    // Shift is a modifier on the key, not a different key: the code stays the
    // one the key carries and the shift travels beside it.
    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::Shift, "A")));
    CK_CHECK(session.take_pending_input() == "\x1b[97;2u");
}

CK_TEST(a_child_that_asked_for_event_types_learns_when_a_key_is_released) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[>10u");  // report all keys, report event types

    ckv::KeyEvent release = key_press(ckv::Key::Char, ckv::Modifier::None, "a");
    release.action = ckv::KeyAction::Release;
    CK_CHECK(view.on_key(release));
    CK_CHECK(session.take_pending_input() == "\x1b[97;1:3u");

    ckv::KeyEvent repeat = key_press(ckv::Key::Up);
    repeat.action = ckv::KeyAction::Repeat;
    CK_CHECK(view.on_key(repeat));
    CK_CHECK(session.take_pending_input() == "\x1b[1;1:2A");
}

CK_TEST(a_release_is_swallowed_rather_than_delivered_as_a_press) {
    // Without event reporting there is nothing a release can be encoded as,
    // and a release arriving as a press would type the key twice.
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[>9u");  // disambiguate + report all keys, no event types
    ckv::KeyEvent release = key_press(ckv::Key::Char, ckv::Modifier::None, "a");
    release.action = ckv::KeyAction::Release;
    CK_CHECK(!view.on_key(release));
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(a_child_that_asked_for_the_text_gets_the_key_and_the_character) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[>24u");  // report all keys + associated text
    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::Shift, "A")));
    CK_CHECK(session.take_pending_input() == "\x1b[97;2;65u");
}

CK_TEST(the_legacy_encoding_is_what_a_child_that_asked_for_nothing_gets) {
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    CK_CHECK(view.on_key(key_press(ckv::Key::Escape)));
    CK_CHECK(session.take_pending_input() == "\x1b");
    CK_CHECK(view.on_key(key_press(ckv::Key::Char, ckv::Modifier::Ctrl, "i")));
    CK_CHECK(session.take_pending_input() == "\t");
    CK_CHECK(view.on_key(key_press(ckv::Key::Up)));
    CK_CHECK(session.take_pending_input() == "\x1b[A");
}

CK_TEST(a_key_sent_by_an_application_arrives_exactly_as_a_pressed_one_does) {
    // An application with a reserved chord has to be able to offer to send it
    // through — otherwise that chord is unreachable to the child forever — and
    // anything replaying keys (a macro, a recorded session) needs the same
    // door. The encoding must be the reader's own, not a second one that drifts.
    ckv::term::TerminalEmulator pressed_session;
    ckv::term::TerminalEmulator sent_session;
    ckv::widgets::TerminalView pressed(pressed_session);
    ckv::widgets::TerminalView sent(sent_session);
    Fixture fixture;
    pressed.set_context(fixture.context());
    sent.set_context(fixture.context());
    pressed.set_bounds(ckv::Rect{0, 0, 10, 3});
    sent.set_bounds(ckv::Rect{0, 0, 10, 3});

    const auto both = [&](const ckv::KeyEvent& event) {
        CK_CHECK(pressed.on_key(event));
        CK_CHECK(sent.send_key(event));
        const std::string from_press = pressed_session.take_pending_input();
        const std::string from_send = sent_session.take_pending_input();
        CK_CHECK(!from_press.empty());
        CK_CHECK(from_send == from_press);
    };

    both(key_press(ckv::Key::Char, ckv::Modifier::Ctrl, "b"));  // the legacy control byte
    both(key_press(ckv::Key::F5));                              // a functional key
    both(key_press(ckv::Key::Up));                              // a cursor key

    // ...and under the kitty protocol, which is the encoding a caller reaching
    // for `encode_key` alone would have silently skipped.
    pressed_session.feed_output("\x1b[>1u");
    sent_session.feed_output("\x1b[>1u");
    both(key_press(ckv::Key::Char, ckv::Modifier::Ctrl, "i"));
    both(key_press(ckv::Key::Escape));

    // The one difference, and the reason this exists: the reserved chord is
    // swallowed by a press and delivered by a send.
    const ckv::KeyEvent escape_chord =
        key_press(ckv::Key::Char, ckv::Modifier::Ctrl | ckv::Modifier::Alt, " ");
    bool intercepted = false;
    pressed.on_parent_escape = [&intercepted] { intercepted = true; };
    CK_CHECK(pressed.on_key(escape_chord));
    CK_CHECK(intercepted);
    CK_CHECK(pressed_session.take_pending_input().empty());
    CK_CHECK(sent.send_key(escape_chord));
    CK_CHECK(!sent_session.take_pending_input().empty());

    // A key with nothing to say says nothing, and says so.
    ckv::KeyEvent release = key_press(ckv::Key::Up);
    release.action = ckv::KeyAction::Release;
    CK_CHECK(!sent.send_key(release));
    CK_CHECK(sent_session.take_pending_input().empty());
}

CK_TEST(a_child_hears_only_the_pointer_motion_the_mode_it_chose_asks_for) {
    // 1000 is presses and releases, 1002 adds motion while a button is held,
    // and only 1003 asked where the pointer is when nothing is held. A view
    // that reports motion to all three sends a program written for 1000 reports
    // between the ones it is parsing for.
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});

    const ckv::MouseEvent hover{ckv::MouseAction::Move, ckv::MouseButton::None, ckv::Point{4, 1},
                                {}, ckv::Modifier::None};
    const ckv::MouseEvent drag{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{4, 1},
                               {}, ckv::Modifier::None};
    const ckv::MouseEvent press{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{4, 1},
                                {}, ckv::Modifier::None};

    // Button events only: a press is the child's, both motions are not — and an
    // event this view does not claim is left for whatever else wants it.
    session.feed_output("\x1b[?1000h\x1b[?1006h");
    CK_CHECK(view.on_mouse(press));
    CK_CHECK(session.take_pending_input() == "\x1b[<0;5;2M");
    CK_CHECK(!view.on_mouse(hover));
    CK_CHECK(!view.on_mouse(drag));
    CK_CHECK(session.take_pending_input().empty());

    // Cell motion: the drag arrives, the hover still does not.
    session.feed_output("\x1b[?1002h");
    CK_CHECK(!view.on_mouse(hover));
    CK_CHECK(session.take_pending_input().empty());
    CK_CHECK(view.on_mouse(drag));
    CK_CHECK(session.take_pending_input() == "\x1b[<32;5;2M");

    // All motion: both, which is what a program that asked for 1003 wants.
    session.feed_output("\x1b[?1003h");
    CK_CHECK(view.on_mouse(hover));
    CK_CHECK(session.take_pending_input() == "\x1b[<35;5;2M");
    CK_CHECK(view.on_mouse(drag));
    CK_CHECK(session.take_pending_input() == "\x1b[<32;5;2M");

    // And a child that stopped tracking hears nothing at all.
    session.feed_output("\x1b[?1003l");
    CK_CHECK(!view.on_mouse(press));
    CK_CHECK(session.take_pending_input().empty());
}

CK_TEST(a_session_that_names_no_tracking_level_still_gets_its_pointer_events) {
    // `TerminalSubsession` is a seam other projects implement. One that fills
    // in only the older, coarser `mouse_reporting_enabled` is saying "the child
    // is tracking" without saying at what level — which is not the same
    // statement as "not tracking", and reading it as one would silence every
    // pointer event such a host delivers.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{10, 3};
    CountingSubsession session(profile);
    session.names_tracking_level = false;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[?1000h\x1b[?1006h");

    const ckv::MouseEvent hover{ckv::MouseAction::Move, ckv::MouseButton::None, ckv::Point{4, 1},
                                {}, ckv::Modifier::None};
    CK_CHECK(view.on_mouse(hover));
    CK_CHECK(session.take_pending_input() == "\x1b[<35;5;2M");
}

CK_TEST(functional_keys_keep_the_encoding_they_already_had) {
    // The protocol deliberately does not renumber the keys that already had
    // an encoding; it only gives them the fields they never had room for.
    ckv::term::TerminalEmulator session;
    ckv::widgets::TerminalView view(session);
    Fixture fixture;
    view.set_context(fixture.context());
    view.set_bounds(ckv::Rect{0, 0, 10, 3});
    session.feed_output("\x1b[>1u");
    CK_CHECK(view.on_key(key_press(ckv::Key::Up, ckv::Modifier::Ctrl)));
    CK_CHECK(session.take_pending_input() == "\x1b[1;5A");
    CK_CHECK(view.on_key(key_press(ckv::Key::F5)));
    CK_CHECK(session.take_pending_input() == "\x1b[15~");
    CK_CHECK(view.on_key(key_press(ckv::Key::PageDown, ckv::Modifier::Shift)));
    CK_CHECK(session.take_pending_input() == "\x1b[6;2~");
}
