// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace ckv {

// The library's release number as three integers, in semantic-versioning order. Compare the
// fields, not version_string(), when a caller needs to gate on a minimum version.
struct Version {
    // The major, minor and patch components of the CMake project version, each non-negative.
    int major;
    int minor;
    int patch;
};

// The version of the ckVision library this program is linked against, taken at build time from
// the CMake project version, so it always matches what the installed package advertises to
// find_package. Reports the linked library, not the headers the caller was compiled with.
Version version() noexcept;
// The same version as "major.minor.patch" (for example "0.1.7"). The view refers to a string
// literal with static storage duration and never dangles.
std::string_view version_string() noexcept;

}  // namespace ckv
