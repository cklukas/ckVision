// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/filesystem.hpp"

#include <algorithm>

#include "cvision/testing/cktest.hpp"

using ckv::FileEntry;
using ckv::MemoryFileSystem;

// --- join / parent -------------------------------------------------------

CK_TEST(join_combines_a_directory_and_a_name_with_exactly_one_slash) {
    MemoryFileSystem fs;
    CK_CHECK(fs.join("/a/b", "c") == "/a/b/c");
}

CK_TEST(join_does_not_produce_a_double_slash_when_the_directory_has_a_trailing_one) {
    MemoryFileSystem fs;
    CK_CHECK(fs.join("/a/b/", "c") == "/a/b/c");
}

CK_TEST(join_at_the_root_does_not_produce_a_double_slash) {
    MemoryFileSystem fs;
    CK_CHECK(fs.join("/", "c") == "/c");
}

CK_TEST(join_strips_a_leading_slash_from_the_name) {
    MemoryFileSystem fs;
    CK_CHECK(fs.join("/a", "/b") == "/a/b");
}

CK_TEST(parent_of_the_root_is_the_root) {
    MemoryFileSystem fs;
    CK_CHECK(fs.parent("/") == "/");
}

CK_TEST(parent_of_a_top_level_entry_is_the_root) {
    MemoryFileSystem fs;
    CK_CHECK(fs.parent("/a") == "/");
}

CK_TEST(parent_strips_the_last_path_segment) {
    MemoryFileSystem fs;
    CK_CHECK(fs.parent("/a/b/c") == "/a/b");
}

CK_TEST(parent_ignores_a_trailing_slash) {
    MemoryFileSystem fs;
    CK_CHECK(fs.parent("/a/b/") == "/a");
}

CK_TEST(normalize_path_accepts_backslashes_and_collapses_repeated_separators) {
    MemoryFileSystem fs;
    CK_CHECK(fs.normalize_path("\\a\\\\b\\") == "/a/b");
}

CK_TEST(join_normalizes_backslashes_in_a_relative_name_fragment) {
    MemoryFileSystem fs;
    CK_CHECK(fs.join("/base", "other\\file.txt") == "/base/other/file.txt");
}

CK_TEST(is_absolute_path_accepts_posix_root_and_drive_root_spellings) {
    MemoryFileSystem fs;
    CK_CHECK(fs.is_absolute_path("/a"));
    CK_CHECK(fs.is_absolute_path("\\a"));
    CK_CHECK(fs.is_absolute_path("C:\\a"));
    CK_CHECK(!fs.is_absolute_path("relative"));
}

// --- MemoryFileSystem: add / exists / is_directory -----------------------

CK_TEST(the_root_always_exists_as_a_directory) {
    MemoryFileSystem fs;
    CK_CHECK(fs.exists("/"));
    CK_CHECK(fs.is_directory("/"));
}

CK_TEST(add_directory_creates_every_missing_ancestor) {
    MemoryFileSystem fs;
    fs.add_directory("/a/b/c");
    CK_CHECK(fs.exists("/a"));
    CK_CHECK(fs.is_directory("/a"));
    CK_CHECK(fs.exists("/a/b"));
    CK_CHECK(fs.is_directory("/a/b"));
    CK_CHECK(fs.exists("/a/b/c"));
    CK_CHECK(fs.is_directory("/a/b/c"));
}

CK_TEST(add_file_creates_its_parent_directories_but_the_file_itself_is_not_a_directory) {
    MemoryFileSystem fs;
    fs.add_file("/a/b/readme.txt");
    CK_CHECK(fs.is_directory("/a"));
    CK_CHECK(fs.is_directory("/a/b"));
    CK_CHECK(fs.exists("/a/b/readme.txt"));
    CK_CHECK(!fs.is_directory("/a/b/readme.txt"));
}

CK_TEST(a_never_added_path_does_not_exist) {
    MemoryFileSystem fs;
    fs.add_directory("/a");
    CK_CHECK(!fs.exists("/nope"));
    CK_CHECK(!fs.is_directory("/nope"));
}

CK_TEST(is_directory_on_an_existing_file_returns_false_not_a_crash) {
    MemoryFileSystem fs;
    fs.add_file("/a.txt");
    CK_CHECK(!fs.is_directory("/a.txt"));
}

// --- list_directory --------------------------------------------------

CK_TEST(list_directory_returns_only_direct_children_not_grandchildren) {
    MemoryFileSystem fs;
    fs.add_directory("/a/b");
    fs.add_file("/a/x.txt");
    const auto entries = fs.list_directory("/a");
    CK_CHECK(entries.size() == 2);
    bool has_b = false, has_x = false;
    for (const auto& e : entries) {
        if (e.name == "b") has_b = e.is_directory;
        if (e.name == "x.txt") has_x = !e.is_directory;
    }
    CK_CHECK(has_b);
    CK_CHECK(has_x);
}

CK_TEST(list_directory_on_a_file_returns_empty_not_an_error) {
    MemoryFileSystem fs;
    fs.add_file("/a.txt");
    CK_CHECK(fs.list_directory("/a.txt").empty());
}

CK_TEST(list_directory_on_a_nonexistent_path_returns_empty_not_an_error) {
    MemoryFileSystem fs;
    CK_CHECK(fs.list_directory("/does/not/exist").empty());
}

CK_TEST(list_directory_on_an_empty_directory_returns_empty) {
    MemoryFileSystem fs;
    fs.add_directory("/empty");
    CK_CHECK(fs.list_directory("/empty").empty());
}

CK_TEST(list_directory_on_the_root_finds_top_level_entries) {
    MemoryFileSystem fs;
    fs.add_directory("/dir1");
    fs.add_file("/file1.txt");
    const auto entries = fs.list_directory("/");
    CK_CHECK(entries.size() == 2);
}
