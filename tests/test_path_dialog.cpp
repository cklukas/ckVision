// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "path_dialog_test_fixture.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/static_text.hpp"

namespace {
using namespace ckv;
using namespace ckv::widgets;
using ckv::testing::PathDialogFixture;
KeyEvent key(Key value, Modifier modifiers = Modifier::None) { return {KeyChord{value, modifiers, ""}}; }
}

CK_TEST(path_dialog_enter_navigates_absolute_relative_dot_parent_and_nested_directories) {
    PathDialogFixture f; f.open();
    for (const auto& value : {"docs", "..", "docs/", "..", "/home/user/docs/nested", "../..", ".", "../images", ".."}) {
        f.enter(value);
        CK_CHECK(f.results.empty());
        CK_CHECK(f.app.focused() == f.field);
        CK_CHECK(f.source.query.operation == DirectoryOperation::Browse);
        CK_CHECK(f.source.query.prefix.empty());
    }
    CK_CHECK(f.field->text() == "/home/");
    f.enter("/"); f.enter("..");
    CK_CHECK(f.field->text() == "/");
    CK_CHECK(f.results.empty());
}

CK_TEST(path_dialog_preview_matches_filtered_rows_and_filter_updates_preserve_typed_text) {
    PathDialogFixture f;
    FileDialogOptions options; options.filters = {{"Text", {"txt"}}, {"CSV", {"csv"}}};
    f.open(options);
    f.type("rep");
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(f.field->completion_candidate()->text == "report.txt");
    const auto id = f.field->completion_candidate()->id;
    CK_CHECK(f.list->model()->index_of(id).has_value());
    auto* filter = ckv::testing::path_dialog_button(*f.window, "Filter: Text");
    CK_CHECK(filter != nullptr); filter->on_press(); f.tick();
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(f.field->completion_candidate()->text == "report.csv");
    CK_CHECK(f.field->on_key(key(Key::Right)));
    CK_CHECK(f.field->text() == "report.csv");
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == "rep");
    f.type("");
    CK_CHECK(f.source.result->entries.front().is_directory);
    f.enter("docs");
    CK_CHECK(f.source.query.extensions == std::vector<std::string>{"csv"});
    CK_CHECK(f.field->text() == "/home/user/docs/");
}

CK_TEST(path_dialog_tab_navigates_and_enter_does_not_accept_ghost_text) {
    PathDialogFixture f; f.open();
    f.type("rep");
    CK_CHECK(f.field->has_completion_preview());
    f.app.dispatch(key(Key::Tab));
    CK_CHECK(f.app.focused() == f.list);
    CK_CHECK(f.field->text() == "rep");
    f.app.dispatch(key(Key::Tab, Modifier::Shift));
    CK_CHECK(f.app.focused() == f.field);
    f.app.dispatch(key(Key::Enter));
    CK_CHECK(f.results.size() == 1);
    if (!f.results.empty()) CK_CHECK(f.results.front().path == "/home/user/rep");
}

CK_TEST(path_dialog_hidden_policy_and_selected_candidate_survive_provider_updates) {
    PathDialogFixture f; f.open();
    f.type("rep");
    const auto rows = f.source.result->entries;
    CK_CHECK(rows.size() == 2);
    if (rows.size() != 2) return;
    const auto second = rows.back();
    const auto index = f.list->model()->index_of(second.id);
    CK_CHECK(index.has_value()); if (!index) return;
    f.list->set_cursor(*index);
    f.tick();
    CK_CHECK(f.field->completion_candidate()->id == second.id);
    f.app.set_focus(f.list); f.app.set_focus(f.field);
    CK_CHECK(f.field->has_selection());
    f.app.dispatch(key(Key::End)); f.tick();
    CK_CHECK(f.field->completion_candidate()->id == second.id);
    CK_CHECK(f.field->text() == "rep");
    auto* hidden = ckv::testing::path_dialog_button(*f.window, "Show Hidden");
    CK_CHECK(hidden != nullptr); if (!hidden) return;
    hidden->on_press(); f.enter("docs");
    CK_CHECK(f.source.query.show_hidden);
    f.enter(".."); f.type(".s");
    CK_CHECK(f.field->completion_candidate()->text == ".secret.txt");
}

