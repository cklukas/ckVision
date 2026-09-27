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

// One item of a directory listing, as returned by FileSystem::list_directory.
struct FileEntry {
    // The entry's last path segment, and whether it is a directory (anything else, including a
    // file of any kind, reports false).
    std::string name;  // the entry's own name, not a full path
    bool is_directory = false;

    // Memberwise equality: same name and same kind.
    friend bool operator==(const FileEntry&, const FileEntry&) = default;
};

// An opaque token identifying one version of a regular file, used to detect that a file changed
// between reading it and writing it back. Only equality is meaningful, and only between
// fingerprints from the same FileSystem: the format is the implementation's own (a revision
// counter in MemoryFileSystem, device, inode, size and modification time in PosixFileSystem).
struct FileFingerprint {
    // The token itself; never parse it.
    std::string value;

    // Equal tokens mean the file is believed unchanged.
    friend bool operator==(const FileFingerprint&, const FileFingerprint&) = default;
};

// A successful whole-file read: the bytes as stored (no newline or encoding conversion) and the
// fingerprint of exactly the version those bytes came from.
struct FileReadResult {
    // The file's contents and the fingerprint to pass back to a conditional write.
    std::string contents;
    FileFingerprint fingerprint;
};

// The outcome of FileSystem::write_file_atomic.
enum class FileWriteStatus {
    // The new contents were published in full.
    Ok,
    // The path cannot hold a file (MemoryFileSystem reports this when it names a directory or
    // lies inside a file).
    NotFound,
    // The write expectation did not hold, so nothing was written.
    Conflict,
    // Any other failure, including a FileSystem that does not support writing. The target holds
    // either its old or its new contents, never a mixture; which one is not reported.
    Error,
};

// The result of a write: its status and, when known, the target's fingerprint afterwards.
struct FileWriteResult {
    // Defaults to Error so a default-constructed result never reads as success. On Ok the
    // fingerprint is the new version's; on Conflict it is the version currently on disk, or empty
    // when no file is there; otherwise it is empty.
    FileWriteStatus status = FileWriteStatus::Error;
    std::optional<FileFingerprint> fingerprint;
};

// A write intent is part of the injected filesystem contract. It keeps file
// controller policy explicit and lets adapters make creation or replacement
// conditional instead of treating an omitted fingerprint as permission to
// overwrite an unrelated file.
enum class FileWriteExpectationKind {
    // Write unconditionally, creating or replacing the file.
    Any,
    // Create only: Conflict if the path already holds a file.
    MustNotExist,
    // Replace only the version identified by the expectation's fingerprint: Conflict if the file
    // is missing, has changed, or no fingerprint was supplied.
    MatchFingerprint,
};

// The condition a write_file_atomic call must satisfy, checked by the FileSystem itself so the
// check and the write are one step. Build it with the factories below rather than by hand.
struct FileWriteExpectation {
    // Which condition applies, and the expected version for MatchFingerprint (ignored otherwise).
    FileWriteExpectationKind kind = FileWriteExpectationKind::Any;
    std::optional<FileFingerprint> fingerprint;

    // An unconditional write, a create-only write, and a replace-if-unchanged write against
    // `value` (typically the fingerprint from the read the caller is saving over).
    static FileWriteExpectation any() noexcept { return {}; }
    static FileWriteExpectation must_not_exist() noexcept { return {FileWriteExpectationKind::MustNotExist, std::nullopt}; }
    static FileWriteExpectation matching(FileFingerprint value) { return {FileWriteExpectationKind::MatchFingerprint, std::move(value)}; }
};

// The injected filesystem service. Implementations must provide listing and existence queries;
// directory creation, whole-file reads and atomic writes have conservative defaults (below) so a
// browse-only adapter need not implement them. Path helpers are virtual so an adapter with other
// host semantics can replace them.
class FileSystem {
public:
    // Destroys the service; implementations may be deleted through a FileSystem pointer.
    virtual ~FileSystem() = default;

