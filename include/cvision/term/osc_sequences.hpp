// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The operating-system commands a terminal session sends outside a frame:
// the window title (OSC 0) and a clipboard export (OSC 52). Every backend
// that speaks VT builds them here, so each one sends the same bytes and the
// safety argument is made once (ARCHITECTURE.md §12, docs/terminal-host-
// integration.md#osc-emission-safety).
#pragma once

#include <string>
#include <string_view>

namespace ckv::term {

// OSC 0 ; title BEL. The title passes through text::sanitize_osc_text first,
// so no control code point, DEL, or malformed byte of `title` reaches the
// host and nothing in it can end the sequence early or start another one.
std::string osc_title_sequence(std::string_view title);

// OSC 52 ; c ; base64 BEL, exporting `text` to the host's clipboard selection.
// The payload is RFC 4648 base64, whose alphabet holds no control byte, so
// the text is carried unchanged — tabs, line breaks and all — and can never
// terminate the sequence.
std::string osc_clipboard_sequence(std::string_view text);

}  // namespace ckv::term
