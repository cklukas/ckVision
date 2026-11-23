// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Themes are flat, inspectable semantic tables (ARCHITECTURE.md §5,
// DECISIONS.md D-007): interned role ids resolve to Style via one
// indexed lookup, no cascade, no selectors.
//
// v1 simplification (documented, not an oversight): ARCHITECTURE
// describes fallback-declaring registration as "an existing role or an
// explicit Style". This module implements the explicit-Style half only
// — every intern() call supplies a concrete fallback Style, not an
// alias to another role. Role-to-role fallback chains are a
// straightforward future extension if a real use case needs them; no
// widget or built-in scheme in M4-M6 does.
#pragma once

#include "cvision/core/export.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/shadow_style.hpp"
#include "cvision/core/style.hpp"

namespace ckv::ui {

// A theme role's interned id: a dense index, assigned from 0 in interning order, into one
// RoleRegistry. An id is meaningless against any other registry.
using RoleId = int;
// The id RoleRegistry::find() returns for a name that was never interned. Not a valid argument to
// any lookup.
inline constexpr RoleId kInvalidRole = -1;

// Instance-owned (D-008): each Application has its own registry, not a
// process-global one. Role names are namespaced strings by convention
// (e.g. "ckv.button.normal"; third-party widgets use their own prefix)
// to avoid collisions across independently developed widget sets.
class CVISION_TYPE_API RoleRegistry {
public:
    // Idempotent: interning the same name twice returns the same id
    // (the fallback from the FIRST call wins; later calls with a
    // different fallback for the same name are a caller bug, not
    // silently accepted — CKV_ASSERT catches it).
    CVISION_MEMBER_API RoleId intern(std::string_view name, Style fallback);

    // The id of an already interned `name`, or kInvalidRole; never interns.
    CVISION_MEMBER_API RoleId find(std::string_view name) const noexcept;
    // The fallback Style and the name `id` was interned with. `id` must be a valid id of this
    // registry (asserted).
    CVISION_MEMBER_API Style fallback(RoleId id) const noexcept;
    CVISION_MEMBER_API const std::string& name(RoleId id) const noexcept;
    // How many roles are interned; every id below this is valid.
    std::size_t size() const noexcept { return names_.size(); }

private:
    std::vector<std::string> names_;
    std::vector<Style> fallbacks_;
};

// A theme: explicit per-role overrides layered over the registry's
// fallbacks, so resolve() always returns something even for a role
// registered after this Theme was constructed (D-007: "a printed
// theme... is the whole truth" — here, truth plus registry fallback).
class CVISION_TYPE_API Theme {
public:
    // An empty theme over `registry`: every role resolves to its fallback until set(). The
    // registry is referenced, not copied, and must outlive the theme and every copy of it.
    explicit Theme(const RoleRegistry& registry) : registry_(&registry) {}

    // Overrides `role` with `style`, replacing any earlier override. `role` must be non-negative
    // (asserted); it is not checked against the registry here, so a role set before it is
    // interned still takes effect once it is.
    CVISION_MEMBER_API void set(RoleId role, Style style);
    // Registers a role-to-role fallback: if `role` has no explicit override,
    // resolve() consults `fallback` before falling back to RoleRegistry.
    CVISION_MEMBER_API void set_fallback(RoleId role, RoleId fallback);
    // The override for `role` if one was set, otherwise any role-to-role fallback,
    // otherwise the registry's fallback. `role` must be a valid id of the registry
    // unless it has an override (the fallback lookup asserts).
    CVISION_MEMBER_API Style resolve(RoleId role) const noexcept;
    // The registry the theme was built over: the roles it can resolve, and their names. A theme
    // editor or a serializer enumerates the theme's roles through it.
    const RoleRegistry& registry() const noexcept { return *registry_; }

    // How a cast shadow darkens what it falls on (D-106). A new theme halves; each built-in scheme
    // states its own (make_classic_theme recolours to dark grey on black). A shadow is a
    // composition effect, so the Application composes every window and popup shadow with its own
    // theme's, never with a subtree's theme override; a Desktop painted without an Application's
    // compositor casts its shadows with the theme it resolves.
    ShadowStyle shadow() const noexcept { return shadow_; }
    void set_shadow(ShadowStyle shadow) noexcept { shadow_ = shadow; }

private:
    const RoleRegistry* registry_;
    std::vector<std::optional<Style>> overrides_;
    std::vector<RoleId> fallbacks_;
    ShadowStyle shadow_;
};

}  // namespace ckv::ui
