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

// A document's version number. A new document is at revision 1, and every change that is
// applied (an edit, a transaction, an undo or redo, a set_text) advances it by exactly one; it
// never returns to an earlier value, so a position stamped with it names bytes of exactly one
// text.
using DocumentRevision = std::uint64_t;

// A byte offset into the document's text as it stood at `revision`. EditorDocument hands out
// positions only on grapheme-cluster boundaries (begin, end, position_at_byte,
// position_at_line_column). Once the document has moved on, the position is stale and every
// EditorDocument call refuses it rather than reinterpreting it against the new text.
struct DocumentPosition {
    // The revision the offset belongs to, and the offset in bytes into that revision's
    // normalized UTF-8 text (LF line breaks, no byte-order mark).
    DocumentRevision revision = 0;
    std::size_t byte = 0;

    // Memberwise: equal only when both the revision and the byte agree.
    friend bool operator==(const DocumentPosition&, const DocumentPosition&) = default;
};

// A half-open byte range of one revision. It is usable for an edit or a read when both ends
// carry the current revision, begin is not after end, and both lie on grapheme boundaries
// inside the text.
struct DocumentRange {
    // The first byte covered and the byte just past the last; equal ends describe an empty
    // range, which is an insertion point.
    DocumentPosition begin;
    DocumentPosition end;

    // Memberwise equality of both ends.
    friend bool operator==(const DocumentRange&, const DocumentRange&) = default;
};

// A position as a logical line and a column within it. Lines are separated by LF only; the
// rows a view wraps a line into are not lines.
struct DocumentLineColumn {
    // Both zero-based: `line` counts the line breaks before the position, and `column` counts
    // the grapheme clusters that precede it on its line.
    std::size_t line = 0;
    std::size_t column = 0;  // grapheme column, not bytes or terminal cells

    // Memberwise equality.
    friend bool operator==(const DocumentLineColumn&, const DocumentLineColumn&) = default;
};

// What EditorDocument does with bytes that are not well-formed UTF-8, whether they arrive
// through set_text or in the text of an edit.
enum class InvalidUtf8Policy {
    // Refuse the whole input: set_text and the edit fail with InvalidUtf8 and nothing changes.
    Reject,
    // Accept the input with every byte that does not begin a well-formed sequence replaced by
    // U+FFFD REPLACEMENT CHARACTER.
    Replace,
};

// A file's line-break convention. The document text itself always uses LF; this records the
// convention the file used so that a save can write it back. set_text records the first line
// break its input contains, and LF when it contains none.
enum class DocumentNewline {
    // LF, CR LF, and a lone CR.
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

// The input policy and limits an EditorDocument is constructed with. options() reads them back;
// nothing changes them afterwards.
struct EditorDocumentOptions {
    // Loading malformed bytes must be an explicit policy choice. The safe
    // default reports an error rather than silently changing source text.
    InvalidUtf8Policy invalid_utf8 = InvalidUtf8Policy::Reject;
    // Zero means no document-size limit. The limit is validated before a
    // transaction mutates the persistent piece tree.
    std::size_t max_document_bytes = 0;
    // History limits. After each recorded change the oldest undo steps are dropped while there
    // are more than max_undo_entries of them, or while the sum over the steps of the document's
    // size after each exceeds max_undo_bytes. That sum is a budget in document bytes, not the
    // memory actually retained, since steps share unchanged text. Zero entries keeps no undo
    // history at all.
    std::size_t max_undo_entries = 256;
    std::size_t max_undo_bytes = 16U * 1024U * 1024U;
};

// Why replace, commit or set_text did not apply. Any status but Ok leaves the text, the revision
// and the history exactly as they were.
enum class DocumentEditStatus {
    // Applied, or there was nothing to apply.
    Ok,
    // The transaction, or a range in it, belongs to an older revision.
    StaleRevision,
    // A range is reversed, reaches past the end, does not lie on grapheme boundaries, or
    // overlaps another range of the same transaction.
    InvalidRange,
    // Text to insert or load is malformed UTF-8 under the Reject policy.
    InvalidUtf8,
    // The result would be larger than max_document_bytes.
    LimitExceeded,
};

// A selection as byte offsets into the text of one revision: the anchor, where the selection
// began, and the caret, its moving end. Equal offsets describe a caret without a selection.
// Unlike a DocumentPosition it names no revision, because it is history metadata: it is stored
// with an undo step and handed back for the text that step restores.
struct DocumentSelection {
    // Where the selection began, and its moving end.
    std::size_t anchor_byte = 0;
    std::size_t caret_byte = 0;

