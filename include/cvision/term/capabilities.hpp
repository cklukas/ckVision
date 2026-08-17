// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Terminal capability model (ARCHITECTURE.md §4). A plain, inspectable
// struct: baseline profile at startup, refined by runtime probes,
// overridable by applications. Every field has a defined degradation.
#pragma once

#include <optional>

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
    Size cell_pixels;                // reported cell pixel size (XTWINOPS 16); {0,0} = unknown
    // The text area's total pixel size (XTWINOPS 14); {0,0} = unknown.
    // Terminals exist (iTerm2) that answer 14 while leaving 16 unanswered;
    // divided by the cell grid this is the fallback cell-metric source.
    Size text_area_pixels;
    // The window pixel size the kernel reports in TIOCGWINSZ's
    // ws_xpixel/ws_ypixel; {0,0} = the terminal left them unset.
    //
    // Worth carrying alongside the escape-sequence answers because it is a
    // genuinely independent source: it needs no round trip, so it is known
    // before the first frame, and terminals have been observed to disagree
    // with their own XTWINOPS replies — notably on scaled (HiDPI) displays,
    // where one answer is in points and the other in device pixels. An
    // image is emitted in the pixels the terminal actually draws with, so
    // that disagreement is not academic: it scales the picture.
    Size window_pixels;
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
    // The sub-parameter form of SGR 4 (`4:3` and friends) and the underline
    // colour SGR 58. Off by default and never assumed: a terminal that does
    // not know the colon form reads `4:3` as two parameters and applies
    // italics, so a shape is only ever sent to a host that has said it can
    // draw one. Without it every shape degrades to the plain rule.
    bool underline_styles = false;
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

// Application-selected capability policy layered over terminal probe evidence.
// A missing field preserves the observed value; an explicit value remains in
// force across later probe replies and resize re-probes. This keeps a client
// preference (for example, temporarily disabling graphics) separate from the
// terminal's own reported facts and makes removing the preference unambiguous.
struct CapabilityOverrides {
    // Forces graphics presentation on or off. Forcing it on is an explicit
    // client policy and does not manufacture unobserved geometry limits.
    std::optional<bool> sixel_graphics;
    // A positive, client-calibrated terminal cell size. `{0, 0}` is rejected
    // by the backend setter; use std::nullopt to return to probe evidence.
    std::optional<Size> cell_pixels;
    // Positive cap for Sixel color registers. A cap refines a reported limit
    // when one exists, otherwise it supplies the client-selected limit.
    std::optional<int> sixel_color_registers;
    // Whether frames are bracketed as one atomic update (DEC 2026). Forcing
    // it off is a client's answer to a host that answers the query and then
    // treats part of a frame differently from the rest — a picture applied
    // outside the bracket that brackets the cells around it tears, and only
    // the person watching it can say so.
    std::optional<bool> synchronized_output;

    friend bool operator==(const CapabilityOverrides&, const CapabilityOverrides&) = default;
};

// Applies client policy without changing the observed capability record.
// This pure function is shared by live and headless terminal backends so a
// scripted test observes the same effective presentation contract as a real
// terminal session.
constexpr Capabilities apply_capability_overrides(Capabilities observed,
                                                   const CapabilityOverrides& overrides) noexcept {
    if (overrides.sixel_graphics) observed.sixel_graphics = *overrides.sixel_graphics;
    if (overrides.cell_pixels) observed.cell_pixels = *overrides.cell_pixels;
    if (overrides.synchronized_output) observed.synchronized_output = *overrides.synchronized_output;
    if (overrides.sixel_color_registers) {
        const int cap = *overrides.sixel_color_registers;
        observed.sixel_color_registers = observed.sixel_color_registers > 0
                                            ? (observed.sixel_color_registers < cap
                                                   ? observed.sixel_color_registers
                                                   : cap)
                                            : cap;
    }
    return observed;
}

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
    caps.underline_styles = false;
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
            caps.underline_styles = false;
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
