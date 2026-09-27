// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Prints the Classic scheme's palette, every standard role with the Style it
// resolves to and, last, the theme's shadow (D-106), as the Markdown table
// that the internal plans embeds between
// its palette markers. The `classic_look` gate (tests/check_classic_look.py)
// runs this program and fails when the embedded table differs from its output,
// so the specification cannot drift from the theme. After a deliberate palette
// change, run it by hand and paste the new table into the specification.
//
// The registry is fresh, so interning the standard roles IS the full role set,
// in declaration order. Rows are grouped by family, the name segment after
// "ckv.", in the order each family first appears; colours and attributes use
// the spelling of the golden dump format (docs/golden-format.md), so a row can
// be read against a dump's style line directly.
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/standard_roles.hpp"

namespace {

std::string color_text(ckv::Color color) {
    if (color.is_default()) return "default";
    std::array<char, 8> buffer{};
    if (color.is_indexed()) {
        std::snprintf(buffer.data(), buffer.size(), "@%u", static_cast<unsigned>(color.index()));
    } else {
        std::snprintf(buffer.data(), buffer.size(), "#%02X%02X%02X", static_cast<unsigned>(color.r()),
                      static_cast<unsigned>(color.g()), static_cast<unsigned>(color.b()));
    }
    return buffer.data();
}

std::string attrs_text(const ckv::Style& style) {
    constexpr std::array<std::pair<ckv::Attr, std::string_view>, 6> kNames{{
        {ckv::Attr::Bold, "bold"},
        {ckv::Attr::Dim, "dim"},
        {ckv::Attr::Italic, "italic"},
        {ckv::Attr::Underline, "underline"},
        {ckv::Attr::Reverse, "reverse"},
        {ckv::Attr::Strike, "strike"},
    }};
    std::string text;
    for (const auto& [flag, name] : kNames) {
        if (!ckv::has_attr(style.attrs, flag)) continue;
        if (!text.empty()) text += ',';
        text += name;
    }
    return text.empty() ? "-" : text;
}

std::string_view family_of(std::string_view name) {
    constexpr std::string_view kPrefix = "ckv.";
    if (name.substr(0, kPrefix.size()) == kPrefix) name.remove_prefix(kPrefix.size());
    return name.substr(0, name.find('.'));
}

}  // namespace

int main() {
    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    const ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    std::vector<std::string_view> families;
    for (std::size_t id = 0; id < registry.size(); ++id) {
        const std::string_view family = family_of(registry.name(static_cast<ckv::ui::RoleId>(id)));
        bool known = false;
        for (const std::string_view seen : families) known = known || seen == family;
        if (!known) families.push_back(family);
    }

    std::printf("| Family | Role | Foreground | Background | Attributes |\n");
    std::printf("|---|---|---|---|---|\n");
    for (const std::string_view family : families) {
        for (std::size_t id = 0; id < registry.size(); ++id) {
            const auto role = static_cast<ckv::ui::RoleId>(id);
            const std::string& name = registry.name(role);
            if (family_of(name) != family) continue;
            const ckv::Style style = theme.resolve(role);
            std::printf("| %.*s | `%s` | `%s` | `%s` | `%s` |\n", static_cast<int>(family.size()), family.data(),
                        name.c_str(), color_text(style.fg).c_str(), color_text(style.bg).c_str(),
                        attrs_text(style).c_str());
        }
    }
    // The theme's shadow is not a role but belongs to the scheme's look all
    // the same (D-106): what every cell a shadow covers is drawn in. It keeps
    // the covered cell's own attributes.
    const ckv::ShadowStyle shadow = theme.shadow();
    if (shadow.kind() == ckv::ShadowStyle::Kind::Recolor)
        std::printf("| shadow | `Theme::shadow()` | `%s` | `%s` | the covered cell's |\n",
                    color_text(shadow.foreground()).c_str(), color_text(shadow.background()).c_str());
    else
        std::printf("| shadow | `Theme::shadow()` | halved | halved | the covered cell's |\n");
    return 0;
}