    // Memberwise equality.
    friend bool operator==(const DocumentSelection&, const DocumentSelection&) = default;
};

// One change, described as a single replacement covering everything it
// touched: the old bytes [replaced_begin_byte, replaced_end_byte) became the
// `inserted_bytes` bytes that now start at replaced_begin_byte. A transaction
// of several separated edits is reported as the one span from its first edit
// to its last; an undo or a redo as that same span of the transaction it
// reverts or replays; a set_text as the whole document. An observer can
// therefore always carry a position through a change with the same arithmetic.
struct DocumentChange {
    // The revision before the change and the one it produced, always one more.
    DocumentRevision previous_revision = 0;
    DocumentRevision revision = 0;
    // The covering replacement described above: a byte range of the old text and the length
    // in bytes of what now stands in its place.
    std::size_t replaced_begin_byte = 0;
    std::size_t replaced_end_byte = 0;
    std::size_t inserted_bytes = 0;
    // Zero-based lines of the old text. For an edit, a transaction, an undo or a redo they are
    // the lines of replaced_begin_byte and replaced_end_byte; for a set_text they are 0 and the
    // larger of the old and the new last line.
    std::size_t first_affected_line = 0;
    std::size_t last_affected_line = 0;
    // For an undo or a redo only, the selection the view that asked for the step restores, as
    // bytes of the new text: after an undo, the selection recorded with the reverted
    // transaction (DocumentTransaction::set_selection_before) or, when none was, a caret after
    // the restored text; after a redo, a caret after the replayed text. Other views of the
    // document carry their own selections through the change instead. Empty for every other
    // change.
    std::optional<DocumentSelection> selection;

    // Memberwise equality.
    friend bool operator==(const DocumentChange&, const DocumentChange&) = default;
};

// The outcome of EditorDocument::replace or commit.
struct DocumentEditResult {
    // Ok or the reason for refusal, and the change made. `change` is empty whenever nothing
    // changed: on every refusal, and for an Ok commit of an empty transaction.
    DocumentEditStatus status = DocumentEditStatus::Ok;
    std::optional<DocumentChange> change;

    // True for Ok, whether or not anything changed.
    explicit operator bool() const noexcept { return status == DocumentEditStatus::Ok; }
};

// One replacement queued in a DocumentTransaction: the text of `range` becomes `text`. On commit
// the text's line breaks are normalized as set_text input's are (CR LF and lone CR become LF) and
// malformed UTF-8 follows the document's policy. Unlike set_text input, a leading U+FEFF is kept:
// in an edit it is a ZERO WIDTH NO-BREAK SPACE, not a byte-order mark.
struct DocumentTextEdit {
    // A range of the transaction's base revision, empty to insert, and its replacement.
    DocumentRange range;
    std::string text;
};

// An instance-owned edit buffer. All ranges are against one base revision;
// commit() validates them before changing the document and advances the
// revision exactly once.
class DocumentTransaction {
public:
    // An empty transaction against `base_revision`. EditorDocument::transaction() is the usual
    // way to obtain one for the current revision.
    explicit DocumentTransaction(DocumentRevision base_revision) : base_revision_(base_revision) {}

