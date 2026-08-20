// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ckv::spin {

namespace {

// The camera sits at the origin looking along +z, with the object one
// camera distance away. A vertex's own z is therefore its distance from
// the viewer, and projecting it is a single divide.
constexpr double kCameraDistance = 3.0;
// How much of the frame's shorter side the unit sphere is allowed to
// cover. The remainder is the margin that keeps a rotating shape from
// touching the window frame at its widest moment.
constexpr double kFillFraction = 0.9;
// What a surface turned fully away from the viewer still receives. Zero
// would leave a face at a grazing angle almost black, which reads as a
// hole in the solid rather than as a face seen edge-on.
constexpr double kAmbient = 0.3;

// Beyond this many output pixels the supersampled grid stops paying for
// itself — a picture that large is already being scaled down by the host's
// own cell metric, and the frame budget is better spent arriving on time.
constexpr std::int64_t kMaxSupersampledPixels = 1'200'000;

int supersample_scale(Size pixels) noexcept {
    const auto area = static_cast<std::int64_t>(pixels.width) * pixels.height;
    return area > 0 && area <= kMaxSupersampledPixels ? kSupersample : 1;
}

// How many brightness steps one material is allowed. Every step is a color
// table entry, and so is every partial-coverage blend of one, so the ramp
// has to fit the host's register budget rather than the eye's appetite.
int shade_levels(int budget) noexcept { return std::clamp(budget / 8, 4, 24); }

double quantize(double brightness, int levels) noexcept {
    const double top = static_cast<double>(levels - 1);
    return std::round(std::clamp(brightness, 0.0, 1.0) * top) / top;
}

Image::Rgba shade(Image::Rgba base, double brightness) noexcept {
    const auto channel = [brightness](std::uint8_t value) {
        return static_cast<std::uint8_t>(
            std::lround(std::clamp(static_cast<double>(value) * brightness, 0.0, 255.0)));
    };
    return Image::Rgba{channel(base.r), channel(base.g), channel(base.b), 255};
}

Image::Rgba material_color(const Mesh& mesh, int material) noexcept {
    if (mesh.materials.empty()) return Image::Rgba{255, 255, 255, 255};
    const auto index = static_cast<std::size_t>(std::max(0, material));
    return mesh.materials[index < mesh.materials.size() ? index : 0];
}

}  // namespace

void FramePalette::reset(Image::Rgba background, int budget) noexcept {
    budget_ = std::clamp(budget, 2, 256);
    entries_[0] = background;
    size_ = 1;
}

std::uint8_t FramePalette::intern(Image::Rgba color) noexcept {
    for (int entry = 0; entry < size_; ++entry) {
        const Image::Rgba present = entries_[static_cast<std::size_t>(entry)];
        if (present.r == color.r && present.g == color.g && present.b == color.b)
            return static_cast<std::uint8_t>(entry);
    }
    if (size_ < budget_) {
        entries_[static_cast<std::size_t>(size_)] = color;
        return static_cast<std::uint8_t>(size_++);
    }
    int nearest = 0;
    long best = std::numeric_limits<long>::max();
    for (int entry = 0; entry < size_; ++entry) {
        const Image::Rgba present = entries_[static_cast<std::size_t>(entry)];
        const long dr = static_cast<long>(present.r) - color.r;
        const long dg = static_cast<long>(present.g) - color.g;
        const long db = static_cast<long>(present.b) - color.b;
        const long distance = dr * dr + dg * dg + db * db;
        if (distance < best) {
            best = distance;
            nearest = entry;
        }
    }
    return static_cast<std::uint8_t>(nearest);
}

Image Renderer::render(const Mesh& mesh, const FrameSpec& spec) {
    Image frame(std::max(0, spec.pixels.width), std::max(0, spec.pixels.height));
    if (frame.empty()) return frame;

    const int scale = supersample_scale(spec.pixels);
    const Size raster{frame.width() * scale, frame.height() * scale};
    palette_.reset(spec.background, spec.color_budget);
    coverage_cache_.fill(-1);
    subpixels_.assign(static_cast<std::size_t>(raster.width) * static_cast<std::size_t>(raster.height),
                      0);

    project(mesh, spec, raster);
    const int levels = shade_levels(palette_.budget());
    if (!mesh.faces.empty()) {
        draw_faces(mesh, levels, raster);
    } else {
        // One and a half pixels of wire: a line exactly one pixel wide
        // never covers a whole sample group, so anti-aliasing washes every
        // wire halfway into the background and the shape loses its colour.
        draw_edges(mesh, levels, scale + 1, raster);
    }
    resolve(frame, scale);
    return frame;
}

