// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_lane_view.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/menu.hpp"

namespace ckv::todo {
namespace {

constexpr int kCardRows = 2;

std::string priority_mark(Priority priority) {
    switch (priority) {
        case Priority::High: return "↑";
        case Priority::Normal: return "•";
        case Priority::Low: return "↓";
        case Priority::Idle: return "⌛";
    }
    return "?";
}

Color palette_color(TodoColor color) {
    return Color::indexed(static_cast<std::uint8_t>(color));
}

bool light_palette_color(TodoColor color) noexcept {
    switch (color) {
        case TodoColor::Brown:
        case TodoColor::LightGray:
        case TodoColor::DarkGray:
        case TodoColor::Green:
        case TodoColor::Cyan:
        case TodoColor::Yellow:
        case TodoColor::White: return true;
        default: return false;
    }
}

}  // namespace

TodoLaneView::TodoLaneView() : ui::View({}, ui::FocusPolicy::TabStop) {
    set_command_context("todo.board");
    set_help_context_key("todo.board");
    scrollbar_ = make<widgets::Scrollbar>(widgets::Orientation::Vertical);
    scrollbar_->set_policy(widgets::ScrollbarPolicy::Auto);
    scrollbar_->on_position_changed = [this](int) { invalidate(); };
}

void TodoLaneView::set_today(std::optional<IsoDate> today) {
    if (today_ == today) return;
    today_ = std::move(today);
    tomorrow_.reset();
    if (today_) {
        const auto parsed = widgets::parse_iso_date(today_->value);
        if (parsed) {
            const auto next = widgets::add_calendar_days(*parsed, 1);
            if (next) tomorrow_ = IsoDate{widgets::format_iso_date(*next)};
        }
    }
    invalidate();
}

void TodoLaneView::set_lane(const TodoWorkspace& workspace, LaneId lane_id) {
    const Lane* source = workspace.find_lane(lane_id);
    if (source == nullptr) return;
    const std::optional<TaskId> previous = selected_task();
    lane_ = *source;
    tasks_.clear();
    const auto ordered = workspace.ordered_tasks(lane_id);
    if (ordered) {
        tasks_.reserve(ordered.value->size());
        for (TaskId task_id : *ordered.value) {
            if (const Task* task = workspace.find_task(task_id)) tasks_.push_back(*task);
        }
    }
    selected_ = tasks_.empty() ? -1 : 0;
    if (previous) select_task(*previous);
    on_resized();
    ensure_selection_visible();
    invalidate();
}

std::optional<TaskId> TodoLaneView::selected_task() const noexcept {
    if (selected_ < 0 || static_cast<std::size_t>(selected_) >= tasks_.size()) return std::nullopt;
    return tasks_[static_cast<std::size_t>(selected_)].id;
}

std::optional<TaskId> TodoLaneView::task_at(Point screen_cell) const noexcept {
    const Rect absolute = absolute_bounds();
    const int row = screen_cell.y - absolute.y - 1;
    if (row < 0 || row >= bounds().height - 2) return std::nullopt;
    const int index = scrollbar_->position() + row / kCardRows;
    if (index < 0 || static_cast<std::size_t>(index) >= tasks_.size()) return std::nullopt;
    return tasks_[static_cast<std::size_t>(index)].id;
}

void TodoLaneView::select_task(TaskId task_id) {
    const auto found = std::find_if(tasks_.begin(), tasks_.end(),
                                    [task_id](const Task& task) { return task.id == task_id; });
    if (found == tasks_.end()) return;
    move_selection(static_cast<int>(found - tasks_.begin()));
}

void TodoLaneView::set_move_source(std::optional<TaskId> task_id) {
    if (move_source_ == task_id) return;
    move_source_ = task_id;
    invalidate();
}

void TodoLaneView::set_move_target(bool target) {
    if (move_target_ == target) return;
    move_target_ = target;
    invalidate();
}

void TodoLaneView::set_drop_marker(std::optional<TaskId> before, bool at_end) {
    if (drop_before_ == before && drop_at_end_ == at_end) return;
    drop_before_ = before;
    drop_at_end_ = at_end;
    invalidate();
}

ui::SizeHint TodoLaneView::horizontal_size_hint() const { return {20, 26, 36}; }

ui::SizeHint TodoLaneView::vertical_size_hint() const {
    const int preferred = std::clamp(static_cast<int>(tasks_.size()) * kCardRows + 2, 6, 16);
    return {4, preferred, ui::kUnboundedExtent};
}

int TodoLaneView::visible_cards() const noexcept { return std::max(1, (bounds().height - 2) / kCardRows); }

void TodoLaneView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 1, std::min(1, bounds().width),
                                std::max(0, bounds().height - 2)});
    scrollbar_->set_range(static_cast<int>(tasks_.size()), visible_cards());
    ensure_selection_visible();
}

