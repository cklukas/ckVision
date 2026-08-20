// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal resilience benchmark: the measured paths are deliberately bounded
// workloads, while the checked invariants make this a deterministic budget
// gate instead of a machine-speed threshold.
#include <cstdio>
#include <span>
#include <string>

#include "ckbench.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/sixel_encoder.hpp"
#include "cvision/term/terminal_emulator.hpp"

namespace {

ckv::term::TerminalSubsessionOptions benchmark_options() {
    ckv::term::TerminalSubsessionOptions options;
    options.max_scrollback_lines = 256;
    options.max_output_bytes = 1U << 20U;
    options.max_parser_work_per_step = 128U << 10U;
    options.max_graphics_payload_bytes = 128U << 10U;
    options.max_image_pixels = 2U * 1024U * 1024U;
    return options;
}

std::string scrollback_workload() {
    std::string output;
    output.reserve(64U << 10U);
    for (int line = 0; line < 900; ++line)
        output += "terminal-resilience-line-" + std::to_string(line) +
                  " payload remains private to the child\r\n";
    return output;
}

// A full-window animation frame as an application produces one: a flat
// background with a shaded solid on it, sized from a terminal's own cell
// metric. Its colours fit the register budget, which is the ordinary case
// for a user interface and the one the encoder is read once for.
ckv::Image animation_frame(int width, int height) {
    ckv::Image image(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int dx = x - width / 2;
            const int dy = y - height / 2;
            const bool inside = dx * dx + dy * dy < (width / 3) * (width / 3);
            // Sixteen shades over one hue, plus the background: the palette a
            // shaded solid on a window surface actually produces.
            const auto shade = static_cast<std::uint8_t>(120 + ((x + y) / 24 % 16) * 8);
            image.set_pixel(x, y, inside ? ckv::Image::Rgba{shade, shade, 96, 255}
                                         : ckv::Image::Rgba{0, 0, 205, 255});
        }
    }
    return image;
}

std::string sixel_workload() {
    std::string output = "\x1bPq#0;2;100;0;0";
    output.append(64U << 10U, '~');
    output += "\x1b\\";
    return output;
}

}  // namespace

