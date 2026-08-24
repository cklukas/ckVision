// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/presenter.hpp"

#include <array>
#include <chrono>
#include <climits>
#include <limits>
#include <string>

#include "cvision/core/assert.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/term/graphics_log.hpp"
#include "cvision/term/pointer_shape_names.hpp"
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

// Why a picture the scene placed did not reach the terminal — in the
// caller's terms, so a reader looking at a window with a fallback in it
// learns whether their host cannot show pictures, or will not show one
// this large. Built only when the graphics log is on.
std::string raster_refusal_reason(const Capabilities& caps, const RasterSlice& slice) {
    const auto pixels = [](Size size) {
        return std::to_string(size.width) + "x" + std::to_string(size.height) + " px";
    };
    if (!caps.sixel_graphics) return "picture dropped: this host reports no Sixel graphics";
    if (slice.image == nullptr || slice.image->empty()) return "picture dropped: the image is empty";
    if (slice.visible_rect.empty()) return "picture dropped: none of it is visible";
    if (slice.full_anchor.empty()) return "picture dropped: it was given no cells to occupy";
    return "picture dropped: " + pixels(Size{slice.image->width(), slice.image->height()}) +
           " is larger than the host's stated maximum Sixel geometry of " +
           pixels(caps.sixel_max_geometry) + " — render within it and the terminal will scale it up";
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

std::optional<std::int64_t> cursor_deadline_after(
    std::int64_t now_nanos, std::int64_t half_period_nanos) noexcept {
    if (now_nanos >
        std::numeric_limits<std::int64_t>::max() - half_period_nanos)
        return std::nullopt;
    return now_nanos + half_period_nanos;
}

Presenter::EncodeKey Presenter::encode_key(const RasterSlice& slice, std::uint64_t fingerprint) const noexcept {
    EncodeKey key;
    key.fingerprint = fingerprint;
    key.color_registers = terminal_.capabilities().sixel_color_registers;
    const Rect& anchor = slice.full_anchor;
    if (anchor.width <= 0 || anchor.height <= 0 || slice.image == nullptr) return key;
    const int img_w = slice.image->width();
    const int img_h = slice.image->height();
    key.crop_x = (slice.visible_rect.x - anchor.x) * img_w / anchor.width;
    key.crop_y = (slice.visible_rect.y - anchor.y) * img_h / anchor.height;
    key.crop_width = (slice.visible_rect.x + slice.visible_rect.width - anchor.x) * img_w / anchor.width - key.crop_x;
    key.crop_height = (slice.visible_rect.y + slice.visible_rect.height - anchor.y) * img_h / anchor.height - key.crop_y;
    const Size cell = terminal_.capabilities().cell_pixels;
    key.target_width = cell.width > 0 ? std::max(1, slice.visible_rect.width * cell.width) : key.crop_width;
    key.target_height = cell.height > 0 ? std::max(1, slice.visible_rect.height * cell.height) : key.crop_height;
    return key;
}

void Presenter::build_presentation(FrameView frame, const std::vector<ActiveRaster>& rasters) {
    const Size size = frame.size();
    presentation_cells_.resize(static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height));
    for (int y = 0; y < size.height; ++y)
        for (int x = 0; x < size.width; ++x)
            presentation_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) +
                                static_cast<std::size_t>(x)] = presentation_cell(frame, Point{x, y}, rasters);
}

void Presenter::mark_rasters_repainted(std::vector<ActiveRaster>& rasters, int y, int x, int end) noexcept {
    for (ActiveRaster& raster : rasters) {
        if (raster.needs_emit) continue;
        const Rect footprint = raster_footprint(raster.slice);
        if (y < footprint.y || y >= footprint.y + footprint.height) continue;
        if (end <= footprint.x || x >= footprint.x + footprint.width) continue;
        raster.needs_emit = true;
    }
}

void Presenter::render_frame(FrameView frame, std::vector<ActiveRaster>& rasters,
                             bool raster_coverage_changed, std::string& out) {
    const Size size = frame.size();
    const Capabilities& caps = terminal_.capabilities();

    Point believed_cursor{-1, -1};
    bool style_known = false;
    Style current_style{};

    for (int y = 0; y < size.height; ++y) {
        int x = 0;
        while (x < size.width) {
            const Cell& first = presentation_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) + static_cast<std::size_t>(x)];
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
                const Cell& c = presentation_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) + static_cast<std::size_t>(run_end)];
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
                           presentation_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) +
                                               static_cast<std::size_t>(run_end)]
                               .is_continuation())
                        ++run_end;
                    unsafe_in_run = true;
                    break;
                }
            }

            for (int rx = x; rx < run_end; ++rx) {
                const Cell& c = presentation_cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) + static_cast<std::size_t>(rx)];
                if (c.is_continuation()) continue;
                if (!style_known || !(c.style() == current_style)) {
                    out += style_to_sgr(c.style(), caps);
                    current_style = c.style();
                    style_known = true;
                }
                out += c.grapheme();
            }

            // Those cells are now text on the host, which is to say the
            // pixels any picture had there are gone.
            mark_rasters_repainted(rasters, y, x, run_end);

            believed_cursor = unsafe_in_run ? Point{-1, -1} : Point{run_end, y};
            x = run_end;
        }
    }
}

