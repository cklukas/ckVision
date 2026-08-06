// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Presenter: diffs a composed frame against the last one presented and
// writes the minimal byte sequence to a Terminal (ARCHITECTURE.md §4).
// Operates purely on core-typed frame data (FrameView/RasterSlice) —
// never on scene::Surface — so term stays independent of scene.
#pragma once

#include <vector>

#include "cvision/core/cursor.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

class Presenter {
public:
    explicit Presenter(Terminal& terminal) : terminal_(terminal) {}

    // Diffs `frame` against the previously presented one and writes
    // only the changed regions, wrapped in a synchronized-output
    // bracket when the terminal supports it. `cursor` positions the
    // real terminal cursor last, after all content; `rasters` are
    // occlusion-sliced raster regions (scene::Compositor's output).
    // With no sixel_graphics capability, the fallback cells the cell
    // diff already rendered are the whole story; with it, each visible
    // slice is cropped to its occlusion-sliced sub-rect (never the
    // whole image — a higher layer may be occluding the rest) and
    // emitted as Sixel data on top, per D-017/ARCHITECTURE.md §7.
    void present(FrameView frame, CursorState cursor,
                 const std::vector<RasterSlice>& rasters = {});

    // Forces the next present() to treat the whole frame as changed —
    // callers use this after a resize or a capability change that
    // affects rendering (e.g. color depth).
    void invalidate() noexcept { force_full_ = true; }

private:
    std::string render_frame(FrameView frame);
    bool cell_changed(FrameView frame, Point p) const noexcept;
    void emit_cursor_move(std::string& out, int x, int y) const;
    void emit_style(std::string& out, const Style& style) const;
    void emit_raster_slices(std::string& out, const std::vector<RasterSlice>& rasters) const;

    Terminal& terminal_;
    std::vector<Cell> previous_cells_;
    Size previous_size_;
    bool force_full_ = true;
};

// Encodes a foreground/background pair's SGR sequence for `depth`,
// including attribute bits — exposed for direct unit testing of the
// color-depth degradation (TrueColor/256/Mono16), independent of the
// full diff pipeline.
std::string style_to_sgr(const Style& style, ColorDepth depth);

// Neutralizes embedded OSC terminator bytes (ESC, BEL) in `text` before
// it is embedded in an OSC sequence (title, future hyperlinks) —
// ARCHITECTURE.md §12: every OSC emission escapes or rejects terminator
// bytes.
std::string sanitize_osc_text(std::string_view text);

}  // namespace ckv::term
