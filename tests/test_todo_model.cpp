// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_model.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "cvision/testing/cktest.hpp"

namespace {

using namespace ckv::todo;

AuditStamp stamp() { return {IsoTimestamp{"2026-08-25T10:30:00Z"}, "demo-user"}; }

WorkspaceSnapshot valid_snapshot() { return TodoWorkspace::empty().snapshot(); }

ModelErrorCode error_for(WorkspaceSnapshot snapshot) {
    return TodoWorkspace::from_snapshot(std::move(snapshot)).error.code;
}

Task one_task() {
    return Task{TaskId{1},
                "Ship the demo",
                "Complete the deterministic model",
                "A note\nwith two lines.",
                Priority::High,
                IsoDate{"2026-08-31"},
                TodoColor::Blue,
                IsoTimestamp{"2026-08-25T10:30:00Z"},
                "creator",
                IsoTimestamp{"2026-08-25T10:31:00Z"},
                "editor"};
}

WorkspaceSnapshot snapshot_with_task() {
    WorkspaceSnapshot snapshot = valid_snapshot();
    snapshot.tasks.push_back(one_task());
    snapshot.boards.front().lanes.front().task_ids.push_back(TaskId{1});
    snapshot.next_task_id = 2;
    return snapshot;
}

}  // namespace

CK_TEST(todo_empty_factory_has_main_and_three_default_lanes) {
    const TodoWorkspace workspace = TodoWorkspace::empty();
    CK_CHECK(workspace.snapshot().schema_version == todo_schema_version);
    CK_CHECK(workspace.snapshot().last_board_id == BoardId{1});
    CK_CHECK(workspace.snapshot().boards.size() == 1);
    CK_CHECK(workspace.snapshot().boards.front().name == "main");
    CK_CHECK(workspace.snapshot().boards.front().lanes.size() == 3);
    CK_CHECK(workspace.snapshot().boards.front().lanes[0].title == "To Do");
    CK_CHECK(workspace.snapshot().boards.front().lanes[1].title == "Doing");
    CK_CHECK(workspace.snapshot().boards.front().lanes[2].title == "Done");
    CK_CHECK(workspace.snapshot().next_board_id == 2);
    CK_CHECK(workspace.snapshot().next_lane_id == 4);
    CK_CHECK(workspace.snapshot().next_task_id == 1);
    CK_CHECK(workspace.generation() == 0);
}

CK_TEST(todo_guided_factory_creates_three_ordinary_audited_tasks) {
    const auto result = TodoWorkspace::guided(stamp());
    CK_CHECK(result);
    if (!result) return;
    const TodoWorkspace& workspace = *result.value;
    CK_CHECK(workspace.snapshot().tasks.size() == 3);
    CK_CHECK(workspace.snapshot().next_task_id == 4);
    CK_CHECK(workspace.snapshot().boards.front().lanes[0].task_ids == std::vector<TaskId>{TaskId{1}});
    CK_CHECK(workspace.snapshot().boards.front().lanes[1].task_ids == std::vector<TaskId>{TaskId{2}});
    CK_CHECK(workspace.snapshot().boards.front().lanes[2].task_ids == std::vector<TaskId>{TaskId{3}});
    for (const Task& task : workspace.snapshot().tasks) {
        CK_CHECK(task.created_at == stamp().timestamp);
        CK_CHECK(task.modified_at == stamp().timestamp);
        CK_CHECK(task.created_by == stamp().identity);
        CK_CHECK(task.modified_by == stamp().identity);
    }
}

CK_TEST(todo_guided_factory_rejects_invalid_audit_facts) {
    CK_CHECK(!TodoWorkspace::guided({IsoTimestamp{"25 August"}, "demo-user"}));
    CK_CHECK(!TodoWorkspace::guided({IsoTimestamp{"2026-08-25T10:30:00Z"}, ""}));
}

CK_TEST(todo_snapshot_round_trip_preserves_every_field_and_lookup) {
    const WorkspaceSnapshot expected = snapshot_with_task();
    auto result = TodoWorkspace::from_snapshot(expected);
    CK_CHECK(result);
    if (!result) return;
    CK_CHECK(result.value->snapshot() == expected);
    CK_CHECK(result.value->find_board(BoardId{1}) != nullptr);
    CK_CHECK(result.value->find_lane(LaneId{1}) != nullptr);
    CK_CHECK(result.value->find_task(TaskId{1}) != nullptr);
    if (result.value->find_task(TaskId{1}) == nullptr) return;
    CK_CHECK(result.value->board_of(LaneId{1}) == BoardId{1});
    CK_CHECK(result.value->lane_of(TaskId{1}) == LaneId{1});
    CK_CHECK(result.value->find_task(TaskId{1})->note == "A note\nwith two lines.");
    CK_CHECK(result.value->find_board(BoardId{99}) == nullptr);
    CK_CHECK(!result.value->board_of(LaneId{99}));
    CK_CHECK(!result.value->lane_of(TaskId{99}));
}

CK_TEST(todo_snapshot_rejects_schema_main_and_selection_errors) {
    auto snapshot = valid_snapshot();
    snapshot.schema_version = 2;
    CK_CHECK(error_for(snapshot) == ModelErrorCode::UnknownSchema);

    snapshot = valid_snapshot();
    snapshot.boards.front().name = "not-main";
    CK_CHECK(error_for(snapshot) == ModelErrorCode::ProtectedMainBoard);

    snapshot = valid_snapshot();
    snapshot.last_board_id = BoardId{99};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidLastBoard);
}

