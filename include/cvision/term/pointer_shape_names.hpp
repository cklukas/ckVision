// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// What a PointerShape is called on the wire (OSC 22), and how a shape a
// host cannot draw degrades to one it can.
//
// The protocol has two vocabularies with the same escape code. The one
// specified by kitty names shapes after the CSS `cursor` property; the one
// the original xterm proposal used names them after X11's cursorfont. A
// host that implements the kitty specification answers a support query and
// therefore identifies itself; a host that implements only the xterm
// proposal answers nothing, so the legacy vocabulary is what silence gets.
#pragma once

#include <string_view>

#include "cvision/core/pointer_shape.hpp"

namespace ckv::term {

// Which spelling a session writes. Legacy is the unverified default, not a
// lesser choice: hosts that implement the kitty specification accept the
// X11 names as aliases as well, so the legacy vocabulary is the one that
// reaches every host known to implement any of this.
enum class PointerShapeVocabulary : std::uint8_t {
    Legacy,    // X11 cursorfont names, per the original xterm proposal
    Standard,  // CSS names, per the kitty specification
};

// The shape to ask for when `shape` itself is unavailable, and Default once
// nothing is left to try. Total and acyclic: repeated application
// terminates at Default for every member.
constexpr PointerShape pointer_shape_fallback(PointerShape shape) noexcept {
    switch (shape) {
        // A corner that cannot be drawn as a diagonal is still a resize, and
        // saying "resize" along one axis is nearer the truth than saying
        // "move" — which is a different gesture, and the one gesture the
        // title bar right next to it actually performs.
        case PointerShape::ResizeNorthWestSouthEast:
        case PointerShape::ResizeNorthEastSouthWest: return PointerShape::Crosshair;
        // A hand that cannot close is still the hand that took hold.
        case PointerShape::Grabbing: return PointerShape::Grab;
        // Busy-but-usable degrades to busy before it degrades to nothing:
        // the wait pointer overstates the situation, silence understates it.
        case PointerShape::Progress: return PointerShape::Wait;
        default: return PointerShape::Default;
    }
}

// The CSS name, which is also the name used to ask a host whether it has
// the shape. Every member of PointerShape maps to a name in the kitty
// specification's table, so this vocabulary needs no degradation of its
// own — a host that answers the query says per shape what it has, and
// pointer_shape_fallback covers what it does not.
constexpr std::string_view pointer_shape_standard_name(PointerShape shape) noexcept {
    switch (shape) {
        case PointerShape::Default: return "default";  // the ordinary arrow, stated not implied
        case PointerShape::Text: return "text";
        case PointerShape::Pointer: return "pointer";
        case PointerShape::Crosshair: return "crosshair";
        case PointerShape::Grab: return "grab";
        case PointerShape::Grabbing: return "grabbing";
        case PointerShape::ResizeEastWest: return "ew-resize";
        case PointerShape::ResizeNorthSouth: return "ns-resize";
        case PointerShape::ResizeNorthWestSouthEast: return "nwse-resize";
        case PointerShape::ResizeNorthEastSouthWest: return "nesw-resize";
        case PointerShape::NotAllowed: return "not-allowed";
        case PointerShape::Wait: return "wait";
        case PointerShape::Progress: return "progress";
    }
    return "default";  // exhaustive enum fallback for defensive builds
}

// The X11 cursorfont name, restricted to shapes an xterm-proposal host is
// known to draw. Two entries are therefore degradations rather than
// translations, and both are recorded here rather than left for a reader to
// discover from behaviour:
//
//   - The diagonal corner resizes have X11 names (`top_left_corner` and
//     friends), but a host that implements only the proposal maps names to
//     whatever its platform's pointer set contains, and the diagonal
//     corner arrow is not reliably in one — macOS has no public one at
//     all. An unrecognized name is worse than a substituted one: it resets
//     the pointer entirely, so a corner would lose its shape rather than
//     take a near one. They are written as the horizontal resize arrow,
//     which says "this resizes" — the part that matters — and leaves only
//     the axis understated. Not the move pointer: moving is a different
//     gesture, and it is the one the title bar an inch away really does.
//   - Progress has no legacy name distinct from Wait, so it borrows it.
//     The distinction survives on hosts that answer the query, which are
//     the hosts that have a separate pointer for it.
//
// Default is the ordinary arrow, and is stated rather than left to the
// host. Handing the pointer back with the protocol's reset would seem more
// polite, but a host is entitled to draw its own pointer while mouse
// reporting is on — iTerm2 draws an I-beam with a circle, to say that the
// mouse is being sent to the running program — and over a full-screen
// application with windows and menus that reads as a text cursor
// everywhere, which is the one thing it is not. An application that has
// taken over the screen owns the pointer on it. The reset belongs to the
// end of the session, where it hands the pointer back for good.
constexpr std::string_view pointer_shape_legacy_name(PointerShape shape) noexcept {
    switch (shape) {
        case PointerShape::Default: return "left_ptr";
        case PointerShape::Text: return "xterm";
        case PointerShape::Pointer: return "hand2";
        case PointerShape::Crosshair: return "crosshair";
        case PointerShape::Grab: return "fleur";
        // No host in this vocabulary has a closed hand, so the hand simply
        // does not close. The open one still says the window is in hand.
        case PointerShape::Grabbing: return "fleur";
        case PointerShape::ResizeEastWest: return "sb_h_double_arrow";
        case PointerShape::ResizeNorthSouth: return "sb_v_double_arrow";
        case PointerShape::ResizeNorthWestSouthEast:
        case PointerShape::ResizeNorthEastSouthWest: return "crosshair";
        case PointerShape::NotAllowed: return "X_cursor";
        case PointerShape::Wait:
        case PointerShape::Progress: return "watch";
    }
    return "";  // exhaustive enum fallback for defensive builds
}

constexpr std::string_view pointer_shape_name(PointerShape shape,
                                              PointerShapeVocabulary vocabulary) noexcept {
    return vocabulary == PointerShapeVocabulary::Standard ? pointer_shape_standard_name(shape)
                                                          : pointer_shape_legacy_name(shape);
}

// The payload of the support query, listing every shape in enum order so a
// reply's comma-separated answers index straight back onto PointerShape.
// Asking about `default` alongside the rest is redundant — ckVision emits
// the reset for it rather than a name — but keeping the list aligned with
// the enum is worth eight bytes sent once per session.
inline constexpr std::string_view kPointerShapeQueryNames =
    "default,text,pointer,crosshair,grab,grabbing,ew-resize,ns-resize,nwse-resize,"
    "nesw-resize,not-allowed,wait,progress";

// The full escape sequence asking which shapes a host has. A host that
// implements the kitty specification answers `OSC 22 ; 1,0,1,… ST`; one
// that implements only the xterm proposal reads the whole payload as a
// shape name, fails to match it, and resets its pointer — which is why a
// session re-asserts the shape it wants once the probe window closes.
inline constexpr std::string_view kPointerShapeQuerySequence =
    "\x1B]22;?default,text,pointer,crosshair,grab,grabbing,ew-resize,ns-resize,nwse-resize,"
    "nesw-resize,not-allowed,wait,progress\x1B\\";

// Returns the pointer to whatever the host would use of its own accord.
// Written when a session ends, so a shape never outlives the application
// that asked for it.
inline constexpr std::string_view kPointerShapeResetSequence = "\x1B]22;\x1B\\";

}  // namespace ckv::term
