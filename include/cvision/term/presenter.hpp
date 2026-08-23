// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Presenter: diffs a composed frame against the last one presented and
// writes the minimal byte sequence to a Terminal (the architecture §4).
// Operates purely on core-typed frame data (FrameView/RasterSlice) —
// never on scene::Surface — so term stays independent of scene.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/cursor.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/pointer_shape.hpp"
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
    // emitted as Sixel data on top, per D-017/the architecture §7.
    void present(FrameView frame, CursorState cursor,
                 const std::vector<RasterSlice>& rasters = {});

    // Asks the host's own mouse pointer to take `shape`, writing nothing
    // when it is already the shape this host was last asked for.
    //
    // Deliberately not a parameter of present(). The pointer is host state
    // rather than frame content, and the two change independently: crossing
    // from a window's title bar onto its content changes the pointer and not
    // one cell, while a whole animating frame can go by without the pointer
    // moving at all. Routing it through present() would mean a frame diff
    // over every cell to emit eighteen bytes, and — worse — would tie the
    // pointer to whether anything happened to need repainting, so it would
    // follow the pointer across the few widgets that redraw on hover and
    // stick everywhere else.
    //
    // Call it once per input batch rather than once per motion report: a
    // burst of reports that crosses three widgets should leave the pointer
    // in one shape, not write three.
    void present_pointer_shape(PointerShape shape);

    // Forces the next present() to treat the whole frame as changed —
    // callers use this after a resize or a capability change that
    // affects rendering (e.g. color depth).
    //
    // It forgets the pointer shape as well, which is what re-states it
    // after a capability probe: the query asking a host which shapes it has
    // is, to a host that implements only the xterm proposal, an unknown
    // shape name — and an unknown name resets the pointer. Forgetting means
    // the next frame asks again rather than assuming the host still has
    // what it was last told.
    void invalidate() noexcept {
        force_full_ = true;
        previous_pointer_shape_name_.reset();
    }

    // Exact payload size passed to Terminal::write() by the most recent
    // present(). This is a deterministic frame-cost counter: an unchanged
    // frame reports zero, while callers can make byte budgets executable
    // without inspecting a terminal backend's private capture buffer.
    std::size_t last_bytes_emitted() const noexcept { return last_bytes_emitted_; }

    // --- Frame completion (the architecture §4) --------------------------
    //
    // Writing a frame proves only that its bytes left this side. A terminal
    // reads its input in order, so a Device Status Report written after a
    // frame is answered only once that frame has been taken in — which
    // turns "sent" into "arrived" for the cost of eight bytes and one CSI
    // parse, with nothing blocked in between.
    //
    // Off by default, and deliberately: it adds bytes to the presented
    // stream, and a byte-exact golden is entitled to the frame it asked
    // for and nothing else. An application that paces itself against its
    // terminal turns it on; one that does not never pays for it.
    void set_frame_completion_tracking(bool enabled) noexcept { track_frame_completion_ = enabled; }
    bool frame_completion_tracking() const noexcept { return track_frame_completion_; }

    // How many frames have been marked with that question since this
    // Presenter was made. Compared against the backend's acknowledgement
    // count, the difference is how far ahead of the terminal we are.
    std::size_t frames_marked() const noexcept { return frames_marked_; }

    // Whether the most recent present() put a picture on the wire. A frame
    // that did is the expensive kind — a host decodes raster pixels, and
    // that cost is why an application may want to let one finish before
    // producing the next.
    bool last_frame_carried_rasters() const noexcept { return last_frame_carried_rasters_; }

