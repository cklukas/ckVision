// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "cvision/widgets/editor_document.hpp"

namespace ckv::widgets {

// What EditorSearch looks for: a literal string, never a pattern.
struct EditorSearchQuery {
    // The UTF-8 text to find. An empty text finds nothing, and since the document's line breaks
    // are LF, a text containing CR never matches.
    std::string text;
    // When false, only ASCII A to Z are matched regardless of case; every other character must
    // match exactly.
    bool case_sensitive = true;
    // When true, a match counts only if neither the byte before it nor the byte after it is an
    // ASCII letter, digit or underscore. Bytes of non-ASCII characters count as non-word bytes.
    bool whole_word = false;
};

// One occurrence, as a range of the revision that was searched; it goes stale with the
// document's next change.
struct EditorSearchMatch {
    // The matched text's range.
    DocumentRange range;

    // Memberwise equality.
    friend bool operator==(const EditorSearchMatch&, const EditorSearchMatch&) = default;
};

// Literal, deterministic search. It deliberately has no implicit regex path;
// a future bounded matcher can extend this API without exposing std::regex.
class EditorSearch {
public:
    // Every occurrence of the query in the document's current text, left to right and
    // non-overlapping: after a match the scan resumes at its end. An occurrence that would begin
    // or end inside a grapheme cluster is skipped.
    static std::vector<EditorSearchMatch> find_all(const EditorDocument& document, const EditorSearchQuery& query);
    // Replaces every match find_all reports with `replacement` in one transaction, so one
    // revision and one undo step. With no match it returns Ok with no change; otherwise it
    // returns what EditorDocument::commit returns, and the replacement is normalized as any
    // edit's text is.
    static DocumentEditResult replace_all(EditorDocument& document, const EditorSearchQuery& query,
                                          const std::string& replacement);
};

}  // namespace ckv::widgets