    // The revision every queued range must belong to.
    DocumentRevision base_revision() const noexcept { return base_revision_; }
    // Queues one replacement without validating it. Every range refers to the base revision's
    // text, not to the text earlier queued edits would leave, so the edits apply as if at once;
    // ranges must not overlap, though they may touch. Where edits touch, the result is fixed
    // regardless of the order they were queued in, except among insertions at one position:
    // those land in the order they were queued, and all of them ahead of a replacement that
    // begins at that position.
    void replace(DocumentRange range, std::string text);
    // The queued replacements, in the order they were queued.
    const std::vector<DocumentTextEdit>& edits() const noexcept { return edits_; }
    // The selection the editing view had before this transaction, as bytes of the base
    // revision's text. It is kept with the transaction's undo step, and an undo hands it back
    // (see DocumentChange::selection). Both offsets must lie on grapheme boundaries inside that
    // text, or commit refuses the transaction with InvalidRange. None by default.
    void set_selection_before(DocumentSelection selection) noexcept { selection_before_ = selection; }
    const std::optional<DocumentSelection>& selection_before() const noexcept { return selection_before_; }
    // Whether nothing is queued. Committing an empty transaction of the current revision returns
    // Ok and changes nothing.
    bool empty() const noexcept { return edits_.empty(); }

private:
    DocumentRevision base_revision_;
    std::vector<DocumentTextEdit> edits_;
    std::optional<DocumentSelection> selection_before_;
};

// A persistent piece-tree document. Original text and inserted text are kept
// immutable; edits replace only O(log pieces) tree nodes and undo/redo retain
// prior roots. Positions are revision-bound, so a stale byte offset can never
// accidentally edit changed text.
class EditorDocument {
public:
    // A subscription's identity, and the callbacks a subscription holds. Ids start at 1, are
    // shared by both kinds of subscription, and a document never reuses one, so 0 is free to
    // mean "no subscription". An Observer is called synchronously after every applied change
    // of the text (an edit, a commit, an undo, a redo, a set_text) with that change; never for
    // a refused one. A StateObserver is called synchronously after mark_clean,
    // set_preferred_newline or set_utf8_bom changes what modified(), preferred_newline() or
    // has_utf8_bom() report; those change no text and never reach an Observer.
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(const DocumentChange&)>;
    using StateObserver = std::function<void()>;

    // Opaque persistent tree handle. It is public only so the implementation's
    // value helpers can remain allocation-free; clients cannot construct or
    // inspect a Node and the document never exposes this handle in its API.
    struct Node;
    using NodePtr = std::shared_ptr<const Node>;

    // Loads `initial_text` as set_text would (line breaks normalized to LF, the newline
    // convention and a leading BOM recorded) and starts at revision 1, unmodified, with no
    // history. Text the options refuse (malformed UTF-8 under Reject, or more than
    // max_document_bytes) leaves the document empty instead; the constructor reports nothing,
    // so a caller that needs to know loads through set_text.
    explicit EditorDocument(std::string initial_text = {}, EditorDocumentOptions options = {});
    // Destruction does not call the observers.
    ~EditorDocument();

    // Not copyable: a document is shared, by shared_ptr, between the views and controllers that
    // work on it. Moving transfers the whole state, including the revision, the history and
    // the subscriptions.
    EditorDocument(const EditorDocument&) = delete;
    EditorDocument& operator=(const EditorDocument&) = delete;
    EditorDocument(EditorDocument&&) noexcept;
    EditorDocument& operator=(EditorDocument&&) noexcept;

    // The options the document was constructed with.
    const EditorDocumentOptions& options() const noexcept { return options_; }
    // The current revision; see DocumentRevision.
    DocumentRevision revision() const noexcept { return revision_; }
    // modified() is true whenever the revision differs from the one mark_clean() last recorded
    // (construction records the first). Every applied change counts, including set_text and an
    // undo back to the recorded text, because revisions never return to an earlier value.
    // mark_clean() notifies the state observers when the document was modified.
    bool modified() const noexcept { return revision_ != clean_revision_; }
    void mark_clean();

    // File-format metadata that travels with the text without being part of it: the line-break
    // convention and whether the file began with a UTF-8 byte-order mark. set_text records both
    // from its input; the setters change them without touching the text, the revision, the
    // modified flag or the history, and notify the state observers when the value changes.
    // FileEditorController applies them when it saves. encoding() is always Utf8.
    DocumentNewline preferred_newline() const noexcept { return preferred_newline_; }
    void set_preferred_newline(DocumentNewline newline);
    DocumentEncoding encoding() const noexcept { return DocumentEncoding::Utf8; }
    bool has_utf8_bom() const noexcept { return utf8_bom_; }
    void set_utf8_bom(bool present);

    // The size of the text in bytes (normalized, BOM excluded); its number of logical lines,
    // which is its LF count plus one, so an empty document has one line and a trailing LF adds
    // an empty last line; and a copy of the whole text.
    std::size_t byte_size() const noexcept;
    std::size_t line_count() const noexcept;
    std::string text() const;

