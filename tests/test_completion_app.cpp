// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "completion_app_test_fixture.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

#include <fstream>
#include <sstream>

namespace {
bool has_caption(ckv::ui::View& view, std::string_view text) {
    if (auto* caption = dynamic_cast<ckv::widgets::StaticText*>(&view); caption && caption->text() == text) return true;
    for (const auto& child : view.children()) if (has_caption(*child, text)) return true;
    return false;
}
void check_frame(ckv::ui::Application& app, const char* path) {
    const auto actual = ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    std::ifstream input(path, std::ios::binary);
    std::ostringstream expected; expected << input.rdbuf();
    CK_CHECK(input.is_open());
    CK_CHECK(actual == expected.str());
}
}
CK_TEST(completion_application_displays_and_accepts_general_field_completion) {
    ckv::testing::CompletionAppFixture f; f.tick();
    auto& field = f.demo->command_field();
    CK_CHECK(field.text() == "open-f");
    CK_CHECK(field.has_completion_preview());
    check_frame(f.app, "golden/completion_app.dump");
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::None, ""}});
    CK_CHECK(field.text() == "open-file");
    CK_CHECK(field.undo());
    CK_CHECK(field.text() == "open-f");
}
CK_TEST(completion_application_uses_responsive_file_and_folder_presentations) {
    ckv::testing::CompletionAppFixture f;
    f.demo->open_file(); f.tick();
    CK_CHECK(f.source.query.directory == "/home/user");
    CK_CHECK(f.source.query.extensions == (std::vector<std::string>{"txt", "md"}));
    auto* field = dynamic_cast<ckv::widgets::InputLine*>(f.app.focused());
    CK_CHECK(field != nullptr); if (!field) return;
    field->set_text("docs");
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}); f.tick();
    CK_CHECK(field->text() == "/home/user/docs/");
    f.demo.reset();
    CK_CHECK(f.source.closed);
    CK_CHECK(f.app.root().children().empty());
}
CK_TEST(file_and_folder_completion_frames_show_the_same_eligible_suggestions) {
    ckv::testing::PathDialogFixture file;
    ckv::widgets::FileDialogOptions options; options.filters = {{"Text", {"txt"}}};
    file.open(options); file.type("rep");
    check_frame(file.app, "golden/file_completion.dump");
    CK_CHECK(file.field->text() == "rep");
    CK_CHECK(file.field->completion_candidate()->text == "report.txt");
    ckv::testing::PathDialogFixture folder;
    folder.attach(ckv::widgets::make_directory_picker(folder.source, "/home/user", folder.roles, folder.app, nullptr, nullptr));
    folder.type("do");
    check_frame(folder.app, "golden/folder_completion.dump");
    CK_CHECK(folder.field->text() == "do");
    CK_CHECK(folder.field->completion_candidate()->text == "docs/");
}

CK_TEST(completion_application_about_command_opens_a_real_modal_and_retires_its_handlers) {
    ckv::testing::CompletionAppFixture f;
    f.app.commands().execute(f.app.commands().standard().help); f.tick();
    CK_CHECK(f.app.focused() != &f.demo->command_field());
    CK_CHECK(f.demo->desktop().children().size() > 3);
    f.demo.reset();
    CK_CHECK(f.app.root().children().empty());
    f.app.commands().execute(f.app.commands().standard().help);
    CK_CHECK(f.app.root().children().empty());
}

CK_TEST(completion_application_keeps_file_folder_and_cancellation_completions_until_detachment) {
    ckv::testing::CompletionAppFixture f;
    f.demo->open_file(); f.tick();
    auto* field = dynamic_cast<ckv::widgets::InputLine*>(f.app.focused());
    CK_CHECK(field != nullptr); if (!field) return;
    field->set_text("report.txt");
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}); f.tick();
    CK_CHECK(has_caption(f.demo->desktop(), "File: /home/user/report.txt"));
    f.demo->open_folder(); f.tick();
    auto* select = ckv::testing::path_dialog_button(f.demo->desktop(), "Select");
    CK_CHECK(select != nullptr); if (!select) return;
    select->on_press(); f.tick();
    CK_CHECK(has_caption(f.demo->desktop(), "Folder: /home/user"));
    f.demo->open_file(); f.tick();
    auto* cancel = ckv::testing::path_dialog_button(f.demo->desktop(), "Cancel");
    CK_CHECK(cancel != nullptr); if (!cancel) return;
    cancel->on_press(); f.tick();
    CK_CHECK(has_caption(f.demo->desktop(), "File selection cancelled."));
}
