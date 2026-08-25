// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/message_box.hpp"

#include "todo_board_view.hpp"
#include "todo_controller.hpp"

namespace ckv::widgets {
class Desktop;
class Memo;
class MenuBar;
struct MenuBarItem;
class StatusLine;
class Window;
}  // namespace ckv::widgets

namespace ckv::todo {

enum class TodoTheme { Classic, Dark, Light, Mono, HighContrast };

struct TodoAppOptions {
    std::optional<InitialWorkspace> first_workspace;
    std::optional<std::string> initial_board_name;
    std::string workspace_description = "persistent local workspace";
};

class TodoApp {
public:
    TodoApp(ui::Application& app,
            TodoRepository& repository,
            CalendarClock& calendar,
            std::string identity,
            TodoAppOptions options = {});
    ~TodoApp();

    TodoApp(const TodoApp&) = delete;
    TodoApp& operator=(const TodoApp&) = delete;

    static constexpr std::string_view kAddTaskKey = "todo.add-task";
    static constexpr std::string_view kEditTaskKey = "todo.edit-task";
    static constexpr std::string_view kEditNoteKey = "todo.edit-note";
    static constexpr std::string_view kArchiveTaskKey = "todo.archive-task";
    static constexpr std::string_view kDeleteTaskKey = "todo.delete-task";
    static constexpr std::string_view kMoveTaskKey = "todo.move-task";
    static constexpr std::string_view kCancelMoveKey = "todo.cancel-move";
    static constexpr std::string_view kLaneActionsKey = "todo.lane-actions";
    static constexpr std::string_view kBoardManagerKey = "todo.board-manager";
    static constexpr std::string_view kNewBoardKey = "todo.new-board";

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* board_window() const noexcept { return board_window_; }
    TodoBoardView* board_view() const noexcept { return board_view_; }
    widgets::MenuBar* menu_bar() const noexcept { return menu_bar_; }
    widgets::StatusLine* status_line() const noexcept { return status_line_; }
    TodoController& controller() noexcept { return controller_; }
    const TodoController& controller() const noexcept { return controller_; }
    bool move_active() const noexcept { return move_.has_value(); }
    std::size_t note_window_count() const noexcept { return notes_.size(); }
    TodoTheme theme() const noexcept { return theme_; }

    void initialize_workspace(InitialWorkspace initial);
    void refresh_board();
    void poll_repository();

private:
    struct NoteSession;

    void declare_commands();
    void bind_board_aliases();
    std::vector<widgets::MenuBarItem> build_menus();
    void build_chrome();
    void build_board_window();
    void install_help();
    void show_help_topic(std::string key);
    void show_about();
    void present_welcome();

    void present_task_dialog(bool editing);
    void accept_task_dialog(bool editing, std::optional<TaskId> task_id,
                            std::string preserved_note, const widgets::DialogResult& result);
    void present_archive_confirmation();
    void present_delete_confirmation();
    void open_note_editor();
    void schedule_note_save(TaskId task_id);
    bool flush_note(TaskId task_id);
    void close_note_session(NoteSession* session);

    void present_lane_actions();
    void present_lane_rename();
    void present_lane_color();
    void present_lane_sort();
    void present_lane_insert(bool before_active);
    void present_lane_merge();
    void present_lane_archive();
    void set_lane_sort(SortMode sort);

    void present_board_manager();
    void present_new_board();
    void present_board_rename(BoardId board_id);
    void present_board_merge(BoardId board_id);
    void present_board_archive(BoardId board_id);
    void switch_board(BoardId board_id);

    void toggle_move();
    void cancel_move();
    void drop_task(TaskId task_id, LaneId target_lane_id, std::optional<TaskId> before_task_id);
    void show_task_context_menu(Point screen_cell);
    void show_lane_context_menu(Point screen_cell);
    void set_theme(TodoTheme theme);
    void refresh_status_items();
    void show_error(std::string title, const ControllerError& error);
    void set_status(std::string message);

    const Task* selected_task() const noexcept;
    std::optional<LaneId> active_lane() const noexcept;
    const Lane* active_lane_record() const noexcept;
    const Board* active_board() const noexcept;
    std::optional<BoardId> board_named(std::string_view name) const noexcept;
    TaskDraft draft_from(const Task& task) const;

    ui::Application& app_;
    CalendarClock& calendar_;
    TodoController controller_;
    TodoAppOptions options_;
    ui::StandardRoles roles_;
    widgets::MemoryHelpProvider help_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::MenuBar* menu_bar_ = nullptr;
    widgets::StatusLine* status_line_ = nullptr;
    widgets::Window* board_window_ = nullptr;
    TodoBoardView* board_view_ = nullptr;

    ui::CommandId add_task_command_ = ui::kInvalidCommand;
    ui::CommandId edit_task_command_ = ui::kInvalidCommand;
    ui::CommandId edit_note_command_ = ui::kInvalidCommand;
    ui::CommandId archive_task_command_ = ui::kInvalidCommand;
    ui::CommandId delete_task_command_ = ui::kInvalidCommand;
    ui::CommandId move_task_command_ = ui::kInvalidCommand;
    ui::CommandId cancel_move_command_ = ui::kInvalidCommand;
    ui::CommandId lane_actions_command_ = ui::kInvalidCommand;
    ui::CommandId lane_rename_command_ = ui::kInvalidCommand;
    ui::CommandId lane_color_command_ = ui::kInvalidCommand;
    ui::CommandId lane_insert_left_command_ = ui::kInvalidCommand;
    ui::CommandId lane_insert_right_command_ = ui::kInvalidCommand;
    ui::CommandId lane_merge_command_ = ui::kInvalidCommand;
    ui::CommandId lane_archive_command_ = ui::kInvalidCommand;
    std::array<ui::CommandId, 6> lane_sort_commands_{};
    ui::CommandId board_manager_command_ = ui::kInvalidCommand;
    ui::CommandId new_board_command_ = ui::kInvalidCommand;
    ui::CommandId keyboard_help_command_ = ui::kInvalidCommand;
    ui::CommandId about_command_ = ui::kInvalidCommand;
    ui::CommandId board_help_command_ = ui::kInvalidCommand;
    ui::CommandId board_quit_command_ = ui::kInvalidCommand;

    std::optional<PendingTaskMove> move_;
    TodoTheme theme_ = TodoTheme::Classic;
    std::vector<std::unique_ptr<NoteSession>> notes_;
    ui::Application::TimerId poll_timer_ = 0;
    ui::Application::TimerId status_timer_ = 0;

    std::optional<widgets::DescriptorDialogPresentation> welcome_dialog_;
    std::optional<widgets::DescriptorDialogPresentation> task_dialog_;
    std::optional<widgets::MessageBoxPresentation> archive_confirmation_;
    std::optional<widgets::MessageBoxPresentation> delete_confirmation_;
    std::optional<widgets::DescriptorDialogPresentation> lane_actions_dialog_;
    std::optional<widgets::DescriptorDialogPresentation> lane_edit_dialog_;
    std::optional<widgets::MessageBoxPresentation> lane_confirmation_;
    std::optional<widgets::DescriptorDialogPresentation> board_manager_dialog_;
    std::optional<widgets::DescriptorDialogPresentation> board_edit_dialog_;
    std::optional<widgets::MessageBoxPresentation> board_confirmation_;
    std::optional<widgets::MessageBoxPresentation> message_box_;
    std::optional<widgets::MessageBoxPresentation> about_box_;
    std::optional<widgets::HelpViewerPresentation> help_viewer_;
};

}  // namespace ckv::todo
