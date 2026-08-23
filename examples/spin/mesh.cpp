// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "mesh.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace ckv::spin {

namespace {

constexpr Image::Rgba rgb(int r, int g, int b) noexcept {
    return Image::Rgba{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                       static_cast<std::uint8_t>(b), 255};
}

// Newell's method: correct for any planar polygon, and — unlike the cross
// product of the first three vertices — independent of whether the three
// vertices that happen to come first are collinear.
Vec3 polygon_normal(const Mesh& mesh, std::span<const int> loop) noexcept {
    Vec3 normal{};
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Vec3 a = mesh.vertices[static_cast<std::size_t>(loop[i])];
        const Vec3 b = mesh.vertices[static_cast<std::size_t>(loop[(i + 1) % loop.size()])];
        normal.x += (a.y - b.y) * (a.z + b.z);
        normal.y += (a.z - b.z) * (a.x + b.x);
        normal.z += (a.x - b.x) * (a.y + b.y);
    }
    return normal;
}

Vec3 polygon_centroid(const Mesh& mesh, std::span<const int> loop) noexcept {
    Vec3 sum{};
    for (const int index : loop) sum = sum + mesh.vertices[static_cast<std::size_t>(index)];
    return loop.empty() ? sum : sum * (1.0 / static_cast<double>(loop.size()));
}

// Adds one face, wound so that its normal points along `outward`.
//
// Stating which way is out is what makes the winding impossible to get
// wrong. Every builder below knows the outward direction of the surface it
// is generating — a cube face has an axis, a torus quad has an analytic
// normal, a convex solid centred on the origin has its own centroid — so
// none of them has to also keep index order straight, and back-face culling
// downstream can trust what it is given.
void add_face(Mesh& mesh, std::vector<int> loop, int material, Vec3 outward) {
    if (dot(polygon_normal(mesh, loop), outward) < 0.0) std::reverse(loop.begin(), loop.end());
    mesh.faces.push_back(Face{std::move(loop), material});
}

// Every edge of every face, each one exactly once.
void add_face_edges(Mesh& mesh) {
    for (const Face& face : mesh.faces) {
        for (std::size_t i = 0; i < face.loop.size(); ++i) {
            const int from = face.loop[i];
            const int to = face.loop[(i + 1) % face.loop.size()];
            const Edge edge{std::min(from, to), std::max(from, to)};
            const auto same = [edge](const Edge& other) {
                return other.from == edge.from && other.to == edge.to;
            };
            if (std::none_of(mesh.edges.begin(), mesh.edges.end(), same)) mesh.edges.push_back(edge);
        }
    }
}

// Scales every vertex so the farthest one sits on the unit sphere. The
// renderer frames a unit-radius object, so normalizing here is what lets
// every shape share one camera without a per-shape fudge factor.
void normalize_radius(Mesh& mesh) {
    double radius = 0.0;
    for (const Vec3& vertex : mesh.vertices) radius = std::max(radius, length(vertex));
    if (radius <= 0.0) return;
    for (Vec3& vertex : mesh.vertices) vertex = vertex * (1.0 / radius);
}

// The eight corners of a cube, indexed so that bit 0 is the x sign, bit 1
// the y sign and bit 2 the z sign — which is what makes each face below a
// readable filter rather than a table of magic numbers.
Mesh cube_shell() {
    Mesh mesh;
    for (int corner = 0; corner < 8; ++corner)
        mesh.vertices.push_back(Vec3{(corner & 1) != 0 ? 1.0 : -1.0, (corner & 2) != 0 ? 1.0 : -1.0,
                                     (corner & 4) != 0 ? 1.0 : -1.0});
    add_face(mesh, {0, 2, 6, 4}, 0, Vec3{-1.0, 0.0, 0.0});
    add_face(mesh, {1, 3, 7, 5}, 1, Vec3{1.0, 0.0, 0.0});
    add_face(mesh, {0, 1, 5, 4}, 2, Vec3{0.0, -1.0, 0.0});
    add_face(mesh, {2, 3, 7, 6}, 3, Vec3{0.0, 1.0, 0.0});
    add_face(mesh, {0, 1, 3, 2}, 4, Vec3{0.0, 0.0, -1.0});
    add_face(mesh, {4, 5, 7, 6}, 5, Vec3{0.0, 0.0, 1.0});
    normalize_radius(mesh);
    return mesh;
}

