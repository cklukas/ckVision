// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/core/filesystem.hpp"
#include "cvision/widgets/editor_window.hpp"

using ckv::MemoryFileSystem;
using ckv::widgets::EditorCloseChoice;
using ckv::widgets::EditorDocument;
using ckv::widgets::EditorFileStatus;
using ckv::widgets::EditorOpenModifiedPolicy;
using ckv::widgets::EditorOpenOptions;
using ckv::widgets::EditorWindow;
using ckv::widgets::InvalidUtf8Policy;

CK_TEST(editor_window_composes_document_editor_controller_and_dirty_chrome_without_an_application_shell) {
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/config.yaml", "name: ckVision\n");
    EditorWindow window("Untitled", std::make_shared<EditorDocument>(), filesystem);
    CK_CHECK(window.open("/project/config.yaml") == EditorFileStatus::Ok);
    CK_CHECK(window.title() == "config.yaml");
    CK_CHECK(window.editor().profile_id() == "yaml");
    const auto end = window.document()->end();
    CK_CHECK(window.document()->replace({end, end}, "enabled: true\n"));
    CK_CHECK(window.title() == "config.yaml *");
    CK_CHECK(!window.close());
    CK_CHECK(window.request_close(EditorCloseChoice::Discard) == EditorFileStatus::Ok);
}

CK_TEST(editor_window_forwards_an_explicit_open_text_policy_to_its_file_controller) {
    ckv::MemoryFileSystem filesystem;
    filesystem.add_file("broken.txt", std::string{"ok\xC3", 3U});
    auto document = std::make_shared<EditorDocument>();
    EditorWindow window("Editor", document, filesystem);
    CK_CHECK(window.open("broken.txt") == EditorFileStatus::InvalidText);
    CK_CHECK(window.open("broken.txt", EditorOpenOptions{InvalidUtf8Policy::Replace}) == EditorFileStatus::Ok);
    CK_CHECK(window.document()->text() == "ok\xEF\xBF\xBD");
}

CK_TEST(editor_window_preserves_its_dirty_document_until_the_client_chooses_discard) {
    MemoryFileSystem filesystem;
    filesystem.add_file("first.txt", "first");
    filesystem.add_file("second.txt", "second");
    EditorWindow window("Editor", std::make_shared<EditorDocument>(), filesystem);
    CK_CHECK(window.open("first.txt") == EditorFileStatus::Ok);
    const auto end = window.document()->end();
    CK_CHECK(window.document()->replace({end, end}, " unsaved"));
    CK_CHECK(window.open("second.txt") == EditorFileStatus::Conflict);
    CK_CHECK(window.document()->text() == "first unsaved");
    CK_CHECK(window.open("second.txt", EditorOpenOptions{InvalidUtf8Policy::Reject,
                                                           EditorOpenModifiedPolicy::Discard}) == EditorFileStatus::Ok);
    CK_CHECK(window.document()->text() == "second");
}
