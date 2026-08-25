// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_model.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "cvision/core/utf8.hpp"

namespace ckv::todo {
namespace {

template <class T>
ModelResult<T> failure(ModelErrorCode code, std::string diagnostic) {
    return ModelResult<T>::failure(code, std::move(diagnostic));
}

bool contains_control(std::string_view text, bool allow_line_breaks) noexcept {
    for (const unsigned char byte : text) {
        if (byte == 0x7FU) return true;
        if (byte >= 0x20U) continue;
        if (allow_line_breaks && (byte == '\t' || byte == '\r' || byte == '\n')) continue;
        return true;
    }
    return false;
}

std::optional<ModelError> validate_text(std::string_view text,
                                        std::size_t limit,
                                        bool required,
                                        bool allow_line_breaks,
                                        std::string_view field) {
    if (required && text.empty()) {
        return ModelError{ModelErrorCode::EmptyText, std::string(field) + " must not be empty"};
    }
    if (text.size() > limit) {
        return ModelError{ModelErrorCode::TextLimit, std::string(field) + " exceeds its byte limit"};
    }
    if (!utf8::is_valid(text)) {
        return ModelError{ModelErrorCode::InvalidUtf8, std::string(field) + " is not valid UTF-8"};
    }
    if (contains_control(text, allow_line_breaks)) {
        return ModelError{ModelErrorCode::InvalidText, std::string(field) + " contains a forbidden control"};
    }
    return std::nullopt;
}

bool all_digits(std::string_view value, std::size_t begin, std::size_t end) noexcept {
    for (std::size_t index = begin; index < end; ++index) {
        if (value[index] < '0' || value[index] > '9') return false;
    }
    return true;
}

int decimal(std::string_view value, std::size_t begin, std::size_t end) noexcept {
    int result = 0;
    for (std::size_t index = begin; index < end; ++index) result = result * 10 + (value[index] - '0');
    return result;
}

bool leap_year(int year) noexcept { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

bool valid_date(std::string_view value) noexcept {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    if (!all_digits(value, 0, 4) || !all_digits(value, 5, 7) || !all_digits(value, 8, 10)) return false;
    const int year = decimal(value, 0, 4);
    const int month = decimal(value, 5, 7);
    const int day = decimal(value, 8, 10);
    if (year == 0 || month < 1 || month > 12 || day < 1) return false;
    constexpr std::array<int, 12> month_days = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int limit = month == 2 && leap_year(year) ? 29 : month_days[static_cast<std::size_t>(month - 1)];
    return day <= limit;
}

bool valid_timestamp(std::string_view value) noexcept {
    if (value.size() != 20 || value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
        value[19] != 'Z') {
        return false;
    }
    if (!valid_date(value.substr(0, 10)) || !all_digits(value, 11, 13) || !all_digits(value, 14, 16) ||
        !all_digits(value, 17, 19)) {
        return false;
    }
    return decimal(value, 11, 13) <= 23 && decimal(value, 14, 16) <= 59 && decimal(value, 17, 19) <= 59;
}

bool valid_priority(Priority value) noexcept {
    return value >= Priority::High && value <= Priority::Idle;
}

bool valid_sort(SortMode value) noexcept {
    return value >= SortMode::Manual && value <= SortMode::Priority;
}

bool valid_color(TodoColor value) noexcept {
    return value >= TodoColor::Black && value <= TodoColor::White;
}

ModelResult<WorkspaceSnapshot> validate_snapshot(WorkspaceSnapshot snapshot) {
    if (snapshot.schema_version != todo_schema_version) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::UnknownSchema, "unsupported TODO schema version");
    }
    if (snapshot.boards.empty() || snapshot.boards.size() > TodoLimits::max_boards) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::WorkspaceLimit, "workspace must contain 1 to 64 boards");
    }
    if (snapshot.tasks.size() > TodoLimits::max_tasks) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::WorkspaceLimit, "workspace task limit exceeded");
    }

    std::unordered_set<std::uint64_t> board_ids;
    std::unordered_set<std::uint64_t> lane_ids;
    std::unordered_set<std::uint64_t> task_ids;
    std::unordered_set<std::string> board_names;
    std::unordered_map<std::uint64_t, std::size_t> memberships;
    std::uint64_t known_string_bytes = 0;
    std::uint64_t max_board_id = 0;
    std::uint64_t max_lane_id = 0;
    std::uint64_t max_task_id = 0;
    std::size_t lane_count = 0;
    bool has_last_board = false;
    bool has_main = false;

    for (const Task& task : snapshot.tasks) {
        if (task.id.value == 0) return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidId, "task id is zero");
        if (!task_ids.insert(task.id.value).second) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::DuplicateId, "duplicate task id");
        }
        max_task_id = std::max(max_task_id, task.id.value);
        if (const auto error = validate_text(task.title, TodoLimits::max_title_bytes, true, false, "task title")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        if (const auto error =
                validate_text(task.details, TodoLimits::max_details_bytes, false, false, "task details")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        if (const auto error = validate_text(task.note, TodoLimits::max_note_bytes, false, true, "task note")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        if (!valid_priority(task.priority) || (task.color && !valid_color(*task.color))) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidValue, "task enum value is invalid");
        }
        if (task.due_date && !valid_date(task.due_date->value)) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidDate, "task due date is not YYYY-MM-DD");
        }
        if (!valid_timestamp(task.created_at.value) || !valid_timestamp(task.modified_at.value)) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidTimestamp, "task audit timestamp is invalid");
        }
        if (task.modified_at.value < task.created_at.value) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::TimestampRegression,
                                              "task modification precedes creation");
        }
        if (const auto error =
                validate_text(task.created_by, TodoLimits::max_identity_bytes, true, false, "creator identity")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        if (const auto error =
                validate_text(task.modified_by, TodoLimits::max_identity_bytes, true, false, "modifier identity")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        known_string_bytes += task.title.size() + task.details.size() + task.note.size() +
                              task.created_at.value.size() + task.created_by.size() + task.modified_at.value.size() +
                              task.modified_by.size() + (task.due_date ? task.due_date->value.size() : 0U);
    }

    for (const Board& board : snapshot.boards) {
        if (board.id.value == 0) return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidId, "board id is zero");
        if (!board_ids.insert(board.id.value).second) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::DuplicateId, "duplicate board id");
        }
        max_board_id = std::max(max_board_id, board.id.value);
        if (const auto error = validate_text(board.name, TodoLimits::max_name_bytes, true, false, "board name")) {
            return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
        }
        if (!board_names.insert(board.name).second) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::DuplicateBoardName, "duplicate board name");
        }
        if (board.id == BoardId{1}) {
            has_main = board.name == "main";
            if (!has_main) {
                return failure<WorkspaceSnapshot>(ModelErrorCode::ProtectedMainBoard,
                                                  "board 1 must be named main");
            }
        }
        if (board.id == snapshot.last_board_id) has_last_board = true;
        if (board.lanes.empty() || board.lanes.size() > TodoLimits::max_lanes_per_board) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::WorkspaceLimit,
                                              "each board must contain 1 to 64 lanes");
        }
        lane_count += board.lanes.size();
        known_string_bytes += board.name.size();
        for (const Lane& lane : board.lanes) {
            if (lane.id.value == 0) return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidId, "lane id is zero");
            if (!lane_ids.insert(lane.id.value).second) {
                return failure<WorkspaceSnapshot>(ModelErrorCode::DuplicateId, "duplicate lane id");
            }
            max_lane_id = std::max(max_lane_id, lane.id.value);
            if (const auto error = validate_text(lane.title, TodoLimits::max_name_bytes, true, false, "lane title")) {
                return failure<WorkspaceSnapshot>(error->code, error->diagnostic);
            }
            if (!valid_sort(lane.sort) || (lane.color && !valid_color(*lane.color))) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidValue, "lane enum value is invalid");
            }
            known_string_bytes += lane.title.size();
            for (TaskId task_id : lane.task_ids) {
                if (!task_ids.contains(task_id.value)) {
                    return failure<WorkspaceSnapshot>(ModelErrorCode::DanglingTask, "lane references an unknown task");
                }
                if (++memberships[task_id.value] != 1U) {
                    return failure<WorkspaceSnapshot>(ModelErrorCode::DuplicateTaskMembership,
                                                      "task belongs to more than one lane");
                }
            }
        }
    }

    if (!has_main) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::ProtectedMainBoard, "workspace has no main board");
    }
    if (!has_last_board) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidLastBoard, "last board does not exist");
    }
    if (lane_count > TodoLimits::max_lanes) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::WorkspaceLimit, "workspace lane limit exceeded");
    }
    for (std::uint64_t task_id : task_ids) {
        if (!memberships.contains(task_id)) {
            return failure<WorkspaceSnapshot>(ModelErrorCode::UnownedTask, "task does not belong to a lane");
        }
    }
    if (known_string_bytes > TodoLimits::max_known_string_bytes) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::WorkspaceLimit,
                                          "aggregate known string byte limit exceeded");
    }
    if (max_board_id == std::numeric_limits<std::uint64_t>::max() ||
        snapshot.next_board_id <= max_board_id || max_lane_id == std::numeric_limits<std::uint64_t>::max() ||
        snapshot.next_lane_id <= max_lane_id || max_task_id == std::numeric_limits<std::uint64_t>::max() ||
        snapshot.next_task_id <= max_task_id) {
        return failure<WorkspaceSnapshot>(ModelErrorCode::InvalidCounter,
                                          "next id counters must exceed all stored ids");
    }
    return ModelResult<WorkspaceSnapshot>::success(std::move(snapshot));
}

