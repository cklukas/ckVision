// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/standard_strings.hpp"

namespace ckv::widgets {

const StandardStrings& english_standard_strings() noexcept {
    static const StandardStrings strings;
    return strings;
}

}  // namespace ckv::widgets
