// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Mouse pointer shapes (OSC 22), end to end: the two host vocabularies and
// their degradations, the support query and the one reply shape that counts
// as an answer, what the Presenter puts on the wire and — just as
// important — what it does not, and which view under the pointer decides.
//
// The policy under test is that silence is not a refusal. OSC 22's support
// query is optional, and hosts that implement only the original xterm
// proposal draw the shapes perfectly well while never replying to anything,
// so a session emits shapes without waiting for permission and treats a
// reply as a refinement when one arrives.
#include "cvision/core/pointer_shape.hpp"

#include <optional>
#include <string>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/term/pointer_shape_names.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using namespace ckv;
using namespace ckv::term;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Button;
using ckv::widgets::Desktop;
using ckv::widgets::Window;

namespace {

// Every shape, so a table-driven check cannot quietly skip the member that
// was added last.
constexpr PointerShape kAllShapes[] = {
    PointerShape::Default,
    PointerShape::Text,
    PointerShape::Pointer,
    PointerShape::Crosshair,
    PointerShape::Grab,
    PointerShape::Grabbing,
    PointerShape::ResizeEastWest,
    PointerShape::ResizeNorthSouth,
    PointerShape::ResizeNorthWestSouthEast,
    PointerShape::ResizeNorthEastSouthWest,
    PointerShape::NotAllowed,
    PointerShape::Wait,
    PointerShape::Progress,
};

static_assert(std::size(kAllShapes) == static_cast<std::size_t>(kPointerShapeCount),
              "a new PointerShape needs a row here, and in both name tables");

Capabilities host_that_answered(std::uint32_t supported) {
    Capabilities caps = baseline_capabilities();
    caps.pointer_shape_vocabulary = PointerShapeVocabulary::Standard;
    caps.pointer_shapes_supported = supported;
    return caps;
}

std::uint32_t all_shapes_supported() {
    std::uint32_t mask = 0;
    for (const PointerShape shape : kAllShapes) mask |= pointer_shape_bit(shape);
    return mask;
}

struct PresenterFixture {
    HeadlessTerminal term{Size{20, 4}};
    Presenter presenter{term};
    std::vector<Cell> cells{static_cast<std::size_t>(20 * 4), Cell::from_grapheme(" ", Style{})};

