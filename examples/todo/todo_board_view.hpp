// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

#include "todo_lane_view.hpp"

namespace ckv::todo {

class TodoBoardView final : public ui::View {
public:
    TodoBoardView();

    bool set_board(const TodoWorkspace& workspace, BoardId board_id);
    BoardId board_id() const noexcept { return board_id_; }
    std::optional<LaneId> active_lane() const noexcept;
    std::optional<TaskId> selected_task() const noexcept;
    TodoLaneView* lane_view(LaneId lane_id) const noexcept;
    std::size_t lane_count() const noexcept { return lanes_.size(); }

    bool select_task(TaskId task_id);
    void set_move_source(std::optional<TaskId> task_id);
    void set_move_target(std::optional<LaneId> lane_id);

    std::function<void(TaskId)> on_selection_changed;
    std::function<void(TaskId)> on_task_activate;
    std::function<void(TaskId)> on_move_toggle;
    std::function<void(LaneId)> on_active_lane_changed;

    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_resized() override;
    bool on_mouse(const MouseEvent& event) override;

private:
    void rebuild(const TodoWorkspace& workspace, const Board& board);
    void layout_lanes();
    void activate_lane(LaneId lane_id);
    void step_lane(LaneId lane_id, int direction);
    void ensure_lane_visible(std::size_t index);

    BoardId board_id_;
    std::vector<TodoLaneView*> lanes_;
    std::size_t active_lane_index_ = 0;
    std::optional<TaskId> move_source_;
    std::optional<LaneId> move_target_;
    widgets::Scrollbar* scrollbar_ = nullptr;
};

}  // namespace ckv::todo
