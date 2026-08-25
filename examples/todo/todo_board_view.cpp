// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_board_view.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "cvision/ui/application.hpp"

namespace ckv::todo {
namespace {

constexpr int kLaneGap = 1;
constexpr int kMinimumLaneWidth = 20;
constexpr int kPreferredLaneWidth = 26;
constexpr int kMaximumLaneWidth = 34;

}  // namespace

TodoBoardView::TodoBoardView() {
    set_help_context_key("todo.board");
    scrollbar_ = make<widgets::Scrollbar>(widgets::Orientation::Horizontal);
    scrollbar_->set_policy(widgets::ScrollbarPolicy::Auto);
    scrollbar_->on_position_changed = [this](int) { layout_lanes(); };
}

bool TodoBoardView::set_board(const TodoWorkspace& workspace, BoardId board_id) {
    const Board* board = workspace.find_board(board_id);
    if (board == nullptr) return false;
    rebuild(workspace, *board);
    board_id_ = board_id;
    layout_lanes();
    invalidate();
    return true;
}

std::optional<LaneId> TodoBoardView::active_lane() const noexcept {
    if (active_lane_index_ >= lanes_.size()) return std::nullopt;
    return lanes_[active_lane_index_]->lane_id();
}

std::optional<TaskId> TodoBoardView::selected_task() const noexcept {
    if (active_lane_index_ >= lanes_.size()) return std::nullopt;
    return lanes_[active_lane_index_]->selected_task();
}

TodoLaneView* TodoBoardView::lane_view(LaneId lane_id) const noexcept {
    const auto found = std::find_if(lanes_.begin(), lanes_.end(),
                                    [lane_id](const TodoLaneView* lane) { return lane->lane_id() == lane_id; });
    return found == lanes_.end() ? nullptr : *found;
}

bool TodoBoardView::select_task(TaskId task_id) {
    for (std::size_t index = 0; index < lanes_.size(); ++index) {
        TodoLaneView* lane = lanes_[index];
        const auto& ids = lane->lane().task_ids;
        if (std::find(ids.begin(), ids.end(), task_id) == ids.end()) continue;
        lane->select_task(task_id);
        active_lane_index_ = index;
        ensure_lane_visible(index);
        if (context().app != nullptr) context().app->set_focus(lane);
        return true;
    }
    return false;
}

void TodoBoardView::set_move_source(std::optional<TaskId> task_id) {
    move_source_ = task_id;
    for (TodoLaneView* lane : lanes_) lane->set_move_source(task_id);
}

void TodoBoardView::set_move_target(std::optional<LaneId> lane_id) {
    move_target_ = lane_id;
    for (TodoLaneView* lane : lanes_) lane->set_move_target(lane_id == lane->lane_id());
}

ui::SizeHint TodoBoardView::horizontal_size_hint() const {
    return {kMinimumLaneWidth, kPreferredLaneWidth * 3 + kLaneGap * 2, ui::kUnboundedExtent};
}

ui::SizeHint TodoBoardView::vertical_size_hint() const { return {6, 18, ui::kUnboundedExtent}; }

void TodoBoardView::on_resized() { layout_lanes(); }

bool TodoBoardView::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Wheel) return false;
    if (event.button == MouseButton::WheelLeft) scrollbar_->set_position(scrollbar_->position() - kPreferredLaneWidth);
    else if (event.button == MouseButton::WheelRight)
        scrollbar_->set_position(scrollbar_->position() + kPreferredLaneWidth);
    else return false;
    return true;
}

