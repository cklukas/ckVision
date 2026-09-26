// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// The three captures a raster-bearing golden pins, as the bytes of its
// checked-in files (docs/golden-format.md):
//
//   * the symbolic scene: the composed frame with one `raster` record per
//     visible, occlusion-sliced picture (`<name>.scene`);
//   * the decoded cells: what HeadlessTerminal's VirtualDisplay rebuilt from
//     the bytes the Presenter wrote (`<name>.dump`);
//   * the decoded pixel plane: the same display's RGBA plane
//     (`<name>.rgba`).
//
// A generator writes these strings to files and the test that owns the same
// script compares its own captures with those files, so both sides spell a
// capture exactly one way.
//
// A paired raster script is one event script played twice, on a Sixel
// profile and on NoGraphics, with the same cell metric (D-035, D-080). Every
// beat whose golden names a file stem S pins four files: S.scene, which both
// runs must compose byte for byte, since the graphics profile changes what
// the Presenter writes and never the composed scene; S.dump and S.rgba from
// the Sixel run; and S_no_graphics.dump from the NoGraphics run, whose plane
// holds no pixel at all.
#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/frame_view.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/virtual_display.hpp"
#include "cvision/ui/application.hpp"
#include "event_script.hpp"

namespace ckv::docgen {

// The cell both profiles of a paired raster script report: small enough that
// a pinned plane stays small, twice as tall as it is wide like most terminal
// cells, and one whole Sixel band (six pixel rows) per cell row.
inline constexpr PixelSize kScriptCellPixels{3, 6};
// headless_sixel_profile() and headless_no_graphics_profile() on that cell.
term::Capabilities script_sixel_profile();
term::Capabilities script_no_graphics_profile();

// scene::capture_frame(app.compositor(), app.current_cursor()), serialized.
std::string capture_scene(const ui::Application& app);
// `frame` and `cursor` as a golden dump of cells alone.
std::string frame_cells_dump(FrameView frame, CursorState cursor);
// frame_cells_dump() of a decoded display's cells and cursor.
std::string display_cells_dump(const term::VirtualDisplay& display);
// `plane` as a raw-plane fixture: the header "ckvision-rgba 1", a
// "<width> <height>" line, then every pixel's R, G, B and A bytes, row-major.
std::string rgba_plane_bytes(const Image& plane);

// Writes `bytes` to `directory`/`file_name` exactly; false on failure.
bool write_golden(const std::filesystem::path& directory, std::string_view file_name,
                  std::string_view bytes);
// The pinned file `directory`/`file_name`, byte for byte; empty when absent.
std::string read_golden(const std::filesystem::path& directory, std::string_view file_name);

// What both runs of a paired raster script show after one beat, captured
// together, and the stem of the four files that pin it.
struct PairedRasterBeat {
    std::string stem;
    std::string scene;           // the Sixel run's symbolic scene
    std::string fallback_scene;  // the NoGraphics run's, which must equal it
    std::string sixel_cells;
    std::string sixel_pixels;
    std::string fallback_cells;
    bool fallback_has_pixels = false;

    bool operator==(const PairedRasterBeat&) const = default;
};
// Captures the beat both runs have just played, which pins `stem`.
PairedRasterBeat capture_paired_raster_beat(std::string stem, const ui::Application& sixel_app,
                                            const term::HeadlessTerminal& sixel_terminal,
                                            const ui::Application& fallback_app,
                                            const term::HeadlessTerminal& fallback_terminal);
// Writes the four files. False when the runs composed different scenes, when
// the NoGraphics display holds a pixel, or when a file cannot be written: a
// generator must not pin a pair that contradicts itself.
bool write_paired_raster_beat(const std::filesystem::path& directory, const PairedRasterBeat& beat);
// Whether `beat` shows exactly the four files pinned for its stem in
// `directory`, both runs composed that one scene, and the NoGraphics
// display holds no pixel.
bool matches_pinned(const PairedRasterBeat& beat, const std::filesystem::path& directory);

// Plays one script on two stages in lockstep -- `sixel` on a Sixel profile,
// `fallback` on NoGraphics -- and writes the four files of every beat whose
// golden names a stem. A beat may name the stem of an earlier one when it
// must return to that frame exactly; it then writes nothing, and the frame
// must be the same, byte for byte. A Stage has `terminal`, `app` and
// `player` members, like every script stage. Returns how many stems were
// written, or -1 when the scripts differ in length, a beat does not return
// to the frame it names, or a pair could not be written.
template <class Stage>
int write_paired_raster_goldens(Stage& sixel, Stage& fallback, const std::filesystem::path& directory) {
    std::vector<PairedRasterBeat> written;
    for (;;) {
        const ScriptBeat* const beat = sixel.player.play_next();
        const ScriptBeat* const fallback_beat = fallback.player.play_next();
        if (beat == nullptr || fallback_beat == nullptr)
            return beat == fallback_beat ? static_cast<int>(written.size()) : -1;
        if (beat->golden.empty()) continue;
        PairedRasterBeat captured = capture_paired_raster_beat(beat->golden, sixel.app, sixel.terminal,
                                                               fallback.app, fallback.terminal);
        const auto earlier = std::find_if(written.begin(), written.end(),
                                          [&captured](const PairedRasterBeat& pinned) {
                                              return pinned.stem == captured.stem;
                                          });
        if (earlier != written.end()) {
            if (!(*earlier == captured)) return -1;
            continue;
        }
        if (!write_paired_raster_beat(directory, captured)) return -1;
        written.push_back(std::move(captured));
    }
}

}  // namespace ckv::docgen
