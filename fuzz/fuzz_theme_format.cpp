// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme_format.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    const ckv::ui::Theme base = ckv::ui::make_classic_theme(registry, roles);

    const ckv::ui::ThemeParseResult parsed = ckv::ui::parse_theme(input, base);
    if (!parsed) {
        // A refusal says why, and never hands back part of a theme.
        ckv::fuzz::require(!parsed.error.message.empty());
        ckv::fuzz::require(parsed.unknown_roles.empty());
        return 0;
    }
    // Anything accepted is a theme whose own text reads back to itself.
    const std::string canonical = ckv::ui::serialize_theme(*parsed.theme);
    const ckv::ui::ThemeParseResult reparsed = ckv::ui::parse_theme(canonical, base);
    ckv::fuzz::require(static_cast<bool>(reparsed));
    ckv::fuzz::require(reparsed.unknown_roles.empty());
    ckv::fuzz::require(ckv::ui::serialize_theme(*reparsed.theme) == canonical);
    ckv::fuzz::require(reparsed.theme->shadow() == parsed.theme->shadow());
    for (std::size_t role = 0; role < registry.size(); ++role) {
        const auto id = static_cast<ckv::ui::RoleId>(role);
        ckv::fuzz::require(reparsed.theme->resolve(id) == parsed.theme->resolve(id));
    }
    return 0;
}
