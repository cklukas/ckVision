// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

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
    static constexpr std::string_view kMoveTaskKey = "todo.move-task";
    static constexpr std::string_view kCancelMoveKey = "todo.cancel-move";

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
    void present_welcome();

    void present_task_dialog(bool editing);
    void accept_task_dialog(bool editing, std::optional<TaskId> task_id,
                            std::string preserved_note, const widgets::DialogResult& result);
    void present_archive_confirmation();
    void open_note_editor();
    void schedule_note_save(TaskId task_id);
    bool flush_note(TaskId task_id);
    void close_note_session(NoteSession* session);

    void toggle_move();
    void cancel_move();
    void set_theme(TodoTheme theme);
    void show_error(std::string title, const ControllerError& error);
    void set_status(std::string message);

    const Task* selected_task() const noexcept;
    std::optional<LaneId> active_lane() const noexcept;
    std::optional<BoardId> board_named(std::string_view name) const noexcept;
    TaskDraft draft_from(const Task& task) const;

    ui::Application& app_;
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
    ui::CommandId move_task_command_ = ui::kInvalidCommand;
    ui::CommandId cancel_move_command_ = ui::kInvalidCommand;
    ui::CommandId board_help_command_ = ui::kInvalidCommand;

    std::optional<PendingTaskMove> move_;
    TodoTheme theme_ = TodoTheme::Classic;
    std::vector<std::unique_ptr<NoteSession>> notes_;
    ui::Application::TimerId poll_timer_ = 0;

    std::optional<widgets::DescriptorDialogPresentation> welcome_dialog_;
    std::optional<widgets::DescriptorDialogPresentation> task_dialog_;
    std::optional<widgets::MessageBoxPresentation> archive_confirmation_;
    std::optional<widgets::MessageBoxPresentation> message_box_;
    std::optional<widgets::HelpViewerPresentation> help_viewer_;
};

}  // namespace ckv::todo
