// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

namespace ckv::detail {

// D-024's POSIX implementation publishes this immutable assertion metadata
// into its async-signal-safe terminal-session ledger before assertion_failed()
// raises SIGABRT. It deliberately lives in the core-facing contract header:
// core never includes a term header, while term (the allowed higher layer)
// supplies the POSIX implementation.
#if defined(CKVISION_HAS_POSIX_TERMINAL)
void publish_assertion_failure(const char* expr, const char* file, int line) noexcept;
#endif

// Reports a failed contract check and terminates the process. The sole,
// deliberate exception to "core performs no I/O" (ARCHITECTURE.md §1) is
// the no-POSIX fallback below. On a PosixTerminal host, D-024 holds this
// immutable metadata until its fatal handler restored every live session,
// then emits it to stderr. A contract violation therefore remains loud
// without writing into an active alternate-screen session.
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
