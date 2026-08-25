// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/editor_document.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"

namespace ckv::widgets {
namespace {

enum class Store { Original, Addition };

struct Piece {
    Store store = Store::Original;
    std::size_t offset = 0;
    std::size_t length = 0;
    std::size_t newlines = 0;
};

struct NormalizedText {
    std::string text;
    DocumentNewline newline = DocumentNewline::Lf;
    bool utf8_bom = false;
};

std::optional<NormalizedText> normalize(std::string_view value, InvalidUtf8Policy policy) {
    if (policy == InvalidUtf8Policy::Reject && !utf8::is_valid(value)) return std::nullopt;
    NormalizedText result;
    result.text.reserve(value.size());
    std::optional<DocumentNewline> first_newline;
    std::size_t position = 0;
    if (value.size() >= 3U && static_cast<unsigned char>(value[0]) == 0xEFU &&
        static_cast<unsigned char>(value[1]) == 0xBBU && static_cast<unsigned char>(value[2]) == 0xBFU) {
        result.utf8_bom = true;
        position = 3U;
    }
    for (; position < value.size();) {
        const std::size_t start = position;
        const char32_t codepoint = utf8::decode(value, position);
        const std::string_view encoded = value.substr(start, position - start);
        if (codepoint == utf8::replacement_char && !utf8::is_valid(encoded)) {
            utf8::encode(utf8::replacement_char, result.text);
            continue;
        }
        if (codepoint == U'\r') {
            if (position < value.size() && value[position] == '\n') {
                ++position;
                if (!first_newline) first_newline = DocumentNewline::Crlf;
            } else {
                if (!first_newline) first_newline = DocumentNewline::Cr;
            }
            result.text.push_back('\n');
        } else {
            if (codepoint == U'\n' && !first_newline) first_newline = DocumentNewline::Lf;
            result.text.append(encoded);
        }
    }
    result.newline = first_newline.value_or(DocumentNewline::Lf);
    return result;
}

std::size_t count_newlines(std::string_view value) noexcept {
    return static_cast<std::size_t>(std::count(value.begin(), value.end(), '\n'));
}

std::uint32_t priority_for(Piece piece) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const std::array<std::uint64_t, 4> values = {static_cast<std::uint64_t>(piece.store == Store::Original),
                                                  static_cast<std::uint64_t>(piece.offset),
                                                  static_cast<std::uint64_t>(piece.length),
                                                  static_cast<std::uint64_t>(piece.newlines)};
    for (const std::uint64_t value : values) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return static_cast<std::uint32_t>(hash ^ (hash >> 32U));
}

}  // namespace

struct EditorDocument::Node {
    Piece piece;
    std::uint32_t priority = 0;
    NodePtr left;
    NodePtr right;
    std::size_t bytes = 0;
    std::size_t newlines = 0;

    Node(Piece value, std::uint32_t node_priority, NodePtr left_child, NodePtr right_child)
        : piece(value), priority(node_priority), left(std::move(left_child)), right(std::move(right_child)) {
        bytes = piece.length + (left ? left->bytes : 0U) + (right ? right->bytes : 0U);
        newlines = piece.newlines + (left ? left->newlines : 0U) + (right ? right->newlines : 0U);
    }
};

struct EditorDocument::HistoryEntry {
    NodePtr before;
    NodePtr after;
    std::size_t retained_bytes = 0;
};

