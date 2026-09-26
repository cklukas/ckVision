// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace ckv {

// Typed values for environmental failures on the small set of fallible
// entry points (ARCHITECTURE.md §11). Programming-contract violations
// are assertions, not Error — see AGENTS.md and DECISIONS.md D-011.
enum class ErrorCode {
    // The terminal session ended or became unusable while in use.
    TerminalLost,
    // The host cannot provide something the operation requires.
    UnsupportedHost,
    // A read or write to the terminal or another resource failed.
    IoFailure,
    // An input that originates outside the program was not acceptable; a bad argument from the
    // calling code itself is a contract violation and asserts instead.
    InvalidArgument,
};

// One environmental failure: a machine-readable category plus human-readable detail. A plain
// copyable value; it holds no handle to the resource that failed.
class Error {
public:
    // Pairs `code` with `message`, which should say what failed in terms a user or log reader can
    // act on; it is stored as given.
    Error(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

    // The category to branch on, and the free-form detail meant for display or logging only, not
    // for parsing.
    ErrorCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};

}  // namespace ckv
