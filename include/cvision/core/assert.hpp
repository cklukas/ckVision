// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

namespace ckv::detail {

// D-024's POSIX implementation publishes this immutable assertion metadata
// into its async-signal-safe terminal-session ledger before assertion_failed()
// raises SIGABRT. It deliberately lives in the core-facing contract header:
// core never includes a term header, while term (the allowed higher layer)
// supplies the POSIX implementation.
//
// Returns whether the metadata will actually be READ: true when a live session
// is holding the fatal handler that prints it, false when the ledger has no
// audience — no session, no restore, nobody to say what broke. False obliges
// the caller to print it instead.
#if defined(CKVISION_HAS_POSIX_TERMINAL)
bool publish_assertion_failure(const char* expr, const char* file, int line) noexcept;
#endif

// Reports a failed contract check and terminates the process. The sole,
// deliberate exception to "core performs no I/O" (the architecture §1) is the
// stderr fallback below. On a PosixTerminal host with a live session, D-024
// holds this immutable metadata until its fatal handler restored every screen
// and then emits it: a contract violation stays loud without writing into an
// active alternate screen. With NO live session there is no alternate screen
// to protect and no handler to do the emitting, so the fallback speaks — which
// is the case every headless test runs under, and the reason one used to abort
// with no message at all.
[[noreturn]] void assertion_failed(const char* expr, const char* file, int line) noexcept;

}  // namespace ckv::detail

// Always-on contract check for programming-contract violations
// (the decision log D-011). Unlike <cassert>'s assert(), CKV_ASSERT is never
// compiled out by NDEBUG: this project's own Release configuration (and
// its CI) build with NDEBUG defined, and a contract violation must still
// fail fast pre-1.0 in every build type, not only in debug builds.
#define CKV_ASSERT(cond)                                                        \
    (void)((cond) ? true                                                        \
                  : (::ckv::detail::assertion_failed(#cond, __FILE__, __LINE__), false))