CK_TEST(path_dialog_delayed_classification_is_superseded_by_edits_and_close_without_result) {
    PathDialogFixture f; f.open();
    f.source.automatic = false;
    f.type("docs");
    f.app.dispatch(key(Key::Enter));
    const auto submitted = f.source.query;
    CK_CHECK(submitted.operation == DirectoryOperation::Resolve);
    f.app.dispatch(key(Key::Enter));
    CK_CHECK(f.source.query.revision == submitted.revision);
    f.type("rep");
    f.source.deliver(submitted); f.tick();
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(f.results.empty());
    f.window->cancel_request();
    CK_CHECK(f.source.closed);
    CK_CHECK(f.results.size() == 1 && !f.results.front().accepted);
    f.source.deliver(submitted); f.tick();
    CK_CHECK(f.results.size() == 1);
}

CK_TEST(path_dialog_missing_directory_form_stays_open_and_reports_failure) {
    PathDialogFixture f; f.open();
    f.enter("missing/");
    CK_CHECK(f.results.empty());
    CK_CHECK(f.field->text() == "missing/");
    auto* status = ckv::testing::path_dialog_child<StaticText>(*f.window);
    CK_CHECK(status->text() == "Directory is unavailable");
    f.enter("docs");
    CK_CHECK(f.field->text() == "/home/user/docs/");
}

CK_TEST(path_dialog_folder_picker_shares_completion_but_select_accepts_only_directories) {
    PathDialogFixture f;
    auto handle = make_directory_picker(f.source, "/home/user", f.roles, f.app, nullptr,
        [&f](bool accepted, std::string path) { f.results.push_back({accepted, std::move(path)}); });
    f.attach(std::move(handle));
    f.type("do");
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(f.field->on_key(key(Key::Right)));
    CK_CHECK(f.field->text() == "docs/");
    f.app.dispatch(key(Key::Enter)); f.tick();
    CK_CHECK(f.results.empty());
    CK_CHECK(f.field->text() == "/home/user/docs/");
    f.type("report.txt");
    auto* select = ckv::testing::path_dialog_button(*f.window, "Select");
    select->on_press(); f.tick();
    CK_CHECK(f.results.empty());
    f.type("."); select->on_press(); f.tick();
    CK_CHECK(f.results.size() == 1);
    if (!f.results.empty()) CK_CHECK(f.results.front().path == "/home/user/docs");
}

CK_TEST(path_dialog_result_callback_can_detach_and_destroy_its_window) {
    PathDialogFixture f;
    Window* target = nullptr;
    int calls = 0;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", f.source, {}, f.roles, f.app, nullptr,
        [&](FileDialogResult) {
            ++calls;
            auto removed = f.desktop->remove_child(target);
            CK_CHECK(removed.get() == target);
        });
    target = handle.window.get(); f.attach(std::move(handle));
    f.enter("report.txt");
    CK_CHECK(calls == 1);
    CK_CHECK(f.source.closed);
}

CK_TEST(path_dialog_caret_queries_reuse_the_current_directory_model_and_leave_classification_pending) {
    PathDialogFixture f; f.open(); f.type("rep");
    const auto browse = f.source.query.revision;
    f.app.dispatch(key(Key::Left));
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(f.source.query.revision == browse);
    f.app.dispatch(key(Key::End));
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(f.source.query.revision == browse);
    f.source.automatic = false; f.type("docs");
    f.app.dispatch(key(Key::Enter));
    const auto classification = f.source.query;
    f.app.dispatch(key(Key::Left));
    CK_CHECK(f.source.query.revision == classification.revision);
    f.source.deliver(classification); f.tick();
    CK_CHECK(f.field->text() == "/home/user/docs/");
    CK_CHECK(f.results.empty());
}

CK_TEST(path_dialog_typed_parent_context_changes_list_and_completion_without_moving_navigation_base) {
    PathDialogFixture f; f.open();
    f.type("../im");
    CK_CHECK(f.source.result->directory == "/home");
    CK_CHECK(f.source.result->entries.size() == 1);
    CK_CHECK(f.field->completion_candidate()->text == "images/");
    CK_CHECK(f.list->model()->index_of(f.field->completion_candidate()->id).has_value());
    f.type("missing/im");
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(f.list->model()->item_count() == 0);
    f.type("rep");
    CK_CHECK(f.source.result->directory == "/home/user");
    f.type("../im"); f.app.dispatch(key(Key::Right)); f.tick();
    CK_CHECK(f.field->text() == "../images/");
    CK_CHECK(f.source.result->directory == "/home/images");
    f.app.dispatch(key(Key::Enter)); f.tick();
    CK_CHECK(f.results.empty());
    CK_CHECK(f.field->text() == "/home/images/");
}

