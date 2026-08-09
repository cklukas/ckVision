// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The library's only filesystem access (ARCHITECTURE.md §5 "Platform
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

#include <string>
#include <string_view>
#include <vector>

namespace ckv {

struct FileEntry {
    std::string name;  // the entry's own name, not a full path
    bool is_directory = false;

    friend bool operator==(const FileEntry&, const FileEntry&) = default;
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
};

// A full in-memory FileSystem for tests: a scripted tree of
// directories and files, no real I/O.
class MemoryFileSystem final : public FileSystem {
public:
    void add_directory(std::string_view path);
    void add_file(std::string_view path);

    std::vector<FileEntry> list_directory(std::string_view path) const override;
    bool exists(std::string_view path) const noexcept override;
    bool is_directory(std::string_view path) const noexcept override;

private:
    struct Node {
        bool is_directory = false;
    };
    // Keyed by full normalized path; "/" always exists as a directory.
    std::vector<std::pair<std::string, Node>> nodes_{{"/", Node{true}}};

    Node* find(std::string_view path) noexcept;
    const Node* find(std::string_view path) const noexcept;
};

}  // namespace ckv
