// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Shared by every widget whose behavior splits along a horizontal-vs-
// vertical axis (Scrollbar, Splitter) — one enum, not a redeclaration
// per widget.
#pragma once

namespace ckv::widgets {

enum class Orientation { Horizontal, Vertical };

}  // namespace ckv::widgets
