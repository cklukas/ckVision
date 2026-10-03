// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "windows_filesystem_test_support.hpp"
#include "cvision/term/windows_filesystem.hpp"

int wmain(int argc, wchar_t** argv) {
    if (argc != 6) return 3;
    ckv::testing::FileTestHandle start(::OpenEventW(SYNCHRONIZE, FALSE, argv[4]));
    if (!start.valid() || ::WaitForSingleObject(start.get(), 10000) != WAIT_OBJECT_0) return 3;
    ckv::FileWriteExpectation expectation;
    if (std::wstring_view(argv[2]) == L"create") expectation = ckv::FileWriteExpectation::must_not_exist();
    else expectation = ckv::FileWriteExpectation::matching({ckv::testing::filesystem_utf8(argv[3])});
    ckv::term::WindowsFileSystem fs;
    const auto result = fs.write_file_atomic(ckv::testing::filesystem_utf8(argv[1]),
                                            ckv::testing::filesystem_utf8(argv[5]), expectation);
    if (result.status == ckv::FileWriteStatus::Ok) return 0;
    if (result.status == ckv::FileWriteStatus::Conflict) return 2;
    return 1;
}
