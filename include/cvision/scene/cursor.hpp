// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// CursorShape/CursorState live in core (core/cursor.hpp) so that
// term::Presenter can consume them without depending on scene
// (the architecture §1/§4). This header just brings the names into
// scene:: for existing scene-layer code and callers.
#pragma once

#include "cvision/core/cursor.hpp"

namespace ckv::scene {

using ckv::CursorShape;
using ckv::CursorState;

}  // namespace ckv::scene
