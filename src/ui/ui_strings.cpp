// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/ui_strings.hpp"

namespace ckv::ui {

const UiStrings& english_ui_strings() noexcept {
    static const UiStrings strings;
    return strings;
}

}  // namespace ckv::ui
