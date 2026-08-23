// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal capability model (the architecture §4). A plain, inspectable
// struct: baseline profile at startup, refined by runtime probes,
// overridable by applications. Every field has a defined degradation.
#pragma once

#include <cstdint>
#include <optional>

#include "cvision/core/geometry.hpp"
#include "cvision/term/pointer_shape_names.hpp"

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

// The kitty keyboard protocol's progressive enhancements, by their protocol
// numbers. ckVision requests every enhancement whose payload its input model
// can carry; ReportAlternateKeys (0x4) is deliberately never requested — the
// model carries a key and the text it produced, not the keyboard layout
// behind them (D-047, D-055).
inline constexpr int kKittyDisambiguateEscapeCodes = 1 << 0;
inline constexpr int kKittyReportEventTypes = 1 << 1;
inline constexpr int kKittyReportAllKeysAsEscapeCodes = 1 << 3;
inline constexpr int kKittyReportAssociatedText = 1 << 4;
inline constexpr int kKittyRequestedFlags = kKittyDisambiguateEscapeCodes | kKittyReportEventTypes |
                                            kKittyReportAllKeysAsEscapeCodes |
                                            kKittyReportAssociatedText;
// What a kitty session that never verified its enhancement set is entitled
// to assume: disambiguation and event types were the protocol's original
// baseline promise, and every earlier ckVision session ran on exactly them.
inline constexpr int kKittyBaselineFlags = kKittyDisambiguateEscapeCodes | kKittyReportEventTypes;

enum class MouseProtocol {
    None,
    X10,  // legacy, no release events, coordinates capped at 223
    // Modern: unlimited coordinates, distinguishes press from release, and
    // reports motion whether or not a button is held (DEC mode 1003 rather
    // than 1002). The pointer's position between clicks is not a luxury —
    // without it nothing can know what the pointer is over, so neither a
    // pointer shape nor a hover highlight can exist at all. The cost is one
    // report per cell the pointer crosses, decoded and routed like any
    // other; a report the ui layer finds nothing new under is dropped
    // before it reaches a view.
    SGR,
};

enum class ColorScheme {
    Unknown,
    Dark,
    Light,
};

// A host-selected baseline identity. ckVision never reads TERM or any other
// environment variable in library code: the embedding host supplies this
// choice explicitly, then may use runtime probes or a fully custom
// Capabilities value to refine it (D-004, the architecture §4).
enum class TerminalProfile {
    ModernVt,
    TmuxConservative,
    ScreenConservative,
    LinuxConsole,
};

