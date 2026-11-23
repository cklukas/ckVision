// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstddef>
#include <memory>

#include "cvision/core/log_source.hpp"

namespace ckv::term {

class Terminal;

// Wake one explicitly supplied terminal, whose lifetime must enclose that of
// the source service. The terminal owns the wake transport and is thread-safe;
// this target does not retain an Application or shorter-lived view.
class CVISION_TYPE_API TerminalLogWake final : public LogWake {
public:
    // Borrow a terminal whose wake transport encloses all source notifications.
    explicit TerminalLogWake(Terminal& terminal) noexcept : terminal_(terminal) {}
    CVISION_MEMBER_API void notify() const noexcept override;
private:
    Terminal& terminal_;
};

// Explicit native notification preference or injected-time polling reconciliation.
enum class LogWatchPreference { Native, Polling };

// Explicit application-owned native execution. One prestarted worker per slot
// (1..8), each with fixed request/result capacity. Request/take/cancel perform
// no file I/O, allocations or worker waits. Source selection may allocate the
// small session object; construction/shutdown belong outside UI dispatch.
class CVISION_TYPE_API NativeLogFileService final : public LogFileService {
public:
    // Prestart bounded slots/read storage outside dispatch; wake is optional/shared.
    CVISION_MEMBER_API NativeLogFileService(std::size_t slots = 2, std::size_t read_bytes = kLogBatchBytes,
                         LogWatchPreference watch = LogWatchPreference::Native,
                         std::shared_ptr<const LogWake> wake = {});
    // All sessions must already be retired. Joins workers outside dispatch;
    // local regular-file I/O is supported, remote filesystem stalls may delay
    // shutdown. Session retirement itself never waits for that I/O.
    CVISION_MEMBER_API ~NativeLogFileService() override;
    CVISION_MEMBER_API NativeLogFileService(const NativeLogFileService&) = delete;
    CVISION_MEMBER_API NativeLogFileService& operator=(const NativeLogFileService&) = delete;
    CVISION_MEMBER_API std::unique_ptr<LogFileSession> open_session() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ckv::term