void TodoLaneView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (inactive_selected_role_ == ui::kInvalidRole)
        inactive_selected_role_ = context().roles->find("ckv.list.selected.inactive");
    if (frame_active_role_ == ui::kInvalidRole)
        frame_active_role_ = context().roles->find("ckv.window.frame.active");
    if (frame_inactive_role_ == ui::kInvalidRole)
        frame_inactive_role_ = context().roles->find("ckv.window.frame.inactive");
}

void TodoLaneView::on_focus(const FocusEvent& event) {
    if (focused_ == event.gained) return;
    focused_ = event.gained;
    invalidate();
    if (focused_ && on_focused) on_focused(lane_.id);
}

void TodoLaneView::move_selection(int index) {
    if (tasks_.empty()) return;
    const int previous = selected_;
    selected_ = std::clamp(index, 0, static_cast<int>(tasks_.size()) - 1);
    ensure_selection_visible();
    invalidate();
    if (selected_ != previous && on_selection_changed) on_selection_changed(tasks_[selected_].id);
}

void TodoLaneView::ensure_selection_visible() {
    if (scrollbar_ == nullptr || selected_ < 0) return;
    if (selected_ < scrollbar_->position()) {
        scrollbar_->set_position(selected_);
    } else if (selected_ >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(selected_ - scrollbar_->viewport_size() + 1);
    }
}

bool TodoLaneView::on_key(const KeyEvent& event) {
    if (widgets::is_keyboard_context_menu_request(event)) {
        const Rect absolute = absolute_bounds();
        if (const auto task = selected_task(); task && on_task_context_menu) {
            const int row = std::max(1, 1 + (selected_ - scrollbar_->position()) * kCardRows);
            on_task_context_menu(*task, Point{absolute.x + 2, absolute.y + row});
        } else if (on_lane_context_menu) {
            on_lane_context_menu(lane_.id, Point{absolute.x + 2, absolute.y});
        }
        return true;
    }
    switch (event.chord.key) {
        case Key::Up:
            if (move_source_ && on_move_step) on_move_step(lane_.id, -1);
            else move_selection(selected_ - 1);
            return true;
        case Key::Down:
            if (move_source_ && on_move_step) on_move_step(lane_.id, 1);
            else move_selection(selected_ + 1);
            return true;
        case Key::PageUp:
            if (move_source_ && on_move_step) on_move_step(lane_.id, -visible_cards());
            else move_selection(selected_ - visible_cards());
            return true;
        case Key::PageDown:
            if (move_source_ && on_move_step) on_move_step(lane_.id, visible_cards());
            else move_selection(selected_ + visible_cards());
            return true;
        case Key::Home:
            if (move_source_ && on_move_step) on_move_step(lane_.id, -static_cast<int>(tasks_.size()) - 1);
            else move_selection(0);
            return true;
        case Key::End:
            if (move_source_ && on_move_step) on_move_step(lane_.id, static_cast<int>(tasks_.size()) + 1);
            else move_selection(static_cast<int>(tasks_.size()) - 1);
            return true;
        case Key::Left:
            if (on_lane_step) on_lane_step(lane_.id, -1);
            return true;
        case Key::Right:
            if (on_lane_step) on_lane_step(lane_.id, 1);
            return true;
        case Key::Tab:
            if (on_lane_step)
                on_lane_step(lane_.id, has_modifier(event.chord.modifiers, Modifier::Shift) ? -1 : 1);
            return true;
        case Key::Enter: {
            std::optional<TaskId> task = selected_task();
            if (!task) task = move_source_;
            if (task && on_move_toggle) on_move_toggle(*task);
            return true;
        }
        case Key::Char:
            if (event.chord.text == " ") {
                std::optional<TaskId> task = selected_task();
                if (!task) task = move_source_;
                if (task && on_move_toggle) on_move_toggle(*task);
                return true;
            }
            return false;
        default: return false;
    }
}