namespace {

using NodePtr = EditorDocument::NodePtr;

std::size_t bytes(const NodePtr& node) noexcept { return node ? node->bytes : 0U; }

NodePtr make_node(Piece piece, std::uint32_t priority, NodePtr left = {}, NodePtr right = {}) {
    return std::make_shared<const EditorDocument::Node>(piece, priority, std::move(left), std::move(right));
}

NodePtr merge(NodePtr left, NodePtr right) {
    if (!left) return right;
    if (!right) return left;
    if (left->priority <= right->priority) {
        return make_node(left->piece, left->priority, left->left, merge(left->right, std::move(right)));
    }
    return make_node(right->piece, right->priority, merge(std::move(left), right->left), right->right);
}

std::size_t piece_newlines(const Piece& piece, const std::string& original, const std::string& additions) {
    const std::string& storage = piece.store == Store::Original ? original : additions;
    return count_newlines(std::string_view(storage).substr(piece.offset, piece.length));
}

std::pair<NodePtr, NodePtr> split(NodePtr root, std::size_t offset, const std::string& original,
                                  const std::string& additions) {
    if (!root) return {};
    const std::size_t left_bytes = bytes(root->left);
    if (offset < left_bytes) {
        auto [before, after] = split(root->left, offset, original, additions);
        return {before, make_node(root->piece, root->priority, std::move(after), root->right)};
    }
    if (offset > left_bytes + root->piece.length) {
        auto [before, after] = split(root->right, offset - left_bytes - root->piece.length, original, additions);
        return {make_node(root->piece, root->priority, root->left, std::move(before)), after};
    }
    const std::size_t in_piece = offset - left_bytes;
    if (in_piece == 0U) return {root->left, make_node(root->piece, root->priority, {}, root->right)};
    if (in_piece == root->piece.length) return {make_node(root->piece, root->priority, root->left, {}), root->right};

    Piece first = root->piece;
    first.length = in_piece;
    Piece second = root->piece;
    second.offset += in_piece;
    second.length -= in_piece;
    // Callers only split at validated grapheme boundaries, so each part remains
    // valid UTF-8. Newline counts stay exact for the augmented line index.
    first.newlines = piece_newlines(first, original, additions);
    second.newlines = piece_newlines(second, original, additions);
    return {make_node(first, priority_for(first), root->left, {}),
            make_node(second, priority_for(second), {}, root->right)};
}

void append_text(const NodePtr& node, const std::string& original, const std::string& additions, std::string& out) {
    if (!node) return;
    append_text(node->left, original, additions, out);
    const std::string& storage = node->piece.store == Store::Original ? original : additions;
    out.append(storage, node->piece.offset, node->piece.length);
    append_text(node->right, original, additions, out);
}

void append_range(const NodePtr& node, const std::string& original, const std::string& additions, std::size_t begin,
                  std::size_t end, std::size_t base, std::string& out) {
    if (!node || begin >= end) return;
    const std::size_t left_bytes = bytes(node->left);
    const std::size_t piece_begin = base + left_bytes;
    const std::size_t piece_end = piece_begin + node->piece.length;
    if (begin < piece_begin) append_range(node->left, original, additions, begin, end, base, out);
    if (begin < piece_end && end > piece_begin) {
        const std::size_t local_begin = std::max(begin, piece_begin) - piece_begin;
        const std::size_t local_end = std::min(end, piece_end) - piece_begin;
        const std::string& storage = node->piece.store == Store::Original ? original : additions;
        out.append(storage, node->piece.offset + local_begin, local_end - local_begin);
    }
    if (end > piece_end)
        append_range(node->right, original, additions, begin, end, piece_end, out);
}

bool is_grapheme_boundary(std::string_view value, std::size_t byte) {
    if (byte == 0U || byte == value.size()) return true;
    for (std::size_t cursor = 0; cursor < value.size();) {
        cursor = text::grapheme_end(value, cursor);
        if (cursor == byte) return true;
        if (cursor > byte) return false;
    }
    return false;
}

std::size_t newlines_before(const NodePtr& node, std::size_t offset, const std::string& original,
                            const std::string& additions) {
    if (!node || offset == 0U) return 0U;
    const std::size_t left_bytes = bytes(node->left);
    if (offset <= left_bytes) return newlines_before(node->left, offset, original, additions);
    const std::size_t left_newlines = node->left ? node->left->newlines : 0U;
    const std::size_t within_piece = std::min(offset - left_bytes, node->piece.length);
    const std::string& storage = node->piece.store == Store::Original ? original : additions;
    const std::size_t piece_newlines = count_newlines(std::string_view(storage).substr(node->piece.offset, within_piece));
    if (offset <= left_bytes + node->piece.length) return left_newlines + piece_newlines;
    return left_newlines + node->piece.newlines +
           newlines_before(node->right, offset - left_bytes - node->piece.length, original, additions);
}

std::size_t newline_offset(const NodePtr& node, std::size_t index, std::size_t base, const std::string& original,
                           const std::string& additions) {
    const std::size_t left_newlines = node->left ? node->left->newlines : 0U;
    const std::size_t left_bytes = bytes(node->left);
    if (index < left_newlines) return newline_offset(node->left, index, base, original, additions);
    index -= left_newlines;
    base += left_bytes;
    if (index < node->piece.newlines) {
        const std::string& storage = node->piece.store == Store::Original ? original : additions;
        const std::string_view piece(storage.data() + node->piece.offset, node->piece.length);
        for (std::size_t byte = 0; byte < piece.size(); ++byte)
            if (piece[byte] == '\n' && index-- == 0U) return base + byte;
    }
    return newline_offset(node->right, index - node->piece.newlines, base + node->piece.length, original, additions);
}

std::optional<std::pair<std::size_t, std::size_t>> line_bounds(const NodePtr& root, std::size_t line,
                                                                 const std::string& original, const std::string& additions) {
    const std::size_t newline_count = root ? root->newlines : 0U;
    if (line > newline_count) return std::nullopt;
    const std::size_t begin = line == 0U ? 0U : newline_offset(root, line - 1U, 0U, original, additions) + 1U;
    const std::size_t end = line == newline_count ? bytes(root) : newline_offset(root, line, 0U, original, additions);
    return std::pair{begin, end};
}

std::string line_text(const NodePtr& root, std::size_t begin, std::size_t end, const std::string& original,
                      const std::string& additions) {
    std::string result;
    result.reserve(end - begin);
    append_range(root, original, additions, begin, end, 0U, result);
    return result;
}

}  // namespace