Mesh make_wire_cube() {
    Mesh mesh = cube_shell();
    add_face_edges(mesh);
    mesh.faces.clear();
    mesh.materials = {rgb(255, 198, 96)};
    return mesh;
}

Mesh make_solid_cube() {
    Mesh mesh = cube_shell();
    // Six hues, none of them the blue a classic window is painted in: a
    // face the colour of its own background is a face nobody can see.
    mesh.materials = {rgb(214, 96, 88),  rgb(226, 158, 72),  rgb(212, 204, 96),
                      rgb(104, 190, 120), rgb(96, 200, 196), rgb(186, 116, 204)};
    return mesh;
}

Mesh make_octahedron() {
    Mesh mesh;
    mesh.vertices = {Vec3{1, 0, 0}, Vec3{-1, 0, 0}, Vec3{0, 1, 0},
                     Vec3{0, -1, 0}, Vec3{0, 0, 1}, Vec3{0, 0, -1}};
    // One triangle per octant, checkered so that adjacent faces differ by
    // hue as well as by shading.
    for (int octant = 0; octant < 8; ++octant) {
        const std::vector<int> loop{(octant & 1) != 0 ? 1 : 0, (octant & 2) != 0 ? 3 : 2,
                                    (octant & 4) != 0 ? 5 : 4};
        const int material = std::popcount(static_cast<unsigned>(octant)) % 2;
        add_face(mesh, loop, material, polygon_centroid(mesh, loop));
    }
    mesh.materials = {rgb(236, 122, 98), rgb(96, 182, 220)};
    normalize_radius(mesh);
    return mesh;
}

Mesh make_icosahedron() {
    Mesh mesh;
    const double phi = std::numbers::phi;
    for (const double sy : {1.0, -1.0}) {
        for (const double sz : {1.0, -1.0}) {
            mesh.vertices.push_back(Vec3{0.0, sy, sz * phi});
            mesh.vertices.push_back(Vec3{sy, sz * phi, 0.0});
            mesh.vertices.push_back(Vec3{sz * phi, 0.0, sy});
        }
    }
    // Any three vertices one edge length apart span a face — which is the
    // definition of a regular icosahedron rather than a transcribed table,
    // so there is no index list here to get wrong. The construction above
    // puts the shortest distance at 2; a face is a triple that realizes it
    // three times over.
    constexpr double kEdge = 2.0;
    constexpr double kTolerance = 1e-9;
    const auto is_edge = [&mesh](std::size_t a, std::size_t b) {
        return std::abs(length(mesh.vertices[a] - mesh.vertices[b]) - kEdge) < kTolerance;
    };
    for (std::size_t a = 0; a < mesh.vertices.size(); ++a)
        for (std::size_t b = a + 1; b < mesh.vertices.size(); ++b)
            for (std::size_t c = b + 1; c < mesh.vertices.size(); ++c)
                if (is_edge(a, b) && is_edge(b, c) && is_edge(a, c)) {
                    const std::vector<int> loop{static_cast<int>(a), static_cast<int>(b),
                                                static_cast<int>(c)};
                    add_face(mesh, loop, 0, polygon_centroid(mesh, loop));
                }
    mesh.materials = {rgb(228, 184, 100)};
    normalize_radius(mesh);
    return mesh;
}

Mesh make_torus() {
    constexpr int kAround = 32;   // segments the ring is divided into
    constexpr int kThrough = 16;  // segments around the tube
    constexpr double kRingRadius = 1.0;
    constexpr double kTubeRadius = 0.4;
    constexpr double kTwoPi = 2.0 * std::numbers::pi;

    Mesh mesh;
    const auto angle = [](int step, int steps) { return kTwoPi * static_cast<double>(step) / steps; };
    const auto point = [&angle](int i, int j) {
        const double u = angle(i, kAround);
        const double v = angle(j, kThrough);
        const double ring = kRingRadius + kTubeRadius * std::cos(v);
        return Vec3{ring * std::cos(u), ring * std::sin(u), kTubeRadius * std::sin(v)};
    };
    for (int i = 0; i < kAround; ++i)
        for (int j = 0; j < kThrough; ++j) mesh.vertices.push_back(point(i, j));

    const auto index = [](int i, int j) { return (i % kAround) * kThrough + (j % kThrough); };
    for (int i = 0; i < kAround; ++i) {
        for (int j = 0; j < kThrough; ++j) {
            // The surface normal of a torus is known in closed form, so the
            // quad states its own outward direction instead of hoping the
            // parametrization was wound the expected way.
            const double u = angle(i, kAround) + kTwoPi / (2 * kAround);
            const double v = angle(j, kThrough) + kTwoPi / (2 * kThrough);
            const Vec3 outward{std::cos(v) * std::cos(u), std::cos(v) * std::sin(u), std::sin(v)};
            add_face(mesh, {index(i, j), index(i + 1, j), index(i + 1, j + 1), index(i, j + 1)}, 0,
                     outward);
        }
    }
    mesh.materials = {rgb(96, 198, 190)};
    normalize_radius(mesh);
    return mesh;
}

