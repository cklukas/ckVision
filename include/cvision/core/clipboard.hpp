// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Platform clipboard-export contract (the architecture §1 and D-039).
// Application owns the portable, in-process clipboard text.  A host may
// inject this narrow output bridge to export a copy operation to its system
// clipboard; imports are deliberately terminal/native-input events, never a
// synchronous hidden platform read.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ckv {

// Ok is confirmed native/memory acceptance. Submitted is an unacknowledged
// terminal request, not proof that a remote host changed its clipboard.
enum class ClipboardWriteStatus { Ok, Submitted, Unsupported, InvalidText, Unavailable, Error };

// The outcome of one export attempt, independent of the Application's safe
// internal copy. A refused export may carry backend-native diagnostic data.
struct ClipboardWriteResult {
    // Whether the backend accepted, submitted, or refused the export.
    ClipboardWriteStatus status = ClipboardWriteStatus::Unsupported;
    // Backend-native error number; zero when no diagnostic number is available.
    std::uint32_t native_error = 0;
    // On failure the host may already have been changed (for example, emptied
    // before a native publication failure). Internal clipboard data is safe.
    bool external_state_may_have_changed = false;

    // True for accepted native/memory writes and submitted terminal requests;
    // Submitted still does not confirm a remote clipboard update.
    bool accepted() const noexcept {
        return status == ClipboardWriteStatus::Ok || status == ClipboardWriteStatus::Submitted;
    }
    // Compares the outcome and all native/partial-effect diagnostic information.
    friend bool operator==(const ClipboardWriteResult&, const ClipboardWriteResult&) = default;
};

// The export side of the clipboard: where an Application sends text the user copied so the host
// system clipboard can receive it too. An Application either owns one (by default one that
// forwards to its Terminal) or borrows a caller-supplied one that must outlive it.
class ClipboardWriter {
public:
    // Destroys the bridge; implementations may be deleted through a ClipboardWriter pointer.
    virtual ~ClipboardWriter() = default;

    // Reports environmental refusal through the result, never by throwing.
    // UI behavior depends on this injected value, not a hidden host lookup.
    virtual ClipboardWriteResult write_text(std::string_view text) = 0;
};

// Deterministic in-memory bridge for tests, replay, and embedding hosts that
// want to observe copy output without touching an OS clipboard.
class MemoryClipboardWriter final : public ClipboardWriter {
public:
    ClipboardWriteResult write_text(std::string_view text) override {
        text_ = std::string(text);
        return {ClipboardWriteStatus::Ok};
    }

    // The text of the most recent write_text call, replacing any earlier one; empty before the
    // first write and after clear(). The reference is to the writer's own storage, so it lives as
    // long as the writer and shows later writes.
    const std::string& text() const noexcept { return text_; }
    void clear() noexcept { text_.clear(); }

private:
    std::string text_;
};

}  // namespace ckv
