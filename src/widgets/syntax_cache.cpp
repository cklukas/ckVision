// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/syntax_cache.hpp"

#include <algorithm>
#include <limits>

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

bool is_boundary(std::string_view value, std::size_t byte) {
    if (byte == 0U || byte == value.size()) return true;
    for (std::size_t position = 0; position < value.size();) {
        position = text::grapheme_end(value, position);
        if (position == byte) return true;
        if (position > byte) return false;
    }
    return false;
}

std::vector<SyntaxSpan> validated_spans(std::string_view line, SyntaxLineResult result) {
    std::sort(result.spans.begin(), result.spans.end(), [](const SyntaxSpan& left, const SyntaxSpan& right) {
        return left.begin_byte < right.begin_byte;
    });
    std::vector<SyntaxSpan> spans;
    spans.reserve(result.spans.size());
    for (const SyntaxSpan& span : result.spans)
        if (span.begin_byte < span.end_byte && span.end_byte <= line.size() &&
            is_boundary(line, span.begin_byte) && is_boundary(line, span.end_byte))
            spans.push_back(span);
    return spans;
}

}  // namespace

void SyntaxCache::clear() noexcept {
    profile_id_.clear();
    lines_.clear();
    pending_ = false;
    pending_line_ = 0;
    pending_minimum_line_ = 0;
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
    std::size_t first = profile_changed ? 0U : std::min(lines_.size(), source_lines.size());
    if (!profile_changed)
        for (std::size_t index = 0; index < std::min(lines_.size(), source_lines.size()); ++index)
            if (lines_[index].text != source_lines[index]) { first = index; break; }
    if (!profile_changed && first == lines_.size() && first == source_lines.size() && !pending_) return {};

    // A previously budgeted pass has already made the source text current for
    // its completed prefix. Keep processing from its saved suffix unless a
    // newer edit invalidates an earlier line.
    if (pending_ && !profile_changed) first = std::min(first, pending_line_);
    const std::size_t minimum_line = profile_changed ? 1U
        : (pending_ && first == pending_line_ ? pending_minimum_line_ : first + 1U);

    const std::size_t old_size = lines_.size();
    lines_.resize(source_lines.size());
    profile_id_ = profile.id;
    if (first >= source_lines.size()) {
        pending_ = false;
        pending_line_ = 0;
        pending_minimum_line_ = 0;
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
        if (index + 1U >= minimum_line && old.text == lines_[index].text && old.incoming_state == lines_[index].incoming_state &&
            old.outgoing_state == lines_[index].outgoing_state && old.spans == lines_[index].spans) {
            report.reached_fixed_point = true;
            break;
        }
        if (report.line_count == max_lines && index + 1U < source_lines.size()) {
            pending_ = true;
            pending_line_ = index + 1U;
            pending_minimum_line_ = minimum_line;
            return report;
        }
    }
    report.reached_fixed_point = true;
    pending_ = false;
    pending_line_ = 0;
    pending_minimum_line_ = 0;
    return report;
}

}  // namespace ckv::widgets
