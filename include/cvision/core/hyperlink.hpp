// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hyperlink targets carried by cells (the decision log D-088). A cell names its
// link by a small id; the grid that holds the cell owns the LinkTable that
// turns the id back into its target. A cell without a link carries the id
// kNoLink in storage its layout already had, so ordinary content pays nothing
// for the feature. Every target a table holds is a valid terminal hyperlink by
// construction (the architecture §12), which is what lets the presenter emit
// one without inspecting it again.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ckv {

// A link's handle within one LinkTable. Ids mean something only against the
// table that issued them: two grids may use the same id for different targets.
using LinkId = std::uint32_t;

// The id of "no link", held by every cell that is not part of one.
inline constexpr LinkId kNoLink = 0;

// The longest target accepted, in bytes. It is the smallest limit a published
// terminal implementation documents for an OSC 8 URI; a longer one would be
// dropped by that terminal anyway, so it is refused here, where the refusal is
// deterministic and testable.
inline constexpr std::size_t kMaxHyperlinkTargetBytes = 2083;

// Whether `target` can be sent to a terminal as an OSC 8 hyperlink: an
// absolute URI (RFC 3986 §3.1: a scheme, a letter followed by letters, digits,
// '+', '-' or '.', then ':') of at most kMaxHyperlinkTargetBytes bytes, every
// byte printable ASCII other than space (0x21-0x7E). The byte rule is the OSC 8
// convention's own: a URI travels percent-encoded, so a character outside
// ASCII is the application's to encode. It also means no C0 or C1 control, no
// DEL and no spelling of a string terminator can be part of a valid target.
// The scheme rule keeps application-internal link names ("guide.md",
// "#section") off the terminal: a terminal has no base to resolve a relative
// reference against, and hands what it is given to the system to open.
bool is_valid_hyperlink_target(std::string_view target) noexcept;

// The hyperlink targets used by one grid of cells, each under a reference
// count so an entry lives exactly as long as some cell (or a caller's hold)
// uses it. Equal targets share one entry and so one id; a freed id is reused,
// most recently freed first, which keeps numbering a deterministic function of
// the sequence of calls.
class LinkTable {
public:
    // An empty table.
    LinkTable() = default;
    // Copies keep what lets release() free an id without allocating, so a
    // copied table is as usable as the original; moves carry it along.
    LinkTable(const LinkTable& other);
    LinkTable& operator=(const LinkTable& other);
    LinkTable(LinkTable&&) noexcept = default;
    LinkTable& operator=(LinkTable&&) noexcept = default;
    ~LinkTable() = default;

    // Adds one reference to the entry for `target`, creating it when there is
    // none, and returns its id. A target that is not valid
    // (is_valid_hyperlink_target) is refused: the result is kNoLink and the
    // table is unchanged.
    LinkId acquire(std::string_view target);
    // Adds one reference to the live entry `id`. kNoLink is ignored; any other
    // id must be live (asserted).
    void retain(LinkId id) noexcept;
    // Removes one reference from the live entry `id`; the entry is freed when
    // none remain. kNoLink is ignored; any other id must be live (asserted).
    void release(LinkId id) noexcept;

    // The target of `id`, or an empty view for kNoLink. Any other id must be
    // live (asserted). The view stays valid until the entry is freed.
    std::string_view target(LinkId id) const noexcept;
    // Whether `id` names a live entry; false for kNoLink.
    bool contains(LinkId id) const noexcept;
    // The number of live entries.
    std::size_t size() const noexcept { return by_target_.size(); }
    // Whether no entry is live.
    bool empty() const noexcept { return by_target_.empty(); }
    // Frees every entry at once, whatever its references; ids start again
    // at 1. For a grid whose cells are all being replaced by unlinked ones.
    void clear() noexcept;

private:
    struct Entry {
        std::string target;
        std::size_t references = 0;
    };

    // Where `target` is, or would be inserted, in by_target_.
    std::vector<LinkId>::const_iterator lower_bound(std::string_view target) const noexcept;

    std::vector<Entry> entries_;     // entries_[id - 1]
    std::vector<LinkId> free_;       // freed ids, reused last-in first-out
    std::vector<LinkId> by_target_;  // live ids ordered by their target
};

}  // namespace ckv
