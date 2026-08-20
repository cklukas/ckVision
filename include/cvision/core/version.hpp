// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace ckv {

struct Version {
    int major;
    int minor;
    int patch;
};

Version version() noexcept;
std::string_view version_string() noexcept;

}  // namespace ckv
