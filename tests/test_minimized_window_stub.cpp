// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Where a put-away window goes (D-064). The question every one of these
// tests is really asking is the one a reader asks: after I press `_`, is my
// window anywhere?
#include "cvision/widgets/minimized_window_stub.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::MinimizedWindowPlacement;
using ckv::widgets::MinimizedWindowStub;
using ckv::widgets::StatusLine;
using ckv::widgets::Window;
namespace ui = ckv::ui;

namespace {

// The desktop under the application's own root, the way every ckVision
// application builds one: these tests press stubs through Application::
// dispatch and read them out of the composed frame, so nothing here may be
// driven by hand.
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Fixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = static_cast<Desktop*>(
            app.root().add_child(std::make_unique<Desktop>(app.root().bounds())));
        app.step(0);
    }

    Window* open(std::string title, Rect where = Rect{4, 2, 40, 10}) {
        Window* const window = desktop->add_window(std::make_unique<Window>(std::move(title)));
        window->set_bounds(where);
        return window;
    }
};

// The one row a stub draws, read off its own bounds out of the composed
// desktop — what a reader looking at the screen would see, not what the
// widget says about itself.
std::string row_at(Fixture& f, Rect where) {
    f.app.step(0);
    const ckv::FrameView frame = f.term.display().frame();
    std::string row;
    for (int x = where.x; x < where.x + where.width; ++x)
        row += frame.at(Point{x, where.y}).grapheme();
    return row;
}

bool screen_contains(Fixture& f, std::string_view needle) {
    f.app.step(0);
    const ckv::FrameView frame = f.term.display().frame();
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(Point{x, y}).grapheme();
        if (row.find(needle) != std::string::npos) return true;
    }
    return false;
}

ckv::MouseEvent press_at(Point cell) {
    return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                           Modifier::None};
}
ckv::MouseEvent release_at(Point cell) {
    return ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                           Modifier::None};
}
ckv::MouseEvent drag_to(Point cell) {
    return ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, cell, std::nullopt,
                           Modifier::None};
}
// A click is a press AND a release on the same cell — the row decides on the
// release, like the window frame's own controls (see the press/release tests).
bool click_at(Application& app, Point cell) {
    const bool pressed = app.dispatch(press_at(cell));
    const bool released = app.dispatch(release_at(cell));
    return pressed && released;
}

}  // namespace

// --- The default ----------------------------------------------------------

CK_TEST(a_desktop_parks_a_minimized_window_where_the_reader_can_see_it) {
    // The whole bug, as a test: before D-064 this window was hidden and
    // NOTHING was drawn for it, on a desktop whose only window it was.
    Fixture f;
    Window* const window = f.open("config.yaml");
    CK_CHECK(f.desktop->minimized_window_placement() == MinimizedWindowPlacement::Parked);
    CK_CHECK(f.desktop->parked_windows().empty());
    CK_CHECK(screen_contains(f, "config.yaml"));

    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    CK_CHECK(f.desktop->parked_windows().front()->window() == window);
    // Hidden, but not gone: the name is still on the screen, on the row the
    // stub was parked in.
    CK_CHECK(!window->visible());
    CK_CHECK(screen_contains(f, "config.yaml"));
}

CK_TEST(a_parked_stub_draws_the_windows_own_frame_with_its_two_remaining_verbs) {
    Fixture f;
    Window* const window = f.open("notes");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();

    const std::string row = row_at(f, stub->bounds());
    // One row, the frame's own vocabulary: corners, rule, the close square
    // and the restore arrow. Spelled out rather than compared to a golden,
    // because what is being asserted is that a reader can READ it.
    CK_CHECK(row == "┌─[■] notes [↑]─┐");
    CK_CHECK(row.starts_with("┌"));
    CK_CHECK(row.ends_with("┐"));
    CK_CHECK(row.find("[■]") != std::string::npos);
    CK_CHECK(row.find("[↑]") != std::string::npos);
    CK_CHECK(row.find("notes") != std::string::npos);
    // The control the window has already used is NOT offered again.
    CK_CHECK(row.find("[_]") == std::string::npos);
}

