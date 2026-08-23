// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Opt-in tracing of the graphics path, for the questions that cannot be
// answered by reading the code: did this child ask about graphics and what was
// it told, did its picture decode, how long did the encode take, and what
// erased a picture that should still be there.
//
// Off unless `CKVISION_GRAPHICS_LOG` names a destination — `-` or `stderr` for
// standard error, anything else for a file. The first process to open that
// file starts it empty, so a run is read without hunting for where it began;
// a contained child session inherits the variable and appends, which is what
// makes a host and its child readable together. Off, a call costs one relaxed
// atomic read; the message is not even built, because every call site asks
// `graphics_log_enabled()` first.
//
// It lives in `term` because this is where the platform already is: `core`,
// `scene`, `ui` and `widgets` read no environment (the architecture §1).
#pragma once

#include <string>
#include <string_view>

namespace ckv::term {

bool graphics_log_enabled() noexcept;

// Writes one line, prefixed with the process id and a monotonic millisecond
// stamp so two ckVision processes — a host and the child inside it — can be
// read together in one file.
void graphics_log(std::string_view message);

}  // namespace ckv::term