void Renderer::project(const Mesh& mesh, const FrameSpec& spec, Size raster) {
    const double cos_yaw = std::cos(spec.yaw);
    const double sin_yaw = std::sin(spec.yaw);
    const double cos_pitch = std::cos(spec.pitch);
    const double sin_pitch = std::sin(spec.pitch);
    // A unit sphere at kCameraDistance subtends this much of the frame at
    // the focal length below, so the object is framed the same way whatever
    // the window's aspect ratio is: sized to the shorter side, centred on
    // both.
    const double half = 0.5 * static_cast<double>(std::min(raster.width, raster.height)) * kFillFraction;
    const double focal = half * std::sqrt(kCameraDistance * kCameraDistance - 1.0);
    const double center_x = 0.5 * static_cast<double>(raster.width);
    const double center_y = 0.5 * static_cast<double>(raster.height);

    view_.resize(mesh.vertices.size());
    screen_.resize(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vec3 vertex = mesh.vertices[i];
        const double x = vertex.x * cos_yaw + vertex.z * sin_yaw;
        const double turned = vertex.z * cos_yaw - vertex.x * sin_yaw;
        const double y = vertex.y * cos_pitch - turned * sin_pitch;
        const double depth = vertex.y * sin_pitch + turned * cos_pitch + kCameraDistance;
        view_[i] = Vec3{x, y, depth};
        screen_[i] = Point2{center_x + focal * x / depth, center_y - focal * y / depth};
    }
}

void Renderer::draw_faces(const Mesh& mesh, int levels, Size raster) {
    order_.clear();
    for (std::size_t i = 0; i < mesh.faces.size(); ++i) {
        const std::vector<int>& loop = mesh.faces[i].loop;
        if (loop.size() < 3) continue;
        // Newell's normal and the centroid in one pass over the camera-space
        // vertices. Newell is translation-invariant over a closed loop, so
        // the camera offset in every z does not disturb it.
        Vec3 normal{};
        Vec3 centroid{};
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const Vec3 a = view_[static_cast<std::size_t>(loop[k])];
            const Vec3 b = view_[static_cast<std::size_t>(loop[(k + 1) % loop.size()])];
            normal.x += (a.y - b.y) * (a.z + b.z);
            normal.y += (a.z - b.z) * (a.x + b.x);
            normal.z += (a.x - b.x) * (a.y + b.y);
            centroid = centroid + a;
        }
        centroid = centroid * (1.0 / static_cast<double>(loop.size()));
        // The light is the viewer: how much of a face is lit is exactly how
        // squarely it is turned towards them, which is what makes a rotating
        // solid read as solid.
        const double facing = dot(normalized(normal), normalized(centroid * -1.0));
        if (facing <= 0.0) continue;  // turned away — and with no depth buffer, not drawn
        order_.push_back(Ordered{static_cast<int>(i), centroid.z, facing});
    }
    // Farthest first: with convex shapes and back faces already dropped, an
    // ordered painter needs no depth buffer.
    std::sort(order_.begin(), order_.end(),
              [](const Ordered& a, const Ordered& b) { return a.depth > b.depth; });

    for (const Ordered& entry : order_) {
        const Face& face = mesh.faces[static_cast<std::size_t>(entry.index)];
        const double brightness = quantize(kAmbient + (1.0 - kAmbient) * entry.light, levels);
        const std::uint8_t index = palette_.intern(shade(material_color(mesh, face.material), brightness));
        fill_polygon(face.loop, index, raster);
    }
}

void Renderer::draw_edges(const Mesh& mesh, int levels, int width, Size raster) {
    order_.clear();
    for (std::size_t i = 0; i < mesh.edges.size(); ++i) {
        const Edge& edge = mesh.edges[i];
        const double depth = 0.5 * (view_[static_cast<std::size_t>(edge.from)].z +
                                    view_[static_cast<std::size_t>(edge.to)].z);
        order_.push_back(Ordered{static_cast<int>(i), depth, 0.0});
    }
    std::sort(order_.begin(), order_.end(),
              [](const Ordered& a, const Ordered& b) { return a.depth > b.depth; });

    const Image::Rgba base = material_color(mesh, 0);
    for (const Ordered& entry : order_) {
        const Edge& edge = mesh.edges[static_cast<std::size_t>(entry.index)];
        // Nearer wire is brighter. A wireframe has no surface to catch the
        // light, so depth is the only cue it has for which way it is turned,
        // and without it a tumbling cube reads as a flat hexagon.
        const double nearness =
            std::clamp((kCameraDistance + 1.0 - entry.depth) * 0.5, 0.0, 1.0);
        const std::uint8_t index =
            palette_.intern(shade(base, quantize(0.35 + 0.65 * nearness, levels)));
        draw_segment(screen_[static_cast<std::size_t>(edge.from)],
                     screen_[static_cast<std::size_t>(edge.to)], index, width, raster);
    }
}