    // Entries directly inside `path`, in implementation-defined order
    // (callers sort if they want a specific one). Empty for a path
    // that doesn't exist or isn't a directory — not an error; a file
    // dialog shows "no entries" rather than throwing over a stale path.
    virtual std::vector<FileEntry> list_directory(std::string_view path) const = 0;

    // Whether anything exists at `path`, and whether it is a directory. Both answer false rather
    // than failing for a path that cannot be examined.
    virtual bool exists(std::string_view path) const noexcept = 0;
    virtual bool is_directory(std::string_view path) const noexcept = 0;

    // Creates `path` and every missing parent directory. Existing directories
    // are success; a conflicting file or platform error is failure.
    // The base implementation creates nothing and returns false.
    virtual bool create_directories(std::string_view path);

    // The path in canonical form under the default rules described at the top of this file:
    // separators become '/', runs of them collapse, a trailing one is dropped except on a root,
    // and a path that is not drive-rooted gains a leading '/' (so "" becomes "/" and "a/b"
    // becomes "/a/b"). "." and ".." segments are kept as written, not resolved.
    virtual std::string normalize_path(std::string_view path) const;
    // True when `path` starts with '/' or '\', or with a drive letter, ':' and a separator
    // ("C:/", "c:\"). A bare "C:" is not absolute. The path is not normalized first.
    virtual bool is_absolute_path(std::string_view path) const noexcept;

    // Joins `directory` and `name` with one '/' and normalizes the result
    // (normalize_path), so it never has "//" — whatever separators either
    // part ends, starts or contains. Absolute-path acceptance is caller
    // policy; join() treats `name` as a child fragment.
    virtual std::string join(std::string_view directory, std::string_view name) const;

    // The path with its last "/segment" removed; "/" for a path with
    // no parent (already at the root). The path is normalized first, and a
    // drive root is its own parent: "C:/dir" and "C:/" both give "C:/".
    virtual std::string parent(std::string_view path) const;

    // The editor workflow uses these explicit operations instead of reaching
    // around the injected service. Existing directory-only adapters can retain
    // the conservative defaults until they implement file content support.
    //
    // read_file returns the whole contents and fingerprint of the regular file at `path`, or
    // nothing when it is missing, not a regular file, or unreadable. write_file_atomic replaces
    // or creates the file so a reader sees either the old or the new contents, after first
    // checking `expectation`; see FileWriteResult for what each status carries. fingerprint
    // returns the current version token of a regular file without reading it, or nothing. The
    // defaults return nothing, a FileWriteStatus::Error result, and nothing respectively.
    virtual std::optional<FileReadResult> read_file(std::string_view path) const;
    virtual FileWriteResult write_file_atomic(std::string_view path, std::string_view contents,
                                              FileWriteExpectation expectation = {});
    virtual std::optional<FileFingerprint> fingerprint(std::string_view path) const;
};

// A full in-memory FileSystem for tests: a scripted tree of
// directories and files, no real I/O.
class MemoryFileSystem final : public FileSystem {
public:
    // Script the tree. Paths are normalized first, and every ancestor is made a directory, an
    // existing file included. add_directory likewise turns the path itself into a directory
    // when it names a file; add_file on an existing node makes it a file with the given contents
    // (empty for the one-argument form) and advances its fingerprint.
    void add_directory(std::string_view path);
    void add_file(std::string_view path);
    void add_file(std::string_view path, std::string contents);

    // The overrides follow the FileSystem contract with these specifics: every path is
    // normalized before lookup; list_directory returns entries in the order they were added;
    // fingerprints are a per-file revision count ("1" when created, plus one per write);
    // write_file_atomic creates missing parent directories and answers NotFound for a path that
    // names a directory or lies inside a file; create_directories fails when any component of
    // the path is a file.
    std::vector<FileEntry> list_directory(std::string_view path) const override;
    bool exists(std::string_view path) const noexcept override;
    bool is_directory(std::string_view path) const noexcept override;
    bool create_directories(std::string_view path) override;
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
    // Whether `normalized`, or any directory it lies in, is an existing file.
    bool file_along(std::string_view normalized) const noexcept;
};

}  // namespace ckv