void DocumentTransaction::replace(DocumentRange range, std::string text) {
    edits_.push_back(DocumentTextEdit{range, std::move(text)});
}

EditorDocument::EditorDocument(std::string initial_text, EditorDocumentOptions options) : options_(options) {
    (void)set_text(std::move(initial_text));
    revision_ = 1;
    clean_revision_ = 1;
}

EditorDocument::~EditorDocument() = default;
EditorDocument::EditorDocument(EditorDocument&&) noexcept = default;
EditorDocument& EditorDocument::operator=(EditorDocument&&) noexcept = default;

std::size_t EditorDocument::byte_size() const noexcept { return root_ ? root_->bytes : 0U; }
std::size_t EditorDocument::line_count() const noexcept { return (root_ ? root_->newlines : 0U) + 1U; }

std::string EditorDocument::text() const {
    std::string result;
    result.reserve(byte_size());
    append_text(root_, original_, additions_, result);
    return result;
}

std::optional<DocumentPosition> EditorDocument::position_at_byte(std::size_t byte) const {
    if (byte > byte_size()) return std::nullopt;
    const auto bounds = line_bounds(root_, newlines_before(root_, byte, original_, additions_), original_, additions_);
    if (!bounds || !is_grapheme_boundary(line_text(root_, bounds->first, bounds->second, original_, additions_), byte - bounds->first))
        return std::nullopt;
    return DocumentPosition{revision_, byte};
}

std::optional<DocumentPosition> EditorDocument::position_at_line_column(std::size_t line, std::size_t grapheme_column) const {
    const auto bounds = line_bounds(root_, line, original_, additions_);
    if (!bounds) return std::nullopt;
    const std::string value = line_text(root_, bounds->first, bounds->second, original_, additions_);
    std::size_t byte = 0U;
    for (std::size_t column = 0; column < grapheme_column; ++column) {
        if (byte >= value.size()) return std::nullopt;
        byte = text::grapheme_end(value, byte);
    }
    return DocumentPosition{revision_, bounds->first + byte};
}

