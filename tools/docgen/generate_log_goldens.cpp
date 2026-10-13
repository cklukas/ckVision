// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/log_view_test_fixture.hpp"
#include "../../tests/log_query_view_test_fixture.hpp"
#include "../../tests/log_delivery_view_test_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: generate_log_goldens <output-directory>\n"; return 2; }
    for (const auto name : ckv::testing::log_golden_names) {
        std::ofstream output(std::filesystem::path(argv[1]) / ("log_" + std::string(name) + ".dump"), std::ios::binary);
        output << ckv::testing::log_view_golden(name);
        if (!output) return 1;
    }
    for (const auto name : ckv::testing::log_query_golden_names) {
        std::ofstream output(std::filesystem::path(argv[1]) / ("log_" + std::string(name) + ".dump"), std::ios::binary);
        output << ckv::testing::log_query_view_golden(name);
        if (!output) return 1;
    }
    for (const auto name : ckv::testing::log_delivery_golden_names) {
        std::ofstream output(std::filesystem::path(argv[1]) / ("log_" + std::string(name) + ".dump"), std::ios::binary);
        output << ckv::testing::log_delivery_view_golden(name);
        if (!output) return 1;
    }
    return 0;
}
