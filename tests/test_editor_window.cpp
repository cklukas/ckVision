// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
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

CK_TEST(editor_window_closes_once_the_client_chose_discard_and_vetoes_again_after_a_new_change) {
    MemoryFileSystem filesystem;
    filesystem.add_file("notes.txt", "notes");
    EditorWindow window("Editor", std::make_shared<EditorDocument>(), filesystem);
    CK_CHECK(window.open("notes.txt") == EditorFileStatus::Ok);
    const auto end = window.document()->end();
    CK_CHECK(window.document()->replace({end, end}, " unsaved"));
    CK_CHECK(!window.close());
    CK_CHECK(window.request_close(EditorCloseChoice::Cancel) == EditorFileStatus::Conflict);
    CK_CHECK(!window.close());
    CK_CHECK(window.request_close(EditorCloseChoice::Discard) == EditorFileStatus::Ok);
    CK_CHECK(window.document()->modified());
    // The discard answered for the changes as they stood; a further change asks again.
    const auto later = window.document()->end();
    CK_CHECK(window.document()->replace({later, later}, "!"));
    CK_CHECK(!window.close());
    CK_CHECK(window.request_close(EditorCloseChoice::Discard) == EditorFileStatus::Ok);
    CK_CHECK(window.close());
}

CK_TEST(a_scripted_editor_window_marks_typed_changes_and_keeps_its_close_control_vetoed) {
    // Application-level script: the window sits on a Desktop, typing reaches
    // its editor through Application::dispatch, and a dispatched click on the
    // close control is refused while the change is unsettled.
    ckv::term::HeadlessTerminal term(ckv::Size{50, 12});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    MemoryFileSystem filesystem;
    filesystem.add_file("/project/notes.txt", "notes\n");
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(app.root().bounds()));
    auto* window = static_cast<EditorWindow*>(desktop->add_window(
        std::make_unique<EditorWindow>("Untitled", std::make_shared<EditorDocument>(), filesystem)));
    window->set_bounds(ckv::Rect{0, 0, 40, 10});
    CK_CHECK(window->open("/project/notes.txt") == EditorFileStatus::Ok);
    app.set_focus(&window->editor());
    app.step(0);
    const auto title_row = [&] {
        std::string out;
        for (int x = 0; x < 40; ++x) out += app.composed_surface().at(ckv::Point{x, 0}).grapheme();
        return out;
    };
    CK_CHECK(title_row().find("notes.txt") != std::string::npos);
    CK_CHECK(title_row().find("notes.txt *") == std::string::npos);

    CK_CHECK(app.dispatch(ckv::TextEvent{"more "}));
    app.step(0);
    CK_CHECK(window->document()->text().find("more ") != std::string::npos);
    CK_CHECK(title_row().find("notes.txt *") != std::string::npos);

    // The close control acts on release; a dirty window refuses it.
    const auto click_close = [&] {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                     std::nullopt, ckv::Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{3, 0},
                                     std::nullopt, ckv::Modifier::None});
        app.step(0);
    };
    click_close();
    CK_CHECK(desktop->windows().size() == 1U);
    CK_CHECK(window->request_close(EditorCloseChoice::Discard) == EditorFileStatus::Ok);
    click_close();
    CK_CHECK(desktop->windows().empty());
}
