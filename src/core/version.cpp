// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/version.hpp"

namespace ckv {

Version version() noexcept { return Version{0, 1, 0}; }

std::string_view version_string() noexcept { return "0.1.0"; }

}  // namespace ckv