bool TodoLaneView::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Wheel) {
        if (event.button == MouseButton::WheelUp) scrollbar_->set_position(scrollbar_->position() - 1);
        else if (event.button == MouseButton::WheelDown) scrollbar_->set_position(scrollbar_->position() + 1);
        else return false;
        return true;
    }
    const Rect absolute = absolute_bounds();
    if (event.action == MouseAction::Move && pressed_task_) {
        if (on_drag) on_drag(*pressed_task_, event.action, event.cell);
        return true;
    }
    if (event.action == MouseAction::Up && pressed_task_) {
        const TaskId released_task = *pressed_task_;
        pressed_task_.reset();
        if (on_drag) on_drag(released_task, event.action, event.cell);
        return true;
    }
    if (event.action != MouseAction::Down && event.action != MouseAction::DoubleClick) return false;
    if (event.button == MouseButton::Right && event.cell.y == absolute.y) {
        if (on_lane_context_menu) on_lane_context_menu(lane_.id, event.cell);
        return true;
    }
    if (event.button != MouseButton::Left && event.button != MouseButton::Right) return false;
    const int row = event.cell.y - absolute.y - 1;
    if (row < 0 || row >= bounds().height - 2) return false;
    const int index = scrollbar_->position() + row / kCardRows;
    if (index < 0 || static_cast<std::size_t>(index) >= tasks_.size()) return false;
    move_selection(index);
    if (event.button == MouseButton::Right) {
        if (on_task_context_menu) on_task_context_menu(tasks_[index].id, event.cell);
        return true;
    }
    if (event.action == MouseAction::Down) {
        pressed_task_ = tasks_[index].id;
        if (on_drag) on_drag(*pressed_task_, event.action, event.cell);
    }
    if (event.action == MouseAction::DoubleClick && on_activate) on_activate(tasks_[index].id);
    return true;
}

std::optional<PointerShape> TodoLaneView::pointer_shape_at(Point cell) const {
    if (pressed_task_) return PointerShape::Grabbing;
    if (cell.y == 0 || (cell.y > 0 && cell.y < bounds().height - 1)) return PointerShape::Grab;
    return PointerShape::Default;
}

Style TodoLaneView::lane_style() const {
    Style style = context().theme->resolve(normal_role_);
    if (lane_.color) {
        style.bg = palette_color(*lane_.color);
        style.fg = Color::indexed(light_palette_color(*lane_.color) ? 0 : 15);
    }
    return style;
}

Style TodoLaneView::frame_style() const {
    Style style = context().theme->resolve(focused_ ? frame_active_role_ : frame_inactive_role_);
    if (lane_.color) style.bg = palette_color(*lane_.color);
    if (move_target_) style.attrs |= Attr::Bold;
    return style;
}

Style TodoLaneView::task_marker_style(const Task& task, Style base) const {
    if (!task.color) return base;
    base.fg = palette_color(*task.color);
    if (base.fg == base.bg) base.attrs |= Attr::Reverse;
    return base;
}

