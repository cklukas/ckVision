// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The operating-system commands ckVision sends: the window title (OSC 0)
// and a clipboard export (OSC 52), which a session sends outside a frame, and
// the hyperlink brackets (OSC 8) the Presenter puts around linked text inside
// one. Every emitter builds them here, so each one sends the same bytes and
// the safety argument is made once (ARCHITECTURE.md §12, docs/terminal-host-
// integration.md#osc-emission-safety).
#pragma once

#include "cvision/core/export.hpp"

#include <string>
#include <string_view>

namespace ckv::term {

// OSC 0 ; title BEL. The title passes through text::sanitize_osc_text first,
// so no control code point, DEL, or malformed byte of `title` reaches the
// host and nothing in it can end the sequence early or start another one.
CVISION_API std::string osc_title_sequence(std::string_view title);

// OSC 52 ; c ; base64 BEL, exporting `text` to the host's clipboard selection.
// The payload is RFC 4648 base64, whose alphabet holds no control byte, so
// the text is carried unchanged — tabs, line breaks and all — and can never
// terminate the sequence.
CVISION_API std::string osc_clipboard_sequence(std::string_view text);

// OSC 8 ; id=<hex> ; target ST, opening a hyperlink to `target` for the text
// that follows (D-088). The target is sent only when it is a valid terminal
// hyperlink (is_valid_hyperlink_target: printable ASCII, no space, an absolute
// URI); anything else is dropped rather than repaired, and the result is then
// empty. The `id` is hyperlink_id(target), so every run of a link — split by
// a wrapped line, by a cursor re-address, or by a later partial repaint —
// belongs to one hyperlink on the host.
CVISION_API std::string osc_hyperlink_open(std::string_view target);

// Appends the OSC 8 opening sequence for `target` directly to `out` without
// intermediate string allocation; returns false and leaves `out` unchanged if
// `target` is not a valid terminal hyperlink.
CVISION_API bool append_osc_hyperlink_open(std::string& out, std::string_view target);

// OSC 8 ; ; ST, ending the hyperlink that osc_hyperlink_open began.
CVISION_API std::string_view osc_hyperlink_close() noexcept;

// The OSC 8 `id` parameter value for `target`: 16 lowercase hexadecimal
// digits of the target's 64-bit FNV-1a hash. A terminal groups the cells of
// one hyperlink by id and target together, so equal targets share a group and
// a hash collision between different targets groups nothing.
CVISION_API std::string hyperlink_id(std::string_view target);

}  // namespace ckv::term