Mesh make_wire_globe() {
    constexpr int kMeridians = 16;
    constexpr int kParallels = 8;  // bands from pole to pole
    constexpr double kPi = std::numbers::pi;

    Mesh mesh;
    // The poles are single vertices; every band between them is a ring of
    // kMeridians. Numbering them this way keeps both edge families — the
    // rings and the meridians — a matter of arithmetic rather than of
    // bookkeeping.
    mesh.vertices.push_back(Vec3{0.0, 1.0, 0.0});
    for (int band = 1; band < kParallels; ++band) {
        const double polar = kPi * static_cast<double>(band) / kParallels;
        for (int meridian = 0; meridian < kMeridians; ++meridian) {
            const double azimuth = 2.0 * kPi * static_cast<double>(meridian) / kMeridians;
            mesh.vertices.push_back(Vec3{std::sin(polar) * std::cos(azimuth), std::cos(polar),
                                         std::sin(polar) * std::sin(azimuth)});
        }
    }
    mesh.vertices.push_back(Vec3{0.0, -1.0, 0.0});

    const int north = 0;
    const int south = static_cast<int>(mesh.vertices.size()) - 1;
    const auto ring = [](int band, int meridian) {
        return 1 + (band - 1) * kMeridians + (meridian % kMeridians);
    };
    for (int meridian = 0; meridian < kMeridians; ++meridian) {
        mesh.edges.push_back(Edge{north, ring(1, meridian)});
        mesh.edges.push_back(Edge{ring(kParallels - 1, meridian), south});
        for (int band = 1; band < kParallels; ++band) {
            mesh.edges.push_back(Edge{ring(band, meridian), ring(band, meridian + 1)});
            if (band + 1 < kParallels)
                mesh.edges.push_back(Edge{ring(band, meridian), ring(band + 1, meridian)});
        }
    }
    mesh.materials = {rgb(124, 210, 238)};
    return mesh;
}

constexpr std::array<ShapeEntry, kShapeCount> kCatalog{{
    {ShapeId::WireCube, "spin.new.wire-cube", "&Wireframe Cube", "Wireframe Cube"},
    {ShapeId::SolidCube, "spin.new.solid-cube", "&Solid Cube", "Solid Cube"},
    {ShapeId::Octahedron, "spin.new.octahedron", "&Octahedron", "Octahedron"},
    {ShapeId::Icosahedron, "spin.new.icosahedron", "&Icosahedron", "Icosahedron"},
    {ShapeId::Torus, "spin.new.torus", "&Torus", "Torus"},
    {ShapeId::WireGlobe, "spin.new.wire-globe", "Wireframe &Globe", "Wireframe Globe"},
}};

}  // namespace

double length(Vec3 v) noexcept { return std::sqrt(dot(v, v)); }

Vec3 normalized(Vec3 v) noexcept {
    const double len = length(v);
    return len > 0.0 ? v * (1.0 / len) : v;
}

std::span<const ShapeEntry> shape_catalog() noexcept { return kCatalog; }

MeshLibrary::MeshLibrary()
    : meshes_{make_wire_cube(),  make_solid_cube(), make_octahedron(),
              make_icosahedron(), make_torus(),     make_wire_globe()} {}

const Mesh& MeshLibrary::mesh(ShapeId id) const noexcept {
    const auto index = static_cast<std::size_t>(id);
    return meshes_[index < meshes_.size() ? index : 0];
}

}  // namespace ckv::spin
