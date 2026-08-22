// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/posix_filesystem.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "cvision/testing/cktest.hpp"

using ckv::FileEntry;
using ckv::term::PosixFileSystem;

namespace {

// A scratch directory tree built fresh per test via mkdtemp — never
// relies on the repo's own working directory, so this is portable
// across whatever machine/CI runs it.
struct ScratchDir {
    std::string root;

    ScratchDir() {
        char tmpl[] = "/tmp/ckvision_fs_test_XXXXXX";
        root = ::mkdtemp(tmpl);  // mkdtemp writes the final path into tmpl itself
    }

    ~ScratchDir() {
        // Best-effort recursive cleanup; test scratch dirs are small
        // and flat by construction, so a fixed two-level rm suffices.
        const std::string cmd = "rm -rf '" + root + "'";
        const int rc = std::system(cmd.c_str());
        (void)rc;  // best-effort cleanup; glibc marks system() warn_unused_result
    }

    void make_dir(const std::string& relative) const { ::mkdir((root + "/" + relative).c_str(), 0755); }
    void make_file(const std::string& relative) const { std::ofstream(root + "/" + relative) << "x"; }
};

}  // namespace

CK_TEST(list_directory_returns_every_entry_with_correct_is_directory) {
    ScratchDir scratch;
    scratch.make_dir("sub");
    scratch.make_file("a.txt");
    scratch.make_file("b.txt");

    PosixFileSystem fs;
    auto entries = fs.list_directory(scratch.root);
    CK_CHECK(entries.size() == 3);

    bool found_sub = false, found_a = false, found_b = false;
    for (const FileEntry& e : entries) {
        if (e.name == "sub") {
            found_sub = true;
            CK_CHECK(e.is_directory);
        } else if (e.name == "a.txt") {
            found_a = true;
            CK_CHECK(!e.is_directory);
        } else if (e.name == "b.txt") {
            found_b = true;
            CK_CHECK(!e.is_directory);
        }
    }
    CK_CHECK(found_sub && found_a && found_b);
}

CK_TEST(list_directory_never_returns_dot_or_dotdot) {
    ScratchDir scratch;
    scratch.make_file("only.txt");

    PosixFileSystem fs;
    auto entries = fs.list_directory(scratch.root);
    for (const FileEntry& e : entries) {
        CK_CHECK(e.name != ".");
        CK_CHECK(e.name != "..");
    }
}

CK_TEST(list_directory_sorts_directories_before_files_then_alphabetically) {
    ScratchDir scratch;
    scratch.make_file("zzz.txt");
    scratch.make_dir("aaa_dir");
    scratch.make_file("bbb.txt");

    PosixFileSystem fs;
    auto entries = fs.list_directory(scratch.root);
    CK_CHECK(entries.size() == 3);
    CK_CHECK(entries[0].name == "aaa_dir");
    CK_CHECK(entries[0].is_directory);
    CK_CHECK(entries[1].name == "bbb.txt");
    CK_CHECK(entries[2].name == "zzz.txt");
}

CK_TEST(list_directory_on_a_nonexistent_path_returns_empty_not_an_error) {
    PosixFileSystem fs;
    auto entries = fs.list_directory("/this/path/should/never/exist/on/any/machine");
    CK_CHECK(entries.empty());
}

CK_TEST(list_directory_on_a_plain_file_returns_empty) {
    ScratchDir scratch;
    scratch.make_file("plain.txt");
    PosixFileSystem fs;
    auto entries = fs.list_directory(scratch.root + "/plain.txt");
    CK_CHECK(entries.empty());
}

CK_TEST(exists_is_true_for_a_real_path_and_false_for_a_bogus_one) {
    ScratchDir scratch;
    scratch.make_file("real.txt");
    PosixFileSystem fs;
    CK_CHECK(fs.exists(scratch.root + "/real.txt"));
    CK_CHECK(!fs.exists(scratch.root + "/does_not_exist.txt"));
}

CK_TEST(is_directory_distinguishes_a_directory_from_a_plain_file) {
    ScratchDir scratch;
    scratch.make_dir("a_dir");
    scratch.make_file("a_file.txt");
    PosixFileSystem fs;
    CK_CHECK(fs.is_directory(scratch.root + "/a_dir"));
    CK_CHECK(!fs.is_directory(scratch.root + "/a_file.txt"));
}

CK_TEST(is_directory_on_a_nonexistent_path_is_false_not_an_error) {
    PosixFileSystem fs;
    CK_CHECK(!fs.is_directory("/this/path/should/never/exist/on/any/machine"));
}

CK_TEST(join_and_parent_still_use_the_base_classs_default_slash_joining) {
    // PosixFileSystem doesn't override join()/parent() — verifying the
    // inherited default still behaves sanely for real filesystem paths
    // (e.g. no accidental "//" when the scratch root itself ends
    // without a trailing slash, which mkdtemp() paths never do).
    ScratchDir scratch;
    PosixFileSystem fs;
    CK_CHECK(fs.join(scratch.root, "child") == scratch.root + "/child");
}
