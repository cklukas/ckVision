// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "cvision/widgets/editor_document.hpp"

namespace ckv::widgets {

struct EditorSearchQuery {
    std::string text;
    bool case_sensitive = true;
    bool whole_word = false;
};

struct EditorSearchMatch {
    DocumentRange range;

    friend bool operator==(const EditorSearchMatch&, const EditorSearchMatch&) = default;
};

// Literal, deterministic search. It deliberately has no implicit regex path;
// a future bounded matcher can extend this API without exposing std::regex.
class EditorSearch {
public:
    static std::vector<EditorSearchMatch> find_all(const EditorDocument& document, const EditorSearchQuery& query);
    static DocumentEditResult replace_all(EditorDocument& document, const EditorSearchQuery& query,
                                          const std::string& replacement);
};

}  // namespace ckv::widgets
