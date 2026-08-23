// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The library's only filesystem access (the architecture §5 "Platform
// services", D-039): file dialogs enumerate through this interface,
// never touch the real filesystem directly, and so golden-test
// headlessly against a scripted in-memory tree. Mirrors Clock's own
// injected-impurity pattern (core/clock.hpp) — production code
// supplies a real implementation at the application boundary; tests
// use MemoryFileSystem.
//
// Paths are normalized by the injected filesystem contract before dialog logic
// reasons about them. The default implementation treats '/' and '\' as
// separators, collapses repeated separators, strips redundant trailing
// separators, and recognizes POSIX-rooted and drive-rooted absolute paths.
// Platform adapters may override when their host semantics differ.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ckv {

struct FileEntry {
    std::string name;  // the entry's own name, not a full path
    bool is_directory = false;

    friend bool operator==(const FileEntry&, const FileEntry&) = default;
};

struct FileFingerprint {
    std::string value;

    friend bool operator==(const FileFingerprint&, const FileFingerprint&) = default;
};

struct FileReadResult {
    std::string contents;
    FileFingerprint fingerprint;
};

enum class FileWriteStatus {
    Ok,
    NotFound,
    Conflict,
    Error,
};

struct FileWriteResult {
    FileWriteStatus status = FileWriteStatus::Error;
    std::optional<FileFingerprint> fingerprint;
};

// A write intent is part of the injected filesystem contract. It keeps file
// controller policy explicit and lets adapters make creation or replacement
// conditional instead of treating an omitted fingerprint as permission to
// overwrite an unrelated file.
enum class FileWriteExpectationKind {
    Any,
    MustNotExist,
    MatchFingerprint,
};

struct FileWriteExpectation {
    FileWriteExpectationKind kind = FileWriteExpectationKind::Any;
    std::optional<FileFingerprint> fingerprint;

    static FileWriteExpectation any() noexcept { return {}; }
    static FileWriteExpectation must_not_exist() noexcept { return {FileWriteExpectationKind::MustNotExist, std::nullopt}; }
    static FileWriteExpectation matching(FileFingerprint value) { return {FileWriteExpectationKind::MatchFingerprint, std::move(value)}; }
};

class FileSystem {
public:
    virtual ~FileSystem() = default;

    // Entries directly inside `path`, in implementation-defined order
    // (callers sort if they want a specific one). Empty for a path
    // that doesn't exist or isn't a directory — not an error; a file
    // dialog shows "no entries" rather than throwing over a stale path.
    virtual std::vector<FileEntry> list_directory(std::string_view path) const = 0;

    virtual bool exists(std::string_view path) const noexcept = 0;
    virtual bool is_directory(std::string_view path) const noexcept = 0;

    virtual std::string normalize_path(std::string_view path) const;
    virtual bool is_absolute_path(std::string_view path) const noexcept;

    // Forward-slash joins `directory` and `name` (collapsing a
    // trailing/leading slash so the result never has "//"). Absolute-path
    // acceptance is caller policy; join() treats `name` as a child fragment.
    virtual std::string join(std::string_view directory, std::string_view name) const;

    // The path with its last "/segment" removed; "/" for a path with
    // no parent (already at the root).
    virtual std::string parent(std::string_view path) const;

    // The editor workflow uses these explicit operations instead of reaching
    // around the injected service. Existing directory-only adapters can retain
    // the conservative defaults until they implement file content support.
    virtual std::optional<FileReadResult> read_file(std::string_view path) const;
    virtual FileWriteResult write_file_atomic(std::string_view path, std::string_view contents,
                                              FileWriteExpectation expectation = {});
    virtual std::optional<FileFingerprint> fingerprint(std::string_view path) const;
};

// A full in-memory FileSystem for tests: a scripted tree of
// directories and files, no real I/O.
class MemoryFileSystem final : public FileSystem {
public:
    void add_directory(std::string_view path);
    void add_file(std::string_view path);
    void add_file(std::string_view path, std::string contents);

    std::vector<FileEntry> list_directory(std::string_view path) const override;
    bool exists(std::string_view path) const noexcept override;
    bool is_directory(std::string_view path) const noexcept override;
    std::optional<FileReadResult> read_file(std::string_view path) const override;
    FileWriteResult write_file_atomic(std::string_view path, std::string_view contents,
                                      FileWriteExpectation expectation = {}) override;
    std::optional<FileFingerprint> fingerprint(std::string_view path) const override;

private:
    struct Node {
        bool is_directory = false;
        std::string contents;
        std::uint64_t revision = 0;
    };
    // Keyed by full normalized path; "/" always exists as a directory.
    std::vector<std::pair<std::string, Node>> nodes_{{"/", Node{true, {}, 0}}};

    Node* find(std::string_view path) noexcept;
    const Node* find(std::string_view path) const noexcept;
};

}  // namespace ckv