void Renderer::fill_polygon(std::span<const int> loop, std::uint8_t index, Size raster) {
    double top = std::numeric_limits<double>::max();
    double bottom = std::numeric_limits<double>::lowest();
    for (const int vertex : loop) {
        top = std::min(top, screen_[static_cast<std::size_t>(vertex)].y);
        bottom = std::max(bottom, screen_[static_cast<std::size_t>(vertex)].y);
    }
    const int first_row = std::max(0, static_cast<int>(std::ceil(top - 0.5)));
    const int last_row = std::min(raster.height - 1, static_cast<int>(std::floor(bottom - 0.5)));

    for (int row = first_row; row <= last_row; ++row) {
        const double scanline = static_cast<double>(row) + 0.5;
        crossings_.clear();
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const Point2 a = screen_[static_cast<std::size_t>(loop[k])];
            const Point2 b = screen_[static_cast<std::size_t>(loop[(k + 1) % loop.size()])];
            // Half-open in y so a vertex shared by two edges is counted once
            // and the polygon has no seams along its own diagonals.
            if ((a.y <= scanline) == (b.y <= scanline)) continue;
            crossings_.push_back(a.x + (scanline - a.y) / (b.y - a.y) * (b.x - a.x));
        }
        std::sort(crossings_.begin(), crossings_.end());
        for (std::size_t pair = 0; pair + 1 < crossings_.size(); pair += 2) {
            const int from = std::max(0, static_cast<int>(std::ceil(crossings_[pair] - 0.5)));
            const int to = std::min(raster.width - 1,
                                    static_cast<int>(std::floor(crossings_[pair + 1] - 0.5)));
            if (to < from) continue;
            const std::size_t offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(raster.width);
            std::fill_n(subpixels_.begin() + static_cast<std::ptrdiff_t>(offset + static_cast<std::size_t>(from)),
                        to - from + 1, index);
        }
    }
}

void Renderer::draw_segment(Point2 from, Point2 to, std::uint8_t index, int width, Size raster) {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
    for (int step = 0; step <= steps; ++step) {
        const double t = static_cast<double>(step) / static_cast<double>(steps);
        stamp(static_cast<int>(std::lround(from.x + dx * t)),
              static_cast<int>(std::lround(from.y + dy * t)), width, index, raster);
    }
}

void Renderer::stamp(int x, int y, int width, std::uint8_t index, Size raster) noexcept {
    const int left = std::max(0, x - width / 2);
    const int top = std::max(0, y - width / 2);
    const int right = std::min(raster.width - 1, left + width - 1);
    const int bottom = std::min(raster.height - 1, top + width - 1);
    for (int row = top; row <= bottom; ++row) {
        const std::size_t offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(raster.width);
        for (int column = left; column <= right; ++column)
            subpixels_[offset + static_cast<std::size_t>(column)] = index;
    }
}

void Renderer::resolve(Image& frame, int scale) {
    const int samples = scale * scale;
    const int stride = frame.width() * scale;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            std::array<std::uint8_t, kSupersampleCount> hits{};
            int hit_count = 0;
            for (int sub_y = 0; sub_y < scale; ++sub_y) {
                const std::size_t offset =
                    static_cast<std::size_t>(y * scale + sub_y) * static_cast<std::size_t>(stride) +
                    static_cast<std::size_t>(x * scale);
                for (int sub_x = 0; sub_x < scale; ++sub_x) {
                    const std::uint8_t index = subpixels_[offset + static_cast<std::size_t>(sub_x)];
                    if (index != 0) hits[static_cast<std::size_t>(hit_count++)] = index;
                }
            }
            if (hit_count == 0) {
                frame.set_pixel(x, y, palette_.color(0));
                continue;
            }
            // The most-covered index wins the pixel and its coverage decides
            // how far it is blended towards the background. Two surfaces
            // meeting inside one pixel is a real edge in the model, and
            // averaging them would smear the crease rather than soften it.
            std::uint8_t winner = hits[0];
            int winner_count = 0;
            for (int i = 0; i < hit_count; ++i) {
                int count = 0;
                for (int j = 0; j < hit_count; ++j) count += hits[static_cast<std::size_t>(j)] == hits[static_cast<std::size_t>(i)] ? 1 : 0;
                const std::uint8_t candidate = hits[static_cast<std::size_t>(i)];
                if (count > winner_count || (count == winner_count && candidate < winner)) {
                    winner = candidate;
                    winner_count = count;
                }
            }
            frame.set_pixel(x, y, palette_.color(coverage_index(winner, hit_count, samples)));
        }
    }
}

std::uint8_t Renderer::coverage_index(std::uint8_t index, int covered, int samples) {
    if (covered >= samples) return index;
    // Keyed on (entry, coverage) alone: the sample count is fixed for the
    // whole frame, so one blend is interned once instead of once per pixel.
    const std::size_t slot = static_cast<std::size_t>(index) * kSupersampleCount +
                             static_cast<std::size_t>(covered);
    if (coverage_cache_[slot] >= 0) return static_cast<std::uint8_t>(coverage_cache_[slot]);
    const Image::Rgba front = palette_.color(index);
    const Image::Rgba back = palette_.color(0);
    const auto mix = [covered, samples](std::uint8_t f, std::uint8_t b) {
        return static_cast<std::uint8_t>((f * covered + b * (samples - covered) + samples / 2) / samples);
    };
    const std::uint8_t blended =
        palette_.intern(Image::Rgba{mix(front.r, back.r), mix(front.g, back.g), mix(front.b, back.b), 255});
    coverage_cache_[slot] = blended;
    return blended;
}

}  // namespace ckv::spin