private:
    // Everything an encoding depends on, and nothing else. Notably not where
    // the picture sits: a Sixel payload carries no position — the cursor move
    // in front of it does — so the same picture, cropped and scaled the same
    // way, is the same bytes wherever it is put. Dragging a window therefore
    // re-sends an encoding rather than recomputing one.
    struct EncodeKey {
        // Deliberately not the image's address. A host re-decodes a child's
        // picture into a fresh object every time the child sends it, and a
        // child redrawing during a drag sends the same picture over and over:
        // keying on the object made every one of those a fresh encode of
        // pixels we had already encoded. The fingerprint is what identifies
        // pixels; the address only identifies an allocation.
        std::uint64_t fingerprint = 0;
        int crop_x = 0;
        int crop_y = 0;
        int crop_width = 0;
        int crop_height = 0;
        int target_width = 0;
        int target_height = 0;
        int color_registers = 0;

        friend bool operator==(const EncodeKey&, const EncodeKey&) = default;
    };

    struct ActiveRaster {
        RasterSlice slice;
        std::uint64_t fingerprint = 0;
        EncodeKey key;
        // What this slice was last encoded to. Encoding a Sixel is by far the
        // most expensive thing a frame can do — hundreds of milliseconds for
        // a picture the size of a dialog — and an unchanged picture on an
        // untouched patch of screen needs neither a fresh encode nor a fresh
        // send. Shared, so carrying it into the next frame costs a pointer.
        std::shared_ptr<const std::string> encoded;
        // Whether the host has to be sent this picture again: it is new, it
        // moved, its pixels changed, or cells it covers were repainted this
        // frame. A Sixel only ever paints, so repainting its cells is exactly
        // what erases it, and what was erased has to go back down.
        bool needs_emit = false;
    };

    Cell presentation_cell(FrameView frame, Point p,
                           const std::vector<ActiveRaster>& rasters) const;
    // Builds the whole frame as it will be presented — one pass, reused by
    // the diff, by the run emitter, and as next frame's comparison basis.
    // Each of those used to rebuild it cell by cell, and a Cell carries a
    // string, so a frame cost three constructions and three allocations per
    // cell to answer a question that has one answer.
    void build_presentation(FrameView frame, const std::vector<ActiveRaster>& rasters);
    bool can_emit_raster_slice(const RasterSlice& slice) const noexcept;
    void render_frame(FrameView frame, std::vector<ActiveRaster>& rasters,
                      bool raster_coverage_changed, std::string& out);
    bool cell_changed(const Cell& cell, Size frame_size, Point p) const noexcept;
    void emit_cursor_move(std::string& out, int x, int y) const;
    // Writes the OSC 22 request for `shape` if the host would draw a
    // different pointer than it is already drawing. Degradation happens
    // here, against this terminal's own capabilities, so callers name the
    // shape they mean and never the shape a particular host can manage.
    void emit_style(std::string& out, const Style& style) const;
    void emit_raster_slices(std::string& out, FrameView frame, std::vector<ActiveRaster>& rasters) const;
    // Marks every raster whose footprint the run [x, end) on row `y` paints
    // over, so it is re-sent after the cells that erased it.
    static void mark_rasters_repainted(std::vector<ActiveRaster>& rasters, int y, int x, int end) noexcept;
    // The crop and scale this slice will be encoded with, computed once so
    // the cache and the encoder cannot disagree about what was cached.
    EncodeKey encode_key(const RasterSlice& slice, std::uint64_t fingerprint) const noexcept;
    static std::uint64_t image_fingerprint(const Image& image) noexcept;
    static bool same_raster_geometry(const std::vector<ActiveRaster>& lhs,
                             const std::vector<ActiveRaster>& rhs) noexcept;
    bool raster_coverage_contains(Point point, const std::vector<ActiveRaster>& rasters) const noexcept;
    // The cells a raster's pixels can reach, which may exceed the cells it
    // was allotted when its pixel size is not a whole number of them.
    static Rect raster_footprint(const RasterSlice& slice) noexcept;

    Terminal& terminal_;
    std::vector<Cell> previous_cells_;
    std::vector<Cell> presentation_cells_;
    std::string output_buffer_;
    Size previous_size_;
    std::optional<CursorState> previous_cursor_;
    // The pointer-shape name this session last gave the host, or nothing
    // while it has never given one. Held as the name rather than the shape
    // because the name is what the host received: two shapes that degrade
    // to one name are one request, and a vocabulary that changed mid-session
    // makes the same shape a different request.
    //
    // The empty-versus-absent distinction matters on the first frame. A
    // session with no opinion must not spend the reset sequence telling a
    // host to keep the pointer it already has — and on a host that draws a
    // deliberate pointer of its own while mouse reporting is on, that reset
    // would be a visible change rather than a no-op.
    std::optional<std::string> previous_pointer_shape_name_;
    // Reused scratch storage avoids repeated allocation for raster-state
    // tracking while retaining the previous frame's coverage for stale-pixel
    // removal.
    std::vector<ActiveRaster> active_rasters_;
    std::vector<ActiveRaster> previous_active_rasters_;
    // When the last frame finished, for the "how long was it idle between
    // paints" figure in the live trace. Only ever read under tracing.
    std::chrono::steady_clock::time_point last_present_finished_{};
    bool force_full_ = true;
    std::size_t last_bytes_emitted_ = 0;
    bool track_frame_completion_ = false;
    std::size_t frames_marked_ = 0;
    bool last_frame_carried_rasters_ = false;
};

// Encodes a style as the SGR sequence a host with these capabilities can
// read: attributes, underline shape and colour where it understands them,
// and colours at the depth it has. Exposed for direct unit testing of the
// degradations (TrueColor/256/Mono16, extended underline or not),
// independent of the full diff pipeline.
std::string style_to_sgr(const Style& style, const Capabilities& caps);

// Neutralizes embedded OSC terminator bytes (ESC, BEL) in `text` before
// it is embedded in an OSC sequence (title, future hyperlinks) —
// The architecture §12: every OSC emission escapes or rejects terminator
// bytes.
std::string sanitize_osc_text(std::string_view text);

}  // namespace ckv::term
