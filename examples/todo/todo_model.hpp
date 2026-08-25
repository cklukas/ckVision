// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ckv::todo {

inline constexpr std::uint32_t todo_schema_version = 1;

struct BoardId {
    std::uint64_t value = 0;
    auto operator<=>(const BoardId&) const = default;
};

struct LaneId {
    std::uint64_t value = 0;
    auto operator<=>(const LaneId&) const = default;
};

struct TaskId {
    std::uint64_t value = 0;
    auto operator<=>(const TaskId&) const = default;
};

enum class Priority : std::uint8_t { High = 1, Normal = 2, Low = 3, Idle = 4 };
enum class SortMode : std::uint8_t { Manual, Color, Due, Created, Modified, Priority };
enum class TodoColor : std::uint8_t {
    Black,
    DarkBlue,
    DarkGreen,
    DarkCyan,
    DarkRed,
    DarkMagenta,
    Brown,
    LightGray,
    DarkGray,
    Blue,
    Green,
    Cyan,
    Red,
    Magenta,
    Yellow,
    White,
};

struct TodoLimits {
    static constexpr std::size_t max_boards = 64;
    static constexpr std::size_t max_lanes_per_board = 64;
    static constexpr std::size_t max_lanes = 512;
    static constexpr std::size_t max_tasks = 16'384;
    static constexpr std::size_t max_name_bytes = 96;
    static constexpr std::size_t max_title_bytes = 512;
    static constexpr std::size_t max_details_bytes = 4 * 1024;
    static constexpr std::size_t max_note_bytes = 256 * 1024;
    static constexpr std::size_t max_identity_bytes = 256;
    static constexpr std::size_t max_known_string_bytes = 1024 * 1024;
};

struct IsoDate {
    std::string value;
    friend bool operator==(const IsoDate&, const IsoDate&) = default;
};

struct IsoTimestamp {
    std::string value;
    friend bool operator==(const IsoTimestamp&, const IsoTimestamp&) = default;
};

struct AuditStamp {
    IsoTimestamp timestamp;
    std::string identity;
    friend bool operator==(const AuditStamp&, const AuditStamp&) = default;
};

struct TaskDraft {
    std::string title;
    std::string details;
    std::string note;
    Priority priority = Priority::Normal;
    std::optional<IsoDate> due_date;
    std::optional<TodoColor> color;
    friend bool operator==(const TaskDraft&, const TaskDraft&) = default;
};

struct Task {
    TaskId id;
    std::string title;
    std::string details;
    std::string note;
    Priority priority = Priority::Normal;
    std::optional<IsoDate> due_date;
    std::optional<TodoColor> color;
    IsoTimestamp created_at;
    std::string created_by;
    IsoTimestamp modified_at;
    std::string modified_by;
    friend bool operator==(const Task&, const Task&) = default;
};

struct Lane {
    LaneId id;
    std::string title;
    std::optional<TodoColor> color;
    SortMode sort = SortMode::Manual;
    std::vector<TaskId> task_ids;
    friend bool operator==(const Lane&, const Lane&) = default;
};

struct Board {
    BoardId id;
    std::string name;
    std::vector<Lane> lanes;
    friend bool operator==(const Board&, const Board&) = default;
};

struct WorkspaceSnapshot {
    std::uint32_t schema_version = todo_schema_version;
    BoardId last_board_id;
    std::uint64_t next_board_id = 1;
    std::uint64_t next_lane_id = 1;
    std::uint64_t next_task_id = 1;
    std::vector<Board> boards;
    std::vector<Task> tasks;
    friend bool operator==(const WorkspaceSnapshot&, const WorkspaceSnapshot&) = default;
};

enum class ModelErrorCode {
    None,
    InvalidUtf8,
    EmptyText,
    InvalidText,
    TextLimit,
    WorkspaceLimit,
    InvalidDate,
    InvalidTimestamp,
    TimestampRegression,
    InvalidValue,
    InvalidId,
    DuplicateId,
    InvalidCounter,
    UnknownSchema,
    BoardNotFound,
    LaneNotFound,
    TaskNotFound,
    DuplicateBoardName,
    InvalidLastBoard,
    DanglingTask,
    DuplicateTaskMembership,
    UnownedTask,
    ProtectedMainBoard,
    FinalLane,
    SameSourceAndTarget,
    CrossBoardLaneMerge,
    InvalidInsertionAnchor,
    ManualOrderRequired,
    StaleTransaction,
};

struct ModelError {
    ModelErrorCode code = ModelErrorCode::None;
    std::string diagnostic;
    friend bool operator==(const ModelError&, const ModelError&) = default;
};

template <class T>
struct ModelResult;

class TodoWorkspace {
public:
    static ModelResult<TodoWorkspace> from_snapshot(WorkspaceSnapshot snapshot);
    static TodoWorkspace empty();
    static ModelResult<TodoWorkspace> guided(const AuditStamp& stamp);

    const WorkspaceSnapshot& snapshot() const noexcept { return snapshot_; }
    std::uint64_t generation() const noexcept { return generation_; }

    const Board* find_board(BoardId id) const noexcept;
    const Lane* find_lane(LaneId id) const noexcept;
    const Task* find_task(TaskId id) const noexcept;
    std::optional<BoardId> board_of(LaneId id) const noexcept;
    std::optional<LaneId> lane_of(TaskId id) const noexcept;

private:
    explicit TodoWorkspace(WorkspaceSnapshot snapshot) : snapshot_(std::move(snapshot)) {}

    WorkspaceSnapshot snapshot_;
    std::uint64_t generation_ = 0;
};

template <class T>
struct ModelResult {
    std::optional<T> value;
    ModelError error;

    explicit operator bool() const noexcept { return value.has_value(); }

    static ModelResult success(T result) {
        ModelResult out;
        out.value.emplace(std::move(result));
        return out;
    }

    static ModelResult failure(ModelErrorCode code, std::string diagnostic) {
        ModelResult out;
        out.error = ModelError{code, std::move(diagnostic)};
        return out;
    }
};

}  // namespace ckv::todo
