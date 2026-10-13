// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::widgets {

// Hard UTF-8 literal byte bound, independent of retained log size.
inline constexpr std::size_t kLogLiteralBytes = 1024;
// Half-open UTF-8 byte interval; renderers expand touched graphemes.
struct LogTextRange {
    // Byte endpoints and value comparison for a single record.
    std::size_t begin = 0, end = 0;
    friend bool operator==(const LogTextRange&, const LogTextRange&) = default;
};
// Resumable byte position/prefix length, and scan completion classification.
struct LogLiteralCursor { std::size_t byte = 0, matched = 0; };
enum class LogLiteralStep { Pending, Match, End };

// Bounded compiled literal, not a regex. Construction reserves all storage.
// set/scan allocate nothing. LF is permitted for explicit multiline records;
// malformed UTF-8 and other display controls are rejected. Insensitive matching
// folds only ASCII A-Z. Match offsets are bytes and may touch part of a grapheme;
// a renderer/reveal operation emphasizes/selects the whole touched grapheme.
class LogLiteral {
public:
    // Reserve the complete bounded pattern/prefix workspace.
    LogLiteral();
    // Check byte/UTF-8/control limits before mutating a compiled pattern.
    static bool valid(std::string_view pattern) noexcept;
    // Replace a valid literal; false preserves the previous pattern.
    bool set(std::string_view pattern, bool case_sensitive = true) noexcept;
    // Borrow the compiled pattern and inspect its ASCII-folding policy.
    std::string_view pattern() const noexcept { return pattern_; }
    bool case_sensitive() const noexcept { return sensitive_; }
    // Object plus reserved pattern and prefix capacities.
    std::size_t allocated_bytes() const noexcept {
        return sizeof(*this) + pattern_.capacity() + prefix_.capacity() * sizeof(std::size_t);
    }
    // Each byte comparison (including prefix fallback) consumes one operation.
    // Match advances the cursor and supports overlapping occurrences. Borrowed
    // value must keep the same bytes while this cursor is resumed.
    LogLiteralStep scan(std::string_view value, LogLiteralCursor& cursor,
                        std::size_t& operations, LogTextRange& match) const noexcept;
private:
    unsigned char fold(unsigned char value) const noexcept;
    std::string pattern_;
    std::vector<std::size_t> prefix_;
    bool sensitive_ = true;
};

struct LogRecord;
// Borrowed by a view. marks covers exactly [begin,end), is cleared by the
// highlighter, and has end-begin bytes. Only that window plus pattern overlap
// may be examined; mark never walks retained records or allocates.
class LogHighlighter {
public:
    // Release an instance whose lifetime must enclose borrowing views.
    virtual ~LogHighlighter() = default;
    // Fill only the supplied record byte window, without allocation or I/O.
    virtual void mark(const LogRecord& record, std::size_t begin, std::size_t end,
                      std::span<unsigned char> marks) const noexcept = 0;
};

}  // namespace ckv::widgets
