// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Presenter: diffs a composed frame against the last one presented and
// writes the minimal byte sequence to a Terminal (ARCHITECTURE.md §4).
// Operates purely on core-typed frame data (FrameView/RasterSlice) —
// never on scene::Surface — so term stays independent of scene.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

    // Exact payload size passed to Terminal::write() by the most recent
    // present(). This is a deterministic frame-cost counter: an unchanged
    // frame reports zero, while callers can make byte budgets executable
    // without inspecting a terminal backend's private capture buffer.
    std::size_t last_bytes_emitted() const noexcept { return last_bytes_emitted_; }

private:
    struct ActiveRaster {
        RasterSlice slice;
        std::uint64_t fingerprint = 0;
    };

    Cell presentation_cell(FrameView frame, Point p,
                           const std::vector<ActiveRaster>& rasters) const;
    bool can_emit_raster_slice(const RasterSlice& slice) const noexcept;
    void render_frame(FrameView frame, const std::vector<ActiveRaster>& rasters,
                      bool raster_coverage_changed, std::string& out);
    bool cell_changed(const Cell& cell, Size frame_size, Point p) const noexcept;
    void emit_cursor_move(std::string& out, int x, int y) const;
    void emit_style(std::string& out, const Style& style) const;
    void emit_raster_slices(std::string& out, FrameView frame, const std::vector<ActiveRaster>& rasters) const;
    static std::uint64_t image_fingerprint(const Image& image) noexcept;
    static bool same_rasters(const std::vector<ActiveRaster>& lhs,
                             const std::vector<ActiveRaster>& rhs) noexcept;
    bool raster_coverage_contains(Point point, const std::vector<ActiveRaster>& rasters) const noexcept;
    // The cells a raster's pixels can reach, which may exceed the cells it
    // was allotted when its pixel size is not a whole number of them.
    static Rect raster_footprint(const RasterSlice& slice) noexcept;

    Terminal& terminal_;
    std::vector<Cell> previous_cells_;
    std::string output_buffer_;
    Size previous_size_;
    std::optional<CursorState> previous_cursor_;
    // Reused scratch storage avoids repeated allocation for raster-state
    // tracking while retaining the previous frame's coverage for stale-pixel
    // removal.
    std::vector<ActiveRaster> active_rasters_;
    std::vector<ActiveRaster> previous_active_rasters_;
    bool force_full_ = true;
    std::size_t last_bytes_emitted_ = 0;
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
