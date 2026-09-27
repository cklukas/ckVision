// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/syntax_cache.hpp"

#include <algorithm>
#include <limits>

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

// Which byte offsets of `line`, 0 through its size, are grapheme-cluster boundaries:
// one walk of the line, however many spans a profile reports on it.
std::vector<bool> cluster_boundaries(std::string_view line) {
    std::vector<bool> result(line.size() + 1U, false);
    result[0] = true;
    for (std::size_t position = 0; position < line.size();) {
        position = text::grapheme_end(line, position);
        result[position] = true;
    }
    return result;
}

std::vector<SyntaxSpan> validated_spans(std::string_view line, SyntaxLineResult result) {
    std::sort(result.spans.begin(), result.spans.end(), [](const SyntaxSpan& left, const SyntaxSpan& right) {
        return left.begin_byte < right.begin_byte;
    });
    std::vector<SyntaxSpan> spans;
    if (result.spans.empty()) return spans;
    spans.reserve(result.spans.size());
    const std::vector<bool> boundary = cluster_boundaries(line);
    for (const SyntaxSpan& span : result.spans)
        if (span.begin_byte < span.end_byte && span.end_byte <= line.size() && boundary[span.begin_byte] &&
            boundary[span.end_byte])
            spans.push_back(span);
    return spans;
}

}  // namespace

void SyntaxCache::clear() noexcept {
    profile_id_.clear();
    lines_.clear();
    pending_ = false;
    pending_line_ = 0;
    pending_settle_line_ = 0;
}

const SyntaxCacheLine* SyntaxCache::line(std::size_t index) const noexcept {
    return index < lines_.size() ? &lines_[index] : nullptr;
}

SyntaxRelexReport SyntaxCache::update(const LanguageProfile& profile, const std::vector<std::string>& source_lines) {
    return update_bounded(profile, source_lines, std::numeric_limits<std::size_t>::max());
}

SyntaxRelexReport SyntaxCache::update_bounded(const LanguageProfile& profile,
                                               const std::vector<std::string>& source_lines,
                                               std::size_t max_lines) {
    // A zero budget never claims that a potentially changed source is current.
    // It is useful for callers that want to poll without starting work.
    if (max_lines == 0U) return SyntaxRelexReport{pending_line_, 0U, false};
    const bool profile_changed = profile_id_ != profile.id;

    // The lines this update must relex: every line under a new profile; otherwise each line
    // whose text differs from the cached text and each line new to the cache. The pass starts
    // at the first of them and may take a line that relexes to its cached entry as the fixed
    // point only once it is past the last of them: an unchanged line between two edits proves
    // nothing about the lines beyond the second.
    std::size_t first = source_lines.size();
    std::size_t last_changed = 0;
    bool changed = false;
    if (profile_changed) {
        first = 0U;
        last_changed = std::numeric_limits<std::size_t>::max();
        changed = true;
    } else {
        const std::size_t common = std::min(lines_.size(), source_lines.size());
        for (std::size_t index = 0; index < common; ++index) {
            if (lines_[index].text == source_lines[index]) continue;
            first = std::min(first, index);
            last_changed = index;
            changed = true;
        }
        if (source_lines.size() > lines_.size()) {
            first = std::min(first, lines_.size());
            last_changed = source_lines.size() - 1U;
            changed = true;
        }
    }
    if (!changed && !pending_) {
        lines_.resize(source_lines.size());
        return {};
    }

    // The pass may take a line after settle_line as the fixed point, never one at or before it.
    std::size_t settle_line = last_changed;
    // A previously budgeted pass has already made the source text current for its completed
    // prefix. It resumes at its saved line unless a newer edit invalidates an earlier one; the
    // resume line is the first that may be the fixed point, unless the interrupted pass had
    // further to go before it could stop. Both lines are cache indices, which an edit between
    // the calls does not renumber: the cache never shifts its entries, and every entry whose
    // text no longer matches the line now at its index is found again above.
    if (pending_ && !profile_changed) {
        first = std::min(first, pending_line_);
        const std::size_t interrupted_settle_line = std::max(pending_line_ - 1U, pending_settle_line_);
        settle_line = changed ? std::max(last_changed, interrupted_settle_line) : interrupted_settle_line;
    }

    const std::size_t old_size = lines_.size();
    lines_.resize(source_lines.size());
    profile_id_ = profile.id;
    if (first >= source_lines.size()) {
        pending_ = false;
        pending_line_ = 0;
        pending_settle_line_ = 0;
        return SyntaxRelexReport{first, 0, true};
    }

    SyntaxRelexReport report{first, 0, false};
    std::string state = first == 0U ? std::string{} : lines_[first - 1U].outgoing_state;
    for (std::size_t index = first; index < source_lines.size(); ++index) {
        const SyntaxCacheLine old = index < old_size ? lines_[index] : SyntaxCacheLine{};
        SyntaxLineResult result = profile.highlight_line(source_lines[index], state);
        std::string outgoing_state = result.next_state;
        SyntaxCacheLine next{source_lines[index], validated_spans(source_lines[index], std::move(result)), state,
                             std::move(outgoing_state)};
        lines_[index] = std::move(next);
        ++report.line_count;
        state = lines_[index].outgoing_state;
        if (index > settle_line && index < old_size && old.text == lines_[index].text &&
            old.incoming_state == lines_[index].incoming_state && old.outgoing_state == lines_[index].outgoing_state &&
            old.spans == lines_[index].spans) {
            report.reached_fixed_point = true;
            break;
        }
        if (report.line_count == max_lines && index + 1U < source_lines.size()) {
            pending_ = true;
            pending_line_ = index + 1U;
            pending_settle_line_ = settle_line;
            return report;
        }
    }
    report.reached_fixed_point = true;
    pending_ = false;
    pending_line_ = 0;
    pending_settle_line_ = 0;
    return report;
}

}  // namespace ckv::widgets
