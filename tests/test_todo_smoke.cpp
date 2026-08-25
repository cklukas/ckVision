// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_app.hpp"
#include "memory_todo_repository.hpp"

#include <memory>
#include <optional>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/window.hpp"

namespace {

using namespace ckv;
using namespace ckv::todo;

CalendarReading smoke_reading() {
    return {IsoTimestamp{"2026-08-25T12:00:00Z"}, IsoDate{"2026-08-25"}};
}

TodoWorkspace smoke_workspace() {
    return *TodoWorkspace::guided({IsoTimestamp{"2026-08-25T11:00:00Z"}, "fixture"}).value;
}

KeyEvent key(Key value, std::string text = {}) {
    return KeyEvent{KeyChord{value, Modifier::None, std::move(text)}};
}

struct SmokeFixture {
    term::HeadlessTerminal terminal{Size{100, 30}};
    ManualClock monotonic;
    ui::Application app{terminal, monotonic};
    MemoryTodoRepository repository{smoke_workspace()};
    FixedCalendarClock calendar{smoke_reading()};
    TodoApp todo{app, repository, calendar, "smoke-test"};
};

const widgets::MenuBarItem* menu_named(const widgets::MenuBar& menu, const std::string& label) {
    for (const auto& item : menu.menus())
        if (item.label == label) return &item;
    return nullptr;
}

}  // namespace

CK_TEST(todo_app_builds_a_windowed_board_with_menu_status_and_focus) {
    SmokeFixture fixture;
    CK_CHECK(fixture.todo.controller().is_open());
    CK_CHECK(fixture.todo.board_window() != nullptr);
    CK_CHECK(fixture.todo.board_window()->movable());
    CK_CHECK(fixture.todo.board_window()->resizable());
    CK_CHECK(fixture.todo.board_view()->lane_count() == 3);
    CK_CHECK(fixture.todo.status_line() != nullptr);
    CK_CHECK(fixture.app.focused() == fixture.todo.board_view()->lane_view(LaneId{1}));
    CK_CHECK(fixture.app.commands().id_for(TodoApp::kAddTaskKey).has_value());
    CK_CHECK(fixture.app.commands().command_for_key(KeyChord{Key::F2, Modifier::None, {}}) ==
             fixture.app.commands().id_for(TodoApp::kAddTaskKey));
}

CK_TEST(todo_board_only_aliases_do_not_run_while_a_task_dialog_has_focus) {
    SmokeFixture fixture;
    const auto edit = fixture.app.commands().id_for(TodoApp::kEditTaskKey);
    CK_CHECK(edit && fixture.app.execute_command(*edit));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Char, "e")));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(!fixture.app.is_modal());
}

CK_TEST(todo_move_mode_commits_across_lanes_and_escape_cancels) {
    SmokeFixture fixture;
    const auto move = fixture.app.commands().id_for(TodoApp::kMoveTaskKey);
    CK_CHECK(move && fixture.app.execute_command(*move));
    CK_CHECK(fixture.todo.move_active());
    CK_CHECK(fixture.app.dispatch(key(Key::Right)));
    CK_CHECK(fixture.app.execute_command(*move));
    CK_CHECK(!fixture.todo.move_active());
    CK_CHECK(fixture.todo.controller().workspace()->lane_of(TaskId{1}) == LaneId{2});

    fixture.todo.board_view()->select_task(TaskId{1});
    CK_CHECK(fixture.app.execute_command(*move));
    CK_CHECK(fixture.app.dispatch(key(Key::Right)));
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
    CK_CHECK(!fixture.todo.move_active());
    CK_CHECK(fixture.todo.controller().workspace()->lane_of(TaskId{1}) == LaneId{2});
}

CK_TEST(todo_note_window_is_modeless_and_autosaves_after_150ms) {
    SmokeFixture fixture;
    const auto note = fixture.app.commands().id_for(TodoApp::kEditNoteKey);
    CK_CHECK(note && fixture.app.execute_command(*note));
    CK_CHECK(fixture.todo.note_window_count() == 1);
    CK_CHECK(!fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(TextEvent{"autosaved note", false, false}));
    fixture.monotonic.advance(150'000'000);
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(fixture.todo.controller().workspace()->find_task(TaskId{1})->note == "autosaved note");
    CK_CHECK(fixture.app.execute_command(fixture.app.commands().standard().close));
    CK_CHECK(fixture.todo.note_window_count() == 0);
}

CK_TEST(todo_archive_confirmation_removes_only_after_yes) {
    SmokeFixture fixture;
    const auto archive = fixture.app.commands().id_for(TodoApp::kArchiveTaskKey);
    CK_CHECK(archive && fixture.app.execute_command(*archive));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Enter)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(!fixture.app.is_modal());
    CK_CHECK(fixture.todo.controller().workspace()->find_task(TaskId{1}) == nullptr);
    CK_CHECK(fixture.repository.archives().size() == 1);
}

CK_TEST(todo_theme_menu_is_radio_marked_and_rebuilds_after_selection) {
    SmokeFixture fixture;
    const widgets::MenuBarItem* options = menu_named(*fixture.todo.menu_bar(), "&Options");
    CK_CHECK(options != nullptr);
    if (options == nullptr) return;
    const auto& choices = options->items.front().children();
    CK_CHECK(choices[0].mark() == widgets::MenuMark::RadioOn);
    choices[1].action()();
    CK_CHECK(fixture.todo.theme() == TodoTheme::Dark);
}

CK_TEST(todo_first_workspace_option_creates_without_a_modal_prompt) {
    term::HeadlessTerminal terminal(Size{80, 24});
    ManualClock monotonic;
    ui::Application app(terminal, monotonic);
    MemoryTodoRepository repository;
    FixedCalendarClock calendar(smoke_reading());
    TodoApp todo(app, repository, calendar, "first-run", TodoAppOptions{InitialWorkspace::Empty, std::nullopt});
    CK_CHECK(todo.controller().is_open());
    CK_CHECK(todo.controller().workspace()->snapshot().tasks.empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(todo_poll_adopts_an_external_committed_revision) {
    SmokeFixture fixture;
    auto external = fixture.repository.load();
    TaskDraft draft;
    draft.title = "Remote task";
    CK_CHECK(external.value->workspace.add_task(LaneId{1}, std::move(draft),
                                                {IsoTimestamp{"2026-08-25T12:01:00Z"}, "other"}));
    CK_CHECK(fixture.repository.commit(external.value->revision, external.value->workspace,
                                       IsoDate{"2026-08-25"}));
    fixture.todo.poll_repository();
    CK_CHECK(fixture.todo.controller().workspace()->find_task(TaskId{4}) != nullptr);
    CK_CHECK(fixture.todo.board_view()->lane_view(LaneId{1})->lane().task_ids.size() == 2);
}

CK_TEST(todo_app_destruction_detaches_ui_and_withdraws_private_commands) {
    term::HeadlessTerminal terminal(Size{80, 24});
    ManualClock monotonic;
    ui::Application app(terminal, monotonic);
    MemoryTodoRepository repository(smoke_workspace());
    FixedCalendarClock calendar(smoke_reading());
    auto todo = std::make_unique<TodoApp>(app, repository, calendar, "lifetime");
    CK_CHECK(!app.root().children().empty());
    todo.reset();
    CK_CHECK(app.root().children().empty());
    CK_CHECK(!app.commands().id_for(TodoApp::kAddTaskKey).has_value());
}
