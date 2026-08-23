// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/capability_report.hpp"

#include <sstream>

#include "cvision/core/version.hpp"

namespace ckv::term {
namespace {

std::string yes_no(bool value) { return value ? "yes" : "no"; }

std::string size_text(Size size, const char* unit) {
    if (size.width <= 0 || size.height <= 0) return "not reported";
    return std::to_string(size.width) + "x" + std::to_string(size.height) + " " + unit;
}

std::string color_depth_text(ColorDepth depth) {
    switch (depth) {
        case ColorDepth::Mono16: return "16 colors";
        case ColorDepth::Color256: return "256 colors";
        case ColorDepth::TrueColor: return "true color (24-bit)";
    }
    return "unknown";
}

std::string mouse_text(MouseProtocol protocol) {
    switch (protocol) {
        case MouseProtocol::None: return "off";
        case MouseProtocol::X10: return "X10 (legacy)";
        case MouseProtocol::SGR: return "SGR (modern)";
    }
    return "unknown";
}

std::string keyboard_text(KeyboardProtocol protocol) {
    switch (protocol) {
        case KeyboardProtocol::Legacy: return "legacy";
        case KeyboardProtocol::ModifyOtherKeys: return "modifyOtherKeys";
        case KeyboardProtocol::Kitty: return "kitty";
    }
    return "unknown";
}

// Whether every key reports its release — the promise that lets a button
// stay down while its key is held (D-055). The verified enhancement set is
// shown either way: "no (kitty flags 3)" is a host that honoured event
// types without all-keys-as-escape-codes, which reports releases only for
// the keys it escape-codes.
std::string key_release_text(const Capabilities& caps) {
    if (caps.keyboard_protocol != KeyboardProtocol::Kitty) return "no";
    const std::string flags = " (kitty flags " + std::to_string(caps.kitty_keyboard_flags) + ")";
    return (keyboard_reports_all_releases(caps) ? "yes" : "no") + flags;
}

std::string scheme_text(ColorScheme scheme) {
    switch (scheme) {
        case ColorScheme::Dark: return "dark";
        case ColorScheme::Light: return "light";
        case ColorScheme::Unknown: return "not reported";
    }
    return "not reported";
}

// The two independent divisions that can each yield a cell metric. Shown
// separately because when they disagree, that disagreement IS the
// diagnosis: an image is emitted in one terminal's pixels and drawn in the
// other's, and the picture comes out scaled by the ratio between them.
std::string divided_cell_text(Size pixels, Size grid) {
    if (pixels.width <= 0 || pixels.height <= 0) return "not reported";
    if (grid.width <= 0 || grid.height <= 0) return "unknown grid";
    return size_text(Size{pixels.width / grid.width, pixels.height / grid.height}, "px");
}
std::string cell_from_window_text(Size window_pixels, Size grid) {
    return divided_cell_text(window_pixels, grid);
}
std::string cell_from_area_text(Size text_area_pixels, Size grid) {
    return divided_cell_text(text_area_pixels, grid);
}

// The metric actually used to place images and to convert pixel mouse
// coordinates, and where it came from — the single most useful line when
// graphics or clicks land in the wrong place.
std::string effective_cell_text(const Capabilities& caps, Size grid) {
    if (caps.cell_pixels.width > 0 && caps.cell_pixels.height > 0)
        return size_text(caps.cell_pixels, "px") + " (reported)";
    if (caps.text_area_pixels.width > 0 && caps.text_area_pixels.height > 0 && grid.width > 0 &&
        grid.height > 0) {
        const Size derived{caps.text_area_pixels.width / grid.width,
                           caps.text_area_pixels.height / grid.height};
        if (derived.width > 0 && derived.height > 0)
            return size_text(derived, "px") + " (derived from text area / grid)";
    }
    return "unknown — images and pixel clicks cannot be placed exactly";
}

}  // namespace

std::vector<CapabilityReportEntry> capability_report(const Capabilities& caps, Size grid) {
    return {
        {"ckVision", std::string(version_string()), "library"},
        {"Cell grid", grid.width > 0 ? size_text(grid, "cells") : std::string("unknown"), "TIOCGWINSZ"},
        {"Cell size", size_text(caps.cell_pixels, "px"), "XTWINOPS 16 (CSI 16 t)"},
        {"Text area", size_text(caps.text_area_pixels, "px"), "XTWINOPS 14 (CSI 14 t)"},
        {"Window pixels", size_text(caps.window_pixels, "px"), "TIOCGWINSZ ws_xpixel/ypixel"},
        {"Cell from window", cell_from_window_text(caps.window_pixels, grid), "window px / grid"},
        {"Cell from text area", cell_from_area_text(caps.text_area_pixels, grid), "text area px / grid"},
        {"Effective cell", effective_cell_text(caps, grid), "derived"},
        {"Color depth", color_depth_text(caps.color_depth), "host profile"},
        {"Color scheme", scheme_text(caps.color_scheme), "OSC 10/11"},
        {"Scheme notifications", yes_no(caps.color_scheme_notifications), "DEC 2031 (DECRQM)"},
        {"Mouse protocol", mouse_text(caps.mouse_protocol), "host profile"},
        {"Pixel mouse", yes_no(caps.pixel_mouse), "DEC 1016 (DECRQM) + XTWINOPS 16"},
        {"Keyboard protocol", keyboard_text(caps.keyboard_protocol), "host profile"},
        {"Key release events", key_release_text(caps), "CSI > u push, CSI ? u readback"},
        {"SIXEL graphics", yes_no(caps.sixel_graphics), "DA1 (CSI c), parameter 4"},
        {"SIXEL color registers",
         caps.sixel_color_registers > 0 ? std::to_string(caps.sixel_color_registers)
                                        : std::string("not reported"),
         "XTSMGRAPHICS (CSI ? 1 ; 4 ; 0 S)"},
        {"SIXEL max geometry", size_text(caps.sixel_max_geometry, "px"),
         "XTSMGRAPHICS (CSI ? 2 ; 4 ; 0 S)"},
        {"Kitty graphics", yes_no(caps.kitty_graphics), "host profile"},
        {"Synchronized output", yes_no(caps.synchronized_output), "DEC 2026 (DECRQM)"},
        {"Bracketed paste", yes_no(caps.bracketed_paste), "DEC 2004"},
        {"Focus events", yes_no(caps.focus_events), "DEC 1004"},
        {"Clipboard write", yes_no(caps.clipboard_write), "OSC 52"},
        {"Hyperlinks", yes_no(caps.hyperlinks), "OSC 8"},
        {"Underline styles", yes_no(caps.underline_styles), "SGR 4:x / 58 (host profile)"},
        {"Ambiguous width is wide", yes_no(caps.ambiguous_width_is_wide), "host policy (D-019)"},
    };
}

std::string capability_report_text(const Capabilities& caps, Size grid) {
    std::ostringstream out;
    out << "ckVision terminal capability report\n";
    out << "===================================\n";
    for (const CapabilityReportEntry& entry : capability_report(caps, grid))
        out << entry.name << ": " << entry.value << "  [" << entry.source << "]\n";
    return std::move(out).str();
}

}  // namespace ckv::term
