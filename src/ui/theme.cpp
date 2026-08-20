// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/theme.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::ui {

RoleId RoleRegistry::intern(std::string_view name, Style fallback) {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) {
            CKV_ASSERT(fallbacks_[i] == fallback);
            return static_cast<RoleId>(i);
        }
    }
    names_.emplace_back(name);
    fallbacks_.push_back(fallback);
    return static_cast<RoleId>(names_.size() - 1);
}

RoleId RoleRegistry::find(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < names_.size(); ++i)
        if (names_[i] == name) return static_cast<RoleId>(i);
    return kInvalidRole;
}

Style RoleRegistry::fallback(RoleId id) const noexcept {
    CKV_ASSERT(id >= 0 && static_cast<std::size_t>(id) < fallbacks_.size());
    return fallbacks_[static_cast<std::size_t>(id)];
}

const std::string& RoleRegistry::name(RoleId id) const noexcept {
    CKV_ASSERT(id >= 0 && static_cast<std::size_t>(id) < names_.size());
    return names_[static_cast<std::size_t>(id)];
}

void Theme::set(RoleId role, Style style) {
    CKV_ASSERT(role >= 0);
    if (static_cast<std::size_t>(role) >= overrides_.size())
        overrides_.resize(static_cast<std::size_t>(role) + 1);
    overrides_[static_cast<std::size_t>(role)] = style;
}

Style Theme::resolve(RoleId role) const noexcept {
    if (role >= 0 && static_cast<std::size_t>(role) < overrides_.size() &&
        overrides_[static_cast<std::size_t>(role)])
        return *overrides_[static_cast<std::size_t>(role)];
    return registry_->fallback(role);
}

}  // namespace ckv::ui
