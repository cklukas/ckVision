// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Explicit injected-file-service workflow for a shared EditorDocument.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "cvision/core/filesystem.hpp"
#include "cvision/widgets/editor_document.hpp"

namespace ckv::widgets {

// The outcome of a FileEditorController operation. Only Ok changes anything.
enum class EditorFileStatus {
    // Done.
    Ok,
    // open could not read the file, most often because the path does not exist.
    NotFound,
    // open read the file but the document refused its contents: malformed UTF-8 under the
    // Reject policy, or more than the document's max_document_bytes.
    InvalidText,
    // Going ahead would lose work or overwrite something unexpected: open over a modified
    // document under EditorOpenModifiedPolicy::Reject, save over a file that changed or
    // vanished since it was opened or last saved, save_as onto an existing file under
    // FailIfExists, or request_close with Cancel.
    Conflict,
    // save, directly or through request_close, before any path was opened or saved as.
    NoPath,
    // The file service failed the write. For save_as this includes a write refused because the
    // target appeared or changed between the existence check and the write.
    Error,
};

// What the user chose when asked about closing a document with unsaved changes; see
// FileEditorController::request_close.
enum class EditorCloseChoice {
    // Keep the document open.
    Cancel,
    // Close and lose the changes.
    Discard,
    // Save first, and close only if the save succeeds.
    Save,
};

// What save_as does when its target already exists.
enum class EditorSaveAsPolicy {
    // Refuse with Conflict.
    FailIfExists,
    // Replace it, provided it is still the file that was found there just before the write.
    Overwrite,
};

// Opening another file must not discard an unsaved document by accident. A
// client presents its Save/Discard/Cancel choice first, then uses Discard only
// after the user selected that explicit outcome.
enum class EditorOpenModifiedPolicy {
    Reject,
    Discard,
};

// The choices one open() call is made with.
struct EditorOpenOptions {
    // How malformed UTF-8 in the file is treated for this load only; the document's own policy
    // for later edits is unchanged. And whether a document with unsaved changes may be
    // replaced.
    InvalidUtf8Policy invalid_utf8 = InvalidUtf8Policy::Reject;
    EditorOpenModifiedPolicy modified_document = EditorOpenModifiedPolicy::Reject;
};

// Connects one EditorDocument to a file through an injected FileSystem. It loads and saves the
// document and remembers the file's fingerprint, so that no save silently overwrites a file
// someone else changed in the meantime. It owns no view and never asks the user anything: the
// choices a user makes arrive as arguments.
class FileEditorController {
public:
    // `document` is shared with the views that edit it; a null one is replaced by a new empty
    // document. `filesystem` is held by reference and must outlive the controller. There is no
    // path until open or save_as succeeds.
    FileEditorController(std::shared_ptr<EditorDocument> document, FileSystem& filesystem);

    // The document; never null.
    const std::shared_ptr<EditorDocument>& document() const noexcept { return document_; }
    // The normalized path last opened or saved as, empty before either; and whether there is
    // one.
    const std::string& path() const noexcept { return path_; }
    bool has_path() const noexcept { return !path_.empty(); }
    // The document's own modified flag.
    bool modified() const noexcept { return document_->modified(); }
    // Whether the file at path() no longer has the fingerprint recorded at the last open or
    // save, which includes a file that has been removed. False when there is no path. Asks the
    // file service afresh on every call.
    bool externally_changed() const;

    // Loads `path`, normalized by the file service, into the document, replacing its text and
    // clearing its history. Refused with Conflict, before reading, when the document is modified
    // and options.modified_document is Reject; NotFound when the file cannot be read;
    // InvalidText when the document refuses the contents. On Ok the path and fingerprint are
    // recorded and the document is marked clean; otherwise the document, path and fingerprint
    // stay as they were.
    EditorFileStatus open(std::string path, EditorOpenOptions options = {});
    // Writes the document to path(), atomically and only if the file still has the recorded
    // fingerprint. LF line breaks are written as the document's preferred newline, preceded by
    // a UTF-8 BOM when has_utf8_bom(). NoPath without a path; Conflict when the file changed or
    // vanished since it was opened or last saved; Error when the write fails. On Ok the new
    // fingerprint is recorded and the document is marked clean.
    EditorFileStatus save();
    // Save As is non-destructive by default. Replacing an existing path is a
    // deliberate application decision and remains fingerprint-conditional.
    // The text is serialized as save() does. On Ok the controller adopts the
    // normalized path and the new fingerprint and marks the document clean;
    // see EditorFileStatus for Conflict and Error. The document need not be
    // modified.
    EditorFileStatus save_as(std::string path, EditorSaveAsPolicy policy = EditorSaveAsPolicy::FailIfExists);
    // Returns Ok only when the caller may close. A modified document requires
    // an explicit choice; Cancel leaves it untouched and reports Conflict.
    // An unmodified document returns Ok whatever the choice. Discard returns
    // Ok without touching the document, which stays modified; Save returns
    // what save() returns. Nothing is closed here: closing is the caller's.
    EditorFileStatus request_close(EditorCloseChoice choice);

private:
    std::string serialized_text() const;

    std::shared_ptr<EditorDocument> document_;
    FileSystem* filesystem_;
    std::string path_;
    std::optional<FileFingerprint> fingerprint_;
};

}  // namespace ckv::widgets
