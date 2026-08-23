// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Revisioned UTF-8 document model for TextEditor.  The document deliberately
// has no View, terminal, filesystem, clock, or syntax-profile dependency: it
// is the reusable editing core described by the editor architecture plan.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::widgets {

using DocumentRevision = std::uint64_t;

struct DocumentPosition {
    DocumentRevision revision = 0;
    std::size_t byte = 0;

    friend bool operator==(const DocumentPosition&, const DocumentPosition&) = default;
};

struct DocumentRange {
    DocumentPosition begin;
    DocumentPosition end;

    friend bool operator==(const DocumentRange&, const DocumentRange&) = default;
};

struct DocumentLineColumn {
    std::size_t line = 0;
    std::size_t column = 0;  // grapheme column, not bytes or terminal cells

    friend bool operator==(const DocumentLineColumn&, const DocumentLineColumn&) = default;
};

enum class InvalidUtf8Policy {
    Reject,
    Replace,
};

enum class DocumentNewline {
    Lf,
    Crlf,
    Cr,
};

// ckVision documents are UTF-8 internally. A UTF-8 BOM is transport metadata,
// not editable document text, and is retained so a file controller can make an
// explicit, lossless save decision.
enum class DocumentEncoding {
    Utf8,
};

struct EditorDocumentOptions {
    // Loading malformed bytes must be an explicit policy choice. The safe
    // default reports an error rather than silently changing source text.
    InvalidUtf8Policy invalid_utf8 = InvalidUtf8Policy::Reject;
    // Zero means no document-size limit. The limit is validated before a
    // transaction mutates the persistent piece tree.
    std::size_t max_document_bytes = 0;
    std::size_t max_undo_entries = 256;
    std::size_t max_undo_bytes = 16U * 1024U * 1024U;
};

enum class DocumentEditStatus {
    Ok,
    StaleRevision,
    InvalidRange,
    InvalidUtf8,
    LimitExceeded,
};

struct DocumentChange {
    DocumentRevision previous_revision = 0;
    DocumentRevision revision = 0;
    std::size_t replaced_begin_byte = 0;
    std::size_t replaced_end_byte = 0;
    std::size_t inserted_bytes = 0;
    std::size_t first_affected_line = 0;
    std::size_t last_affected_line = 0;
};

struct DocumentEditResult {
    DocumentEditStatus status = DocumentEditStatus::Ok;
    std::optional<DocumentChange> change;

    explicit operator bool() const noexcept { return status == DocumentEditStatus::Ok; }
};

struct DocumentTextEdit {
    DocumentRange range;
    std::string text;
};

// An instance-owned edit buffer. All ranges are against one base revision;
// commit() validates them before changing the document and advances the
// revision exactly once.
class DocumentTransaction {
public:
    explicit DocumentTransaction(DocumentRevision base_revision) : base_revision_(base_revision) {}

    DocumentRevision base_revision() const noexcept { return base_revision_; }
    void replace(DocumentRange range, std::string text);
    const std::vector<DocumentTextEdit>& edits() const noexcept { return edits_; }
    bool empty() const noexcept { return edits_.empty(); }

private:
    DocumentRevision base_revision_;
    std::vector<DocumentTextEdit> edits_;
};

// A persistent piece-tree document. Original text and inserted text are kept
// immutable; edits replace only O(log pieces) tree nodes and undo/redo retain
// prior roots. Positions are revision-bound, so a stale byte offset can never
// accidentally edit changed text.
class EditorDocument {
public:
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(const DocumentChange&)>;

    // Opaque persistent tree handle. It is public only so the implementation's
    // value helpers can remain allocation-free; clients cannot construct or
    // inspect a Node and the document never exposes this handle in its API.
    struct Node;
    using NodePtr = std::shared_ptr<const Node>;

    explicit EditorDocument(std::string initial_text = {}, EditorDocumentOptions options = {});
    ~EditorDocument();

    EditorDocument(const EditorDocument&) = delete;
    EditorDocument& operator=(const EditorDocument&) = delete;
    EditorDocument(EditorDocument&&) noexcept;
    EditorDocument& operator=(EditorDocument&&) noexcept;

    const EditorDocumentOptions& options() const noexcept { return options_; }
    DocumentRevision revision() const noexcept { return revision_; }
    bool modified() const noexcept { return revision_ != clean_revision_; }
    void mark_clean() noexcept { clean_revision_ = revision_; }

    DocumentNewline preferred_newline() const noexcept { return preferred_newline_; }
    void set_preferred_newline(DocumentNewline newline) noexcept { preferred_newline_ = newline; }
    DocumentEncoding encoding() const noexcept { return DocumentEncoding::Utf8; }
    bool has_utf8_bom() const noexcept { return utf8_bom_; }
    void set_utf8_bom(bool present) noexcept { utf8_bom_ = present; }

    std::size_t byte_size() const noexcept;
    std::size_t line_count() const noexcept;
    std::string text() const;

    DocumentPosition begin() const noexcept { return DocumentPosition{revision_, 0}; }
    DocumentPosition end() const noexcept { return DocumentPosition{revision_, byte_size()}; }
    std::optional<DocumentPosition> position_at_byte(std::size_t byte) const;
    std::optional<DocumentPosition> position_at_line_column(std::size_t line, std::size_t grapheme_column) const;
    std::optional<DocumentLineColumn> line_column(DocumentPosition position) const;
    std::string text(DocumentRange range) const;

    DocumentTransaction transaction() const { return DocumentTransaction{revision_}; }
    DocumentEditResult replace(DocumentRange range, std::string text);
    DocumentEditResult commit(DocumentTransaction transaction);
    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }
    bool undo();
    bool redo();
    void clear_history();

    // Replaces all text and clears history. Input is normalized to LF. With a
    // Reject policy this returns InvalidUtf8 and leaves the document untouched.
    DocumentEditStatus set_text(std::string text);
    // Explicit callers such as a file controller may choose a one-shot input
    // policy without changing the document's normal edit policy.
    DocumentEditStatus set_text(std::string text, InvalidUtf8Policy policy);

    ObserverId subscribe(Observer observer);
    void unsubscribe(ObserverId observer) noexcept;

private:
    struct HistoryEntry;

    DocumentEditResult commit_edits(const std::vector<DocumentTextEdit>& edits, DocumentRevision base_revision,
                                    bool record_history);
    bool range_is_current_and_valid(DocumentRange range) const;
    void notify(const DocumentChange& change);

    EditorDocumentOptions options_;
    std::string original_;
    std::string additions_;
    NodePtr root_;
    DocumentRevision revision_ = 1;
    DocumentRevision clean_revision_ = 1;
    DocumentNewline preferred_newline_ = DocumentNewline::Lf;
    bool utf8_bom_ = false;
    std::vector<HistoryEntry> undo_;
    std::vector<HistoryEntry> redo_;
    std::size_t undo_bytes_ = 0;
    ObserverId next_observer_id_ = 1;
    std::vector<std::pair<ObserverId, Observer>> observers_;
};

}  // namespace ckv::widgets