bool run_terminal_benchmarks() {
    const ckv::term::TerminalCapabilityProfile profile = [] {
        auto value = ckv::term::embedded_xterm_sixel_profile();
        value.cells = ckv::Size{120, 40};
        value.cell_pixels = ckv::Size{8, 16};
        return value;
    }();
    const ckv::term::TerminalSubsessionOptions options = benchmark_options();
    const std::string flood = scrollback_workload();
    const std::string sixel = sixel_workload();
    bool budgets_hold = true;
    std::size_t sink = 0;

    ckbench::run("terminal_scrollback_flood", 4, [&] {
        ckv::term::TerminalEmulator emulator(profile, options);
        emulator.feed_output(flood);
        const auto snapshot = emulator.snapshot();
        const std::size_t scrollback_limit = options.max_scrollback_lines *
                                              static_cast<std::size_t>(profile.cells.width);
        sink += snapshot.scrollback.size();
        if (snapshot.scrollback.size() > scrollback_limit ||
            snapshot.cell_buffer.size() != static_cast<std::size_t>(profile.cells.width * profile.cells.height))
            budgets_hold = false;
    });

    ckv::term::TerminalEmulator resized(profile, options);
    resized.feed_output("ready");
    ckbench::run("terminal_repeated_resize", 500, [&] {
        const int width = sink % 2U == 0U ? 120 : 96;
        const int height = sink % 3U == 0U ? 40 : 32;
        resized.resize(ckv::Size{width, height}, profile.cell_pixels);
        const auto snapshot = resized.snapshot();
        sink += snapshot.cell_buffer.size();
        if (snapshot.cells != ckv::Size{width, height} ||
            snapshot.cell_buffer.size() != static_cast<std::size_t>(width * height))
            budgets_hold = false;
    });

    // U0-b, the measurement the item asks for: what a multiplexer's server
    // pays to read one terminal at its flush tick, with the history the
    // terminal is holding and without it.
    //
    // All three cases apply the SAME mutation first — a status line rewritten
    // in place, which is what a tick with work in it actually looks like — so
    // the difference between the numbers is the cost of the read and nothing
    // else. An earlier version fed a newline here instead, which scrolled the
    // whole screen and pushed a line into the history: that made the damage
    // case look four times more expensive than a full snapshot, because it was
    // measuring the emulator's work rather than the host's.
    //
    // The shape to look for: the first grows with how long the terminal has
    // been alive, and the other two do not.
    const char* const tick_mutation = "\x1b[5;1H status: a line a program rewrites every frame ";
    ckv::term::TerminalEmulator loaded(profile, options);
    loaded.feed_output(flood);

    // What a program printing steadily costs once the history is FULL, which
    // is the state a long-lived terminal spends all of its life in.
    //
    // The history is stored as flat rows, so dropping the oldest line by
    // erasing the vector's front moves every cell still in it: O(history) per
    // scrolled line, which at ten thousand lines of a hundred and twenty
    // columns is over a million cell moves for one line of output. It is
    // replaced by a start offset, with the dead prefix reclaimed once it is as
    // large as what is kept — one history copy per history's worth of lines.
    //
    // The two cases differ ONLY in how much history is held. A per-line cost
    // that is flat between them is the property; a cost that grows with the
    // history is the defect coming back, and no absolute number would show
    // that on its own.
    const auto steady_scroll = [&](const char* name, std::size_t history_lines) {
        ckv::term::TerminalSubsessionOptions deep = options;
        deep.max_scrollback_lines = history_lines;
        ckv::term::TerminalEmulator emulator(profile, deep);
        // Fed in chunks, because a single feed is bounded by
        // max_parser_work_per_step and the rest of one huge write would be
        // dropped — the benchmark would then measure an empty history.
        const std::size_t total = history_lines + static_cast<std::size_t>(profile.cells.height);
        std::string chunk;
        for (std::size_t line = 0; line < total; ++line) {
            chunk += "history line " + std::to_string(line) + " already scrolled away\r\n";
            if (chunk.size() < 32U << 10U && line + 1 < total) continue;
            emulator.feed_output(chunk);
            chunk.clear();
        }
        const std::size_t full = history_lines * static_cast<std::size_t>(profile.cells.width);
        if (emulator.scrollback().size() != full) budgets_hold = false;
        ckbench::run(name, 2000, [&] {
            emulator.feed_output("one more line, and the oldest one leaves\r\n");
            sink += emulator.cells().size();
        });
        // Still exactly full: the trim keeps capacity, it does not drift.
        if (emulator.scrollback().size() != full) budgets_hold = false;
    };
    steady_scroll("terminal_steady_scroll_full_history_1k", 1000);
    steady_scroll("terminal_steady_scroll_full_history_10k", 10000);

    ckbench::run("terminal_snapshot_with_history", 2000, [&] {
        loaded.feed_output(tick_mutation);
        const auto snapshot = loaded.snapshot();
        sink += snapshot.cell_buffer.size() + snapshot.scrollback.size();
        if (snapshot.scrollback.empty()) budgets_hold = false;
    });

    // What a host pays to learn where the cursor went, which it must ask on
    // every tick because damage flags the cursor, the modes and the title
    // independently of any cell. Before status() the only answer available was
    // a snapshot — so the copy U0-b removed came straight back through the
    // fields around it.
    ckbench::run("terminal_status_scalars_only", 2000, [&] {
        loaded.feed_output(tick_mutation);
        const ckv::core::TerminalStatus status = loaded.status();
        sink += static_cast<std::size_t>(status.cursor.position.x);
        if (status.cells != profile.cells) budgets_hold = false;
    });

    ckv::core::TerminalSnapshotOptions lean;
    lean.include_scrollback = false;
    lean.include_rasters = false;
    ckbench::run("terminal_snapshot_grid_only", 2000, [&] {
        loaded.feed_output(tick_mutation);
        const auto snapshot = loaded.snapshot(lean);
        sink += snapshot.cell_buffer.size();
        // The grid is still whole; only what the caller declined is absent.
        if (!snapshot.scrollback.empty() ||
            snapshot.cell_buffer.size() != static_cast<std::size_t>(profile.cells.width * profile.cells.height))
            budgets_hold = false;
    });

    // The host has caught up before the loop starts. Without this the first
    // iteration still carries construction damage — every row, full width —
    // which is correct behaviour and the wrong starting point for measuring a
    // steady-state tick.
    loaded.clear_damage();
    ckbench::run("terminal_damage_and_borrowed_cells", 2000, [&] {
        loaded.feed_output(tick_mutation);
        // What WP-4b will do: ask what changed, read only those rows out of the
        // borrowed grid, and say it has caught up. No copy of anything the host
        // is not about to send.
        const ckv::term::TerminalDamage& damage = loaded.damage();
        const std::span<const ckv::Cell> cells = loaded.cells();
        std::size_t touched = 0;
        for (std::size_t row = 0; row < damage.rows.size(); ++row) {
            const auto& span = damage.rows[row];
            if (span.empty()) continue;
            for (int column = span.first; column < span.last; ++column) {
                const std::size_t index = row * static_cast<std::size_t>(profile.cells.width) +
                                          static_cast<std::size_t>(column);
                if (index < cells.size() && !cells[index].grapheme().empty()) ++touched;
            }
        }
        sink += touched;
        loaded.clear_damage();
        // One row of a 120x40 screen changed, so a host that reads damage
        // touches about 50 cells where a full snapshot copies 4800 plus the
        // history.
        if (touched > static_cast<std::size_t>(profile.cells.width)) budgets_hold = false;
    });

    ckbench::run("terminal_large_sixel_payload", 8, [&] {
        ckv::term::TerminalEmulator emulator(profile, options);
        emulator.set_raster_identity(52);
        emulator.feed_output(sixel);
        const auto snapshot = emulator.snapshot();
        sink += snapshot.rasters.size();
        if (snapshot.rasters.size() != 1U || snapshot.rasters.front().image == nullptr ||
            !snapshot.diagnostics.empty())
            budgets_hold = false;
    });

    // The other direction: what an application spends turning a frame into
    // Sixel, which is the owning thread's per-frame cost for animated raster
    // content and therefore what an animation's frame rate is bounded by.
    const ckv::Image frame = animation_frame(722, 608);
    ckbench::run("sixel_encode_animation_frame", 20, [&] {
        const std::string encoded = ckv::term::encode_sixel(frame, 256);
        sink += encoded.size();
        if (encoded.size() < 1024U || encoded.compare(0, 2, "\x1bP") != 0) budgets_hold = false;
    });

    std::printf("  (terminal budgets: scrollback <= %zu cells, parser <= %zu bytes/step, "
                "graphics <= %zu bytes)\n",
                options.max_scrollback_lines * static_cast<std::size_t>(profile.cells.width),
                options.max_parser_work_per_step, options.max_graphics_payload_bytes);
    std::printf("checksum %zu\n", sink);
    if (!budgets_hold)
        std::fputs("terminal budget failure: bounded workload exceeded a checked invariant\n", stderr);
    return budgets_hold;
}