void TodoBoardView::rebuild(const TodoWorkspace& workspace, const Board& board) {
    std::map<LaneId, std::optional<TaskId>> selections;
    for (TodoLaneView* lane : lanes_) selections.emplace(lane->lane_id(), lane->selected_task());
    const std::optional<LaneId> previous_active = active_lane();
    for (TodoLaneView* lane : lanes_) remove_child(lane);
    lanes_.clear();

    for (const Lane& lane : board.lanes) {
        auto view = std::make_unique<TodoLaneView>();
        TodoLaneView* observer = view.get();
        observer->set_lane(workspace, lane.id);
        if (const auto found = selections.find(lane.id); found != selections.end() && found->second)
            observer->select_task(*found->second);
        observer->set_move_source(move_source_);
        observer->set_move_target(move_target_ == lane.id);
        observer->on_selection_changed = [this, lane_id = lane.id](TaskId task_id) {
            activate_lane(lane_id);
            if (on_selection_changed) on_selection_changed(task_id);
        };
        observer->on_activate = [this](TaskId task_id) {
            if (on_task_activate) on_task_activate(task_id);
        };
        observer->on_move_toggle = [this](TaskId task_id) {
            if (on_move_toggle) on_move_toggle(task_id);
        };
        observer->on_lane_step = [this](LaneId lane_id, int direction) { step_lane(lane_id, direction); };
        observer->on_focused = [this](LaneId lane_id) { activate_lane(lane_id); };
        lanes_.push_back(add(std::move(view)));
    }

    active_lane_index_ = 0;
    if (previous_active) {
        const auto found = std::find_if(lanes_.begin(), lanes_.end(), [previous_active](const TodoLaneView* lane) {
            return lane->lane_id() == *previous_active;
        });
        if (found != lanes_.end()) active_lane_index_ = static_cast<std::size_t>(found - lanes_.begin());
    }
}

void TodoBoardView::layout_lanes() {
    if (scrollbar_ == nullptr) return;
    const int width = std::max(0, bounds().width);
    const int count = static_cast<int>(lanes_.size());
    const int visible_columns = std::max(1, std::min(3, count));
    const int distributed = count == 0 ? kPreferredLaneWidth : (width - kLaneGap * (visible_columns - 1)) / visible_columns;
    const int lane_width = std::clamp(distributed, kMinimumLaneWidth, kMaximumLaneWidth);
    const int extent = count == 0 ? 0 : count * lane_width + (count - 1) * kLaneGap;
    scrollbar_->set_range(extent, std::max(1, width));
    const bool show_scrollbar = scrollbar_->should_show();
    const int lane_height = std::max(0, bounds().height - (show_scrollbar ? 1 : 0));
    scrollbar_->set_bounds(Rect{0, std::max(0, bounds().height - 1), width,
                                show_scrollbar ? std::min(1, bounds().height) : 0});
    for (int index = 0; index < count; ++index) {
        const int x = index * (lane_width + kLaneGap) - scrollbar_->position();
        lanes_[static_cast<std::size_t>(index)]->set_bounds(Rect{x, 0, lane_width, lane_height});
    }
}

void TodoBoardView::activate_lane(LaneId lane_id) {
    const auto found = std::find_if(lanes_.begin(), lanes_.end(),
                                    [lane_id](const TodoLaneView* lane) { return lane->lane_id() == lane_id; });
    if (found == lanes_.end()) return;
    const std::size_t index = static_cast<std::size_t>(found - lanes_.begin());
    if (active_lane_index_ == index) return;
    active_lane_index_ = index;
    ensure_lane_visible(index);
    if (on_active_lane_changed) on_active_lane_changed(lane_id);
}

void TodoBoardView::step_lane(LaneId lane_id, int direction) {
    const auto found = std::find_if(lanes_.begin(), lanes_.end(),
                                    [lane_id](const TodoLaneView* lane) { return lane->lane_id() == lane_id; });
    if (found == lanes_.end() || lanes_.empty()) return;
    const int index = static_cast<int>(found - lanes_.begin());
    const int target = std::clamp(index + direction, 0, static_cast<int>(lanes_.size()) - 1);
    active_lane_index_ = static_cast<std::size_t>(target);
    ensure_lane_visible(active_lane_index_);
    if (context().app != nullptr) context().app->set_focus(lanes_[active_lane_index_]);
    if (on_active_lane_changed) on_active_lane_changed(lanes_[active_lane_index_]->lane_id());
}

void TodoBoardView::ensure_lane_visible(std::size_t index) {
    if (index >= lanes_.size() || scrollbar_ == nullptr) return;
    const Rect lane = lanes_[index]->bounds();
    const int left = lane.x + scrollbar_->position();
    const int right = left + lane.width;
    if (left < scrollbar_->position()) scrollbar_->set_position(left);
    else if (right > scrollbar_->position() + bounds().width)
        scrollbar_->set_position(right - bounds().width);
}

}  // namespace ckv::todo
