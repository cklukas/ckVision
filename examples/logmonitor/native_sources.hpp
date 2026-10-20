// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "logmonitor_app.hpp"
#include "cvision/core/clock.hpp"
namespace ckv::logmonitor {
struct NativeLogSourceOptions {
    // Explicit source-side limit, distinct from recoverable local view filters.
    // Empty subscribes to all accessible processes. A name/PID is argument data.
    std::string apple_process;
    std::string journal_identifier;
    // Empty discovers Application/System plus enabled operational channels.
    // A supplied channel/bookmark is an explicit source-side resume request.
    std::string windows_channel, windows_bookmark;
};
// Construct before Application: discovery/probes and owned workers are native
// app-boundary operations. Catalog callbacks borrow this service; retire all
// feeds before destruction. No app/view pointer is captured by native workers.
class NativeLogSources {
public:
    explicit NativeLogSources(const Clock& clock, std::shared_ptr<const LogWake> wake, NativeLogSourceOptions options = {});
    ~NativeLogSources();
    std::vector<NativeSourceDescriptor> catalog();
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}  // namespace ckv::logmonitor
