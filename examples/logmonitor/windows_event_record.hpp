// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/core/log_mailbox.hpp"
namespace ckv::logmonitor {
// Borrowed native observations, independent of winevt.h for deterministic replay.
struct WindowsEventRecord {
    std::string_view xml, channel, provider, bookmark, rendered_message;
    std::optional<std::uint32_t> event_id;
    std::optional<std::uint64_t> record_id, filetime;
    std::optional<std::uint8_t> level;
    std::uint32_t render_error = 0;
};
struct WindowsRecordResult { bool accepted = false, metadata_clipped = false; };
WindowsRecordResult encode_windows_event(const WindowsEventRecord& event, LogMessage& output) noexcept;
}  // namespace ckv::logmonitor
