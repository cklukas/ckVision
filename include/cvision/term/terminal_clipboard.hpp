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

// Forwards write_text() to Terminal::write_clipboard(), so a write is silently dropped while
// the terminal's capabilities deny clipboard writes.
class TerminalClipboardWriter final : public ClipboardWriter {
public:
    // Borrows `terminal`, which must outlive this writer.
    explicit TerminalClipboardWriter(Terminal& terminal) noexcept : terminal_(terminal) {}

    void write_text(std::string_view text) override { terminal_.write_clipboard(text); }

private:
    Terminal& terminal_;
};

}  // namespace ckv::term
