// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/term/terminal_emulator.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    ckv::term::TerminalSubsessionOptions options;
    options.max_control_bytes = 1024;
    options.max_printable_run_bytes = 1024;
    options.max_parser_work_per_step = 1024;
    options.max_image_pixels = 80 * 24 * 9 * 18;
    ckv::term::TerminalEmulator emulator(ckv::term::embedded_xterm_sixel_profile(), options);
    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t chunk = 1 + (static_cast<unsigned char>(input[offset]) % 31U);
        const std::size_t count = std::min(chunk, input.size() - offset);
        emulator.feed_output(std::string_view(input).substr(offset, count));
        offset += count;
    }
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    ckv::fuzz::require(snapshot.cell_buffer.size() == static_cast<std::size_t>(snapshot.cells.width * snapshot.cells.height));
    ckv::fuzz::require(snapshot.diagnostics.size() <= 64);
    return 0;
}