CK_TEST(a_stub_is_parked_on_the_desktops_bottom_edge_above_the_status_line) {
    Fixture f;
    f.desktop->dock_bottom(std::make_unique<StatusLine>());
    f.app.step(0);
    Window* const window = f.open("one");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    const Rect where = f.desktop->parked_windows().front()->bounds();
    // Row 22 on a 24-row desktop with a one-row status line: the last row
    // that is not furniture.
    CK_CHECK(where.height == 1);
    CK_CHECK(where.y == 22);
    CK_CHECK(where.x == 0);
}

CK_TEST(parked_stubs_stand_in_the_order_their_windows_were_put_away) {
    Fixture f;
    Window* const first = f.open("alpha", Rect{2, 2, 30, 8});
    Window* const second = f.open("beta", Rect{34, 2, 30, 8});
    // Put away in the opposite order to the one they were opened in, so a
    // parking row that merely echoed windows() would get this wrong.
    second->set_minimized(true);
    first->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 2U);
    CK_CHECK(f.desktop->parked_windows()[0]->window() == second);
    CK_CHECK(f.desktop->parked_windows()[1]->window() == first);
    // Side by side with a cell between them, neither overlapping the other.
    const Rect left = f.desktop->parked_windows()[0]->bounds();
    const Rect right = f.desktop->parked_windows()[1]->bounds();
    CK_CHECK(left.y == right.y);
    CK_CHECK(right.x >= left.x + left.width + 1);
}

// --- Getting the window back ---------------------------------------------

CK_TEST(clicking_a_parked_stub_brings_its_window_back_the_size_it_left) {
    Fixture f;
    const Rect where{6, 3, 44, 11};
    Window* const window = f.open("config.yaml", where);
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    const Rect stub = f.desktop->parked_windows().front()->bounds();

    CK_CHECK(click_at(f.app, Point{stub.x + stub.width / 2, stub.y}));
    CK_CHECK(!window->minimized());
    CK_CHECK(window->visible());
    // D-056: restoring replays nothing, because nothing was recorded.
    CK_CHECK(window->bounds() == where);
    // And the row it was parked in is gone with it.
    CK_CHECK(f.desktop->parked_windows().empty());
    CK_CHECK(f.desktop->active_window() == window);
}

CK_TEST(the_restore_control_and_the_caption_are_the_same_request) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    // Aimed at the arrow specifically, rather than anywhere on the row.
    int restore_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_restore_control(Point{x, 0})) restore_x = x;
    CK_CHECK(restore_x > 0);
    CK_CHECK(click_at(f.app, Point{where.x + restore_x, where.y}));
    CK_CHECK(!window->minimized());
}

CK_TEST(a_parked_window_can_be_got_back_without_a_mouse) {
    // A window reachable only by pointer is only half reachable, and the
    // desktop this feature exists for is a terminal.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    CK_CHECK(stub->focusable());
    f.app.set_focus(stub);
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, Modifier::None, ""}}));
    CK_CHECK(!window->minimized());
}

CK_TEST(closing_a_parked_window_from_its_stub_still_asks_the_window_first) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    bool asked = false;
    // The refusal an unsaved editor makes. A stub that closed the window
    // behind close_request's back would lose the reader's work from a row
    // they cannot even see the document in.
    window->close_request = [&asked] {
        asked = true;
        return false;
    };
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    int close_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_close_control(Point{x, 0})) close_x = x;
    CK_CHECK(close_x > 0);

    CK_CHECK(click_at(f.app, Point{where.x + close_x, where.y}));
    CK_CHECK(asked);
    // Refused, so the window is still here — and so is its row.
    CK_CHECK(f.desktop->windows().size() == 1U);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);

    window->close_request = [] { return true; };
    CK_CHECK(click_at(f.app, Point{where.x + close_x, where.y}));
    // Window::close with no on_closed schedules its own detach rather than
    // deleting itself inside the click, so the removal lands on the step.
    f.app.step(0);
    CK_CHECK(f.desktop->windows().empty());
    CK_CHECK(f.desktop->parked_windows().empty());
}

CK_TEST(a_parked_windows_row_goes_when_the_window_is_removed_by_its_application) {
    // Not every window leaves through its own close control: an application
    // removes windows for reasons of its own, and a row left pointing at
    // freed storage would be the worst of the failures this feature can have.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    const std::unique_ptr<Window> taken = f.desktop->remove_window(window);
    CK_CHECK(taken != nullptr);
    CK_CHECK(f.desktop->parked_windows().empty());
    CK_CHECK(!screen_contains(f, "config.yaml"));
}

