// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/core/filesystem.hpp"
#include "cvision/widgets/file_editor_controller.hpp"

using ckv::MemoryFileSystem;
using ckv::widgets::EditorFileStatus;
using ckv::widgets::EditorDocument;
using ckv::widgets::EditorOpenModifiedPolicy;
using ckv::widgets::EditorOpenOptions;
using ckv::widgets::EditorSaveAsPolicy;
using ckv::widgets::FileEditorController;
using ckv::widgets::InvalidUtf8Policy;

CK_TEST(file_editor_controller_loads_preserves_newlines_and_saves_through_injected_filesystem) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/config.yaml", "name: ckv\r\nversion: 1\r\n");
    auto document = std::make_shared<EditorDocument>();
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.open("/project/config.yaml") == EditorFileStatus::Ok);
    CK_CHECK(document->text() == "name: ckv\nversion: 1\n");
    const auto position = document->end();
    CK_CHECK(document->replace({position, position}, "enabled: true\n"));
    CK_CHECK(controller.save() == EditorFileStatus::Ok);
    const auto stored = filesystem.read_file("/project/config.yaml");
    CK_CHECK(stored.has_value());
    CK_CHECK(stored->contents == "name: ckv\r\nversion: 1\r\nenabled: true\r\n");
}

CK_TEST(file_editor_controller_preserves_an_explicit_utf8_bom_on_save) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/with-bom.txt", "\xEF\xBB\xBFone\r\ntwo\r\n");
    auto document = std::make_shared<EditorDocument>();
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.open("/project/with-bom.txt") == EditorFileStatus::Ok);
    CK_CHECK(document->has_utf8_bom());
    CK_CHECK(document->text() == "one\ntwo\n");
    const auto end = document->end();
    CK_CHECK(document->replace({end, end}, "three\n"));
    CK_CHECK(controller.save() == EditorFileStatus::Ok);
    const auto stored = filesystem.read_file("/project/with-bom.txt");
    CK_CHECK(stored.has_value());
    CK_CHECK(stored->contents == "\xEF\xBB\xBFone\r\ntwo\r\nthree\r\n");
}

CK_TEST(file_editor_controller_refuses_silent_external_overwrite) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/file.txt", "initial");
    auto document = std::make_shared<EditorDocument>();
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.open("/project/file.txt") == EditorFileStatus::Ok);
    const auto end = document->end();
    CK_CHECK(document->replace({end, end}, " local"));
    filesystem.add_file("/project/file.txt", "external");
    CK_CHECK(controller.externally_changed());
    CK_CHECK(controller.save() == EditorFileStatus::Conflict);
}

CK_TEST(file_editor_controller_save_as_refuses_an_existing_path_unless_overwrite_is_explicit) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/existing.txt", "external");
    auto document = std::make_shared<EditorDocument>("local");
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.save_as("/project/existing.txt") == EditorFileStatus::Conflict);
    const auto unchanged = filesystem.read_file("/project/existing.txt");
    CK_CHECK(unchanged.has_value());
    CK_CHECK(unchanged->contents == "external");
    CK_CHECK(controller.save_as("/project/existing.txt", EditorSaveAsPolicy::Overwrite) == EditorFileStatus::Ok);
    const auto replaced = filesystem.read_file("/project/existing.txt");
    CK_CHECK(replaced.has_value());
    CK_CHECK(replaced->contents == "local");
}

CK_TEST(file_editor_controller_requires_explicit_close_choice_for_dirty_document) {
    MemoryFileSystem filesystem;
    auto document = std::make_shared<EditorDocument>("draft");
    FileEditorController controller(document, filesystem);
    const auto end = document->end();
    CK_CHECK(document->replace({end, end}, "!"));
    CK_CHECK(controller.request_close(ckv::widgets::EditorCloseChoice::Cancel) == EditorFileStatus::Conflict);
    CK_CHECK(controller.request_close(ckv::widgets::EditorCloseChoice::Discard) == EditorFileStatus::Ok);
}

CK_TEST(file_editor_controller_requires_an_explicit_invalid_utf8_replacement_open_policy) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/malformed.txt", std::string{"ok\xC3", 3});
    auto document = std::make_shared<EditorDocument>();
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.open("/project/malformed.txt") == EditorFileStatus::InvalidText);
    CK_CHECK(controller.open("/project/malformed.txt", {ckv::widgets::InvalidUtf8Policy::Replace}) == EditorFileStatus::Ok);
    CK_CHECK(document->text() == "ok\xEF\xBF\xBD");
}

CK_TEST(file_editor_controller_never_replaces_a_dirty_document_without_an_explicit_discard_choice) {
    MemoryFileSystem filesystem;
    filesystem.add_file("first.txt", "first");
    filesystem.add_file("second.txt", "second");
    auto document = std::make_shared<EditorDocument>();
    FileEditorController controller(document, filesystem);
    CK_CHECK(controller.open("first.txt") == EditorFileStatus::Ok);
    const auto end = document->end();
    CK_CHECK(document->replace({end, end}, " unsaved"));
    CK_CHECK(controller.open("second.txt") == EditorFileStatus::Conflict);
    CK_CHECK(document->text() == "first unsaved");
    CK_CHECK(controller.open("second.txt", EditorOpenOptions{InvalidUtf8Policy::Reject,
                                                               EditorOpenModifiedPolicy::Discard}) == EditorFileStatus::Ok);
    CK_CHECK(document->text() == "second");
}
