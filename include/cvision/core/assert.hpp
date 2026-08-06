// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

namespace ckv::detail {

// Reports a failed contract check and terminates the process. The sole,
// deliberate exception to "core performs no I/O" (ARCHITECTURE.md §1):
// a contract violation is a program-termination event, not part of
// core's normal control flow — DECISIONS.md D-011 requires it to "fail
// fast, loudly, in all build types until 1.0", and a diagnostic write
// immediately before abort() is what "loudly" means.
[[noreturn]] void assertion_failed(const char* expr, const char* file, int line) noexcept;

}  // namespace ckv::detail

// Always-on contract check for programming-contract violations
// (DECISIONS.md D-011). Unlike <cassert>'s assert(), CKV_ASSERT is never
// compiled out by NDEBUG: this project's own Release configuration (and
// its CI) build with NDEBUG defined, and a contract violation must still
// fail fast pre-1.0 in every build type, not only in debug builds.
#define CKV_ASSERT(cond)                                                        \
    (void)((cond) ? true                                                        \
                  : (::ckv::detail::assertion_failed(#cond, __FILE__, __LINE__), false))
