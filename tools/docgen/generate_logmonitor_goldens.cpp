// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "logmonitor_test_fixture.hpp"
#include "native_log_source_test_fixture.hpp"
#include <fstream>
#include <filesystem>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    for (const auto name : ckv::testing::logmonitor_golden_names) {
        std::ofstream output(std::filesystem::path(argv[1]) / ("logmonitor_" + std::string(name) + ".dump"), std::ios::binary);
        output << ckv::testing::logmonitor_golden(name); if (!output) return 1;
    }
    for (const auto name : ckv::testing::native_log_golden_names) {
        std::ofstream output(std::filesystem::path(argv[1]) / ("logmonitor_native_" + std::string(name) + ".dump"), std::ios::binary);
        output << ckv::testing::native_log_golden(name); if (!output) return 1;
    }
    return 0;
}
