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

// The cached lexing of one logical line: the source text it was lexed from, the spans the
// highlighter produced (sorted by begin_byte, with invalid spans dropped as SyntaxSpan
// describes), and the lexer state on entry to and exit from the line.
struct SyntaxCacheLine {
    // The source text, its validated spans, and the states the highlighter was handed and
    // returned; incoming_state is empty for line 0.
    std::string text;
    std::vector<SyntaxSpan> spans;
    std::string incoming_state;
    std::string outgoing_state;
};

// What one update or update_bounded call did.
struct SyntaxRelexReport {
    // The index of the first line relexed. A zero-budget update_bounded reports the line a
    // pending pass would resume from, or 0 when nothing is pending.
    std::size_t first_line = 0;
    // How many lines the highlighter was called for; 0 when the cache was already current.
    std::size_t line_count = 0;
    // False only when the call stopped at its line budget with lines still to relex, or
    // was given a budget of zero; has_pending_work() then says whether to call again.
    bool reached_fixed_point = true;
};

// Relexes only a changed suffix. Once an unchanged source line past the last
// changed one again receives the same incoming state, output state, and spans,
// later cached entries are valid by induction and no longer invoke the
// highlighter.
class SyntaxCache {
public:
    // Forgets every cached line, the remembered profile id and any pending pass, so the
    // next update lexes the whole document.
    void clear() noexcept;
    // Completes the current invalidation synchronously. This is appropriate
    // for small documents and deterministic tooling. Interactive clients with
    // potentially large off-screen suffixes should use update_bounded().
    //
    // The changed lines are every line when `profile` has a different id from the last
    // update's (a profile is recognised by its id alone, so replacing a profile's highlighter
    // under the same id needs clear() first), and otherwise each line whose text differs from
    // the cached text and each line new to the cache. The cache is resized to `source_lines`;
    // relexing starts at the first changed line and stops at the first relexed line after
    // the last changed one whose text, states and spans all equal the cached ones, so
    // several separated edits in one update are all relexed.
    SyntaxRelexReport update(const LanguageProfile& profile, const std::vector<std::string>& source_lines);
    // Relexes at most max_lines logical lines. If reached_fixed_point is false,
    // call this again with the same profile and current source lines from an
    // explicit application task. No worker thread or clock is involved.
    SyntaxRelexReport update_bounded(const LanguageProfile& profile, const std::vector<std::string>& source_lines,
                                     std::size_t max_lines);
    // True while a budgeted pass stopped short and its remaining suffix still needs
    // relexing; an unbounded update or clear() ends it.
    bool has_pending_work() const noexcept { return pending_; }

    // The cache's lines, one per source line of the last update given a non-zero budget.
    // An index past a pending pass's progress still holds its earlier lexing, or an empty
    // entry for a line new to the cache. line() returns nullptr for an index past the end.
    // The reference and pointers are invalidated by the next update or clear.
    const std::vector<SyntaxCacheLine>& lines() const noexcept { return lines_; }
    const SyntaxCacheLine* line(std::size_t index) const noexcept;

private:
    std::string profile_id_;
    std::vector<SyntaxCacheLine> lines_;
    bool pending_ = false;
    std::size_t pending_line_ = 0;
    // The last line the interrupted pass had to relex before a fixed point could end it.
    std::size_t pending_settle_line_ = 0;
};

}  // namespace ckv::widgets
