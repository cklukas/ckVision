// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#if defined(_WIN32)
#include <cstdint>

#include "cvision/core/clipboard.hpp"

namespace ckv::term {

// Native CF_UNICODETEXT export, independent of a console session (D-039).
// Owns an invisible message-only window only while publishing, not the console
// host's borrowed window. Construct, use and destroy on the same thread; no message pump,
// global class registration, clipboard reads or worker thread is required.
class CVISION_API WindowsClipboardWriter final : public ClipboardWriter {
public:
    // Binds the adapter to the constructing thread, without creating a window.
    WindowsClipboardWriter();
    ~WindowsClipboardWriter() override;
    WindowsClipboardWriter(const WindowsClipboardWriter&) = delete;
    WindowsClipboardWriter& operator=(const WindowsClipboardWriter&) = delete;

    // Explicit result, without a contention retry. Invalid UTF-8, embedded NUL,
    // allocation/owner-window failure or contention leave the clipboard alone.
    // Once EmptyClipboard succeeds, an OS publication failure can leave it
    // empty; Application's independent internal clipboard is unaffected.
    ClipboardWriteResult write_text(std::string_view text) override;

private:
    std::uint32_t thread_ = 0;
};

}  // namespace ckv::term
#endif
