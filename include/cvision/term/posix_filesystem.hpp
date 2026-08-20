// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The one concrete real-filesystem FileSystem (D-039), mirroring
// PosixTerminal/PosixClock's pattern: core/ui/widgets never touch the
// real filesystem (D-039's injected-impurity rule; core::FileSystem is
// the abstract contract), so an application needs SOMETHING at the
// term-layer boundary to inject when it actually wants to browse the
// real disk (a File dialog, a directory tree, ...) rather than a
// scripted MemoryFileSystem. Nothing implemented this anywhere in the
// library before — MemoryFileSystem existed for tests, but no
// interactive application could construct a working file browser
// without writing this itself from scratch.
#pragma once

#include "cvision/core/filesystem.hpp"

namespace ckv::term {

class PosixFileSystem final : public FileSystem {
public:
    std::vector<FileEntry> list_directory(std::string_view path) const override;
    bool exists(std::string_view path) const noexcept override;
    bool is_directory(std::string_view path) const noexcept override;
    std::optional<FileReadResult> read_file(std::string_view path) const override;
    FileWriteResult write_file_atomic(std::string_view path, std::string_view contents,
                                      FileWriteExpectation expectation = {}) override;
    std::optional<FileFingerprint> fingerprint(std::string_view path) const override;
};

}  // namespace ckv::term
