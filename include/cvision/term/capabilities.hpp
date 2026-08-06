// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Terminal capability model (ARCHITECTURE.md §4). A plain, inspectable
// struct: baseline profile at startup, refined by runtime probes,
// overridable by applications. Every field has a defined degradation.
#pragma once

#include "cvision/core/geometry.hpp"

namespace ckv::term {

enum class ColorDepth {
    Mono16,
    Color256,
    TrueColor,
};

enum class KeyboardProtocol {
    Legacy,           // bare escape sequences; ESC-vs-Alt needs a timeout (D-019/decoder)
    ModifyOtherKeys,  // xterm modifyOtherKeys level 2
    Kitty,            // kitty keyboard protocol: disambiguated, key-release capable
};

enum class MouseProtocol {
    None,
    X10,  // legacy, no release events, coordinates capped at 223
    SGR,  // modern, unlimited coordinates, distinguishes press/release
};

enum class ColorScheme {
    Unknown,
    Dark,
    Light,
};

struct Capabilities {
    ColorDepth color_depth = ColorDepth::TrueColor;
    KeyboardProtocol keyboard_protocol = KeyboardProtocol::Legacy;
    MouseProtocol mouse_protocol = MouseProtocol::SGR;
    bool pixel_mouse = false;        // SGR-Pixels (mode 1016): mouse events carry pixel coords
    Size cell_pixels;                // reported cell pixel size (XTWINOPS); {0,0} = unknown
    bool sixel_graphics = false;
    bool kitty_graphics = false;
    bool synchronized_output = false;  // DEC mode 2026
    bool clipboard_write = false;      // OSC 52
    bool focus_events = false;         // DEC mode 1004
    bool bracketed_paste = false;      // DEC mode 2004
    bool hyperlinks = false;           // OSC 8
    ColorScheme color_scheme = ColorScheme::Unknown;

    // D-019 width-agreement policy: whether East-Asian-Ambiguous-width
    // codepoints should be treated as wide. The documented v1 default
    // is narrow (false) everywhere; a per-profile override lives here
    // for when real-world data justifies one (tracked under D-OPEN-7).
    bool ambiguous_width_is_wide = false;

    friend bool operator==(const Capabilities&, const Capabilities&) = default;
};

// The conservative baseline every session starts on, before any probe
// response arrives — the first frame ships on this, never blocking for
// refinement (ARCHITECTURE.md §4).
constexpr Capabilities baseline_capabilities() noexcept {
    Capabilities caps;
    caps.color_depth = ColorDepth::Color256;
    caps.keyboard_protocol = KeyboardProtocol::Legacy;
    caps.mouse_protocol = MouseProtocol::SGR;
    caps.pixel_mouse = false;
    caps.sixel_graphics = false;
    caps.kitty_graphics = false;
    caps.synchronized_output = false;
    caps.clipboard_write = false;
    caps.focus_events = false;
    caps.bracketed_paste = false;
    caps.hyperlinks = false;
    caps.color_scheme = ColorScheme::Unknown;
    caps.ambiguous_width_is_wide = false;
    return caps;
}

}  // namespace ckv::term