CK_TEST(a_parked_stub_follows_its_windows_name) {
    Fixture f;
    Window* const window = f.open("untitled");
    window->set_minimized(true);
    CK_CHECK(screen_contains(f, "untitled"));
    window->set_title("config.yaml");
    CK_CHECK(screen_contains(f, "config.yaml"));
    CK_CHECK(!screen_contains(f, "untitled"));
}

// --- Where it sits --------------------------------------------------------

CK_TEST(a_parked_stub_is_not_covered_by_a_maximized_window) {
    // The one property that decides whether this feature works at all. A
    // stub drawn under the windows would be exactly as reachable as the
    // hidden window it stands for.
    Fixture f;
    Window* const parked = f.open("parked", Rect{2, 2, 30, 8});
    Window* const cover = f.open("cover", Rect{2, 2, 30, 8});
    parked->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    cover->toggle_zoom(f.desktop->content_area());
    f.app.step(0);
    CK_CHECK(cover->maximized());
    CK_CHECK(screen_contains(f, "parked"));
}

CK_TEST(the_parking_row_is_re_laid_when_the_desktop_resizes) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().front()->bounds().y == 23);
    f.desktop->set_bounds(Rect{0, 0, 80, 18});
    f.app.step(0);
    CK_CHECK(f.desktop->parked_windows().front()->bounds().y == 17);
}

// --- The other two placements --------------------------------------------

CK_TEST(a_host_that_lists_its_own_windows_gets_the_bare_hiding_it_builds_on) {
    // ckmux and anything else with its own switcher bar: the window goes
    // away and this Desktop draws nothing for it, which is what it did
    // before there was a choice to make.
    Fixture f;
    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::HostListed);
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(window->minimized());
    CK_CHECK(!window->visible());
    CK_CHECK(f.desktop->parked_windows().empty());
    CK_CHECK(!screen_contains(f, "config.yaml"));
    // Still listed, which is what a host's own bar reads (D-056).
    CK_CHECK(f.desktop->windows().size() == 1U);
}

CK_TEST(switching_placement_settles_the_windows_that_are_already_put_away) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    // Away from Parked: the row comes down, the window stays away.
    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::HostListed);
    CK_CHECK(f.desktop->parked_windows().empty());
    CK_CHECK(window->minimized());
    // Back to Parked: the row goes up again for what is still minimized.
    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::Parked);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    CK_CHECK(f.desktop->parked_windows().front()->window() == window);
}

CK_TEST(disabling_minimizing_takes_the_control_off_every_window) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    CK_CHECK(window->minimizable());
    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::Disabled);
    CK_CHECK(!window->minimizable());
    // Including one that arrives after the switch.
    Window* const later = f.open("later", Rect{10, 4, 30, 8});
    CK_CHECK(!later->minimizable());
    // And a window minimized past the missing control does not stay away:
    // Disabled means there is no state in which a window is unreachable.
    later->set_minimized(true);
    CK_CHECK(!later->minimized());
    CK_CHECK(later->visible());
    CK_CHECK(f.desktop->parked_windows().empty());
}

CK_TEST(disabling_minimizing_does_not_strand_a_window_that_was_already_away) {
    // Turning the feature off while a window is parked would otherwise take
    // away the only control that could bring it back.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::Disabled);
    CK_CHECK(!window->minimized());
    CK_CHECK(window->visible());
    CK_CHECK(f.desktop->parked_windows().empty());
}

CK_TEST(leaving_disabled_gives_the_control_back_only_to_the_windows_it_was_taken_from) {
    // A window an application fixed itself — a modal is the case that
    // matters, and Desktop::present_modal does exactly this — must not be
    // handed a minimize control by a placement switch it had no part in.
    Fixture f;
    Window* const ordinary = f.open("ordinary", Rect{2, 2, 30, 8});
    Window* const fixed = f.open("fixed", Rect{34, 2, 30, 8});
    fixed->set_minimizable(false);

    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::Disabled);
    CK_CHECK(!ordinary->minimizable());
    CK_CHECK(!fixed->minimizable());

    f.desktop->set_minimized_window_placement(MinimizedWindowPlacement::Parked);
    CK_CHECK(ordinary->minimizable());
    CK_CHECK(!fixed->minimizable());
}