Task guided_task(TaskId id, std::string title, std::string details, const AuditStamp& stamp) {
    return Task{id,
                std::move(title),
                std::move(details),
                {},
                Priority::Normal,
                std::nullopt,
                std::nullopt,
                stamp.timestamp,
                stamp.identity,
                stamp.timestamp,
                stamp.identity};
}

}  // namespace

ModelResult<TodoWorkspace> TodoWorkspace::from_snapshot(WorkspaceSnapshot snapshot) {
    auto validated = validate_snapshot(std::move(snapshot));
    if (!validated) return failure<TodoWorkspace>(validated.error.code, std::move(validated.error.diagnostic));
    return ModelResult<TodoWorkspace>::success(TodoWorkspace(std::move(*validated.value)));
}

TodoWorkspace TodoWorkspace::empty() {
    WorkspaceSnapshot snapshot;
    snapshot.last_board_id = BoardId{1};
    snapshot.next_board_id = 2;
    snapshot.next_lane_id = 4;
    snapshot.next_task_id = 1;
    snapshot.boards = {Board{BoardId{1},
                             "main",
                             {Lane{LaneId{1}, "To Do", std::nullopt, SortMode::Manual, {}},
                              Lane{LaneId{2}, "Doing", std::nullopt, SortMode::Manual, {}},
                              Lane{LaneId{3}, "Done", std::nullopt, SortMode::Manual, {}}}}};
    return TodoWorkspace(std::move(snapshot));
}