void Presenter::emit_raster_slices(std::string& out, FrameView frame,
                                   std::vector<ActiveRaster>& rasters) const {
    // No graphics capability: the fallback cells the cell-diff pass
    // already rendered are the whole story — nothing more to emit.
    for (ActiveRaster& active : rasters) {
        // A picture already on the host, on cells nothing repainted, is
        // already right. Re-sending it every frame was the whole cost of
        // having one on screen: a re-encode and a quarter of a megabyte,
        // per frame, to arrive at the pixels that were already there.
        if (!active.needs_emit) continue;
        const RasterSlice& slice = active.slice;
        const Rect& anchor = slice.full_anchor;
        if (anchor.width <= 0 || anchor.height <= 0) continue;
        const Image& image = *slice.image;

        // Occlusion may have sliced visible_rect down to a sub-rect of
        // full_anchor; the image itself is never cropped, so the key holds
        // the proportional pixel-space sub-rect and ONLY that is encoded —
        // emitting the whole image at the slice's position would redraw
        // pixels a higher layer is supposed to be occluding.
        const int px_x0 = active.key.crop_x;
        const int px_y0 = active.key.crop_y;
        const int crop_w = active.key.crop_width;
        const int crop_h = active.key.crop_height;
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
        // Cropping, scaling and encoding all depend only on the image and
        // this slice's geometry, so the answer survives as long as both do:
        // a picture repainted over is re-sent from these bytes rather than
        // computed again.
        const auto encode_started = graphics_log_enabled() ? std::chrono::steady_clock::now()
                                                           : std::chrono::steady_clock::time_point{};
        const bool had_encoding = active.encoded != nullptr;
        if (active.encoded == nullptr) {
            const int target_w = active.key.target_width;
            const int target_h = active.key.target_height;
            // A producer that sized its image from the cell metric, and whose
            // picture is wholly visible, hands over exactly the pixels this
            // would copy. Copying them into a second image of the same size —
            // a megabyte or two, allocated and thrown away every frame an
            // animation presents — buys nothing, so the encoder reads the
            // caller's own image instead.
            const bool copy_would_be_identity = px_x0 == 0 && px_y0 == 0 && crop_w == image.width() &&
                                                crop_h == image.height() && target_w == crop_w &&
                                                target_h == crop_h;
            if (copy_would_be_identity) {
                active.encoded = std::make_shared<const std::string>(
                    encode_sixel(image, active.key.color_registers));
            } else {
                Image cropped(target_w, target_h);
                for (int y = 0; y < target_h; ++y) {
                    const int src_y = px_y0 + (target_h == crop_h ? y : y * crop_h / target_h);
                    for (int x = 0; x < target_w; ++x) {
                        const int src_x = px_x0 + (target_w == crop_w ? x : x * crop_w / target_w);
                        cropped.set_pixel(x, y, image.pixel(src_x, src_y));
                    }
                }
                active.encoded = std::make_shared<const std::string>(
                    encode_sixel(cropped, active.key.color_registers));
            }
        }

        emit_cursor_move(out, slice.visible_rect.x, slice.visible_rect.y);
        // P2=0 Sixel background pixels use the terminal's current
        // background color. Re-establish the clean backing cell's style
        // immediately before the DCS rather than inheriting whichever
        // style happened to end the cell-diff traversal.
        out += style_to_sgr(frame.at(Point{slice.visible_rect.x, slice.visible_rect.y}).style(),
                            terminal_.capabilities());
        out += *active.encoded;
        if (graphics_log_enabled() && !had_encoding) {
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                        encode_started)
                                  .count();
            graphics_log("  encoded a " + std::to_string(active.key.target_width) + "x" +
                         std::to_string(active.key.target_height) + " picture into " +
                         std::to_string(active.encoded->size() / 1024) + " KiB in " + std::to_string(ms) + " ms");
        }
    }
}

