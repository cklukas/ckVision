// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

namespace ckv::examples {

inline constexpr std::string_view kCopyrightNotice =
    "Copyright (c) 2026 C. Klukas. All rights reserved.";

inline std::string about_text(std::string description) {
    if (!description.empty()) description += "\n\n";
    description += kCopyrightNotice;
    return description;
}

}  // namespace ckv::examples
