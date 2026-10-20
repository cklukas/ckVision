// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ckv::widgets {

// Non-zero identity of a candidate, stable across updates to one source.
using CompletionId = std::uint64_t;

// Publication limits: providers supply at most this many candidates, each
// with insertion text no larger than kMaxCompletionTextBytes. InputLine rejects
// oversized publications without enumerating them.
inline constexpr std::size_t kMaxCompletionCandidates = 64;
inline constexpr std::size_t kMaxCompletionTextBytes = 32768;

// One committed editing state. Positions are grapheme indices, not UTF-8 bytes.
struct CompletionRequest {
    // Monotonic field-local identity; results for a different revision are stale.
    std::uint64_t revision = 0;
    // Committed value, excluding any preview.
    std::string text;
    // Caret in [0, grapheme_count(text)].
    std::size_t cursor = 0;
    // Whether selection is anchored, including an empty anchored selection.
    bool has_selection = false;
    // Selected half-open grapheme range; both equal cursor without selection.
    std::size_t selection_begin = 0;
    std::size_t selection_end = 0;
};

// An explicit edit proposal. InputLine previews exact suffix extensions only;
// callers can select and accept other replacements through the same edit API.
struct CompletionCandidate {
    // Non-zero source identity; duplicate identities in a publication are invalid.
    CompletionId id = 0;
    // Half-open range of committed graphemes replaced upon acceptance.
    std::size_t begin = 0;
    std::size_t end = 0;
    // UTF-8 insertion text; must be non-empty and valid, without control bytes.
    std::string text;
};

// Pending discovery and partial results are explicit; partial does not mean
// that a candidate is unique in the entire source.
enum class CompletionState { Pending, Partial, Ready, Failed };

// A bounded immutable-to-the-reader publication, owned by its provider.
struct CompletionSnapshot {
    // Exactly the request revision this publication answers.
    std::uint64_t revision = 0;
    // Discovery status; Failed offers no inline preview.
    CompletionState state = CompletionState::Ready;
    // Ordered candidates; at most kMaxCompletionCandidates.
    std::vector<CompletionCandidate> candidates;
};

// An instance-owned, UI-thread provider. request() must return promptly: no
// blocking I/O, full-source scan/sort, or worker wait. An asynchronous source
// publishes on the UI thread then calls InputLine::completion_changed(). A
// provider never calls back into the field during request()/snapshot().
class CompletionProvider {
public:
    // Safe destruction through the interface.
    virtual ~CompletionProvider() = default;
    // Start/replace a query; superseded requests may be discarded by the source.
    virtual void request(const CompletionRequest& request) = 0;
    // Current publication, valid until the next provider mutation.
    virtual const CompletionSnapshot& snapshot() const noexcept = 0;
};

// Convenience provider for static vocabularies: construction sorts/deduplicates
// once; queries use a prefix index and publish a bounded slice. Case-sensitive
// exact prefixes preserve the typed spelling. Supply a separate provider for
// fuzzy/domain-specific matching. Construct large vocabularies outside dispatch.
class PrefixCompletionProvider final : public CompletionProvider {
public:
    // Owns the vocabulary; ignores empty/oversized entries.
    explicit PrefixCompletionProvider(std::vector<std::string> choices);
    void request(const CompletionRequest& request) override;
    const CompletionSnapshot& snapshot() const noexcept override { return snapshot_; }
private:
    std::vector<std::string> choices_;
    CompletionSnapshot snapshot_;
};

}  // namespace ckv::widgets
