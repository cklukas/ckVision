// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

#include "todo_model.hpp"

namespace ckv::todo {

class TodoLaneView final : public ui::View {
public:
    TodoLaneView();

    void set_lane(const TodoWorkspace& workspace, LaneId lane_id);
    LaneId lane_id() const noexcept { return lane_.id; }
    const Lane& lane() const noexcept { return lane_; }
    std::optional<TaskId> selected_task() const noexcept;
    void select_task(TaskId task_id);

    void set_move_source(std::optional<TaskId> task_id);
    void set_move_target(bool target);
    bool move_target() const noexcept { return move_target_; }

    std::function<void(TaskId)> on_selection_changed;
    std::function<void(TaskId)> on_activate;
    std::function<void(TaskId)> on_move_toggle;
    std::function<void(LaneId, int)> on_lane_step;
    std::function<void(LaneId)> on_focused;

    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_resized() override;
    void on_attached() override;
    void on_focus(const FocusEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void draw(scene::Painter& painter) override;

private:
    int visible_cards() const noexcept;
    void move_selection(int index);
    void ensure_selection_visible();
    Style lane_style() const;
    Style frame_style() const;
    Style task_marker_style(const Task& task, Style base) const;

    Lane lane_;
    std::vector<Task> tasks_;
    int selected_ = -1;
    bool focused_ = false;
    bool move_target_ = false;
    std::optional<TaskId> move_source_;
    widgets::Scrollbar* scrollbar_ = nullptr;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId inactive_selected_role_ = ui::kInvalidRole;
    ui::RoleId frame_active_role_ = ui::kInvalidRole;
    ui::RoleId frame_inactive_role_ = ui::kInvalidRole;
};

}  // namespace ckv::todo
