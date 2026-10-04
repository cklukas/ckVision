// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <limits>
#include <type_traits>

#include "cvision/core/process_resources.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/testing/cktest.hpp"

CK_TEST(process_resource_identity_preserves_the_entire_native_dword_range) {
    static_assert(std::is_signed_v<ckv::core::ProcessId>);
    const ckv::core::ProcessId identity = std::numeric_limits<std::uint32_t>::max();
    CK_CHECK(identity > 0);
    CK_CHECK(identity == 4'294'967'295LL);
    CK_CHECK(static_cast<std::uint32_t>(identity) == std::numeric_limits<std::uint32_t>::max());
}

CK_TEST(process_resource_absence_is_not_a_zero_measurement) {
    ckv::term::TerminalEmulator emulator;
    const ckv::core::TerminalSubsession& session = emulator;
    CK_CHECK(session.process_id() == -1);
    const auto result = session.process_resources();
    CK_CHECK(result.state == ckv::core::ProcessResourceState::Unsupported);
    CK_CHECK(!result.cpu_time_nanos);
    CK_CHECK(!result.rss_bytes);
    CK_CHECK(!result.private_rss_bytes);
    CK_CHECK(result.live_processes == 0);
    CK_CHECK(result.system_error == 0);
    ckv::core::ProcessResources zero;
    zero.state = ckv::core::ProcessResourceState::Available;
    zero.rss_bytes = 0;
    CK_CHECK(zero.rss_bytes.has_value());
    CK_CHECK(zero.rss_bytes == 0);
}