CK_TEST(a_window_that_arrives_already_minimized_arrives_with_its_row) {
    // "Open this in the background" — the window is never activated, so the
    // minimize observer that normally parks it never fires.
    Fixture f;
    auto owned = std::make_unique<Window>("background");
    owned->set_minimized(true);
    Window* const window = f.desktop->add_window(std::move(owned));
    CK_CHECK(window->minimized());
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    CK_CHECK(screen_contains(f, "background"));
}

CK_TEST(removing_a_parked_stubs_popup_takes_it_out_of_the_parked_registry) {
    // parked_windows() holds raw pointers to views the POPUP list owns, so
    // every path that destroys a stub must scrub the registry — not just
    // unpark_window. A Desktop tears its children down through
    // remove_child(), which routes a stub to remove_popup(), and a registry
    // still holding that pointer is read moments later by the next window's
    // removal, on freed memory. That read is what ASan catches (Linux only,
    // as it happened); this is the same defect one step earlier, where it is
    // a number rather than a crash — and therefore visible on every platform,
    // with no sanitizer at all. The window itself is deliberately left as it
    // is: a host that removes a stub popup by hand has asked for that, and
    // teardown must never resurrect windows.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const std::unique_ptr<ui::View> taken = f.desktop->remove_popup(stub);
    CK_CHECK(taken != nullptr);
    CK_CHECK(f.desktop->parked_windows().empty());
}

// --- Press arms, release decides -------------------------------------------
//
// The owner's report, verbatim in substance: the row's controls reacted on
// the button going DOWN, where the window frame's close and restore controls
// react on the button coming UP — so a reader could not press, move away, and
// let go without the action happening. These cases pin the frame's semantics
// onto the row.

CK_TEST(the_close_control_fires_on_release_and_not_on_the_press) {
    Fixture f;
    int closes = 0;
    Window* const window = f.open("config.yaml");
    window->close_request = [&closes] {
        ++closes;
        return false;  // refused, so the row stays and can be pressed again
    };
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    int close_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_close_control(Point{x, 0})) close_x = x;
    const Point on_close{where.x + close_x, where.y};

    CK_CHECK(f.app.dispatch(press_at(on_close)));
    CK_CHECK(closes == 0);  // armed, not fired
    CK_CHECK(f.app.dispatch(release_at(on_close)));
    CK_CHECK(closes == 1);  // decided on the release
}

CK_TEST(a_press_on_the_close_control_released_elsewhere_is_taken_back) {
    Fixture f;
    int closes = 0;
    int restores = 0;
    Window* const window = f.open("config.yaml");
    window->close_request = [&closes] {
        ++closes;
        return false;
    };
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    stub->on_restore = [&restores] { ++restores; };  // observe without restoring
    const Rect where = stub->bounds();
    int close_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_close_control(Point{x, 0})) close_x = x;
    const Point on_close{where.x + close_x, where.y};
    const Point on_caption{where.x + where.width / 2, where.y};
    const Point off_row{where.x + close_x, where.y - 3};

    // Down on close, slide onto the caption, up there: neither control fires —
    // not the one it went down on, and not the one it came up on.
    CK_CHECK(f.app.dispatch(press_at(on_close)));
    CK_CHECK(f.app.dispatch(drag_to(on_caption)));
    CK_CHECK(f.app.dispatch(release_at(on_caption)));
    CK_CHECK(closes == 0);
    CK_CHECK(restores == 0);

    // Down on close, slide off the row entirely, up out there: nothing.
    CK_CHECK(f.app.dispatch(press_at(on_close)));
    CK_CHECK(f.app.dispatch(drag_to(off_row)));
    f.app.dispatch(release_at(off_row));
    CK_CHECK(closes == 0);
    CK_CHECK(restores == 0);
    // And the row is still here, armed no longer, ready for a real click.
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    CK_CHECK(f.app.dispatch(press_at(on_close)));
    CK_CHECK(f.app.dispatch(release_at(on_close)));
    CK_CHECK(closes == 1);
}