    // The pointer is written on its own, never through a frame, so nothing
    // here has to paint one first.
    std::string present(PointerShape shape) {
        term.clear_written();
        presenter.present_pointer_shape(shape);
        return std::string(term.written_bytes());
    }
};

struct AppFixture {
    HeadlessTerminal term{Size{60, 20}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    AppFixture() { app.theme() = make_classic_theme(app.roles(), roles); }

    // `button` is what a real drag reports: motion with no button held is
    // how a window learns that a release it never saw has happened, so a
    // test that drags must hold the button it pressed.
    void move_to(Point cell, MouseButton button = MouseButton::None) {
        term.inject_event(MouseEvent{MouseAction::Move, button, cell, std::nullopt, Modifier::None});
        app.step(0);
    }
    void press_at(Point cell) {
        term.inject_event(MouseEvent{MouseAction::Down, MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }
};

}  // namespace

// --- Vocabularies and degradation ---------------------------------------

CK_TEST(every_shape_has_a_name_in_both_vocabularies) {
    // Including Default. Having no opinion is the ordinary arrow and is
    // named like anything else — the protocol's empty reset belongs to the
    // end of a session, not to a frame in the middle of one.
    for (const PointerShape shape : kAllShapes) {
        CK_CHECK(!pointer_shape_standard_name(shape).empty());
        CK_CHECK(!pointer_shape_legacy_name(shape).empty());
    }
    CK_CHECK(pointer_shape_legacy_name(PointerShape::Default) == "left_ptr");
    CK_CHECK(kPointerShapeResetSequence == "\x1B]22;\x1B\\");
}

CK_TEST(the_legacy_vocabulary_uses_only_names_both_known_host_families_accept) {
    // The X11 cursorfont names in the intersection of what a host
    // implementing the kitty specification accepts as an alias and what a
    // host implementing only the xterm proposal is known to draw. A name
    // outside this set is not a translation but a guess, and a guess that
    // misses resets the pointer rather than approximating it.
    const std::string_view accepted[] = {"left_ptr", "xterm",    "hand2",
                                          "crosshair", "fleur",   "watch",
                                          "X_cursor",  "sb_h_double_arrow", "sb_v_double_arrow"};
    for (const PointerShape shape : kAllShapes) {
        const std::string_view name = pointer_shape_legacy_name(shape);
        bool found = false;
        for (const std::string_view candidate : accepted) found = found || candidate == name;
        CK_CHECK(found);
    }
}

CK_TEST(the_diagonal_corners_degrade_to_a_pointer_the_legacy_vocabulary_really_has) {
    // Documented degradation, asserted rather than left to be discovered:
    // no diagonal corner arrow is reliably present in the legacy set, so a
    // corner says "resize" and understates only the axis. Deliberately not
    // the move pointer, which would name the one gesture the title bar
    // beside it really performs.
    CK_CHECK(pointer_shape_legacy_name(PointerShape::ResizeNorthWestSouthEast) == "crosshair");
    CK_CHECK(pointer_shape_legacy_name(PointerShape::ResizeNorthEastSouthWest) == "crosshair");
    // The hand belongs to the title bar, not to a corner -- and it cannot
    // close, so both halves of the grab pair are the open one here.
    CK_CHECK(pointer_shape_legacy_name(PointerShape::Grab) == "fleur");
    CK_CHECK(pointer_shape_legacy_name(PointerShape::Grabbing) == "fleur");
    // Never hand1: this vocabulary's two hands disagree across hosts. kitty
    // reads hand1 as the open "grab" hand, iTerm2 reads it as the pointing
    // finger, so a grab written as hand1 would be a finger on one of them.
    CK_CHECK(pointer_shape_legacy_name(PointerShape::Pointer) == "hand2");
    // The standard vocabulary keeps the distinction, because a host that
    // answers the query is a host that has the pointers.
    CK_CHECK(pointer_shape_standard_name(PointerShape::ResizeNorthWestSouthEast) == "nwse-resize");
    CK_CHECK(pointer_shape_standard_name(PointerShape::ResizeNorthEastSouthWest) == "nesw-resize");
}

CK_TEST(the_fallback_chain_terminates_at_default_from_every_shape) {
    for (PointerShape shape : kAllShapes) {
        int steps = 0;
        while (shape != PointerShape::Default) {
            shape = pointer_shape_fallback(shape);
            ++steps;
            CK_CHECK(steps <= kPointerShapeCount);
        }
    }
}

CK_TEST(an_unverified_host_is_asked_for_every_shape_it_was_never_asked_about) {
    const Capabilities caps = baseline_capabilities();
    CK_CHECK(caps.pointer_shape_vocabulary == PointerShapeVocabulary::Legacy);
    for (const PointerShape shape : kAllShapes) {
        CK_CHECK(host_draws_pointer_shape(caps, shape));
        CK_CHECK(effective_pointer_shape(caps, shape) == shape);
    }
}

CK_TEST(a_host_that_answered_is_taken_at_its_word_shape_by_shape) {
    Capabilities caps = host_that_answered(all_shapes_supported());
    CK_CHECK(effective_pointer_shape(caps, PointerShape::Progress) == PointerShape::Progress);

    // Withdraw exactly one shape: the request degrades one step down its
    // own chain rather than collapsing to nothing.
    caps.pointer_shapes_supported &= ~pointer_shape_bit(PointerShape::Progress);
    CK_CHECK(effective_pointer_shape(caps, PointerShape::Progress) == PointerShape::Wait);

    // Withdraw the fallback too, and it keeps walking.
    caps.pointer_shapes_supported &= ~pointer_shape_bit(PointerShape::Wait);
    CK_CHECK(effective_pointer_shape(caps, PointerShape::Progress) == PointerShape::Default);

    // A host with nothing at all still resolves, and resolves to the reset.
    const Capabilities barren = host_that_answered(0);
    for (const PointerShape shape : kAllShapes)
        CK_CHECK(effective_pointer_shape(barren, shape) == PointerShape::Default);
}

CK_TEST(a_profile_that_is_never_told_where_the_pointer_is_asks_for_no_shapes) {
    // Not caution about the escape code: these profiles have no mouse, so
    // there is no pointer position for a shape to be a function of.
    for (const TerminalProfile profile :
         {TerminalProfile::TmuxConservative, TerminalProfile::ScreenConservative,
          TerminalProfile::LinuxConsole}) {
        const Capabilities caps = capabilities_for_profile(profile);
        CK_CHECK(caps.mouse_protocol == MouseProtocol::None);
        CK_CHECK(!caps.pointer_shapes);
        CK_CHECK(!host_draws_pointer_shape(caps, PointerShape::Text));
    }
    CK_CHECK(capabilities_for_profile(TerminalProfile::ModernVt).pointer_shapes);
}

CK_TEST(an_application_can_silence_pointer_shapes_for_a_host_that_misbehaves) {
    CapabilityOverrides overrides;
    overrides.pointer_shapes = false;
    const Capabilities caps = apply_capability_overrides(baseline_capabilities(), overrides);
    CK_CHECK(!caps.pointer_shapes);
    CK_CHECK(effective_pointer_shape(caps, PointerShape::Text) == PointerShape::Default);
}

// --- The support query and its reply -------------------------------------

CK_TEST(the_support_query_names_every_shape_in_enum_order) {
    // The reply's flags are indexed straight back onto PointerShape, so the
    // query's order is load-bearing rather than cosmetic.
    std::string expected;
    for (const PointerShape shape : kAllShapes) {
        if (!expected.empty()) expected += ',';
        expected += pointer_shape_standard_name(shape);
    }
    CK_CHECK(std::string(kPointerShapeQueryNames) == expected);
    CK_CHECK(std::string(kPointerShapeQuerySequence) == "\x1B]22;?" + expected + "\x1B\\");
}

CK_TEST(a_support_reply_earns_the_standard_vocabulary_and_its_own_flags) {
    InputDecoder decoder(baseline_capabilities());
    const std::vector<TerminalEvent> events =
        decoder.feed("\x1B]22;1,1,1,1,1,1,1,1,0,0,1,1,0\x1B\\", 0);
    CK_CHECK(events.size() == 1);
    const auto& changed = std::get<CapabilityChangedEvent>(events.at(0));
    CK_CHECK(changed.capabilities.pointer_shape_vocabulary == PointerShapeVocabulary::Standard);
    CK_CHECK(host_draws_pointer_shape(changed.capabilities, PointerShape::Text));
    CK_CHECK(!host_draws_pointer_shape(changed.capabilities, PointerShape::ResizeNorthWestSouthEast));
    CK_CHECK(!host_draws_pointer_shape(changed.capabilities, PointerShape::Progress));
    // ...and the withdrawn ones degrade rather than disappearing.
    CK_CHECK(effective_pointer_shape(changed.capabilities, PointerShape::ResizeNorthWestSouthEast) ==
             PointerShape::Crosshair);
    CK_CHECK(effective_pointer_shape(changed.capabilities, PointerShape::Progress) == PointerShape::Wait);
}

CK_TEST(a_shape_name_coming_back_through_osc_22_is_not_mistaken_for_a_support_map) {
    // OSC 22 carries both the question and the answer, and a host may reply
    // with a NAME to a different query form. Reading one as a flag list
    // would invent support for whichever shapes its characters landed on.
    for (const std::string_view reply : {"\x1B]22;hand2\x1B\\", "\x1B]22;0\x1B\\",
                                          "\x1B]22;1\x1B\\", "\x1B]22;\x1B\\",
                                          "\x1B]22;1,2,1\x1B\\", "\x1B]22;1,,1\x1B\\"}) {
        InputDecoder decoder(baseline_capabilities());
        CK_CHECK(decoder.feed(reply, 0).empty());
    }
}

CK_TEST(an_over_long_flag_list_is_refused_rather_than_indexing_past_the_enum) {
    InputDecoder decoder(baseline_capabilities());
    std::string reply = "\x1B]22;";
    for (int i = 0; i < kPointerShapeCount + 4; ++i) reply += (i == 0 ? "1" : ",1");
    reply += "\x1B\\";
    CK_CHECK(decoder.feed(reply, 0).empty());
}

CK_TEST(a_session_that_asks_for_no_shapes_ignores_an_answer_about_them) {
    Capabilities caps = baseline_capabilities();
    caps.pointer_shapes = false;
    InputDecoder decoder(caps);
    CK_CHECK(decoder.feed("\x1B]22;1,1,1,1,1,1,1,1,1,1,1,1,1\x1B\\", 0).empty());
}

// --- What reaches the wire ----------------------------------------------

CK_TEST(a_session_states_the_ordinary_arrow_rather_than_leaving_it_to_the_host) {
    // The reverse of what politeness suggests, and deliberately. A host may
    // draw its own pointer while mouse reporting is on -- iTerm2 draws an
    // I-beam with a circle -- and over a full-screen application with
    // windows and menus that reads as a text cursor everywhere. Staying
    // quiet leaves it there, so the arrow is stated on the first frame.
    HeadlessTerminal term{Size{20, 4}};
    Presenter presenter{term};
    presenter.present_pointer_shape(PointerShape::Default);
    CK_CHECK(std::string(term.written_bytes()) == "\x1B]22;left_ptr\x1B\\");
    // ...and having said it once, it does not keep saying it.
    term.clear_written();
    presenter.present_pointer_shape(PointerShape::Default);
    CK_CHECK(term.written_bytes().empty());
}

CK_TEST(a_shape_is_written_once_and_not_restated_every_frame) {
    PresenterFixture f;
    CK_CHECK(f.present(PointerShape::Text) == "\x1B]22;xterm\x1B\\");
    CK_CHECK(f.present(PointerShape::Text).empty());
    CK_CHECK(f.present(PointerShape::Pointer) == "\x1B]22;hand2\x1B\\");
    // Back to having no opinion: that IS a change, and it is the arrow --
    // not the reset, which would hand the pointer back to whatever the host
    // draws over a program that is reporting the mouse.
    CK_CHECK(f.present(PointerShape::Default) == "\x1B]22;left_ptr\x1B\\");
    CK_CHECK(f.present(PointerShape::Default).empty());
}

CK_TEST(two_requests_this_host_cannot_tell_apart_are_written_once) {
    // Both corners degrade to the same legacy pointer. Re-sending it as the
    // pointer travels from one corner to the other would be bytes per
    // motion report for a screen that does not change.
    PresenterFixture f;
    CK_CHECK(f.present(PointerShape::Grab) == "\x1B]22;fleur\x1B\\");
    CK_CHECK(f.present(PointerShape::Grabbing).empty());  // no closed hand to change to
    // A shape that really is different still gets through.
    CK_CHECK(f.present(PointerShape::ResizeEastWest) == "\x1B]22;sb_h_double_arrow\x1B\\");
}

CK_TEST(a_re_present_re_states_the_shape_because_a_probe_may_have_reset_it) {
    // The support query is, to a host that implements only the xterm
    // proposal, an unknown shape name — and an unknown name resets the
    // pointer. Whatever forces a full re-present must therefore stop
    // assuming the host still has what it was last told.
    PresenterFixture f;
    CK_CHECK(f.present(PointerShape::Crosshair) == "\x1B]22;crosshair\x1B\\");
    f.presenter.invalidate();
    // invalidate() also forces a full repaint, so the frame's own cells are
    // on the wire beside the shape. What matters is that the shape is there
    // at all rather than being suppressed as unchanged.
    CK_CHECK(f.present(PointerShape::Crosshair).find("\x1B]22;crosshair\x1B\\") != std::string::npos);
}

CK_TEST(a_host_that_answered_is_written_to_in_its_own_vocabulary) {
    HeadlessTerminal term{Size{20, 4}};
    term.set_capabilities(host_that_answered(all_shapes_supported()));
    Presenter presenter{term};
    presenter.present_pointer_shape(PointerShape::ResizeNorthWestSouthEast);
    CK_CHECK(std::string(term.written_bytes()) == "\x1B]22;nwse-resize\x1B\\");
}

CK_TEST(a_silenced_host_is_written_nothing_at_all) {
    HeadlessTerminal term{Size{20, 4}};
    Capabilities caps = baseline_capabilities();
    caps.pointer_shapes = false;
    term.set_capabilities(caps);
    Presenter presenter{term};
    presenter.present_pointer_shape(PointerShape::Text);
    CK_CHECK(term.written_bytes().empty());
}

CK_TEST(a_pointer_that_merely_moved_does_not_become_a_frame) {
    // The pointer is host state, not frame content, and is written on its
    // own path for exactly that reason. A shape change must not be wrapped
    // in a synchronized-output bracket, must not be counted as a frame the
    // terminal is then asked to acknowledge, and must not cost a frame diff
    // over every cell to produce eighteen bytes.
    HeadlessTerminal term{Size{20, 4}};
    Capabilities caps = baseline_capabilities();
    caps.synchronized_output = true;
    term.set_capabilities(caps);
    Presenter presenter{term};
    presenter.set_frame_completion_tracking(true);
    std::vector<Cell> cells(static_cast<std::size_t>(20 * 4), Cell::from_grapheme(" ", Style{}));
    presenter.present(FrameView(cells.data(), Size{20, 4}), CursorState{}, {});
    const std::size_t marked_before = presenter.frames_marked();
    const std::size_t frame_bytes = presenter.last_bytes_emitted();
    term.clear_written();

    presenter.present_pointer_shape(PointerShape::Text);
    const std::string written(term.written_bytes());
    CK_CHECK(written == "\x1B]22;xterm\x1B\\");
    CK_CHECK(written.find("\x1B[?2026h") == std::string::npos);
    CK_CHECK(written.find("\x1B[5n") == std::string::npos);
    CK_CHECK(presenter.frames_marked() == marked_before);
    // It is not a frame, so it does not disturb the frame cost counter.
    CK_CHECK(presenter.last_bytes_emitted() == frame_bytes);
}

CK_TEST(the_deterministic_display_reads_back_the_shape_the_host_was_given) {
    // ckVision's own display model has to understand everything ckVision
    // emits, or a headless verification cannot see this feature at all.
    VirtualDisplay display{Size{10, 3}};
    CK_CHECK(display.pointer_shape_name().empty());
    CK_CHECK(display.write("\x1B]22;hand2\x1B\\"));
    CK_CHECK(display.pointer_shape_name() == "hand2");
    CK_CHECK(display.write("\x1B]22;\x1B\\"));
    CK_CHECK(display.pointer_shape_name().empty());
    // BEL terminates an OSC too, and BEL is a C0 byte that plain text may
    // not contain — so the state it is read in is what distinguishes them.
    CK_CHECK(display.write("\x1B]22;xterm\a"));
    CK_CHECK(display.pointer_shape_name() == "xterm");
}

// --- Which view decides ---------------------------------------------------

CK_TEST(the_pointer_takes_the_shape_of_whatever_it_is_over) {
    AppFixture f;
    auto* desktop = f.app.root().make<Desktop>();
    auto owned = std::make_unique<Window>("Shapes");
    owned->set_bounds(Rect{4, 2, 30, 12});
    Window* window = desktop->add_window(std::move(owned));
    auto button = std::make_unique<Button>("Press");
    button->set_bounds(Rect{2, 2, 10, 2});
    Button* pressable = static_cast<Button*>(window->add_child(std::move(button)));
    f.app.step(0);

    // Nothing under the pointer yet: no view, no opinion.
    CK_CHECK(f.app.hovered_view() == nullptr);
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);

    const Rect button_bounds = pressable->absolute_bounds();
    f.move_to(Point{button_bounds.x + 1, button_bounds.y});
    CK_CHECK(f.app.hovered_view() == pressable);
    CK_CHECK(pressable->hovered());
    CK_CHECK(f.app.pointer_shape() == PointerShape::Pointer);

    // A disabled control is still there and still refuses.
    pressable->set_enabled(false);
    CK_CHECK(f.app.pointer_shape() == PointerShape::NotAllowed);
    pressable->set_enabled(true);

    // Onto the window's own title row, which moves it.
    const Rect window_bounds = window->absolute_bounds();
    f.move_to(Point{window_bounds.x + 8, window_bounds.y});
    CK_CHECK(!pressable->hovered());
    CK_CHECK(f.app.hovered_view() == window);
    CK_CHECK(f.app.pointer_shape() == PointerShape::Grab);

    // Onto a corner, which resizes it along a diagonal.
    f.move_to(Point{window_bounds.x + window_bounds.width - 1,
                    window_bounds.y + window_bounds.height - 1});
    CK_CHECK(f.app.pointer_shape() == PointerShape::ResizeNorthWestSouthEast);
    f.move_to(Point{window_bounds.x, window_bounds.y + window_bounds.height - 1});
    CK_CHECK(f.app.pointer_shape() == PointerShape::ResizeNorthEastSouthWest);

    // Off everything: the desktop has no opinion, so neither do we.
    f.move_to(Point{1, 19});
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);
}

CK_TEST(a_resize_keeps_its_pointer_for_as_long_as_the_drag_lasts) {
    // A resize drag spends nearly all its life away from the corner it
    // started on. A shape that reverted the moment the pointer left the
    // grip would show the corner's affordance only while it was not in use.
    AppFixture f;
    auto* desktop = f.app.root().make<Desktop>();
    auto owned = std::make_unique<Window>("Drag");
    owned->set_bounds(Rect{4, 2, 30, 12});
    Window* window = desktop->add_window(std::move(owned));
    f.app.step(0);

    const Rect start = window->absolute_bounds();
    f.press_at(Point{start.x + start.width - 1, start.y + start.height - 1});
    CK_CHECK(f.app.pointer_shape() == PointerShape::ResizeNorthWestSouthEast);

    f.move_to(Point{start.x + start.width + 6, start.y + start.height + 3}, MouseButton::Left);
    CK_CHECK(f.app.hovered_view() == window);
    CK_CHECK(f.app.pointer_shape() == PointerShape::ResizeNorthWestSouthEast);

    f.term.inject_event(MouseEvent{MouseAction::Up, MouseButton::Left,
                                   Point{start.x + start.width + 6, start.y + start.height + 3},
                                   std::nullopt, Modifier::None});
    f.app.step(0);
    f.move_to(Point{1, 19});
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);
}

