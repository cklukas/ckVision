// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The shape of the mouse pointer, as a view means it rather than as any
// one host spells it. A sibling of CursorState in cursor.hpp and separate
// from it on purpose: that is the text caret the application owns and
// draws, this is the host's own pointer, which ckVision can only ask for.
// The names hosts know it by, and whether a host knows any, belong to the
// term layer (term/pointer_shape_names.hpp).
#pragma once

#include <cstdint>

namespace ckv {

// The shapes ckVision asks for, named for what the pointer is over rather
// than for the picture a host draws. Deliberately far smaller than the CSS
// set the protocol defines: every member here is one a widget in this
// library actually requests, and a shape no widget can ask for would be an
// untested entry in two name tables.
enum class PointerShape : std::uint8_t {
    // Nothing under the pointer asked for anything. Emitted as a shape in
    // its own right, not as silence: leaving the previous shape in place
    // when the pointer moves off a control is exactly the bug that makes
    // the feature feel broken.
    Default,
    Text,        // selectable or editable text
    Pointer,     // something that activates when clicked
    Crosshair,   // a surface where the exact cell is the point
    Move,        // a handle that relocates what it belongs to
    ResizeEastWest,            // a vertical edge or divider
    ResizeNorthSouth,          // a horizontal edge or divider
    ResizeNorthWestSouthEast,  // top-left and bottom-right corners
    ResizeNorthEastSouthWest,  // top-right and bottom-left corners
    NotAllowed,  // present, hit-testable, and refusing
    Wait,        // the application cannot accept input right now
    Progress,    // the application is busy but still accepting input
};

// One past the last shape, for tables that must cover every member. Kept
// beside the enum so adding a shape without extending its name tables is a
// compile error rather than a silent Default at run time.
inline constexpr int kPointerShapeCount = 12;

}  // namespace ckv
