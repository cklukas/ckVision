// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>

#include "cvision/term/capability_report.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    if (size >= 1) {
        caps.sixel_graphics = (data[0] & 1u) != 0;
        caps.kitty_graphics = (data[0] & 2u) != 0;
        caps.pixel_mouse = (data[0] & 4u) != 0;
        caps.decstbm_scroll = (data[0] & 8u) != 0;
        caps.synchronized_output = (data[0] & 16u) != 0;
        caps.pointer_shape = (data[0] & 32u) != 0;
        caps.underline_styles = (data[0] & 64u) != 0;
    }
    if (size >= 2) {
        caps.color_depth = static_cast<ckv::term::ColorDepth>(data[1] % 5);
        caps.keyboard_protocol = static_cast<ckv::term::KeyboardProtocol>((data[1] >> 3) % 2);
        caps.mouse_protocol = static_cast<ckv::term::MouseProtocol>((data[1] >> 4) % 2);
    }
    if (size >= 3) {
        caps.color_scheme = static_cast<ckv::term::ColorScheme>(data[2] % 3);
        caps.cell_pixels = ckv::PixelSize{static_cast<int>(data[2] & 0x1Fu),
                                          static_cast<int>((data[2] >> 3) & 0x1Fu)};
    }
    if (size >= 4) {
        caps.sixel_color_registers = static_cast<int>(data[3]) * 4;
    }
    const ckv::Size grid{size >= 5 ? static_cast<int>(data[4]) : 80,
                         size >= 6 ? static_cast<int>(data[5]) : 24};
    const auto entries = ckv::term::capability_report(caps, grid);
    const auto text = ckv::term::capability_report_text(caps, grid);
    ckv::fuzz::require(!entries.empty());
    ckv::fuzz::require(!text.empty());
    return 0;
}
