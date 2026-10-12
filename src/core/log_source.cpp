// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/log_source.hpp"

#include <limits>

#include "cvision/core/ascii.hpp"

namespace ckv {

bool LogLimits::valid() const noexcept {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    // A single worst-case sanitized record (three bytes for each bad byte),
    // raw bytes, and maximum metadata must fit even with no older records.
    if (record_bytes == 0 || record_bytes > (maximum - kLogMetadataBytes) / 4) return false;
    if (records == 0 || records > maximum / 1024) return false;
    if (retained_bytes < record_bytes * 4 + kLogMetadataBytes) return false;
    // Keep aggregate storage accounting representable too, including fixed
    // slot/object overhead and the separately allocated partial raw/text.
    auto storage = records * 1024;
    if (storage > maximum - 16384) return false;
    storage += 16384;
    if (retained_bytes > maximum - storage) return false;
    storage += retained_bytes;
    if (record_bytes * 4 > maximum - storage) return false;
    if (batch_bytes == 0 || batch_bytes > kLogBatchBytes || pending_batches == 0 ||
        pending_batches > maximum / batch_bytes) return false;
    return filter_records_per_step > 0 && frame_interval_nanos > 0 &&
        reconcile_interval_nanos > 0;
}

bool LogMetadata::valid() const noexcept {
    if (fields.size() > kLogMaxFields) return false;
    if (static_cast<unsigned>(severity) > static_cast<unsigned>(LogSeverity::Critical)) return false;
    std::size_t bytes = 0;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        if (field.name.empty() || field.name.size() > kLogMetadataBytes - bytes) return false;
        bytes += field.name.size();
        if (field.value.size() > kLogMetadataBytes - bytes) return false;
        bytes += field.value.size();
        for (std::size_t j = 0; j < i; ++j)
            if (fields[j].name == field.name) return false;
    }
    return true;
}

bool LogFileRequest::valid() const noexcept {
    if (revision == 0 || bytes == 0 || bytes > kLogBatchBytes) return false;
    if (path.size() > 32768 || path.find('\0') != std::string::npos) return false;
    if (follow != LogFollowMode::Path && follow != LogFollowMode::OpenFile) return false;
    if (operation == LogFileOperation::Inspect) {
        const bool drive = path.size() >= 3 && is_ascii_alpha(path[0]) && path[1] == ':' &&
            (path[2] == '/' || path[2] == '\\');
        return path.starts_with('/') || path.starts_with("\\\\") || drive;
    }
    return operation == LogFileOperation::Read && path.empty() &&
        offset <= std::numeric_limits<std::uint64_t>::max() - bytes;
}

}  // namespace ckv