CK_TEST(a_press_on_the_caption_released_off_the_row_does_not_restore) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    const Point on_caption{where.x + where.width / 2, where.y};
    const Point off_row{where.x + where.width / 2, where.y - 4};

    CK_CHECK(f.app.dispatch(press_at(on_caption)));
    CK_CHECK(window->minimized());  // armed, not restored
    CK_CHECK(f.app.dispatch(drag_to(off_row)));
    f.app.dispatch(release_at(off_row));
    CK_CHECK(window->minimized());  // taken back
    CK_CHECK(f.desktop->parked_windows().size() == 1U);

    // Slid off and back on again before letting go: the press still counts,
    // because it was released over what it went down on.
    CK_CHECK(f.app.dispatch(press_at(on_caption)));
    CK_CHECK(f.app.dispatch(drag_to(off_row)));
    CK_CHECK(f.app.dispatch(drag_to(on_caption)));
    CK_CHECK(f.app.dispatch(release_at(on_caption)));
    CK_CHECK(!window->minimized());
}

CK_TEST(the_armed_control_wears_the_pressed_face_only_while_the_pointer_is_over_it) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    int close_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_close_control(Point{x, 0})) close_x = x;
    const Point on_close{where.x + close_x, where.y};
    const Point off_row{where.x + close_x, where.y - 3};
    const ckv::ui::RoleId pressed_role = f.app.roles().find("ckv.window.control.pressed");
    const ckv::ui::RoleId control_role = f.app.roles().find("ckv.window.control");
    const ckv::Style pressed = f.app.theme().resolve(pressed_role);
    const ckv::Style control = f.app.theme().resolve(control_role);
    CK_CHECK(pressed.fg != control.fg || pressed.bg != control.bg);  // or the face says nothing

    f.app.step(0);
    const ckv::Style at_rest = f.app.current_frame().at(on_close).style();
    CK_CHECK(f.app.dispatch(press_at(on_close)));
    f.app.step(0);
    const ckv::Style armed = f.app.current_frame().at(on_close).style();
    CK_CHECK(armed.fg == pressed.fg);
    CK_CHECK(armed.fg != at_rest.fg || armed.bg != at_rest.bg);
    CK_CHECK(f.app.dispatch(drag_to(off_row)));
    f.app.step(0);
    const ckv::Style disarmed = f.app.current_frame().at(on_close).style();
    CK_CHECK(disarmed.fg == at_rest.fg && disarmed.bg == at_rest.bg);
    f.app.dispatch(release_at(off_row));
}

CK_TEST(a_release_the_row_never_saw_disarms_it_when_the_pointer_comes_back) {
    // Releasing outside the terminal delivers the release to nobody. The
    // next thing this row hears is motion with no button held — and that is
    // the proof the press ended. Without this, a row could stay drawn armed
    // forever, and a later release over it would fire a press the reader let
    // go of somewhere else long ago.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    const Point on_caption{where.x + where.width / 2, where.y};
    CK_CHECK(f.app.dispatch(press_at(on_caption)));
    CK_CHECK(window->minimized());
    // The pointer comes back over the row with no button down.
    f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::None, on_caption,
                                   std::nullopt, Modifier::None});
    // A release now is a release of nothing: the press it would have decided
    // ended where this row could not see it.
    f.app.dispatch(release_at(on_caption));
    CK_CHECK(window->minimized());
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    // And a fresh click still works.
    CK_CHECK(click_at(f.app, on_caption));
    CK_CHECK(!window->minimized());
}

// --- The keyboard half of the same grammar ---------------------------------

namespace {
ckv::KeyEvent enter_press(bool reports_release) {
    ckv::KeyEvent event{ckv::KeyChord{ckv::Key::Enter, Modifier::None, ""}};
    event.action = ckv::KeyAction::Press;
    event.reports_release = reports_release;
    return event;
}
ckv::KeyEvent enter_release() {
    ckv::KeyEvent event{ckv::KeyChord{ckv::Key::Enter, Modifier::None, ""}};
    event.action = ckv::KeyAction::Release;
    event.reports_release = true;
    return event;
}
}  // namespace

