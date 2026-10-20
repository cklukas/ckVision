// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/core/log_mailbox.hpp"

namespace ckv::logmonitor {
enum class NativeJsonSchema { Apple, Journal };
enum class NativeJsonResult { Record, Invalid, Limit };
// RFC 8259 framing/parser bounds. The original envelope stays in the record;
// selected scalars become bounded metadata. No locale, I/O or allocations.
class NativeJsonDecoder {
public:
    NativeJsonResult decode(std::string_view envelope, NativeJsonSchema schema,
                            LogMessage& output) noexcept;
    bool metadata_clipped() const noexcept { return metadata_clipped_; }
private:
    std::array<std::array<char, 384>, 8> values_{};
    std::array<std::size_t, 8> sizes_{};
    std::array<bool, 8> present_{};
    bool metadata_clipped_ = false;
};
// Explicit numeric offset is mandatory; unknown/invalid/out-of-range stays null.
std::optional<std::int64_t> native_timestamp(std::string_view text) noexcept;
}  // namespace ckv::logmonitor
