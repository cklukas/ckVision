// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/terminal_host_summary_internal.hpp"
#include "cvision/testing/cktest.hpp"

CK_TEST(native_host_summary_plain_vt_text_is_a_platform_independent_golden) {
    const ckv::term::Capabilities caps;
    const auto summary = ckv::term::terminal_host_summary(caps, {80, 24});
    CK_CHECK(summary == "terminal: sixel=NO cell=0x0px grid=80x24 registers=0 max-geometry=0x0 "
                        "keyboard=legacy synchronized-output=no overrides{sixel=- sync=- cell=-}");
    CK_CHECK(ckv::term::terminal_host_summary(caps, {80, 24}) == summary);
}

CK_TEST(native_host_summary_verified_graphics_keyboard_and_overrides_text_is_golden) {
    ckv::term::Capabilities caps;
    caps.sixel_graphics = true;
    caps.cell_pixels = {9, 18};
    caps.sixel_color_registers = 256;
    caps.sixel_max_geometry = {1920, 1080};
    caps.keyboard_protocol = ckv::term::KeyboardProtocol::Kitty;
    caps.kitty_keyboard_flags = 31;
    caps.synchronized_output = true;
    ckv::term::CapabilityOverrides overrides;
    overrides.sixel_graphics = true;
    overrides.synchronized_output = false;
    overrides.cell_pixels = ckv::PixelSize{10, 20};
    CK_CHECK(ckv::term::terminal_host_summary(caps, {120, 40}, overrides) ==
        "terminal: sixel=yes cell=9x18px grid=120x40 registers=256 max-geometry=1920x1080 "
        "keyboard=kitty(flags 31) synchronized-output=yes overrides{sixel=on sync=off cell=10x20}");
    CK_CHECK(caps.cell_pixels == ckv::PixelSize(9, 18));
    CK_CHECK(caps.synchronized_output);
}

CK_TEST(native_host_summary_keeps_negative_overrides_distinct_from_unknown) {
    ckv::term::Capabilities caps;
    caps.keyboard_protocol = ckv::term::KeyboardProtocol::ModifyOtherKeys;
    ckv::term::CapabilityOverrides overrides;
    overrides.sixel_graphics = false;
    overrides.synchronized_output = true;
    CK_CHECK(ckv::term::terminal_host_summary(caps, {57, 10}, overrides) ==
        "terminal: sixel=NO cell=0x0px grid=57x10 registers=0 max-geometry=0x0 "
        "keyboard=modifyOtherKeys synchronized-output=no overrides{sixel=off sync=on cell=-}");
}
