// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/presenter.hpp"

#include <array>
#include <climits>
#include <string>

#include "cvision/core/palette.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/term/sixel_encoder.hpp"

namespace ckv::term {
namespace {

struct Rgb {
    int r, g, b;
};

// Basic 16 ANSI colors, standard values (public terminal convention).
constexpr std::array<Rgb, 16> kBasic16{{
    {0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {128, 128, 0}, {0, 0, 128}, {128, 0, 128},
    {0, 128, 128}, {192, 192, 192}, {128, 128, 128}, {255, 0, 0}, {0, 255, 0},
    {255, 255, 0}, {0, 0, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255},
}};

// The standard xterm 256-color palette: 0-15 basic, 16-231 a 6x6x6 cube
// at the well-known level sequence, 232-255 a 24-step grayscale ramp.
// Built once from these documented public values, then searched
// exhaustively for the nearest color — this avoids relying on any
// closed-form RGB->index formula that could be subtly misremembered.
std::array<Rgb, 256> build_256_palette() {
    std::array<Rgb, 256> pal{};
    for (int i = 0; i < 16; ++i) pal[static_cast<std::size_t>(i)] = kBasic16[static_cast<std::size_t>(i)];
    constexpr int levels[6] = {0, 95, 135, 175, 215, 255};
    int idx = 16;
    for (int r = 0; r < 6; ++r)
        for (int g = 0; g < 6; ++g)
            for (int b = 0; b < 6; ++b) pal[static_cast<std::size_t>(idx++)] = Rgb{levels[r], levels[g], levels[b]};
    for (int i = 0; i < 24; ++i) {
        const int v = 8 + i * 10;
        pal[static_cast<std::size_t>(232 + i)] = Rgb{v, v, v};
    }
    return pal;
}

int nearest_in(const Rgb* palette, int count, int start, uint8_t r, uint8_t g, uint8_t b) noexcept {
    int best = start;
    int best_dist = INT_MAX;
    for (int i = start; i < count; ++i) {
        const int dr = palette[i].r - r;
        const int dg = palette[i].g - g;
        const int db = palette[i].b - b;
        const int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return best;
}

int nearest_256(uint8_t r, uint8_t g, uint8_t b) noexcept {
    static const std::array<Rgb, 256> pal = build_256_palette();
    return nearest_in(pal.data(), 256, 16, r, g, b);  // search cube+gray; skip theme-variable basic 16
}

int nearest_16(uint8_t r, uint8_t g, uint8_t b) noexcept {
    return nearest_in(kBasic16.data(), 16, 0, r, g, b);
}

void append_color_sgr(std::string& out, Color color, bool is_bg, ColorDepth depth) {
    if (color.is_default()) return;
    const int base_fg = is_bg ? 48 : 38;
    // A palette index goes out as a palette index. The host has a palette of
    // its own, themed by the person using it, and index 1 means "their red"
    // there just as it did to whoever asked for it. Resolving it here would
    // substitute ckVision's opinion of red for theirs, and — where the host
    // has fewer colours — would quantise a number that needed no quantising.
    if (color.is_indexed()) {
        const int index = color.index();
        if (depth != ColorDepth::Mono16) {
            out += ';';
            out += std::to_string(base_fg);
            out += ";5;";
            out += std::to_string(index);
            return;
        }
        // Sixteen colours: the low sixteen are exactly the codes this host
        // has, and the rest are resolved and matched to the nearest of them.
        const int basic = index < 16 ? index
                                     : nearest_16(palette_color(index).r(), palette_color(index).g(),
                                                  palette_color(index).b());
        out += ';';
        out += std::to_string((basic < 8) ? (basic + (is_bg ? 40 : 30)) : (basic - 8 + (is_bg ? 100 : 90)));
        return;
    }
    switch (depth) {
        case ColorDepth::TrueColor:
            out += ';';
            out += std::to_string(base_fg);
            out += ";2;";
            out += std::to_string(color.r());
            out += ';';
            out += std::to_string(color.g());
            out += ';';
            out += std::to_string(color.b());
            break;
        case ColorDepth::Color256:
            out += ';';
            out += std::to_string(base_fg);
            out += ";5;";
            out += std::to_string(nearest_256(color.r(), color.g(), color.b()));
            break;
        case ColorDepth::Mono16: {
            const int idx = nearest_16(color.r(), color.g(), color.b());
            const int code = (idx < 8) ? (idx + (is_bg ? 40 : 30)) : (idx - 8 + (is_bg ? 100 : 90));
            out += ';';
            out += std::to_string(code);
            break;
        }
    }
}

// SGR 4 is the underline every terminal has had since underlines existed.
// Its sub-parameter form — `4:3` for a curl, `4:4` for dots — names a shape
// instead, and a host that has never heard of it must not be sent one: a
// terminal that reads `4:3` as two ordinary parameters applies italics to
// text that asked for a wavy rule. Where the capability is absent every shape
// therefore degrades to the plain rule, which is exactly what an underline
// meant before the shapes existed and is still legible as emphasis.
//
// The underline's own colour (`58`) travels with it, in the colon form the
// terminals that implement it document; `59` is unnecessary because every
// style begins with a full reset.
void append_underline_sgr(std::string& out, const Style& style, bool extended) {
    if (!extended) {
        out += ";4";
        return;
    }
    switch (style.underline) {
        case UnderlineShape::Straight: out += ";4"; break;
        case UnderlineShape::Double: out += ";4:2"; break;
        case UnderlineShape::Curly: out += ";4:3"; break;
        case UnderlineShape::Dotted: out += ";4:4"; break;
        case UnderlineShape::Dashed: out += ";4:5"; break;
    }
    const Color color = style.underline_color;
    if (color.is_default()) return;  // the rule follows the text, as it does by default
    if (color.is_indexed()) {
        out += ";58:5:";
        out += std::to_string(color.index());
        return;
    }
    out += ";58:2::";
    out += std::to_string(color.r());
    out += ':';
    out += std::to_string(color.g());
    out += ':';
    out += std::to_string(color.b());
}

// D-019: without an interoperable terminal width-reporting protocol, only
// ASCII has a sufficiently narrow, stable rendering contract to trust its
// natural cursor advance. Every non-ASCII grapheme is therefore a cursor
// barrier, including ordinary CJK text and precomposed accented letters—not
// just the well-known ZWJ/variation-selector emoji cases. This leaves
// ckVision's logical cell geometry unchanged; it merely re-establishes the
// next position absolutely so a host disagreement corrupts at most the one
// grapheme just emitted, never later cells in the frame.
//
// `ambiguous_width_is_wide` remains part of the public capability report for
// hosts and applications that need to state their layout convention. It must
// not relax this output-synchronization boundary until D-OPEN-7 supplies an
// interoperable refinement protocol.
bool is_width_unsafe(std::string_view grapheme) noexcept {
    std::size_t pos = 0;
    while (pos < grapheme.size()) {
        const char32_t cp = utf8::decode(grapheme, pos);
        if (cp > 0x7F) return true;
    }
    return false;
}

}  // namespace

std::string style_to_sgr(const Style& style, const Capabilities& caps) {
    std::string out = "\x1B[0";
    if (has_attr(style.attrs, Attr::Bold)) out += ";1";
    if (has_attr(style.attrs, Attr::Dim)) out += ";2";
    if (has_attr(style.attrs, Attr::Italic)) out += ";3";
    if (has_attr(style.attrs, Attr::Underline)) append_underline_sgr(out, style, caps.underline_styles);
    if (has_attr(style.attrs, Attr::Reverse)) out += ";7";
    if (has_attr(style.attrs, Attr::Strike)) out += ";9";
    append_color_sgr(out, style.fg, false, caps.color_depth);
    append_color_sgr(out, style.bg, true, caps.color_depth);
    out += 'm';
    return out;
}

std::string sanitize_osc_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const unsigned char b = static_cast<unsigned char>(c);
        if (b == 0x1B || b == 0x07) continue;  // strip: would prematurely terminate the OSC
        out += c;
    }
    return out;
}

bool Presenter::can_emit_raster_slice(const RasterSlice& slice) const noexcept {
    const Capabilities& caps = terminal_.capabilities();
    if (!caps.sixel_graphics || !slice.image || slice.image->empty() || slice.visible_rect.empty() ||
        slice.full_anchor.empty())
        return false;

    const Rect& anchor = slice.full_anchor;
    const Image& image = *slice.image;
    const int crop_width = (slice.visible_rect.x + slice.visible_rect.width - anchor.x) * image.width() /
                               anchor.width -
                           (slice.visible_rect.x - anchor.x) * image.width() / anchor.width;
    const int crop_height = (slice.visible_rect.y + slice.visible_rect.height - anchor.y) * image.height() /
                                anchor.height -
                            (slice.visible_rect.y - anchor.y) * image.height() / anchor.height;
    if (crop_width <= 0 || crop_height <= 0) return false;
    return (caps.sixel_max_geometry.width <= 0 || crop_width <= caps.sixel_max_geometry.width) &&
           (caps.sixel_max_geometry.height <= 0 || crop_height <= caps.sixel_max_geometry.height);
}

Cell Presenter::presentation_cell(FrameView frame, Point p,
                                  const std::vector<ActiveRaster>& rasters) const {
    const Cell& source = frame.at(p);
    for (const ActiveRaster& raster : rasters) {
        if (raster.slice.visible_rect.contains(p)) {
            // The fallback remains mandatory scene content, but it must not
            // be emitted beneath an active opaque Sixel image. Keep its
            // style so writing this blank also establishes the surrounding
            // panel/window background before the DCS.
            return Cell::from_grapheme(" ", source.style());
        }
    }
    return source;
}

bool Presenter::cell_changed(const Cell& cell, Size frame_size, Point p) const noexcept {
    if (force_full_ || previous_size_ != frame_size) return true;
    const Cell& prev = previous_cells_[static_cast<std::size_t>(p.y) *
                                            static_cast<std::size_t>(previous_size_.width) +
                                        static_cast<std::size_t>(p.x)];
    return !(cell == prev);
}

void Presenter::emit_cursor_move(std::string& out, int x, int y) const {
    out += "\x1B[";
    out += std::to_string(y + 1);
    out += ';';
    out += std::to_string(x + 1);
    out += 'H';
}

void emit_cursor_shape(std::string& out, CursorShape shape, bool blink) {
    int parameter = 2;
    if (shape == CursorShape::Underline) parameter = blink ? 3 : 4;
    else if (shape == CursorShape::Bar) parameter = blink ? 5 : 6;
    else parameter = blink ? 1 : 2;
    out += "\x1B[" + std::to_string(parameter) + " q";
}

void Presenter::render_frame(FrameView frame, const std::vector<ActiveRaster>& rasters,
                             bool raster_coverage_changed, std::string& out) {
    const Size size = frame.size();
    const Capabilities& caps = terminal_.capabilities();

    Point believed_cursor{-1, -1};
    bool style_known = false;
    Style current_style{};

    for (int y = 0; y < size.height; ++y) {
        int x = 0;
        while (x < size.width) {
            const Cell first = presentation_cell(frame, Point{x, y}, rasters);
            const Point point{x, y};
            const bool raster_dirty = raster_coverage_changed &&
                                      (raster_coverage_contains(point, rasters) ||
                                       raster_coverage_contains(point, previous_active_rasters_));
            if (!raster_dirty && !cell_changed(first, size, point)) {
                ++x;
                continue;
            }
            if (first.is_continuation()) {
                ++x;  // an orphaned continuation cell (shouldn't occur at a run start); skip
                continue;
            }

            if (believed_cursor.x != x || believed_cursor.y != y) emit_cursor_move(out, x, y);

            int run_end = x;
            bool unsafe_in_run = false;
            while (run_end < size.width) {
                const Cell c = presentation_cell(frame, Point{run_end, y}, rasters);
                if (c.is_continuation()) {
                    ++run_end;
                    continue;
                }
                const Point run_point{run_end, y};
                const bool run_raster_dirty = raster_coverage_changed &&
                                              (raster_coverage_contains(run_point, rasters) ||
                                               raster_coverage_contains(run_point, previous_active_rasters_));
                if (run_end != x && !run_raster_dirty && !cell_changed(c, size, run_point)) break;
                const bool this_unsafe = is_width_unsafe(c.grapheme());
                ++run_end;
                if (this_unsafe) {
                    // Stop the run right here (after consuming any of
                    // this same glyph's own continuation cells): the
                    // terminal's cursor position after an unsafe glyph
                    // is not trusted, so whatever comes next — even if
                    // geometrically adjacent — must get its own
                    // explicit repositioning rather than an assumed
                    // natural advance (D-019). Merging it into the same
                    // run (the original approach) made this invisible:
                    // a run only ever ends where a cursor move was
                    // already needed for an unrelated reason (a gap or
                    // a row change), so the safety reset was never
                    // actually observable.
                    while (run_end < size.width &&
                           presentation_cell(frame, Point{run_end, y}, rasters).is_continuation())
                        ++run_end;
                    unsafe_in_run = true;
                    break;
                }
            }

            for (int rx = x; rx < run_end; ++rx) {
                const Cell c = presentation_cell(frame, Point{rx, y}, rasters);
                if (c.is_continuation()) continue;
                if (!style_known || !(c.style() == current_style)) {
                    out += style_to_sgr(c.style(), caps);
                    current_style = c.style();
                    style_known = true;
                }
                out += c.grapheme();
            }

            believed_cursor = unsafe_in_run ? Point{-1, -1} : Point{run_end, y};
            x = run_end;
        }
    }
}

void Presenter::emit_raster_slices(std::string& out, FrameView frame,
                                   const std::vector<ActiveRaster>& rasters) const {
    // No graphics capability: the fallback cells the cell-diff pass
    // already rendered are the whole story — nothing more to emit.
    for (const ActiveRaster& active : rasters) {
        const RasterSlice& slice = active.slice;
        const Rect& anchor = slice.full_anchor;
        if (anchor.width <= 0 || anchor.height <= 0) continue;
        const Image& image = *slice.image;
        const int img_w = image.width();
        const int img_h = image.height();

        // Occlusion may have sliced visible_rect down to a sub-rect of
        // full_anchor; the image itself is never cropped, so map the
        // cell-space sub-rect back to a proportional pixel-space
        // sub-rect and encode ONLY that — emitting the whole image at
        // the slice's position would redraw pixels a higher layer is
        // supposed to be occluding.
        const int px_x0 = (slice.visible_rect.x - anchor.x) * img_w / anchor.width;
        const int px_y0 = (slice.visible_rect.y - anchor.y) * img_h / anchor.height;
        const int px_x1 =
            (slice.visible_rect.x + slice.visible_rect.width - anchor.x) * img_w / anchor.width;
        const int px_y1 =
            (slice.visible_rect.y + slice.visible_rect.height - anchor.y) * img_h / anchor.height;
        const int crop_w = px_x1 - px_x0;
        const int crop_h = px_y1 - px_y0;
        if (crop_w <= 0 || crop_h <= 0) continue;

        // Scale the cropped region to the pixels those cells actually are.
        // A producer that sized its image from the cell metric hands over an
        // exact match and this is a copy; one that hands over an image at its
        // own natural size — the common case for a picture that came from a
        // file — would otherwise be drawn at that size in the corner of a
        // reservation many times larger, because a Sixel is emitted pixel for
        // pixel and nothing downstream resizes it. Scaling here keeps that
        // out of every widget: the scene says which cells an image occupies,
        // and this layer is the one that knows how many pixels a cell is.
        const Size cell = terminal_.capabilities().cell_pixels;
        const int target_w =
            cell.width > 0 ? std::max(1, slice.visible_rect.width * cell.width) : crop_w;
        const int target_h =
            cell.height > 0 ? std::max(1, slice.visible_rect.height * cell.height) : crop_h;
        Image cropped(target_w, target_h);
        for (int y = 0; y < target_h; ++y) {
            const int src_y = px_y0 + (target_h == crop_h ? y : y * crop_h / target_h);
            for (int x = 0; x < target_w; ++x) {
                const int src_x = px_x0 + (target_w == crop_w ? x : x * crop_w / target_w);
                cropped.set_pixel(x, y, image.pixel(src_x, src_y));
            }
        }

        emit_cursor_move(out, slice.visible_rect.x, slice.visible_rect.y);
        // P2=0 Sixel background pixels use the terminal's current
        // background color. Re-establish the clean backing cell's style
        // immediately before the DCS rather than inheriting whichever
        // style happened to end the cell-diff traversal.
        out += style_to_sgr(frame.at(Point{slice.visible_rect.x, slice.visible_rect.y}).style(),
                            terminal_.capabilities());
        out += encode_sixel(cropped, terminal_.capabilities().sixel_color_registers);
    }
}

std::uint64_t Presenter::image_fingerprint(const Image& image) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t prime = 0x100000001b3ULL;
    const std::size_t bytes = static_cast<std::size_t>(image.stride()) * static_cast<std::size_t>(image.height());
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= image.data()[index];
        hash *= prime;
    }
    return hash;
}

