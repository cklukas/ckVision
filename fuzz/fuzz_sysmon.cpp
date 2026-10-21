// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../examples/sysmon/monitor.hpp"
#include "../examples/sysmon/settings.hpp"
#include "fuzz_common.hpp"
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const auto sample = ckv::sysmon::parse_sample(input);
    if (sample) ckv::fuzz::require(ckv::sysmon::parse_sample(ckv::sysmon::sample_text(*sample)).has_value());
    (void)ckv::sysmon::parse_ollama(input);
    (void)ckv::sysmon::parse_v3d_activity(input);
    (void)ckv::sysmon::parse_v3d_buffers(input);
    const auto gpu = ckv::sysmon::counter_gpu_id(input);
    if (gpu) ckv::fuzz::require(ckv::sysmon::counter_gpu_id(*gpu) == gpu);
    if (const auto settings = ckv::sysmon::parse_settings(input))
        ckv::fuzz::require(ckv::sysmon::parse_settings(ckv::sysmon::settings_text(*settings)) == settings);
    return 0;
}
