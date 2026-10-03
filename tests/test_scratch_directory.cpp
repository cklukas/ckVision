// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "scratch_directory.hpp"
#include "cvision/testing/cktest.hpp"

CK_TEST(scratch_directories_are_unique_children_of_the_selected_root_and_clean_up) {
    std::filesystem::path retired;
    {
        ckv::testing::ScratchDirectory first("first");
        ckv::testing::ScratchDirectory second("first");
        CK_CHECK(first.path().parent_path() == ckv::testing::scratch_root());
        CK_CHECK(second.path().parent_path() == ckv::testing::scratch_root());
        CK_CHECK(first.path() != second.path());
        CK_CHECK(std::filesystem::is_directory(first.path()));
        CK_CHECK(std::filesystem::is_directory(second.path()));
        retired = first.path();
    }
    CK_CHECK(!std::filesystem::exists(retired));
    CK_CHECK(std::filesystem::is_directory(ckv::testing::scratch_root()));
}

CK_TEST(scratch_directory_rejects_path_fragments_before_creating_or_removing_anything) {
    for (const std::string_view prefix : {"", "../parent", "a/b", "a\\b", "."}) {
        bool rejected = false;
        try { ckv::testing::ScratchDirectory unsafe(prefix); }
        catch (const std::invalid_argument&) { rejected = true; }
        CK_CHECK(rejected);
    }
    CK_CHECK(std::filesystem::is_directory(ckv::testing::scratch_root()));
}