std::optional<DocumentLineColumn> EditorDocument::line_column(DocumentPosition position) const {
    if (position.revision != revision_ || position.byte > byte_size()) return std::nullopt;
    DocumentLineColumn result;
    result.line = newlines_before(root_, position.byte, original_, additions_);
    const auto bounds = line_bounds(root_, result.line, original_, additions_);
    if (!bounds) return std::nullopt;
    const std::string value = line_text(root_, bounds->first, bounds->second, original_, additions_);
    const std::size_t local = position.byte - bounds->first;
    if (!is_grapheme_boundary(value, local)) return std::nullopt;
    for (std::size_t cursor = 0; cursor < local; cursor = text::grapheme_end(value, cursor)) ++result.column;
    return result;
}

std::string EditorDocument::text(DocumentRange range) const {
    if (!range_is_current_and_valid(range)) return {};
    std::string result;
    result.reserve(range.end.byte - range.begin.byte);
    append_range(root_, original_, additions_, range.begin.byte, range.end.byte, 0, result);
    return result;
}

bool EditorDocument::range_is_current_and_valid(DocumentRange range) const {
    if (range.begin.revision != revision_ || range.end.revision != revision_ || range.begin.byte > range.end.byte ||
        range.end.byte > byte_size())
        return false;
    return position_at_byte(range.begin.byte).has_value() && position_at_byte(range.end.byte).has_value();
}

DocumentEditResult EditorDocument::replace(DocumentRange range, std::string value) {
    auto transaction_value = transaction();
    transaction_value.replace(range, std::move(value));
    return commit(std::move(transaction_value));
}

DocumentEditResult EditorDocument::commit(DocumentTransaction transaction_value) {
    return commit_edits(transaction_value.edits(), transaction_value.base_revision(), true);
}

DocumentEditResult EditorDocument::commit_edits(const std::vector<DocumentTextEdit>& edits, DocumentRevision base_revision,
                                                 bool record_history) {
    if (base_revision != revision_) return DocumentEditResult{DocumentEditStatus::StaleRevision, std::nullopt};
    if (edits.empty()) return DocumentEditResult{};

    struct PreparedEdit {
        DocumentRange range;
        std::string text;
    };
    std::vector<PreparedEdit> prepared;
    prepared.reserve(edits.size());
    for (const DocumentTextEdit& edit : edits) {
        if (edit.range.begin.revision != revision_ || edit.range.end.revision != revision_)
            return DocumentEditResult{DocumentEditStatus::StaleRevision, std::nullopt};
        if (!range_is_current_and_valid(edit.range)) return DocumentEditResult{DocumentEditStatus::InvalidRange, std::nullopt};
        const auto normalized = normalize(edit.text, options_.invalid_utf8);
        if (!normalized) return DocumentEditResult{DocumentEditStatus::InvalidUtf8, std::nullopt};
        prepared.push_back(PreparedEdit{edit.range, normalized->text});
    }
    std::sort(prepared.begin(), prepared.end(), [](const PreparedEdit& left, const PreparedEdit& right) {
        return left.range.begin.byte > right.range.begin.byte;
    });
    for (std::size_t index = 1; index < prepared.size(); ++index) {
        if (prepared[index - 1].range.begin.byte < prepared[index].range.end.byte)
            return DocumentEditResult{DocumentEditStatus::InvalidRange, std::nullopt};
    }

    std::size_t removed = 0;
    std::size_t inserted_total = 0;
    for (const PreparedEdit& edit : prepared) {
        removed += edit.range.end.byte - edit.range.begin.byte;
        inserted_total += edit.text.size();
    }
    const std::size_t retained = byte_size() - removed;
    if (options_.max_document_bytes != 0U &&
        (retained > options_.max_document_bytes || inserted_total > options_.max_document_bytes - retained))
        return DocumentEditResult{DocumentEditStatus::LimitExceeded, std::nullopt};

    const NodePtr before = root_;
    const std::size_t first = prepared.back().range.begin.byte;
    const std::size_t last = prepared.front().range.end.byte;
    const auto first_line = line_column(DocumentPosition{revision_, first});
    const auto last_line = line_column(DocumentPosition{revision_, last});
    std::size_t inserted = 0;

    for (const PreparedEdit& edit : prepared) {
        auto [prefix, tail] = split(root_, edit.range.begin.byte, original_, additions_);
        auto [discarded, suffix] =
            split(std::move(tail), edit.range.end.byte - edit.range.begin.byte, original_, additions_);
        (void)discarded;
        NodePtr insertion;
        if (!edit.text.empty()) {
            const Piece piece{Store::Addition, additions_.size(), edit.text.size(), count_newlines(edit.text)};
            additions_ += edit.text;
            insertion = make_node(piece, priority_for(piece));
            inserted += edit.text.size();
        }
        root_ = merge(merge(std::move(prefix), std::move(insertion)), std::move(suffix));
    }

    const DocumentRevision previous = revision_;
    ++revision_;
    if (record_history) {
        redo_.clear();
        undo_.push_back(HistoryEntry{before, root_, byte_size()});
        undo_bytes_ += byte_size();
        while (!undo_.empty() && (undo_.size() > options_.max_undo_entries || undo_bytes_ > options_.max_undo_bytes)) {
            undo_bytes_ -= undo_.front().retained_bytes;
            undo_.erase(undo_.begin());
        }
    }
    DocumentChange change{previous, revision_, first, last, inserted, first_line ? first_line->line : 0U,
                          last_line ? last_line->line : 0U};
    notify(change);
    return DocumentEditResult{DocumentEditStatus::Ok, change};
}

