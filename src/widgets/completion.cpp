// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/completion.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

PrefixCompletionProvider::PrefixCompletionProvider(std::vector<std::string> choices)
    : choices_(std::move(choices)) {
    std::erase_if(choices_, [](const std::string& choice) {
        return choice.empty() || choice.size() > kMaxCompletionTextBytes;
    });
    std::sort(choices_.begin(), choices_.end());
    choices_.erase(std::unique(choices_.begin(), choices_.end()), choices_.end());
    snapshot_.candidates.reserve(kMaxCompletionCandidates);
}

void PrefixCompletionProvider::request(const CompletionRequest& query) {
    snapshot_.revision = query.revision;
    snapshot_.state = CompletionState::Ready;
    snapshot_.candidates.clear();
    if (query.has_selection || query.text.size() > kMaxCompletionTextBytes) return;
    const auto count = text::split_graphemes(query.text).size();
    if (query.cursor != count) return;
    auto match = std::lower_bound(choices_.begin(), choices_.end(), query.text);
    for (; match != choices_.end() && match->starts_with(query.text); ++match) {
        if (snapshot_.candidates.size() == kMaxCompletionCandidates) break;
        if (*match == query.text) continue;
        const auto identity = static_cast<CompletionId>(match - choices_.begin()) + 1;
        snapshot_.candidates.push_back(CompletionCandidate{identity, 0, count, *match});
    }
}

}  // namespace ckv::widgets