std::uint64_t Presenter::image_fingerprint(const Image& image) noexcept {
    // Eight bytes at a time. This runs on every image on screen, every
    // frame, purely to notice a picture that was rewritten in place through
    // the same pointer; a byte at a time made simply having a picture on
    // screen cost a megabyte of hashing per frame.
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t prime = 0x100000001b3ULL;
    const std::size_t bytes = static_cast<std::size_t>(image.stride()) * static_cast<std::size_t>(image.height());
    const unsigned char* const data = image.data();
    std::size_t index = 0;
    for (; index + sizeof(std::uint64_t) <= bytes; index += sizeof(std::uint64_t)) {
        std::uint64_t word = 0;
        for (std::size_t byte = 0; byte < sizeof(std::uint64_t); ++byte)
            word |= static_cast<std::uint64_t>(data[index + byte]) << (byte * 8);
        hash ^= word;
        hash *= prime;
    }
    for (; index < bytes; ++index) {
        hash ^= data[index];
        hash *= prime;
    }
    return hash;
}

bool Presenter::same_raster_geometry(const std::vector<ActiveRaster>& lhs,
                                     const std::vector<ActiveRaster>& rhs) noexcept {
    // Geometry only, deliberately: what this comparison decides is whether
    // the CELLS beneath the pictures have to be re-established, and cells
    // care about where pictures are, not what they show. A picture replaced
    // in place paints over every pixel of its predecessor, so re-clearing
    // the cells under it buys nothing and costs the reader a visible hole:
    // any render a host performs between the clear and the picture shows
    // the cleared surface where the picture was (observed on iTerm2 as a
    // blue flash, once per animation frame).
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const RasterSlice& a = lhs[index].slice;
        const RasterSlice& b = rhs[index].slice;
        if (a.id != b.id || !(a.visible_rect == b.visible_rect) || !(a.full_anchor == b.full_anchor))
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

void Presenter::present_pointer_shape(PointerShape shape) {
    const Capabilities caps = terminal_.capabilities();
    if (!caps.pointer_shapes) return;
    // What this host will actually draw, which is what has to be compared
    // against last time: two shapes that both degrade to the same pointer
    // are the same request, and re-sending one of them changes nothing on
    // screen while still costing bytes on every pointer move.
    const PointerShape effective = effective_pointer_shape(caps, shape);
    // Compared as the NAME rather than the shape, because the name is what
    // the host is told and two shapes can share one. Both window corners
    // resolve to the same legacy pointer, and the pointer crossing from one
    // to the other would otherwise re-send identical bytes on every motion
    // report for a screen that does not change.
    const std::string_view name = pointer_shape_name(effective, caps.pointer_shape_vocabulary);
    if (previous_pointer_shape_name_ && *previous_pointer_shape_name_ == name) return;
    // Nothing is skipped for having no opinion. "No opinion" is the ordinary
    // arrow and is stated like any other shape, including on the very first
    // frame: a host may be drawing a pointer of its own while mouse
    // reporting is on, and staying quiet leaves that pointer in place over
    // an application that has taken the whole screen.
    previous_pointer_shape_name_ = std::string(name);
    std::string out("\x1B]22;");
    out += name;
    out += "\x1B\\";
    terminal_.write(out);
}

void Presenter::append_cursor(std::string& out, CursorState cursor,
                              bool content_was_emitted) const {
    const bool cursor_changed = force_full_ || !previous_cursor_ ||
                                cursor != *previous_cursor_;
    if (cursor.visible) {
        // Cell/raster output moves the terminal cursor. Cursor-only phase
        // changes do not, so a stable visible phase remains a zero-byte no-op.
        if (cursor_changed || content_was_emitted)
            emit_cursor_move(out, cursor.position.x, cursor.position.y);
        if (cursor_changed) {
            emit_cursor_shape(out, cursor.shape, cursor.blink);
            out += "\x1B[?25h";
        }
    } else if (cursor_changed) {
        out += "\x1B[?25l";
    }
}

bool Presenter::advance_cursor_phase(std::int64_t now_nanos) noexcept {
    if (!next_cursor_blink_nanos_ || now_nanos < *next_cursor_blink_nanos_)
        return false;

    const std::int64_t elapsed = now_nanos - *next_cursor_blink_nanos_;
    const std::int64_t half_period_nanos =
        logical_cursor_->blink_half_period_nanos;
    const std::int64_t periods =
        elapsed / half_period_nanos + 1;
    if ((periods & 1) != 0) cursor_blink_visible_ = !cursor_blink_visible_;

    const std::int64_t room = std::numeric_limits<std::int64_t>::max() -
                              *next_cursor_blink_nanos_;
    if (periods > room / half_period_nanos) {
        next_cursor_blink_nanos_.reset();
    } else {
        *next_cursor_blink_nanos_ += periods * half_period_nanos;
    }
    return true;
}

void Presenter::set_logical_cursor(CursorState cursor,
                                   std::int64_t now_nanos) {
    CKV_ASSERT(now_nanos >= 0);
    CKV_ASSERT(!cursor.blink || cursor.blink_half_period_nanos > 0);
    const bool changed = !logical_cursor_ || cursor != *logical_cursor_;
    logical_cursor_ = cursor;
    if (!cursor.visible || !cursor.blink) {
        cursor_blink_visible_ = true;
        next_cursor_blink_nanos_.reset();
        return;
    }
    if (changed || !next_cursor_blink_nanos_) {
        // A newly focused or moved caret is immediately visible for a whole
        // half-period. Keystrokes therefore cannot make it appear to vanish.
        cursor_blink_visible_ = true;
        next_cursor_blink_nanos_ = cursor_deadline_after(
            now_nanos, cursor.blink_half_period_nanos);
        return;
    }
    (void)advance_cursor_phase(now_nanos);
}

CursorState Presenter::physical_cursor() const noexcept {
    if (!logical_cursor_) return {};
    CursorState cursor = *logical_cursor_;
    if (cursor.visible && cursor.blink) {
        if (!cursor_blink_visible_) return {};
        // The host cursor stays steady. Its user preference can no longer
        // suppress, slow, or double the cadence ckVision owns.
        cursor.blink = false;
    }
    return cursor;
}

bool Presenter::advance_cursor_blink(std::int64_t now_nanos) {
    CKV_ASSERT(now_nanos >= 0);
    if (!logical_cursor_ || !logical_cursor_->visible ||
        !logical_cursor_->blink || !advance_cursor_phase(now_nanos))
        return false;

    const CursorState cursor = physical_cursor();
    std::string& out = output_buffer_;
    out.clear();
    append_cursor(out, cursor, false);
    last_bytes_emitted_ = out.size();
    if (!out.empty()) terminal_.write(out);
    previous_cursor_ = cursor;
    return true;
}

void Presenter::present(FrameView frame, CursorState cursor,
                         std::int64_t now_nanos,
                         const std::vector<RasterSlice>& rasters) {
    set_logical_cursor(cursor, now_nanos);
    cursor = physical_cursor();
    // Live timing of one frame, end to end. The interesting number is not the
    // work this process does but how long the host takes to swallow it: a
    // terminal decoding a quarter-megabyte Sixel blocks the write, and that
    // shows up here and nowhere else.
    const bool tracing = graphics_log_enabled();
    const auto frame_started = tracing ? std::chrono::steady_clock::now()
                                       : std::chrono::steady_clock::time_point{};
    double idle_ms = 0;
    if (tracing && last_present_finished_ != std::chrono::steady_clock::time_point{})
        idle_ms = std::chrono::duration<double, std::milli>(frame_started - last_present_finished_).count();
    active_rasters_.clear();
    if (active_rasters_.capacity() < rasters.size()) active_rasters_.reserve(rasters.size());
    for (const RasterSlice& slice : rasters) {
        if (!can_emit_raster_slice(slice)) {
            // A picture the scene placed, absent from the screen, and
            // otherwise silent about it — the exact question this log
            // exists to answer. Every other line below reports pictures
            // that DID reach the terminal, so a refusal has to say so
            // here or it is indistinguishable from a frame that never
            // had one.
            if (graphics_log_enabled())
                graphics_log("presenter: " + raster_refusal_reason(terminal_.capabilities(), slice));
            continue;
        }
        const std::uint64_t fingerprint = image_fingerprint(*slice.image);
        ActiveRaster active{slice, fingerprint, encode_key(slice, fingerprint), nullptr, true};
        // Two separate questions, and answering them as one is what made a
        // dragged window re-encode its picture every frame. Does this need
        // *encoding*? Only if no previous slice produced these very bytes.
        // Does it need *sending*? Only if it moved, changed, or was painted
        // over. A full repaint keeps neither answer, since the cell metric
        // the picture was scaled to may itself have changed.
        if (!force_full_) {
            for (const ActiveRaster& previous : previous_active_rasters_) {
                if (!(previous.key == active.key)) continue;
                active.encoded = previous.encoded;
                // A picture that moved is sent again, every time, at every
                // position it passes through. Holding it back until the
                // movement stopped was measurably cheaper and visibly wrong:
                // the picture vanished for the whole of a drag and came back
                // only when some unrelated repaint happened to ask for it.
                // Cheap is not a reason to leave the screen lying, and the
                // encode is cached, so what a moved picture costs now is the
                // send and nothing else.
                active.needs_emit = previous.slice.id != slice.id ||
                                    !(previous.slice.visible_rect == slice.visible_rect) ||
                                    !(previous.slice.full_anchor == slice.full_anchor);
                break;
            }
        }
        active_rasters_.push_back(std::move(active));
    }
    const bool raster_coverage_changed =
        !same_raster_geometry(active_rasters_, previous_active_rasters_);

    std::string& out = output_buffer_;
    out.clear();
    build_presentation(frame, active_rasters_);
    render_frame(frame, active_rasters_, raster_coverage_changed, out);
    emit_raster_slices(out, frame, active_rasters_);

    // Writing cells moves the terminal's own cursor, so after any painting
    // it sits wherever the last run happened to end -- and a VISIBLE cursor
    // is then drawn there: a stray bright cell that wanders with whatever
    // was repainted, which during a window drag is the window's edge.
    // Putting it back is therefore part of finishing a frame, not something
    // to do only when the application moved it.
    append_cursor(out, cursor, !out.empty());

    bool frame_sends_raster = false;
    for (const ActiveRaster& active : active_rasters_)
        frame_sends_raster = frame_sends_raster || active.needs_emit;

    if (terminal_.capabilities().synchronized_output && !out.empty()) {
        constexpr std::string_view kSynchronizedOutputBegin = "\x1B[?2026h";
        constexpr std::string_view kSynchronizedOutputEnd = "\x1B[?2026l";
        out.reserve(out.size() + kSynchronizedOutputBegin.size() + kSynchronizedOutputEnd.size());
        out.insert(0, kSynchronizedOutputBegin);
        out += kSynchronizedOutputEnd;
    }

    if (graphics_log_enabled() && (!active_rasters_.empty() || !previous_active_rasters_.empty())) {
        std::size_t sending = 0;
        for (const ActiveRaster& active : active_rasters_) sending += active.needs_emit ? 1U : 0U;
        graphics_log("presenter: frame with " + std::to_string(active_rasters_.size()) + " picture(s), " +
                     std::to_string(sending) + " re-sent, host sixel=" +
                     (terminal_.capabilities().sixel_graphics ? "yes" : "NO") + ", " +
                     std::to_string(out.size() / 1024) + " KiB total");
    }
    // The question goes last, after the frame it is asking about, because
    // a terminal answers it having read everything before it. An empty
    // frame is not asked about: there is nothing for the terminal to have
    // finished, and a question with no frame in front of it would only
    // measure the round trip of the question.
    last_frame_carried_rasters_ = frame_sends_raster;
    if (track_frame_completion_ && !out.empty()) {
        out += "\x1B[5n";
        ++frames_marked_;
    }
    last_bytes_emitted_ = out.size();
    const auto write_started = tracing ? std::chrono::steady_clock::now()
                                       : std::chrono::steady_clock::time_point{};
    if (!out.empty()) terminal_.write(out);
    if (tracing) {
        const auto finished = std::chrono::steady_clock::now();
        const double write_ms = std::chrono::duration<double, std::milli>(finished - write_started).count();
        const double frame_ms = std::chrono::duration<double, std::milli>(finished - frame_started).count();
        std::size_t pictures_sent = 0;
        for (const ActiveRaster& active : active_rasters_) pictures_sent += active.needs_emit ? 1U : 0U;
        if (!out.empty() || !active_rasters_.empty())
            graphics_log("frame: " + std::to_string(frame_ms) + " ms (write " + std::to_string(write_ms) +
                         " ms of " + std::to_string(out.size() / 1024) + " KiB), " +
                         std::to_string(active_rasters_.size()) + " picture(s), " +
                         std::to_string(pictures_sent) + " sent, " + std::to_string(idle_ms) +
                         " ms since the last frame");
        last_present_finished_ = finished;
    }

    // The frame just presented becomes the next one's basis, and the buffer
    // it displaced becomes the next one's scratch — sized now, so a steady
    // frame allocates nothing at all.
    previous_cells_.swap(presentation_cells_);
    presentation_cells_.resize(previous_cells_.size());
    previous_size_ = frame.size();
    previous_cursor_ = cursor;
    if (previous_active_rasters_.capacity() < active_rasters_.size())
        previous_active_rasters_.reserve(active_rasters_.size());
    previous_active_rasters_ = active_rasters_;
    force_full_ = false;
}

}  // namespace ckv::term