CK_TEST(a_view_with_no_opinion_lets_the_frame_around_it_answer) {
    // The one thing the optional return is for: a content pane fills its
    // window and cares about none of it, and must not overrule the border.
    AppFixture f;
    auto* desktop = f.app.root().make<Desktop>();
    auto owned = std::make_unique<Window>("Content");
    owned->set_bounds(Rect{4, 2, 30, 12});
    Window* window = desktop->add_window(std::move(owned));
    window->set_content(std::make_unique<ckv::ui::View>());
    f.app.step(0);

    const Rect bounds = window->absolute_bounds();
    f.move_to(Point{bounds.x + 6, bounds.y + 5});
    CK_CHECK(f.app.hovered_view() != window);   // the content pane took the hit
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);  // and had nothing to say

    f.move_to(Point{bounds.x + 6, bounds.y});
    CK_CHECK(f.app.pointer_shape() == PointerShape::Grab);
}

CK_TEST(a_view_destroyed_under_the_pointer_leaves_no_dangling_hover) {
    AppFixture f;
    auto* desktop = f.app.root().make<Desktop>();
    auto owned = std::make_unique<Window>("Doomed");
    owned->set_bounds(Rect{4, 2, 30, 12});
    Window* window = desktop->add_window(std::move(owned));
    f.app.step(0);

    const Rect bounds = window->absolute_bounds();
    f.move_to(Point{bounds.x + 8, bounds.y});
    CK_CHECK(f.app.hovered_view() == window);

    desktop->detach_child(window);  // returned ownership drops on this line
    CK_CHECK(f.app.hovered_view() == nullptr);
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);
    f.app.step(0);
}

