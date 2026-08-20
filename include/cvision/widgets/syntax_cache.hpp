// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Deterministic line-state cache shared by source-editor views. It owns no
// document and does no I/O: callers provide the current logical lines.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cvision/widgets/syntax_profile.hpp"

namespace ckv::widgets {

struct SyntaxCacheLine {
    std::string text;
    std::vector<SyntaxSpan> spans;
    std::string incoming_state;
    std::string outgoing_state;
};

struct SyntaxRelexReport {
    std::size_t first_line = 0;
    std::size_t line_count = 0;
    bool reached_fixed_point = true;
};

// Relexes only a changed suffix. Once an unchanged source line again receives
// the same incoming state, output state, and spans, later cached entries are
// valid by induction and no longer invoke the highlighter.
class SyntaxCache {
public:
    void clear() noexcept;
    // Completes the current invalidation synchronously. This is appropriate
    // for small documents and deterministic tooling. Interactive clients with
    // potentially large off-screen suffixes should use update_bounded().
    SyntaxRelexReport update(const LanguageProfile& profile, const std::vector<std::string>& source_lines);
    // Relexes at most max_lines logical lines. If reached_fixed_point is false,
    // call this again with the same profile and current source lines from an
    // explicit application task. No worker thread or clock is involved.
    SyntaxRelexReport update_bounded(const LanguageProfile& profile, const std::vector<std::string>& source_lines,
                                     std::size_t max_lines);
    bool has_pending_work() const noexcept { return pending_; }

    const std::vector<SyntaxCacheLine>& lines() const noexcept { return lines_; }
    const SyntaxCacheLine* line(std::size_t index) const noexcept;

private:
    std::string profile_id_;
    std::vector<SyntaxCacheLine> lines_;
    bool pending_ = false;
    std::size_t pending_line_ = 0;
    std::size_t pending_minimum_line_ = 0;
};

}  // namespace ckv::widgets
