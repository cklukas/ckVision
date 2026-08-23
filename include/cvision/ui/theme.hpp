// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Themes are flat, inspectable semantic tables (the architecture §5,
// The decision log D-007): interned role ids resolve to Style via one
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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/style.hpp"

namespace ckv::ui {

using RoleId = int;
inline constexpr RoleId kInvalidRole = -1;

// Instance-owned (D-008): each Application has its own registry, not a
// process-global one. Role names are namespaced strings by convention
// (e.g. "ckv.button.normal"; third-party widgets use their own prefix)
// to avoid collisions across independently developed widget sets.
class RoleRegistry {
public:
    // Idempotent: interning the same name twice returns the same id
    // (the fallback from the FIRST call wins; later calls with a
    // different fallback for the same name are a caller bug, not
    // silently accepted — CKV_ASSERT catches it).
    RoleId intern(std::string_view name, Style fallback);

    RoleId find(std::string_view name) const noexcept;
    Style fallback(RoleId id) const noexcept;
    const std::string& name(RoleId id) const noexcept;
    std::size_t size() const noexcept { return names_.size(); }

private:
    std::vector<std::string> names_;
    std::vector<Style> fallbacks_;
};

// A theme: explicit per-role overrides layered over the registry's
// fallbacks, so resolve() always returns something even for a role
// registered after this Theme was constructed (D-007: "a printed
// theme... is the whole truth" — here, truth plus registry fallback).
class Theme {
public:
    explicit Theme(const RoleRegistry& registry) : registry_(&registry) {}

    void set(RoleId role, Style style);
    Style resolve(RoleId role) const noexcept;

private:
    const RoleRegistry* registry_;
    std::vector<std::optional<Style>> overrides_;
};

}  // namespace ckv::ui