bool EditorDocument::undo() {
    if (undo_.empty()) return false;
    HistoryEntry entry = undo_.back();
    undo_.pop_back();
    undo_bytes_ -= entry.retained_bytes;
    const DocumentRevision previous = revision_;
    root_ = entry.before;
    ++revision_;
    redo_.push_back(std::move(entry));
    notify(DocumentChange{previous, revision_, 0, byte_size(), byte_size(), 0, line_count() - 1U});
    return true;
}

bool EditorDocument::redo() {
    if (redo_.empty()) return false;
    HistoryEntry entry = redo_.back();
    redo_.pop_back();
    const DocumentRevision previous = revision_;
    root_ = entry.after;
    ++revision_;
    undo_bytes_ += entry.retained_bytes;
    undo_.push_back(std::move(entry));
    notify(DocumentChange{previous, revision_, 0, byte_size(), byte_size(), 0, line_count() - 1U});
    return true;
}

void EditorDocument::clear_history() {
    undo_.clear();
    redo_.clear();
    undo_bytes_ = 0;
}

DocumentEditStatus EditorDocument::set_text(std::string value) { return set_text(std::move(value), options_.invalid_utf8); }

DocumentEditStatus EditorDocument::set_text(std::string value, InvalidUtf8Policy policy) {
    const auto normalized = normalize(value, policy);
    if (!normalized) return DocumentEditStatus::InvalidUtf8;
    if (options_.max_document_bytes != 0U && normalized->text.size() > options_.max_document_bytes)
        return DocumentEditStatus::LimitExceeded;
    const std::size_t previous_bytes = byte_size();
    const std::size_t previous_lines = line_count();
    original_ = normalized->text;
    additions_.clear();
    preferred_newline_ = normalized->newline;
    utf8_bom_ = normalized->utf8_bom;
    if (original_.empty()) {
        root_.reset();
    } else {
        const Piece piece{Store::Original, 0, original_.size(), count_newlines(original_)};
        root_ = make_node(piece, priority_for(piece));
    }
    clear_history();
    const DocumentRevision previous = revision_;
    ++revision_;
    notify(DocumentChange{previous, revision_, 0, previous_bytes, byte_size(), 0, std::max(previous_lines, line_count()) - 1U});
    return DocumentEditStatus::Ok;
}

EditorDocument::ObserverId EditorDocument::subscribe(Observer observer) {
    const ObserverId identifier = next_observer_id_++;
    observers_.push_back({identifier, std::move(observer)});
    return identifier;
}

void EditorDocument::unsubscribe(ObserverId observer) noexcept {
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(), [observer](const auto& candidate) {
                         return candidate.first == observer;
                     }),
                     observers_.end());
}

void EditorDocument::notify(const DocumentChange& change) {
    const auto observers = observers_;
    for (const auto& [identifier, observer] : observers) {
        (void)identifier;
        if (observer) observer(change);
    }
}

}  // namespace ckv::widgets