std::string TodoLaneView::due_badge(const Task& task) const {
    if (!task.due_date) return {};
    std::string label;
    if (!today_) {
        label = task.due_date->value;
    } else if (task.due_date->value < today_->value) {
        label = "! overdue";
    } else if (task.due_date == today_) {
        label = "! today";
    } else if (task.due_date == tomorrow_) {
        label = "tomorrow";
    } else {
        label = task.due_date->value;
    }
    if (task.due_time) label += " " + task.due_time->value;
    return label;
}

void TodoLaneView::draw(scene::Painter& painter) {
    if (bounds().width <= 0 || bounds().height <= 0 || !context().valid()) return;
    const Style normal = lane_style();
    const Style frame = frame_style();
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    if (bounds().width >= 2 && bounds().height >= 2) {
        painter.draw_box(Rect{0, 0, bounds().width, bounds().height}, scene::LineStyle::Single, frame);
        const std::string title = lane_.title + " (" + std::to_string(tasks_.size()) + ")";
        painter.draw_text(Point{2, 0}, text::elide_to_width(title, std::max(0, bounds().width - 4)), frame);
    }

    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    const int content_width = std::max(0, bounds().width - 2);
    for (int slot = 0; slot < visible_cards(); ++slot) {
        const int index = top + slot;
        const int y = 1 + slot * kCardRows;
        if (y >= bounds().height - 1) break;
        if (index < 0 || static_cast<std::size_t>(index) >= tasks_.size()) continue;
        const Task& task = tasks_[static_cast<std::size_t>(index)];
        const ui::RoleId selection = focused_ ? selected_role_ : inactive_selected_role_;
        Style card = index == selected_ ? context().theme->resolve(selection) : normal;
        if (move_source_ == task.id) card.attrs |= Attr::Underline | Attr::Bold;
        painter.fill(Rect{1, y, content_width, std::min(kCardRows, bounds().height - 1 - y)},
                     Cell::from_grapheme(" ", card));

        const std::string marker = priority_mark(task.priority);
        const std::string badge_text = due_badge(task);
        const std::string badge = badge_text.empty() ? std::string{} : " " + badge_text;
        const int marker_width = text::text_width(marker);
        const int title_x = 1 + marker_width + 1;
        const int title_width = std::max(0, content_width - marker_width - 1 - text::text_width(badge));
        painter.draw_text(Point{1, y}, marker, task_marker_style(task, card));
        painter.draw_text(Point{title_x, y}, text::elide_to_width(task.title, title_width), card);
        if (!badge.empty()) {
            const int x = 1 + content_width - text::text_width(badge);
            painter.draw_text(Point{x, y}, badge, card);
        }
        if (y + 1 < bounds().height - 1)
            painter.draw_text(Point{title_x, y + 1},
                              text::elide_to_width(task.details, std::max(0, content_width - marker_width - 1)),
                              card);
    }

    if (move_target_ && (drop_before_ || drop_at_end_)) {
        int marker_y = bounds().height - 2;
        if (drop_before_) {
            const auto found = std::find_if(tasks_.begin(), tasks_.end(), [this](const Task& task) {
                return task.id == *drop_before_;
            });
            if (found != tasks_.end()) {
                const int index = static_cast<int>(found - tasks_.begin());
                marker_y = 1 + (index - scrollbar_->position()) * kCardRows;
            }
        } else {
            marker_y = 1 + (static_cast<int>(tasks_.size()) - scrollbar_->position()) * kCardRows;
        }
        if (marker_y >= 1 && marker_y < bounds().height - 1) {
            const std::string marker = drop_before_ ? "▶" : "▶ drop here";
            painter.draw_text(Point{1, marker_y},
                              text::elide_to_width(marker, std::max(0, bounds().width - 2)), frame);
        }
    }
}

}  // namespace ckv::todo