CK_TEST(on_a_session_that_reports_releases_enter_restores_when_the_key_comes_up) {
    // Button's rule (D-055), on the stub: the press arms, the release commits.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    f.app.set_focus(stub);
    CK_CHECK(f.app.dispatch(enter_press(/*reports_release=*/true)));
    CK_CHECK(window->minimized());  // armed, not restored
    CK_CHECK(f.app.dispatch(enter_release()));
    CK_CHECK(!window->minimized());
    CK_CHECK(f.desktop->parked_windows().empty());
}

CK_TEST(escape_takes_back_a_keyboard_press_in_flight_on_the_stub) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    f.app.set_focus(stub);
    CK_CHECK(f.app.dispatch(enter_press(true)));
    ckv::KeyEvent escape{ckv::KeyChord{ckv::Key::Escape, Modifier::None, ""}};
    escape.action = ckv::KeyAction::Press;
    CK_CHECK(f.app.dispatch(escape));  // consumed, because it cancelled something
    CK_CHECK(f.app.dispatch(enter_release()) == false);  // nothing left to commit
    CK_CHECK(window->minimized());
    CK_CHECK(f.desktop->parked_windows().size() == 1U);
    // And Escape with nothing in flight is not this row's to consume.
    CK_CHECK(f.app.dispatch(escape) == false);
}

CK_TEST(focus_moving_away_takes_back_a_keyboard_press_in_flight_on_the_stub) {
    Fixture f;
    Window* const first = f.open("config.yaml");
    Window* const second = f.open("notes.md", Rect{20, 5, 30, 8});
    first->set_minimized(true);
    second->set_minimized(true);
    CK_CHECK(f.desktop->parked_windows().size() == 2U);
    MinimizedWindowStub* const stub_a = f.desktop->parked_windows()[0];
    MinimizedWindowStub* const stub_b = f.desktop->parked_windows()[1];
    f.app.set_focus(stub_a);
    CK_CHECK(f.app.dispatch(enter_press(true)));
    f.app.set_focus(stub_b);  // Tab, or anything else, moves focus away
    CK_CHECK(f.app.dispatch(enter_release()) == false);
    CK_CHECK(first->minimized());
    CK_CHECK(second->minimized());
    CK_CHECK(f.desktop->parked_windows().size() == 2U);
}

CK_TEST(on_a_session_that_cannot_report_a_release_enter_acts_at_once_and_is_seen_to) {
    // Legacy bytes never report a release: waiting would wait forever. The
    // press acts immediately — and is acknowledged visibly, so an accepted
    // keystroke does not look exactly like an ignored one.
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    f.app.set_focus(stub);
    CK_CHECK(f.app.dispatch(enter_press(/*reports_release=*/false)));
    CK_CHECK(!window->minimized());
    CK_CHECK(f.desktop->parked_windows().empty());
    // The flash timer then finds the row gone, and must be harmless about it.
    f.clock.advance(200'000'000);
    f.app.step(0);
    CK_CHECK(!window->minimized());
}

CK_TEST(a_keyboard_armed_stub_wears_the_pressed_face_on_its_restore_control) {
    Fixture f;
    Window* const window = f.open("config.yaml");
    window->set_minimized(true);
    MinimizedWindowStub* const stub = f.desktop->parked_windows().front();
    const Rect where = stub->bounds();
    int restore_x = -1;
    for (int x = 0; x < where.width; ++x)
        if (stub->point_in_restore_control(Point{x, 0})) restore_x = x;
    const Point on_restore{where.x + restore_x, where.y};
    const ckv::ui::RoleId pressed_role = f.app.roles().find("ckv.window.control.pressed");
    const ckv::Style pressed = f.app.theme().resolve(pressed_role);
    f.app.set_focus(stub);
    f.app.step(0);
    const ckv::Style at_rest = f.app.current_frame().at(on_restore).style();
    CK_CHECK(f.app.dispatch(enter_press(true)));
    f.app.step(0);
    const ckv::Style armed = f.app.current_frame().at(on_restore).style();
    CK_CHECK(armed.fg == pressed.fg);
    CK_CHECK(armed.fg != at_rest.fg || armed.bg != at_rest.bg);
    CK_CHECK(f.app.dispatch(enter_release()));
    CK_CHECK(!window->minimized());
}
