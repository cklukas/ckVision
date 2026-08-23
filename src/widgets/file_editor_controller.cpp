// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/file_editor_controller.hpp"

namespace ckv::widgets {

FileEditorController::FileEditorController(std::shared_ptr<EditorDocument> document, FileSystem& filesystem)
    : document_(std::move(document)), filesystem_(&filesystem) {
    if (!document_) document_ = std::make_shared<EditorDocument>();
}

bool FileEditorController::externally_changed() const {
    if (path_.empty() || !fingerprint_) return false;
    const auto current = filesystem_->fingerprint(path_);
    return !current || *current != *fingerprint_;
}

EditorFileStatus FileEditorController::open(std::string path, EditorOpenOptions options) {
    if (document_->modified() && options.modified_document == EditorOpenModifiedPolicy::Reject)
        return EditorFileStatus::Conflict;
    const std::string normalized = filesystem_->normalize_path(path);
    const auto file = filesystem_->read_file(normalized);
    if (!file) return EditorFileStatus::NotFound;
    if (document_->set_text(file->contents, options.invalid_utf8) != DocumentEditStatus::Ok) return EditorFileStatus::InvalidText;
    path_ = normalized;
    fingerprint_ = file->fingerprint;
    document_->mark_clean();
    return EditorFileStatus::Ok;
}

std::string FileEditorController::serialized_text() const {
    std::string output = document_->text();
    if (document_->preferred_newline() != DocumentNewline::Lf) {
        const std::string newline = document_->preferred_newline() == DocumentNewline::Crlf ? "\r\n" : "\r";
        std::string converted;
        converted.reserve(output.size() + output.size() / 16U);
        for (char ch : output) {
            if (ch == '\n') converted += newline;
            else converted.push_back(ch);
        }
        output = std::move(converted);
    }
    if (document_->has_utf8_bom()) output.insert(0, "\xEF\xBB\xBF");
    return output;
}

EditorFileStatus FileEditorController::save() {
    if (path_.empty()) return EditorFileStatus::NoPath;
    if (!fingerprint_) return EditorFileStatus::Error;
    if (externally_changed()) return EditorFileStatus::Conflict;
    const FileWriteResult result = filesystem_->write_file_atomic(path_, serialized_text(),
                                                                   FileWriteExpectation::matching(*fingerprint_));
    if (result.status == FileWriteStatus::Conflict) return EditorFileStatus::Conflict;
    if (result.status != FileWriteStatus::Ok || !result.fingerprint) return EditorFileStatus::Error;
    fingerprint_ = result.fingerprint;
    document_->mark_clean();
    return EditorFileStatus::Ok;
}

EditorFileStatus FileEditorController::save_as(std::string path, EditorSaveAsPolicy policy) {
    const std::string normalized = filesystem_->normalize_path(path);
    const auto existing = filesystem_->fingerprint(normalized);
    if (existing && policy == EditorSaveAsPolicy::FailIfExists) return EditorFileStatus::Conflict;
    const FileWriteExpectation expectation = existing ? FileWriteExpectation::matching(*existing)
                                                       : FileWriteExpectation::must_not_exist();
    const FileWriteResult result = filesystem_->write_file_atomic(normalized, serialized_text(), expectation);
    if (result.status != FileWriteStatus::Ok || !result.fingerprint) return EditorFileStatus::Error;
    path_ = normalized;
    fingerprint_ = result.fingerprint;
    document_->mark_clean();
    return EditorFileStatus::Ok;
}

EditorFileStatus FileEditorController::request_close(EditorCloseChoice choice) {
    if (!modified() || choice == EditorCloseChoice::Discard) return EditorFileStatus::Ok;
    if (choice == EditorCloseChoice::Cancel) return EditorFileStatus::Conflict;
    return save();
}

}  // namespace ckv::widgets
