// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal-session implementation of core::ClipboardWriter.  This is a
// separate adapter so Application depends on the lower-layer clipboard
// contract rather than treating Terminal as a general platform-services bag.
#pragma once

#include "cvision/core/clipboard.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

class TerminalClipboardWriter final : public ClipboardWriter {
public:
    explicit TerminalClipboardWriter(Terminal& terminal) noexcept : terminal_(terminal) {}

    void write_text(std::string_view text) override { terminal_.write_clipboard(text); }

private:
    Terminal& terminal_;
};

}  // namespace ckv::term