bool Presenter::same_rasters(const std::vector<ActiveRaster>& lhs,
                             const std::vector<ActiveRaster>& rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const RasterSlice& a = lhs[index].slice;
        const RasterSlice& b = rhs[index].slice;
        if (a.id != b.id || !(a.visible_rect == b.visible_rect) || !(a.full_anchor == b.full_anchor) ||
            a.image.get() != b.image.get() || lhs[index].fingerprint != rhs[index].fingerprint)
            return false;
    }
    return true;
}

bool Presenter::raster_coverage_contains(Point point, const std::vector<ActiveRaster>& rasters) const noexcept {
    for (const ActiveRaster& raster : rasters)
        if (raster_footprint(raster.slice).contains(point)) return true;
    return false;
}

Rect Presenter::raster_footprint(const RasterSlice& slice) noexcept {
    // The cells an image can put pixels on — which is not always the cells
    // it was given. Its pixel size need not be a whole number of cells: the
    // metric it was built from is the terminal's own report, and that report
    // is rounded, sometimes wrong, and in either case says nothing about
    // where the terminal will stop drawing. An image that ends part-way
    // through the next row still colours that row.
    //
    // Which matters because a Sixel only ever paints. Nothing erases what it
    // covered except a later repaint of those exact cells, so a cell inside
    // the real footprint but outside the reservation keeps its pixels for
    // good — and once the image moves on, they stay where the image used to
    // be. Reaching one cell further costs a repaint of a cell that likely
    // needed none; not reaching it leaves a smear that never goes away.
    const Rect& visible = slice.visible_rect;
    if (visible.empty()) return visible;
    return Rect{visible.x, visible.y, visible.width + 1, visible.height + 1};
}

