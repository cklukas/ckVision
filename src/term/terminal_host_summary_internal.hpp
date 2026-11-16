// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "cvision/term/capabilities.hpp"

namespace ckv::term {

// Implementation detail shared by the native backends, not an application
// API. Called only when a host injected a trace; formats the same capability
// vocabulary without reading environment or platform state.
inline std::string terminal_host_summary(const Capabilities& caps, Size grid,
                                          const CapabilityOverrides& overrides = {}) {
    const auto size_text = [](auto size) {
        return std::to_string(size.width) + "x" + std::to_string(size.height);
    };
    const std::string keyboard = caps.keyboard_protocol == KeyboardProtocol::Kitty
        ? "kitty(flags " + std::to_string(caps.kitty_keyboard_flags) + ")"
        : (caps.keyboard_protocol == KeyboardProtocol::ModifyOtherKeys ? "modifyOtherKeys" : "legacy");
    return "terminal: sixel=" + std::string(caps.sixel_graphics ? "yes" : "NO") +
        " cell=" + size_text(caps.cell_pixels) + "px grid=" + size_text(grid) +
        " registers=" + std::to_string(caps.sixel_color_registers) +
        " max-geometry=" + size_text(caps.sixel_max_geometry) + " keyboard=" + keyboard +
        " synchronized-output=" + (caps.synchronized_output ? "yes" : "no") +
        " overrides{sixel=" + (overrides.sixel_graphics ? (*overrides.sixel_graphics ? "on" : "off") : "-") +
        " sync=" + (overrides.synchronized_output ? (*overrides.synchronized_output ? "on" : "off") : "-") +
        " cell=" + (overrides.cell_pixels ? size_text(*overrides.cell_pixels) : "-") + "}";
}

}  // namespace ckv::term
