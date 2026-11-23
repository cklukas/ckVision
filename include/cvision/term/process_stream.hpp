// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/core/process_stream.hpp"

namespace ckv::term {
// Prestarted private-pipe workers; 1–8 concurrent roots. Construct/shutdown
// outside UI dispatch. Retire sessions before shutdown. The borrowed clock is
// thread-safe and encloses workers; wake targets enclose any queue use.
class CVISION_TYPE_API NativeProcessStreamService final : public core::ProcessStreamService {
public:
    // Borrow a thread-safe enclosing clock and prestart 1..8 private pipe workers.
    CVISION_MEMBER_API NativeProcessStreamService(const Clock& clock, std::size_t slots = 2,
                               std::shared_ptr<const LogWake> work_wake = {});
    CVISION_MEMBER_API ~NativeProcessStreamService() override;
    CVISION_MEMBER_API NativeProcessStreamService(const NativeProcessStreamService&) = delete;
    CVISION_MEMBER_API NativeProcessStreamService& operator=(const NativeProcessStreamService&) = delete;
    CVISION_MEMBER_API core::ProcessStreamOpenResult start(core::ProcessStreamRequest request) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace ckv::term