CK_TEST(todo_snapshot_rejects_zero_duplicate_and_nonmonotonic_ids) {
    auto snapshot = valid_snapshot();
    snapshot.boards.front().id = BoardId{};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidId);

    snapshot = valid_snapshot();
    snapshot.boards.front().lanes[1].id = snapshot.boards.front().lanes[0].id;
    CK_CHECK(error_for(snapshot) == ModelErrorCode::DuplicateId);

    snapshot = snapshot_with_task();
    snapshot.next_task_id = 1;
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidCounter);
}

CK_TEST(todo_snapshot_rejects_dangling_duplicate_and_unowned_tasks) {
    auto snapshot = valid_snapshot();
    snapshot.boards.front().lanes.front().task_ids = {TaskId{99}};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::DanglingTask);

    snapshot = snapshot_with_task();
    snapshot.boards.front().lanes[1].task_ids = {TaskId{1}};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::DuplicateTaskMembership);

    snapshot = snapshot_with_task();
    snapshot.boards.front().lanes.front().task_ids.clear();
    CK_CHECK(error_for(snapshot) == ModelErrorCode::UnownedTask);
}

CK_TEST(todo_snapshot_rejects_invalid_utf8_empty_and_oversized_text) {
    auto snapshot = valid_snapshot();
    snapshot.boards.front().name = std::string("bad\xFF", 4);
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidUtf8);

    snapshot = valid_snapshot();
    snapshot.boards.front().lanes.front().title.clear();
    CK_CHECK(error_for(snapshot) == ModelErrorCode::EmptyText);

    snapshot = snapshot_with_task();
    snapshot.tasks.front().title.assign(TodoLimits::max_title_bytes + 1, 'x');
    CK_CHECK(error_for(snapshot) == ModelErrorCode::TextLimit);

    snapshot = snapshot_with_task();
    snapshot.tasks.front().title = "two\tcolumns";
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidText);

    snapshot = snapshot_with_task();
    snapshot.tasks.front().note = "tabs\tand\nlines are valid";
    CK_CHECK(TodoWorkspace::from_snapshot(snapshot));
}

CK_TEST(todo_snapshot_rejects_unknown_enum_values) {
    auto snapshot = snapshot_with_task();
    snapshot.tasks.front().priority = static_cast<Priority>(99);
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidValue);

    snapshot = valid_snapshot();
    snapshot.boards.front().lanes.front().sort = static_cast<SortMode>(99);
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidValue);
}

CK_TEST(todo_snapshot_accepts_real_leap_day_and_rejects_invalid_dates) {
    auto snapshot = snapshot_with_task();
    snapshot.tasks.front().due_date = IsoDate{"2024-02-29"};
    CK_CHECK(TodoWorkspace::from_snapshot(snapshot));

    snapshot.tasks.front().due_date = IsoDate{"2025-02-29"};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidDate);
    snapshot.tasks.front().due_date = IsoDate{"2026-13-01"};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidDate);
}

CK_TEST(todo_snapshot_rejects_invalid_and_regressing_timestamps) {
    auto snapshot = snapshot_with_task();
    snapshot.tasks.front().modified_at = IsoTimestamp{"2026-08-25T25:00:00Z"};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::InvalidTimestamp);

    snapshot = snapshot_with_task();
    snapshot.tasks.front().modified_at = IsoTimestamp{"2026-08-25T10:29:59Z"};
    CK_CHECK(error_for(snapshot) == ModelErrorCode::TimestampRegression);
}

CK_TEST(todo_snapshot_enforces_board_lane_task_and_aggregate_limits) {
    auto snapshot = valid_snapshot();
    snapshot.boards.front().lanes.resize(TodoLimits::max_lanes_per_board + 1,
                                         Lane{LaneId{99}, "extra", std::nullopt, SortMode::Manual, {}});
    CK_CHECK(error_for(snapshot) == ModelErrorCode::WorkspaceLimit);

    snapshot = valid_snapshot();
    snapshot.boards.clear();
    CK_CHECK(error_for(snapshot) == ModelErrorCode::WorkspaceLimit);

    snapshot = snapshot_with_task();
    snapshot.tasks.front().note.assign(TodoLimits::max_note_bytes, 'n');
    snapshot.tasks.front().details.assign(TodoLimits::max_details_bytes, 'd');
    CK_CHECK(TodoWorkspace::from_snapshot(snapshot));
}

CK_TEST(todo_snapshot_allows_duplicate_lane_titles_but_not_board_names) {
    auto snapshot = valid_snapshot();
    snapshot.boards.front().lanes[1].title = snapshot.boards.front().lanes[0].title;
    CK_CHECK(TodoWorkspace::from_snapshot(snapshot));

    snapshot = valid_snapshot();
    snapshot.boards.push_back(Board{BoardId{2}, "main", {Lane{LaneId{4}, "Only", std::nullopt, {}, {}}}});
    snapshot.next_board_id = 3;
    snapshot.next_lane_id = 5;
    CK_CHECK(error_for(snapshot) == ModelErrorCode::DuplicateBoardName);
}
