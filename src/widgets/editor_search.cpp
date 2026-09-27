// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/editor_search.hpp"

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

char folded_ascii(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool equal_at(std::string_view haystack, std::size_t position, std::string_view needle, bool sensitive) noexcept {
    if (position + needle.size() > haystack.size()) return false;
    for (std::size_t i = 0; i < needle.size(); ++i)
        if (sensitive ? haystack[position + i] != needle[i]
                      : folded_ascii(haystack[position + i]) != folded_ascii(needle[i]))
            return false;
    return true;
}

bool word_byte(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}

bool whole_word_at(std::string_view value, std::size_t begin, std::size_t end) noexcept {
    return (begin == 0U || !word_byte(value[begin - 1U])) && (end == value.size() || !word_byte(value[end]));
}

// Whether the clusters from the boundary `begin` have a boundary at `end`.
bool clusters_end_at(std::string_view value, std::size_t begin, std::size_t end) noexcept {
    std::size_t current = begin;
    while (current < end) current = text::grapheme_end(value, current);
    return current == end;
}

}  // namespace

std::vector<EditorSearchMatch> EditorSearch::find_all(const EditorDocument& document, const EditorSearchQuery& query) {
    std::vector<EditorSearchMatch> matches;
    if (query.text.empty()) return matches;
    const std::string value = document.text();
    // One walk over the document's clusters: a match begins on a boundary and
    // must end on one, and the walk resumes at the end of each match.
    for (std::size_t position = 0; position + query.text.size() <= value.size();) {
        const std::size_t end = position + query.text.size();
        if (equal_at(value, position, query.text, query.case_sensitive) &&
            clusters_end_at(value, position, end) && (!query.whole_word || whole_word_at(value, position, end))) {
            // Both ends are proven boundaries of this revision's text, which
            // is all position_at_byte would check, one line walk per end.
            matches.push_back(EditorSearchMatch{DocumentRange{DocumentPosition{document.revision(), position},
                                                              DocumentPosition{document.revision(), end}}});
            position = end;
        } else {
            position = text::grapheme_end(value, position);
        }
    }
    return matches;
}

DocumentEditResult EditorSearch::replace_all(EditorDocument& document, const EditorSearchQuery& query,
                                             const std::string& replacement) {
    const std::vector<EditorSearchMatch> matches = find_all(document, query);
    if (matches.empty()) return DocumentEditResult{};
    DocumentTransaction transaction = document.transaction();
    for (const EditorSearchMatch& match : matches) transaction.replace(match.range, replacement);
    return document.commit(std::move(transaction));
}

}  // namespace ckv::widgets
