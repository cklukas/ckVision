// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Platform clipboard-export contract (the architecture §1 and D-039).
// Application owns the portable, in-process clipboard text.  A host may
// inject this narrow output bridge to export a copy operation to its system
// clipboard; imports are deliberately terminal/native-input events, never a
// synchronous hidden platform read.
#pragma once

#include <string>
#include <string_view>

namespace ckv {

class ClipboardWriter {
public:
    virtual ~ClipboardWriter() = default;

    // Best effort: a bridge that cannot export text must make this a no-op.
    // The UI never branches on host capability and remains deterministic.
    virtual void write_text(std::string_view text) = 0;
};

// Deterministic in-memory bridge for tests, replay, and embedding hosts that
// want to observe copy output without touching an OS clipboard.
class MemoryClipboardWriter final : public ClipboardWriter {
public:
    void write_text(std::string_view text) override { text_ = std::string(text); }

    const std::string& text() const noexcept { return text_; }
    void clear() noexcept { text_.clear(); }

private:
    std::string text_;
};

}  // namespace ckv
