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
    // xterm modifyOtherKeys level 2. This is host-selected: its public
    // protocol lacks a stack that would let a session restore an arbitrary
    // pre-existing host setting exactly.
    ModifyOtherKeys,
    // Kitty keyboard protocol: disambiguated, key-release capable. The POSIX
    // backend uses kitty's documented push/pop stack around its session.
    Kitty,
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

// A host-selected baseline identity. ckVision never reads TERM or any other
// environment variable in library code: the embedding host supplies this
// choice explicitly, then may use runtime probes or a fully custom
// Capabilities value to refine it (D-004, ARCHITECTURE.md §4).
enum class TerminalProfile {
    ModernVt,
    TmuxConservative,
    ScreenConservative,
    LinuxConsole,
};

struct Capabilities {
    ColorDepth color_depth = ColorDepth::TrueColor;
    KeyboardProtocol keyboard_protocol = KeyboardProtocol::Legacy;
    MouseProtocol mouse_protocol = MouseProtocol::SGR;
    bool pixel_mouse = false;        // SGR-Pixels (mode 1016): mouse events carry pixel coords
    Size cell_pixels;                // reported cell pixel size (XTWINOPS); {0,0} = unknown
    bool sixel_graphics = false;
    // Verified Sixel limits reported by XTSMGRAPHICS. A zero value means the
    // host has not reported a finite limit; it is not a promise of support.
    int sixel_color_registers = 0;
    Size sixel_max_geometry;  // pixels; {0,0} = no finite reported maximum
    bool kitty_graphics = false;
    bool synchronized_output = false;  // DEC mode 2026
    bool clipboard_write = false;      // OSC 52
    bool focus_events = false;         // DEC mode 1004
    bool bracketed_paste = false;      // DEC mode 2004
    bool hyperlinks = false;           // OSC 8
    ColorScheme color_scheme = ColorScheme::Unknown;
    // DEC mode 2031 is active for this session and the terminal has
    // positively established that it can send CSI ? 997 ; {1,2} n color
    // preference notifications. False means color_scheme is a one-shot
    // startup hint only.
    bool color_scheme_notifications = false;

    // D-019 host layout policy: whether East-Asian-Ambiguous-width codepoints
    // are conventionally wide. The documented v1 default is narrow (false)
    // everywhere. ckVision keeps one deterministic logical cell model; until
    // D-OPEN-7 establishes an interoperable width-reporting protocol, the
    // Presenter nevertheless re-addresses after every non-ASCII grapheme for
    // every policy, so a terminal disagreement cannot shift later cells.
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
    caps.focus_events = true;
    caps.bracketed_paste = true;
    caps.hyperlinks = false;
    caps.color_scheme = ColorScheme::Unknown;
    caps.color_scheme_notifications = false;
    caps.ambiguous_width_is_wide = false;
    return caps;
}

// Curated conservative baselines. They guarantee only documented behavior
// common to the named host class; a host with stronger, version-specific
// evidence supplies an explicit Capabilities override instead.
constexpr Capabilities capabilities_for_profile(TerminalProfile profile) noexcept {
    switch (profile) {
        case TerminalProfile::ModernVt: return baseline_capabilities();
        case TerminalProfile::TmuxConservative: {
            Capabilities caps = baseline_capabilities();
            caps.mouse_protocol = MouseProtocol::None;
            caps.focus_events = false;
            caps.bracketed_paste = false;
            caps.clipboard_write = false;
            caps.synchronized_output = false;
            caps.sixel_graphics = false;
            caps.kitty_graphics = false;
            caps.pixel_mouse = false;
            caps.cell_pixels = {};
            caps.color_scheme = ColorScheme::Unknown;
            return caps;
        }
        case TerminalProfile::ScreenConservative: {
            Capabilities caps = capabilities_for_profile(TerminalProfile::TmuxConservative);
            caps.color_depth = ColorDepth::Mono16;
            return caps;
        }
        case TerminalProfile::LinuxConsole: {
            Capabilities caps = capabilities_for_profile(TerminalProfile::ScreenConservative);
            // D-OPEN-6: without an optional mouse daemon dependency, the
            // Linux console profile remains keyboard-only.
            caps.mouse_protocol = MouseProtocol::None;
            return caps;
        }
    }
    return baseline_capabilities();  // exhaustive enum fallback for defensive builds
}

}  // namespace ckv::term
