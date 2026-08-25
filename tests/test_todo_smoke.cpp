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
#include "cvision/widgets/status_line.hpp"
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
    bool found_board = false;
    bool found_calendar = false;
    for (const widgets::StatusLineItem& item : fixture.todo.status_line()->items()) {
        if (item.label == "Board: main" &&
            item.command == fixture.app.commands().id_for(TodoApp::kBoardManagerKey).value_or(ui::kInvalidCommand))
            found_board = true;
        if (item.label == "2026-08-25 12:00Z") found_calendar = true;
    }
    CK_CHECK(found_board);
    CK_CHECK(found_calendar);
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
    const widgets::MenuBarItem* view = menu_named(*fixture.todo.menu_bar(), "&View");
    CK_CHECK(view != nullptr);
    if (view == nullptr) return;
    const auto& choices = view->items.front().children();
    CK_CHECK(choices[0].mark() == widgets::MenuMark::RadioOn);
    choices[1].action()();
    CK_CHECK(fixture.todo.theme() == TodoTheme::Dark);
}

CK_TEST(todo_task_delete_is_distinct_from_archive_and_requires_confirmation) {
    SmokeFixture fixture;
    const auto remove = fixture.app.commands().id_for(TodoApp::kDeleteTaskKey);
    CK_CHECK(remove && fixture.app.execute_command(*remove));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Enter)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(fixture.todo.controller().workspace()->find_task(TaskId{1}) == nullptr);
    CK_CHECK(fixture.repository.archives().empty());
}

