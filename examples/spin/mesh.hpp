// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Spin example's model layer: rigid three-dimensional shapes as plain
// data, and the catalog its "new window" menu is built from.
//
// Nothing here knows about views, terminals, threads or time. A Mesh is
// built once and never mutated afterwards, which is the whole of this
// example's sharing contract: the application owns one MeshLibrary and
// hands out const references, so any number of render threads may read
// the same shape at the same time without a lock.
#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "cvision/core/image.hpp"

namespace ckv::spin {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator*(Vec3 v, double s) noexcept { return Vec3{v.x * s, v.y * s, v.z * s}; }
constexpr double dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double length(Vec3 v) noexcept;

// The zero vector normalizes to itself. A degenerate face has no direction
// to report, and inventing one would light a surface that isn't there.
Vec3 normalized(Vec3 v) noexcept;

// One segment of a wireframe.
struct Edge {
    int from = 0;
    int to = 0;
};

// A planar convex polygon over vertex indices, wound counter-clockwise as
// seen from outside the solid, painted in the material `material` names.
struct Face {
    std::vector<int> loop;
    int material = 0;
};

// A shape is either a wireframe (edges, no faces) or a solid (faces, no
// edges) — the renderer draws whichever one it finds. The distinction is
// data rather than a flag because the two carry different information:
// a wireframe means every edge to be shown, including the ones at the
// back, and a solid without a depth buffer must not show its own.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    // Base colors, before shading. Edges use materials[0].
    std::vector<Image::Rgba> materials;
};

enum class ShapeId {
    WireCube,
    SolidCube,
    Octahedron,
    Icosahedron,
    Torus,
    WireGlobe,
};

inline constexpr std::size_t kShapeCount = 6;

// One catalog row: the shape, the key its "new window" command is
// declared under, the menu label that opens it (carrying its own '&'
// mnemonic), and the stem those windows are titled with.
struct ShapeEntry {
    ShapeId id = ShapeId::WireCube;
    std::string_view command_key;
    std::string_view menu_label;
    std::string_view title;
};

// Every shape the example offers, in menu order.
std::span<const ShapeEntry> shape_catalog() noexcept;

// The meshes themselves, built once at construction and const thereafter.
class MeshLibrary {
public:
    MeshLibrary();

    const Mesh& mesh(ShapeId id) const noexcept;

private:
    std::array<Mesh, kShapeCount> meshes_;
};

}  // namespace ckv::spin