struct Capabilities {
    ColorDepth color_depth = ColorDepth::TrueColor;
    KeyboardProtocol keyboard_protocol = KeyboardProtocol::Legacy;
    // The kitty enhancement flags verified to be in effect for this session:
    // the value the host reported back (CSI ? u) masked to what ckVision
    // requests — never the value that was merely asked for (D-047's rule,
    // applied from the client side). Zero outside a kitty session and zero
    // inside one whose flag set was never verified; both read as "no promise
    // beyond the legacy encodings". An embedder that selects Kitty
    // explicitly may also state the flag contract here; leaving it zero
    // makes the POSIX backend request kKittyBaselineFlags and lets a probing
    // session negotiate upward from there.
    int kitty_keyboard_flags = 0;
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
    // Whether this session writes mouse pointer shapes (OSC 22).
    //
    // Alone among the fields here it does not record what a host answered,
    // because the protocol affords no answer to record. Its query is
    // optional, and the hosts that implement only the original xterm
    // proposal — which is to say the hosts that draw the shapes perfectly
    // well — never reply to anything. Silence is therefore not evidence of
    // absence, and a capability that waited for proof would switch the
    // feature off precisely where it works.
    //
    // Defaulting it on is safe in a way that no other unproven capability
    // here would be: an unrecognized OSC is discarded by every conforming
    // host, so the failure mode is that nothing happens. Contrast
    // underline_styles, off by default because a host that misreads it
    // draws the wrong thing. `CapabilityOverrides::pointer_shapes` turns it
    // off for a host observed to do something stranger than ignore it.
    bool pointer_shapes = true;
    // Which spelling the session writes. Legacy until a host answers the
    // support query, which only a host implementing the kitty
    // specification does — and answering is what proves it can read the
    // CSS names in the first place.
    PointerShapeVocabulary pointer_shape_vocabulary = PointerShapeVocabulary::Legacy;
    // One bit per PointerShape, in enum order, from a host's own answer to
    // the support query. Meaningful only while pointer_shape_vocabulary is
    // Standard; under Legacy no host has said anything and every shape is
    // attempted. See host_draws_pointer_shape() below, which is what
    // callers should ask rather than reading this directly.
    std::uint32_t pointer_shapes_supported = 0;
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

// Whether every key on this session reports its release: kitty event types
// together with all-keys-as-escape-codes, both verified. This is the promise
// a control that stays visibly pressed while its key is held must have —
// event types alone leave Enter, Space and every other text-producing key on
// legacy encodings that never report one.
constexpr bool keyboard_reports_all_releases(const Capabilities& caps) noexcept {
    return caps.keyboard_protocol == KeyboardProtocol::Kitty &&
           (caps.kitty_keyboard_flags & kKittyReportEventTypes) != 0 &&
           (caps.kitty_keyboard_flags & kKittyReportAllKeysAsEscapeCodes) != 0;
}

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
    // Silences OSC 22 for a host that does something other than ignore it.
    // Forcing it on is meaningless and therefore not offered: it is already
    // on, for every host, until this says otherwise.
    std::optional<bool> pointer_shapes;

    friend bool operator==(const CapabilityOverrides&, const CapabilityOverrides&) = default;
};

// The bit `shape` occupies in Capabilities::pointer_shapes_supported.
constexpr std::uint32_t pointer_shape_bit(PointerShape shape) noexcept {
    return std::uint32_t{1} << static_cast<int>(shape);
}

// Whether this host will draw `shape` if asked. Under the legacy
// vocabulary the honest answer is "nobody said otherwise", and that is the
// answer given: the shapes are attempted, and a host that lacks one simply
// does not change its pointer.
constexpr bool host_draws_pointer_shape(const Capabilities& caps, PointerShape shape) noexcept {
    if (!caps.pointer_shapes) return false;
    if (caps.pointer_shape_vocabulary != PointerShapeVocabulary::Standard) return true;
    return (caps.pointer_shapes_supported & pointer_shape_bit(shape)) != 0;
}

// The shape this host will actually be asked for when a view wants
// `shape`: the request itself, or the nearest thing down its fallback
// chain that the host has. Terminates because pointer_shape_fallback does.
constexpr PointerShape effective_pointer_shape(const Capabilities& caps,
                                               PointerShape shape) noexcept {
    for (int guard = 0; guard < kPointerShapeCount; ++guard) {
        if (shape == PointerShape::Default || host_draws_pointer_shape(caps, shape)) return shape;
        shape = pointer_shape_fallback(shape);
    }
    return PointerShape::Default;
}

// Applies client policy without changing the observed capability record.
// This pure function is shared by live and headless terminal backends so a
// scripted test observes the same effective presentation contract as a real
// terminal session.
constexpr Capabilities apply_capability_overrides(Capabilities observed,
                                                   const CapabilityOverrides& overrides) noexcept {
    if (overrides.sixel_graphics) observed.sixel_graphics = *overrides.sixel_graphics;
    if (overrides.pointer_shapes) observed.pointer_shapes = *overrides.pointer_shapes;
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
// refinement (the architecture §4).
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
    caps.pointer_shapes = true;
    caps.pointer_shape_vocabulary = PointerShapeVocabulary::Legacy;
    caps.pointer_shapes_supported = 0;
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
            // A pointer shape is a statement about where the pointer is,
            // and this profile is never told. Silence here is not caution
            // about the escape code; it is that there is nothing to say.
            caps.pointer_shapes = false;
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
