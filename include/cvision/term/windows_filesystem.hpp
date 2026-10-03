// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/filesystem.hpp"

namespace ckv::term {

// Native Windows FileSystem (D-039/D-123). Names are UTF-8, contents are opaque
// bytes. Queries/read follow links; writes refuse a final reparse point.
// Stateless instances serialize conditional writes through a persistent
// ".ckvision-write.lock" per directory (reserved basename). Device paths,
// wildcards, alternate streams, invalid UTF-8 and embedded NUL are rejected.
// Native rename publication preserves an existing DACL, but not other file
// metadata/streams; creation inherits the directory DACL. Requires no privilege
// elevation. Conditional serialization covers cooperating adapters, not
// arbitrary external replacements or mapped-memory writers.
// Revisions combine native identity/metadata with a streamed SHA-256 content
// digest, so equal timestamp ticks cannot hide a same-size byte change.
class WindowsFileSystem final : public FileSystem {
public:
    // Directories first, then files, each sorted by UTF-8 byte order. Dot entries
    // and names that cannot be represented as valid UTF-8 are omitted.
    std::vector<FileEntry> list_directory(std::string_view path) const override;
    bool exists(std::string_view path) const noexcept override;
    bool is_directory(std::string_view path) const noexcept override;
    bool create_directories(std::string_view path) override;

    // Absolute drive/UNC form with '/' separators and preserved roots. Relative
    // names resolve against the process cwd, dot components are resolved; an
    // invalid name yields empty. No case-folding or link resolution is done.
    std::string normalize_path(std::string_view path) const override;
    // Drive-rooted and UNC paths are absolute; a root-relative slash or bare
    // drive letter is not (each depends on a process drive/cwd).
    bool is_absolute_path(std::string_view path) const noexcept override;
    // Joins a child fragment; an absolute second argument is not accepted.
    std::string join(std::string_view directory, std::string_view name) const override;
    // Drive and UNC share roots are their own parents; invalid names yield empty.
    std::string parent(std::string_view path) const override;

    std::optional<FileReadResult> read_file(std::string_view path) const override;
    FileWriteResult write_file_atomic(std::string_view path, std::string_view contents,
                                      FileWriteExpectation expectation = {}) override;
    std::optional<FileFingerprint> fingerprint(std::string_view path) const override;
};

}  // namespace ckv::term
