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

enum class EditorFileStatus {
    Ok,
    NotFound,
    InvalidText,
    Conflict,
    NoPath,
    Error,
};

enum class EditorCloseChoice {
    Cancel,
    Discard,
    Save,
};

enum class EditorSaveAsPolicy {
    FailIfExists,
    Overwrite,
};

// Opening another file must not discard an unsaved document by accident. A
// client presents its Save/Discard/Cancel choice first, then uses Discard only
// after the user selected that explicit outcome.
enum class EditorOpenModifiedPolicy {
    Reject,
    Discard,
};

struct EditorOpenOptions {
    InvalidUtf8Policy invalid_utf8 = InvalidUtf8Policy::Reject;
    EditorOpenModifiedPolicy modified_document = EditorOpenModifiedPolicy::Reject;
};

class FileEditorController {
public:
    FileEditorController(std::shared_ptr<EditorDocument> document, FileSystem& filesystem);

    const std::shared_ptr<EditorDocument>& document() const noexcept { return document_; }
    const std::string& path() const noexcept { return path_; }
    bool has_path() const noexcept { return !path_.empty(); }
    bool modified() const noexcept { return document_->modified(); }
    bool externally_changed() const;

    EditorFileStatus open(std::string path, EditorOpenOptions options = {});
    EditorFileStatus save();
    // Save As is non-destructive by default. Replacing an existing path is a
    // deliberate application decision and remains fingerprint-conditional.
    EditorFileStatus save_as(std::string path, EditorSaveAsPolicy policy = EditorSaveAsPolicy::FailIfExists);
    // Returns Ok only when the caller may close. A modified document requires
    // an explicit choice; Cancel leaves it untouched and reports Conflict.
    EditorFileStatus request_close(EditorCloseChoice choice);

private:
    std::string serialized_text() const;

    std::shared_ptr<EditorDocument> document_;
    FileSystem* filesystem_;
    std::string path_;
    std::optional<FileFingerprint> fingerprint_;
};

}  // namespace ckv::widgets