CK_TEST(a_pointer_crossing_a_frame_that_needs_no_repaint_still_reaches_the_host) {
    // The regression this exists for: presentation used to be skipped
    // entirely when no cell was damaged, so the shape only reached the
    // terminal when something else happened to need repainting. It
    // therefore followed the pointer across the handful of widgets that
    // redraw on hover -- a button lighting up -- and nowhere else, which
    // looks exactly like a cursor that gets stuck.
    AppFixture f;
    auto* desktop = f.app.root().make<Desktop>();
    auto owned = std::make_unique<Window>("Static");
    owned->set_bounds(Rect{4, 2, 30, 12});
    Window* window = desktop->add_window(std::move(owned));
    // Content that draws the same thing wherever the pointer is, which is
    // what almost every dialog body is.
    window->set_content(std::make_unique<ckv::ui::View>());
    f.app.step(0);

    const Rect bounds = window->absolute_bounds();
    f.move_to(Point{bounds.x + 8, bounds.y});  // the title bar: a hand
    CK_CHECK(f.app.pointer_shape() == PointerShape::Grab);
    f.term.clear_written();

    // Down into the body. Nothing on screen changes; the pointer must.
    f.move_to(Point{bounds.x + 8, bounds.y + 4});
    CK_CHECK(f.app.pointer_shape() == PointerShape::Default);
    const std::string written(f.term.written_bytes());
    CK_CHECK(written == "\x1B]22;left_ptr\x1B\\");  // the shape alone, no frame

    // ...and having arrived, it is not restated on every later frame.
    f.term.clear_written();
    f.move_to(Point{bounds.x + 9, bounds.y + 4});
    CK_CHECK(f.term.written_bytes().empty());
}