CK_TEST(path_dialog_save_navigates_existing_directories_and_accepts_a_new_committed_filename) {
    PathDialogFixture f;
    FileDialogOptions options; options.filters = {{"Text", {"txt"}}}; options.show_hidden = true;
    auto handle = make_file_dialog(FileDialogMode::Save, "/home/user", f.source, options, f.roles,
        f.app, nullptr, [&f](FileDialogResult result) { f.results.push_back(std::move(result)); });
    f.attach(std::move(handle));
    f.enter("docs");
    CK_CHECK(f.results.empty());
    CK_CHECK(f.field->text() == "/home/user/docs/");
    CK_CHECK(f.source.query.extensions == std::vector<std::string>{"txt"});
    CK_CHECK(f.source.query.show_hidden);
    f.enter("new-name.txt");
    CK_CHECK(f.results.size() == 1);
    if (!f.results.empty()) {
        CK_CHECK(f.results.front().accepted);
        CK_CHECK(f.results.front().path == "/home/user/docs/new-name.txt");
    }
}

CK_TEST(path_dialog_paging_from_a_pin_only_publication_preserves_its_existing_boundary) {
    for (bool backward : {false, true}) {
        PathDialogFixture f; f.source.automatic = false; f.open();
        const auto publish = [&](std::vector<DirectoryItem> entries, bool boundaries) {
            auto result = std::make_shared<DirectorySnapshot>();
            result->revision = f.source.query.revision;
            result->source_generation = f.source.query.source_generation;
            result->state = DirectoryState::Ready;
            result->directory = "/home/user";
            result->entries = std::move(entries);
            result->matching = result->discovered = 100;
            result->has_previous = result->has_next = true;
            if (boundaries) { result->first = result->entries.front(); result->last = result->entries.back(); }
            f.source.result = std::move(result); f.tick();
        };
        publish({{10, "report-10.txt", false}}, true);
        auto* next = ckv::testing::path_dialog_button(*f.window, "Next page");
        CK_CHECK(next != nullptr); if (!next) return;
        next->on_press();
        CK_CHECK(f.source.query.boundary && f.source.query.boundary->id == 10);
        publish({{11, "report-11.txt", false}, {99, "report-99.txt", false}}, true);
        const auto selected = f.list->model()->index_of(99);
        CK_CHECK(selected.has_value()); if (!selected) return;
        f.list->set_cursor(*selected);
        CK_CHECK(f.source.query.preferred && f.source.query.preferred->id == 99);
        publish({{99, "report-99.txt", false}}, false);
        auto* page = ckv::testing::path_dialog_button(*f.window, backward ? "Previous page" : "Next page");
        CK_CHECK(page != nullptr); if (!page) return;
        page->on_press();
        CK_CHECK(f.source.query.boundary && f.source.query.boundary->id == 10);
        CK_CHECK(!f.source.query.preferred);
        CK_CHECK(f.source.query.backward == backward);
    }
}

CK_TEST(path_dialog_refilters_known_rows_and_previews_before_a_pending_lookup_finishes) {
    PathDialogFixture f;
    FileDialogOptions options; options.filters = {{"Text", {"txt"}}}; f.open(options);
    const auto original_query = f.source.query;
    const auto original_publication = f.source.result;
    f.source.automatic = false;
    f.field->set_text("rep");
    const auto pending_query = f.source.query;
    CK_CHECK(f.field->has_completion_preview());
    auto* status = ckv::testing::path_dialog_child<StaticText>(*f.window);
    CK_CHECK(status != nullptr); if (!status) return;
    CK_CHECK(status->text() == "Finding directory matches...");
    const auto candidate = f.field->completion_candidate();
    CK_CHECK(candidate && candidate->text == "report.txt");
    if (candidate) CK_CHECK(f.list->model()->index_of(candidate->id).has_value());
    CK_CHECK(original_publication->revision == original_query.revision);
    f.source.deliver(original_query); f.tick();
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(status->text() == "Finding directory matches...");
    f.source.deliver(pending_query); f.tick();
    CK_CHECK(status->text() == "/home/user  1 matching");
    CK_CHECK(f.field->completion_candidate()->text == "report.txt");
}

CK_TEST(path_dialog_completion_preserves_a_basename_mark_that_shares_the_parent_separator_grapheme) {
    PathDialogFixture f;
    const std::string mark = "\xCC\x81";
    const std::string name = mark + "report.txt";
    f.source.fs.add_file("/home/user/docs/" + name);
    f.open(); f.enter("docs");
    const std::string partial = "/home/user/docs/" + mark + "rep";
    f.type(partial);
    CK_CHECK(f.field->has_completion_preview());
    f.app.dispatch(key(Key::Right)); f.tick();
    CK_CHECK(f.field->text() == "/home/user/docs/" + name);
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == partial);
}