    // Positions at the start and at the end of the text, of the current revision.
    DocumentPosition begin() const noexcept { return DocumentPosition{revision_, 0}; }
    DocumentPosition end() const noexcept { return DocumentPosition{revision_, byte_size()}; }
    // A current position at `byte`; nullopt when it lies past the end or inside a grapheme
    // cluster.
    std::optional<DocumentPosition> position_at_byte(std::size_t byte) const;
    // The position of zero-based `grapheme_column` on zero-based `line`. The column may equal
    // the line's grapheme count, which is the line's end; nullopt when the line does not exist
    // or the column lies beyond its end.
    std::optional<DocumentPosition> position_at_line_column(std::size_t line, std::size_t grapheme_column) const;
    // The zero-based line and grapheme column of `position`; nullopt when the position is
    // stale, past the end, or not on a grapheme boundary.
    std::optional<DocumentLineColumn> line_column(DocumentPosition position) const;
    // The text in `range`; an empty string when the range is stale or invalid, which is the
    // same answer an empty range gets.
    std::string text(DocumentRange range) const;

    // An empty transaction based on the current revision.
    DocumentTransaction transaction() const { return DocumentTransaction{revision_}; }
    // Replaces `range` with `text` as a transaction of that one edit; see commit.
    DocumentEditResult replace(DocumentRange range, std::string text);
    // Applies every edit of `transaction` at once, or none. It is refused with StaleRevision
    // when the transaction or a range is not of the current revision, InvalidRange for a
    // reversed, out-of-bounds, mid-grapheme or overlapping range or a recorded selection that
    // is out of bounds or mid-grapheme, InvalidUtf8 for text the
    // policy rejects, and LimitExceeded when the result would outgrow max_document_bytes. On
    // success the revision advances by exactly one, the redo history is cleared, one undo step
    // is recorded (evicting old ones under the history limits) together with the transaction's
    // selection_before, and the observers receive one DocumentChange covering all the edits.
    DocumentEditResult commit(DocumentTransaction transaction);
    // Whether undo() or redo() has a step to take.
    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }
    // Step back or forward over one recorded change, restoring the exact text of that point.
    // Each advances the revision, so positions taken before are stale and modified() stays
    // true, notifies the observers with the change, and returns it: the covering span of the
    // transaction it reverts or replays, with the selection to restore (see DocumentChange).
    // They return nullopt, having changed nothing, when there is no step. The newline
    // convention and BOM flag are not part of the history.
    std::optional<DocumentChange> undo();
    std::optional<DocumentChange> redo();
    // Forgets every undo and redo step. The text, the revision and the modified flag stay as
    // they are, and observers are not called.
    void clear_history();

    // Replaces all text and clears history. Input is normalized to LF, and the
    // newline convention and a leading BOM are recorded from it (the BOM is not
    // kept in the text). With a Reject policy, malformed input returns
    // InvalidUtf8, and text longer than max_document_bytes returns
    // LimitExceeded, both leaving the document untouched. On success the
    // revision advances, observers see a whole-document change, and the
    // document counts as modified until mark_clean().
    DocumentEditStatus set_text(std::string text);
    // Explicit callers such as a file controller may choose a one-shot input
    // policy without changing the document's normal edit policy.
    DocumentEditStatus set_text(std::string text, InvalidUtf8Policy policy);

    // subscribe and subscribe_state register `observer` and return its id; unsubscribe removes
    // the subscription of either kind with that id and ignores an unknown one. Observers of a
    // kind run in subscription order over a snapshot of their list, so subscribing or
    // unsubscribing inside a notification takes effect from the next one; an observer removed
    // during a notification is still called for that one.
    ObserverId subscribe(Observer observer);
    ObserverId subscribe_state(StateObserver observer);
    void unsubscribe(ObserverId observer) noexcept;

private:
    struct HistoryEntry;

    DocumentEditResult commit_edits(const std::vector<DocumentTextEdit>& edits, DocumentRevision base_revision,
                                    const std::optional<DocumentSelection>& selection_before);
    bool range_is_current_and_valid(DocumentRange range) const;
    void notify(const DocumentChange& change);
    void notify_state();

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
    std::vector<std::pair<ObserverId, StateObserver>> state_observers_;
};

}  // namespace ckv::widgets