void Presenter::present(FrameView frame, CursorState cursor,
                         const std::vector<RasterSlice>& rasters) {
    active_rasters_.clear();
    if (active_rasters_.capacity() < rasters.size()) active_rasters_.reserve(rasters.size());
    for (const RasterSlice& slice : rasters) {
        if (!can_emit_raster_slice(slice)) continue;
        active_rasters_.push_back(ActiveRaster{slice, image_fingerprint(*slice.image)});
    }
    const bool raster_coverage_changed = !same_rasters(active_rasters_, previous_active_rasters_);

    std::string& out = output_buffer_;
    out.clear();
    render_frame(frame, active_rasters_, raster_coverage_changed, out);
    emit_raster_slices(out, frame, active_rasters_);

    // Writing cells moves the terminal's own cursor, so after any painting
    // it sits wherever the last run happened to end -- and a VISIBLE cursor
    // is then drawn there: a stray bright cell that wanders with whatever
    // was repainted, which during a window drag is the window's edge.
    // Putting it back is therefore part of finishing a frame, not something
    // to do only when the application moved it.
    const bool cursor_changed = force_full_ || !previous_cursor_ || cursor != *previous_cursor_;
    if (cursor.visible) {
        // Nothing was painted, so nothing moved it: leave the frame empty
        // rather than spending bytes to restate where it already is.
        if (cursor_changed || !out.empty())
            emit_cursor_move(out, cursor.position.x, cursor.position.y);
        if (cursor_changed) {
            emit_cursor_shape(out, cursor.shape, cursor.blink);
            out += "\x1B[?25h";
        }
    } else if (cursor_changed) {
        out += "\x1B[?25l";
    }

    if (terminal_.capabilities().synchronized_output && !out.empty()) {
        constexpr std::string_view kSynchronizedOutputBegin = "\x1B[?2026h";
        constexpr std::string_view kSynchronizedOutputEnd = "\x1B[?2026l";
        out.reserve(out.size() + kSynchronizedOutputBegin.size() + kSynchronizedOutputEnd.size());
        out.insert(0, kSynchronizedOutputBegin);
        out += kSynchronizedOutputEnd;
    }

    last_bytes_emitted_ = out.size();
    if (!out.empty()) terminal_.write(out);

    previous_cells_.resize(static_cast<std::size_t>(frame.size().width) *
                           static_cast<std::size_t>(frame.size().height));
    for (int y = 0; y < frame.size().height; ++y)
        for (int x = 0; x < frame.size().width; ++x)
            previous_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.size().width) +
                             static_cast<std::size_t>(x)] =
                             presentation_cell(frame, Point{x, y}, active_rasters_);
    previous_size_ = frame.size();
    previous_cursor_ = cursor;
    if (previous_active_rasters_.capacity() < active_rasters_.size())
        previous_active_rasters_.reserve(active_rasters_.size());
    previous_active_rasters_ = active_rasters_;
    force_full_ = false;
}

}  // namespace ckv::term