CK_TEST(todo_lane_commands_insert_sort_and_expose_checked_menu_state) {
    SmokeFixture fixture;
    const auto insert = fixture.app.commands().id_for("todo.insert-lane-right");
    CK_CHECK(insert && fixture.app.execute_command(*insert));
    CK_CHECK(fixture.app.dispatch(TextEvent{"Review", false, false}));
    CK_CHECK(fixture.app.dispatch(key(Key::Enter)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(fixture.todo.board_view()->lane_count() == 4);
    const Lane* review = fixture.todo.controller().workspace()->find_lane(LaneId{4});
    CK_CHECK(review != nullptr);
    if (review == nullptr) return;
    CK_CHECK(review->title == "Review");

    const auto priority_sort = fixture.app.commands().id_for("todo.lane-sort.priority");
    CK_CHECK(priority_sort && fixture.app.execute_command(*priority_sort));
    CK_CHECK(fixture.todo.controller().workspace()->find_lane(LaneId{4})->sort == SortMode::Priority);
    const widgets::MenuBarItem* lane = menu_named(*fixture.todo.menu_bar(), "&Lane");
    CK_CHECK(lane != nullptr);
    if (lane != nullptr) CK_CHECK(lane->items.front().children()[5].mark() == widgets::MenuMark::RadioOn);
}

CK_TEST(todo_board_creation_selects_and_persists_the_new_board) {
    SmokeFixture fixture;
    const auto add = fixture.app.commands().id_for(TodoApp::kNewBoardKey);
    CK_CHECK(add && fixture.app.execute_command(*add));
    CK_CHECK(fixture.app.dispatch(TextEvent{"Release", false, false}));
    CK_CHECK(fixture.app.dispatch(key(Key::Enter)));
    fixture.app.step(fixture.monotonic.now_nanos());
    const TodoWorkspace* workspace = fixture.todo.controller().workspace();
    CK_CHECK(workspace->snapshot().boards.size() == 2);
    CK_CHECK(workspace->snapshot().last_board_id == BoardId{2});
    CK_CHECK(fixture.todo.board_view()->board_id() == BoardId{2});
    CK_CHECK(fixture.todo.board_window()->title() == "TODO — Release");
    bool found_board = false;
    for (const widgets::StatusLineItem& item : fixture.todo.status_line()->items())
        if (item.label == "Board: Release") found_board = true;
    CK_CHECK(found_board);
}

CK_TEST(todo_lane_and_board_manager_shortcuts_are_board_scoped) {
    SmokeFixture fixture;
    CK_CHECK(fixture.app.commands().command_for_key(KeyChord{Key::F7, Modifier::None, {}}) ==
             fixture.app.commands().id_for(TodoApp::kLaneActionsKey));
    CK_CHECK(fixture.app.commands().command_for_key(KeyChord{Key::Char, Modifier::None, "m"}) ==
             fixture.app.commands().id_for(TodoApp::kBoardManagerKey));
    const auto manager = fixture.app.commands().id_for(TodoApp::kBoardManagerKey);
    CK_CHECK(manager && fixture.app.execute_command(*manager));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(!fixture.app.is_modal());

    CK_CHECK(fixture.app.dispatch(
        KeyEvent{KeyChord{Key::F10, Modifier::Shift, {}}, KeyAction::Press, false}));
    CK_CHECK(fixture.app.input_capture() != nullptr);
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
}

CK_TEST(todo_pointer_drag_moves_through_the_same_durable_controller_path) {
    SmokeFixture fixture;
    const Rect source_lane = fixture.todo.board_view()->lane_view(LaneId{1})->absolute_bounds();
    const Rect target_lane = fixture.todo.board_view()->lane_view(LaneId{2})->absolute_bounds();
    const Point source{source_lane.x + 3, source_lane.y + 1};
    const Point target{target_lane.x + 3, target_lane.y + 1};
    CK_CHECK(fixture.app.dispatch(
        MouseEvent{MouseAction::Down, MouseButton::Left, source, std::nullopt, Modifier::None}));
    CK_CHECK(fixture.app.dispatch(
        MouseEvent{MouseAction::Move, MouseButton::None, target, std::nullopt, Modifier::None}));
    CK_CHECK(fixture.app.dispatch(
        MouseEvent{MouseAction::Up, MouseButton::Left, target, std::nullopt, Modifier::None}));
    CK_CHECK(fixture.todo.controller().workspace()->lane_of(TaskId{1}) == LaneId{2});
}

CK_TEST(todo_task_right_click_opens_command_context_menu_and_escape_dismisses_it) {
    SmokeFixture fixture;
    const Rect lane = fixture.todo.board_view()->lane_view(LaneId{1})->absolute_bounds();
    CK_CHECK(fixture.app.dispatch(MouseEvent{MouseAction::Down, MouseButton::Right,
                                             Point{lane.x + 3, lane.y + 1}, std::nullopt, Modifier::None}));
    CK_CHECK(fixture.app.input_capture() != nullptr);
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
    CK_CHECK(fixture.app.input_capture() == nullptr);
}

CK_TEST(todo_about_reports_build_and_workspace_facts) {
    SmokeFixture fixture;
    const auto about = fixture.app.commands().id_for("todo.about");
    CK_CHECK(about && fixture.app.execute_command(*about));
    CK_CHECK(fixture.app.is_modal());
    CK_CHECK(fixture.app.dispatch(key(Key::Enter)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(!fixture.app.is_modal());
}

CK_TEST(todo_transient_status_expires_to_context_and_move_guidance) {
    SmokeFixture fixture;
    const auto move = fixture.app.commands().id_for(TodoApp::kMoveTaskKey);
    CK_CHECK(move && fixture.app.execute_command(*move));
    fixture.monotonic.advance(3'000'000'000);
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(fixture.todo.status_line()->current_hint() == "Arrow keys move; Enter commits; Esc cancels.");
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
}

CK_TEST(todo_q_alias_is_scoped_to_the_board) {
    SmokeFixture fixture;
    const auto edit = fixture.app.commands().id_for(TodoApp::kEditTaskKey);
    CK_CHECK(edit && fixture.app.execute_command(*edit));
    CK_CHECK(fixture.app.dispatch(key(Key::Char, "q")));
    CK_CHECK(!fixture.app.quit_requested());
    CK_CHECK(fixture.app.dispatch(key(Key::Escape)));
    fixture.app.step(fixture.monotonic.now_nanos());
    CK_CHECK(fixture.app.dispatch(key(Key::Char, "q")));
    CK_CHECK(fixture.app.quit_requested());
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
