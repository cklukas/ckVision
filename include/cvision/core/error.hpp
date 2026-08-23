// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace ckv {

// Typed values for environmental failures on the small set of fallible
// entry points (the architecture §11). Programming-contract violations
// are assertions, not Error — see the engineering standard and the decision log D-011.
enum class ErrorCode {
    TerminalLost,
    UnsupportedHost,
    IoFailure,
    InvalidArgument,
};

class Error {
public:
    Error(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

    ErrorCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};

}  // namespace ckv