ModelResult<TodoWorkspace> TodoWorkspace::guided(const AuditStamp& stamp) {
    WorkspaceSnapshot snapshot = empty().snapshot();
    snapshot.next_task_id = 4;
    snapshot.tasks = {guided_task(TaskId{1}, "Add your first task", "Press F2 or Insert", stamp),
                      guided_task(TaskId{2}, "Open contextual help", "Press F1 anywhere", stamp),
                      guided_task(TaskId{3}, "Try move mode", "Select a task and press F9", stamp)};
    snapshot.boards.front().lanes[0].task_ids = {TaskId{1}};
    snapshot.boards.front().lanes[1].task_ids = {TaskId{2}};
    snapshot.boards.front().lanes[2].task_ids = {TaskId{3}};
    return from_snapshot(std::move(snapshot));
}

const Board* TodoWorkspace::find_board(BoardId id) const noexcept {
    const auto found = std::find_if(snapshot_.boards.begin(), snapshot_.boards.end(),
                                    [id](const Board& board) { return board.id == id; });
    return found == snapshot_.boards.end() ? nullptr : &*found;
}

const Lane* TodoWorkspace::find_lane(LaneId id) const noexcept {
    for (const Board& board : snapshot_.boards) {
        const auto found = std::find_if(board.lanes.begin(), board.lanes.end(),
                                        [id](const Lane& lane) { return lane.id == id; });
        if (found != board.lanes.end()) return &*found;
    }
    return nullptr;
}

const Task* TodoWorkspace::find_task(TaskId id) const noexcept {
    const auto found = std::find_if(snapshot_.tasks.begin(), snapshot_.tasks.end(),
                                    [id](const Task& task) { return task.id == id; });
    return found == snapshot_.tasks.end() ? nullptr : &*found;
}

std::optional<BoardId> TodoWorkspace::board_of(LaneId id) const noexcept {
    for (const Board& board : snapshot_.boards) {
        if (std::any_of(board.lanes.begin(), board.lanes.end(), [id](const Lane& lane) { return lane.id == id; })) {
            return board.id;
        }
    }
    return std::nullopt;
}

std::optional<LaneId> TodoWorkspace::lane_of(TaskId id) const noexcept {
    for (const Board& board : snapshot_.boards) {
        for (const Lane& lane : board.lanes) {
            if (std::find(lane.task_ids.begin(), lane.task_ids.end(), id) != lane.task_ids.end()) return lane.id;
        }
    }
    return std::nullopt;
}

}  // namespace ckv::todo
