// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/version.hpp"

// The numbers come from `project(ckvision VERSION ...)` by way of
// target_compile_definitions, so this file cannot drift from the version the
// installed package advertises. A build that does not define them is a build
// whose version this file would have to invent: it fails here instead, loudly,
// rather than shipping a library that misreports itself.
#if !defined(CKVISION_VERSION_MAJOR) || !defined(CKVISION_VERSION_MINOR) || \
    !defined(CKVISION_VERSION_PATCH) || !defined(CKVISION_VERSION_STRING)
#error "ckVision version macros are missing: build src/core/version.cpp through the cvision target, which defines them from PROJECT_VERSION."
#endif

namespace ckv {

Version version() noexcept {
    return Version{CKVISION_VERSION_MAJOR, CKVISION_VERSION_MINOR, CKVISION_VERSION_PATCH};
}

std::string_view version_string() noexcept { return CKVISION_VERSION_STRING; }

}  // namespace ckv
