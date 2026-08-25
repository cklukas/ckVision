// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "todo_app.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/core/version.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::todo {
namespace {

constexpr std::int64_t kNoteSaveDelayNanos = 150'000'000;
constexpr std::int64_t kRepositoryPollNanos = 1'000'000'000;
constexpr std::int64_t kStatusHintNanos = 3'000'000'000;

const std::array<std::string, 4> kPriorityNames = {"High", "Normal", "Low", "Idle"};
const std::array<std::string, 6> kSortNames = {"Manual", "Color", "Due", "Created", "Modified", "Priority"};
const std::array<std::string_view, 6> kSortCommandKeys = {
    "todo.lane-sort.manual", "todo.lane-sort.color", "todo.lane-sort.due",
    "todo.lane-sort.created", "todo.lane-sort.modified", "todo.lane-sort.priority"};
const std::array<std::string, 17> kColorNames = {
    "None", "Black", "Dark blue", "Dark green", "Dark cyan", "Dark red", "Dark magenta", "Brown",
    "Light gray", "Dark gray", "Blue", "Green", "Cyan", "Red", "Magenta", "Yellow", "White"};

class ObservedMemo final : public widgets::Memo {
public:
    std::function<void()> on_changed;

    bool on_key(const KeyEvent& event) override {
        const std::string before = text();
        const bool handled = Memo::on_key(event);
        if (text() != before && on_changed) on_changed();
        return handled;
    }

    bool on_text(const TextEvent& event) override {
        const std::string before = text();
        const bool handled = Memo::on_text(event);
        if (text() != before && on_changed) on_changed();
        return handled;
    }
};

widgets::FieldDescriptor combo_field(std::string label, std::vector<std::string> options, int selection) {
    widgets::FieldDescriptor field;
    field.label = std::move(label);
    field.kind = widgets::FieldKind::Combo;
    field.options = std::move(options);
    field.initial_selection = selection;
    return field;
}

widgets::FieldDescriptor radio_field(std::string label, std::vector<std::string> options, int selection) {
    widgets::FieldDescriptor field;
    field.label = std::move(label);
    field.kind = widgets::FieldKind::Radio;
    field.options = std::move(options);
    field.initial_selection = selection;
    return field;
}

std::vector<std::string> strings(const auto& values) {
    return std::vector<std::string>(values.begin(), values.end());
}

std::optional<TodoColor> color_from_selection(int selection) {
    if (selection <= 0 || selection > 16) return std::nullopt;
    return static_cast<TodoColor>(selection - 1);
}

int color_selection(std::optional<TodoColor> color) {
    return color ? static_cast<int>(*color) + 1 : 0;
}

}  // namespace

struct TodoApp::NoteSession {
    TaskId task_id;
    widgets::Window* window = nullptr;
    ObservedMemo* memo = nullptr;
    ui::Application::TimerId timer = 0;
    std::string last_saved;
};

TodoApp::TodoApp(ui::Application& app,
                 TodoRepository& repository,
                 CalendarClock& calendar,
                 std::string identity,
                 TodoAppOptions options)
    : app_(app),
      calendar_(calendar),
      controller_(repository, calendar, std::move(identity)),
      options_(std::move(options)),
      roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);
    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add(std::move(desktop));

    declare_commands();
    build_chrome();
    build_board_window();
    install_help();

    const auto opened = controller_.open();
    if (!opened) {
        show_error("Cannot open TODO workspace", opened.error);
    } else if (*opened.value == WorkspaceOpenState::Missing) {
        if (options_.first_workspace)
            initialize_workspace(*options_.first_workspace);
        else
            present_welcome();
    } else {
        refresh_board();
    }

    poll_timer_ = app_.start_timer(kRepositoryPollNanos, true, [this] { poll_repository(); });
}

TodoApp::~TodoApp() {
    if (poll_timer_ != 0) app_.cancel_timer(poll_timer_);
    if (status_timer_ != 0) app_.cancel_timer(status_timer_);
    while (!notes_.empty()) {
        std::unique_ptr<NoteSession> note = std::move(notes_.back());
        notes_.pop_back();
        if (note->timer != 0) app_.cancel_timer(note->timer);
        if (note->memo != nullptr) note->memo->on_changed = {};
        if (note->window != nullptr) {
            note->window->close_request = {};
            note->window->on_closed = {};
            desktop_->remove_window(note->window);
        }
    }
    app_.set_help_provider({});
    if (desktop_ != nullptr && desktop_->parent() != nullptr) app_.root().remove_child(desktop_);
    desktop_ = nullptr;
    app_.commands().withdraw(add_task_command_);
    app_.commands().withdraw(edit_task_command_);
    app_.commands().withdraw(edit_note_command_);
    app_.commands().withdraw(archive_task_command_);
    app_.commands().withdraw(delete_task_command_);
    app_.commands().withdraw(move_task_command_);
    app_.commands().withdraw(cancel_move_command_);
    app_.commands().withdraw(lane_actions_command_);
    app_.commands().withdraw(lane_rename_command_);
    app_.commands().withdraw(lane_color_command_);
    app_.commands().withdraw(lane_insert_left_command_);
    app_.commands().withdraw(lane_insert_right_command_);
    app_.commands().withdraw(lane_merge_command_);
    app_.commands().withdraw(lane_archive_command_);
    for (const ui::CommandId command : lane_sort_commands_) app_.commands().withdraw(command);
    app_.commands().withdraw(board_manager_command_);
    app_.commands().withdraw(new_board_command_);
    app_.commands().withdraw(keyboard_help_command_);
    app_.commands().withdraw(about_command_);
    app_.commands().withdraw(board_help_command_);
    app_.commands().withdraw(board_quit_command_);
}

void TodoApp::declare_commands() {
    add_task_command_ = app_.commands().declare(
        {.key = std::string(kAddTaskKey), .title = "&Add task...", .category = "Tasks", .context = "todo.board",
         .handler = [this] { present_task_dialog(false); }});
    edit_task_command_ = app_.commands().declare(
        {.key = std::string(kEditTaskKey), .title = "&Edit task...", .category = "Tasks", .context = "todo.board",
         .handler = [this] { present_task_dialog(true); }});
    edit_note_command_ = app_.commands().declare(
        {.key = std::string(kEditNoteKey), .title = "Edit &note", .category = "Tasks", .context = "todo.board",
         .handler = [this] { open_note_editor(); }});
    archive_task_command_ = app_.commands().declare(
        {.key = std::string(kArchiveTaskKey), .title = "&Archive task...", .category = "Tasks", .context = "todo.board",
         .handler = [this] { present_archive_confirmation(); }});
    delete_task_command_ = app_.commands().declare(
        {.key = std::string(kDeleteTaskKey), .title = "&Delete task...", .category = "Tasks", .context = "todo.board",
         .handler = [this] { present_delete_confirmation(); }});
    move_task_command_ = app_.commands().declare(
        {.key = std::string(kMoveTaskKey), .title = "&Move task", .category = "Tasks", .context = "todo.board",
         .handler = [this] { toggle_move(); }});
    cancel_move_command_ = app_.commands().declare(
        {.key = std::string(kCancelMoveKey), .title = "Cancel move", .category = "Tasks", .context = "todo.board",
         .visibility = ui::CommandVisibility::Hidden, .handler = [this] { cancel_move(); }});
    lane_actions_command_ = app_.commands().declare(
        {.key = std::string(kLaneActionsKey), .title = "Lane &actions...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_actions(); }});
    lane_rename_command_ = app_.commands().declare(
        {.key = "todo.rename-lane", .title = "&Rename...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_rename(); }});
    lane_color_command_ = app_.commands().declare(
        {.key = "todo.color-lane", .title = "&Color...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_color(); }});
    lane_insert_left_command_ = app_.commands().declare(
        {.key = "todo.insert-lane-left", .title = "Insert &left...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_insert(true); }});
    lane_insert_right_command_ = app_.commands().declare(
        {.key = "todo.insert-lane-right", .title = "Insert &right...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_insert(false); }});
    lane_merge_command_ = app_.commands().declare(
        {.key = "todo.merge-lane", .title = "&Merge into...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_merge(); }});
    lane_archive_command_ = app_.commands().declare(
        {.key = "todo.archive-lane", .title = "Archive lane...", .category = "Lanes", .context = "todo.board",
         .handler = [this] { present_lane_archive(); }});
    for (std::size_t index = 0; index < lane_sort_commands_.size(); ++index) {
        const SortMode sort = static_cast<SortMode>(index);
        lane_sort_commands_[index] = app_.commands().declare(
            {.key = std::string(kSortCommandKeys[index]), .title = kSortNames[index], .category = "Lanes",
             .context = "todo.board", .handler = [this, sort] { set_lane_sort(sort); }});
    }
    board_manager_command_ = app_.commands().declare(
        {.key = std::string(kBoardManagerKey), .title = "&Board Manager...", .category = "Boards",
         .context = "todo.board", .handler = [this] { present_board_manager(); }});
    new_board_command_ = app_.commands().declare(
        {.key = std::string(kNewBoardKey), .title = "&New Board...", .category = "Boards",
         .context = "todo.board", .handler = [this] { present_new_board(); }});
    keyboard_help_command_ = app_.commands().declare(
        {.key = "todo.keyboard-help", .title = "&Keyboard reference", .category = "Help",
         .handler = [this] { show_help_topic("todo.keyboard"); }});
    about_command_ = app_.commands().declare(
        {.key = "todo.about", .title = "&About ckVision TODO", .category = "Help",
         .handler = [this] { show_about(); }});
    board_help_command_ = app_.commands().declare(
        {.key = "todo.board-help", .title = "Board help", .category = "Help", .context = "todo.board",
         .visibility = ui::CommandVisibility::Hidden,
         .handler = [this] { app_.execute_command(app_.commands().standard().help); }});
    board_quit_command_ = app_.commands().declare(
        {.key = "todo.board-quit", .title = "Board quit", .category = "File", .context = "todo.board",
         .visibility = ui::CommandVisibility::Hidden,
         .handler = [this] { app_.execute_command(app_.commands().standard().quit); }});

    app_.commands().set_enabled_predicate(add_task_command_, [this] { return active_lane().has_value() && !move_; });
    const auto task_available = [this] { return selected_task() != nullptr && !move_; };
    app_.commands().set_enabled_predicate(edit_task_command_, task_available);
    app_.commands().set_enabled_predicate(edit_note_command_, task_available);
    app_.commands().set_enabled_predicate(archive_task_command_, task_available);
    app_.commands().set_enabled_predicate(delete_task_command_, task_available);
    app_.commands().set_enabled_predicate(move_task_command_,
                                          [this] { return move_.has_value() || selected_task() != nullptr; });
    app_.commands().set_enabled_predicate(cancel_move_command_, [this] { return move_.has_value(); });
    const auto lane_available = [this] { return active_lane().has_value() && !move_; };
    app_.commands().set_enabled_predicate(lane_actions_command_, lane_available);
    app_.commands().set_enabled_predicate(lane_rename_command_, lane_available);
    app_.commands().set_enabled_predicate(lane_color_command_, lane_available);
    app_.commands().set_enabled_predicate(lane_insert_left_command_, lane_available);
    app_.commands().set_enabled_predicate(lane_insert_right_command_, lane_available);
    app_.commands().set_enabled_predicate(lane_merge_command_, [this] {
        const Board* board = active_board();
        return board != nullptr && board->lanes.size() > 1 && !move_;
    });
    app_.commands().set_enabled_predicate(lane_archive_command_, [this] {
        const Board* board = active_board();
        return board != nullptr && board->lanes.size() > 1 && !move_;
    });
    for (const ui::CommandId command : lane_sort_commands_)
        app_.commands().set_enabled_predicate(command, lane_available);
    app_.commands().set_enabled_predicate(board_manager_command_, [this] {
        return controller_.workspace() != nullptr && !move_;
    });
    app_.commands().set_enabled_predicate(new_board_command_, [this] {
        return controller_.workspace() != nullptr && !move_;
    });
    bind_board_aliases();
}

void TodoApp::bind_board_aliases() {
    const auto bind = [this](Key key, std::string text, ui::CommandId command,
                             Modifier modifiers = Modifier::None) {
        app_.commands().bind_key(KeyChord{key, modifiers, std::move(text)}, command);
    };
    bind(Key::F2, {}, add_task_command_);
    bind(Key::Insert, {}, add_task_command_);
    bind(Key::Char, "+", add_task_command_);
    bind(Key::F3, {}, edit_task_command_);
    bind(Key::Char, "e", edit_task_command_);
    bind(Key::F4, {}, edit_note_command_);
    bind(Key::Char, "n", edit_note_command_);
    bind(Key::F8, {}, archive_task_command_);
    bind(Key::Char, "a", archive_task_command_);
    bind(Key::Delete, {}, delete_task_command_);
    bind(Key::Char, "d", delete_task_command_);
    bind(Key::F9, {}, move_task_command_);
    bind(Key::Enter, {}, move_task_command_);
    bind(Key::Char, " ", move_task_command_);
    bind(Key::Escape, {}, cancel_move_command_);
    bind(Key::F7, {}, lane_actions_command_);
    bind(Key::Char, "m", board_manager_command_);
    bind(Key::Char, "h", board_help_command_);
    bind(Key::Char, "?", board_help_command_);
    bind(Key::Char, "q", board_quit_command_);
}

std::vector<widgets::MenuBarItem> TodoApp::build_menus() {
    widgets::MenuBarItem tasks{"&Tasks",
                               {widgets::MenuItem::command(add_task_command_),
                                widgets::MenuItem::command(edit_task_command_),
                                widgets::MenuItem::command(edit_note_command_),
                                widgets::MenuItem::separator(),
                                widgets::MenuItem::command(archive_task_command_),
                                widgets::MenuItem::command(delete_task_command_),
                                widgets::MenuItem::command(move_task_command_)}};
    const SortMode active_sort = active_lane_record() != nullptr ? active_lane_record()->sort : SortMode::Manual;
    std::vector<widgets::MenuItem> sort_items;
    for (std::size_t index = 0; index < lane_sort_commands_.size(); ++index) {
        sort_items.push_back(widgets::MenuItem::command(lane_sort_commands_[index])
                                 .with_mark(static_cast<SortMode>(index) == active_sort
                                                ? widgets::MenuMark::RadioOn
                                                : widgets::MenuMark::RadioOff));
    }
    widgets::MenuBarItem lane{
        "&Lane",
        {widgets::MenuItem::submenu("&Sort", std::move(sort_items)),
         widgets::MenuItem::command(lane_color_command_), widgets::MenuItem::command(lane_rename_command_),
         widgets::MenuItem::command(lane_insert_left_command_),
         widgets::MenuItem::command(lane_insert_right_command_), widgets::MenuItem::separator(),
         widgets::MenuItem::command(lane_merge_command_), widgets::MenuItem::command(lane_archive_command_)}};
    const auto theme_item = [this](std::string label, TodoTheme theme) {
        return widgets::MenuItem::action(std::move(label), [this, theme] { set_theme(theme); })
            .with_mark(theme_ == theme ? widgets::MenuMark::RadioOn : widgets::MenuMark::RadioOff);
    };
    widgets::MenuBarItem view{
        "&View",
        {widgets::MenuItem::submenu("&Theme",
                                    {theme_item("&Classic", TodoTheme::Classic),
                                     theme_item("&Dark", TodoTheme::Dark),
                                     theme_item("&Light", TodoTheme::Light),
                                     theme_item("&Mono", TodoTheme::Mono),
                                     theme_item("&High contrast", TodoTheme::HighContrast)}),
         widgets::MenuItem::submenu(
             "&Window",
             {widgets::MenuItem::command(app_.commands().standard().next_window),
              widgets::MenuItem::command(app_.commands().standard().previous_window),
              widgets::MenuItem::separator(), widgets::MenuItem::command(app_.commands().standard().zoom),
              widgets::MenuItem::command(app_.commands().standard().minimize),
              widgets::MenuItem::command(app_.commands().standard().window_list),
              widgets::MenuItem::separator(), widgets::MenuItem::command(app_.commands().standard().tile_grid),
              widgets::MenuItem::command(app_.commands().standard().cascade)})}};
    widgets::MenuBarItem help{
        "&Help", {widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().help,
                                                                           "&Context Help"}),
                  widgets::MenuItem::command(keyboard_help_command_), widgets::MenuItem::separator(),
                  widgets::MenuItem::command(about_command_)}};
    widgets::MenuBarItem file{
        "&File", {widgets::MenuItem::command(board_manager_command_), widgets::MenuItem::command(new_board_command_),
                  widgets::MenuItem::command(
                      widgets::CommandPresentation{about_command_, "Workspace &information..."}),
                  widgets::MenuItem::separator(),
                  widgets::MenuItem::command(
                      widgets::CommandPresentation{app_.commands().standard().quit, "E&xit"})}};
    return {std::move(file), std::move(tasks), std::move(lane), std::move(view), std::move(help)};
}

void TodoApp::build_chrome() {
    auto menu = std::make_unique<widgets::MenuBar>(build_menus());
    menu_bar_ = menu.get();
    desktop_->dock_top(std::move(menu));

    auto status = std::make_unique<widgets::StatusLine>();
    status_line_ = status.get();
    status->set_hint_provider([this](const std::string& key) {
        if (move_) return std::string{"Arrow keys move; Enter commits; Esc cancels."};
        if (key == "todo.board") return std::string{"Arrows select tasks and lanes; Enter starts or finishes a move."};
        if (key == "todo.note") return std::string{"Notes autosave after a short pause and flush on close."};
        return std::string{};
    });
    desktop_->dock_bottom(std::move(status));
    refresh_status_items();
}

void TodoApp::build_board_window() {
    auto window = std::make_unique<widgets::Window>("TODO");
    const Rect area = desktop_->content_area();
    const int width = std::max(40, std::min(92, area.width - 4));
    const int height = std::max(12, std::min(24, area.height - 2));
    window->set_bounds(Rect{std::max(0, (area.width - width) / 2), std::max(0, (area.height - height) / 2),
                            width, height});
    window->set_min_size(Size{30, 8});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);
    auto board = std::make_unique<TodoBoardView>();
    board_view_ = board.get();
    board->on_task_activate = [this](TaskId) { present_task_dialog(true); };
    board->on_move_toggle = [this](TaskId) { toggle_move(); };
    board->on_task_drop = [this](TaskId task_id, LaneId lane_id, std::optional<TaskId> before) {
        drop_task(task_id, lane_id, before);
    };
    board->on_task_context_menu = [this](TaskId, Point screen_cell) { show_task_context_menu(screen_cell); };
    board->on_lane_context_menu = [this](LaneId, Point screen_cell) { show_lane_context_menu(screen_cell); };
    board->on_active_lane_changed = [this](LaneId lane_id) {
        if (move_) board_view_->set_move_target(lane_id);
        if (menu_bar_ != nullptr) menu_bar_->set_menus(build_menus());
    };
    window->set_content(std::move(board));
    board_window_ = desktop_->add_window(std::move(window));
}

void TodoApp::install_help() {
    help_.add_topic("todo.board", widgets::HelpTopic{
                                      "TODO board",
                                      "Use Up/Down to select a task and Left/Right to change lanes. F2 adds, F3 edits, "
                                      "F4 opens a note, F8 archives, and F9 or Enter starts and finishes move mode.",
                                      {{"todo.tasks", "Tasks"}, {"todo.moving", "Moving"},
                                       {"todo.lanes", "Lanes"}, {"todo.boards", "Boards"},
                                       {"todo.keyboard", "Keyboard"}, {"todo.persistence", "Persistence"}}});
    help_.add_topic("todo.tasks", widgets::HelpTopic{"Tasks", "Task changes are durable before the board reports success.",
                                                       {{"todo.board", "Board"}, {"todo.persistence", "Persistence"}}});
    help_.add_topic("todo.moving", widgets::HelpTopic{"Moving", "Start move mode, choose a lane and insertion task with arrows, then press Enter. Escape cancels.",
                                                        {{"todo.board", "Board"}}});
    help_.add_topic("todo.persistence", widgets::HelpTopic{"Persistence", "There is no Save command. Changes are atomically committed with daily backups; archive data is written before removal.",
                                                             {{"todo.board", "Board"}, {"todo.boards", "Boards"}}});
    help_.add_topic("todo.note", widgets::HelpTopic{"Notes", "Note windows are modeless. Editing coalesces for 150 ms and closing flushes pending text.",
                                                     {{"todo.board", "Board"}}});
    help_.add_topic("todo.lanes", widgets::HelpTopic{
                                      "Lanes",
                                      "F7 opens lane actions. Lanes can be renamed, colored, sorted, inserted, merged, or archived. Only Manual sorting permits reordering.",
                                      {{"todo.board", "Board"}, {"todo.keyboard", "Keyboard"}}});
    help_.add_topic("todo.boards", widgets::HelpTopic{
                                       "Boards",
                                       "Press m or click the Board status item to create, switch, rename, merge, or archive Boards. The main Board is protected.",
                                       {{"todo.board", "Board"}, {"todo.persistence", "Persistence"}}});
    help_.add_topic("todo.keyboard", widgets::HelpTopic{
                                         "Keyboard reference",
                                         "F1 Help; F2 Add; F3 Edit; F4 Note; F5 Zoom; F6 Next window; F7 Lane actions; F8 Archive; F9 Move; F10 Menu. Delete removes permanently. m selects a Board; Esc cancels.",
                                         {{"todo.board", "Board"}, {"todo.lanes", "Lanes"}, {"todo.boards", "Boards"}}});
    app_.set_help_provider([this](const std::string& key) {
        show_help_topic(key.empty() ? "todo.board" : key);
    });
}

void TodoApp::show_help_topic(std::string key) {
    help_viewer_ = widgets::present_help_viewer(help_, std::move(key), app_, *desktop_, roles_);
}

void TodoApp::show_about() {
    about_box_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Info, "About ckVision TODO",
         "ckVision TODO " + std::string(ckv::version_string()) + "\n" + options_.workspace_description +
             "\n\nChanges save automatically. F1 opens contextual help.",
         widgets::MessageBoxButtons::Ok});
    about_box_->set_completion_handler([](widgets::MessageBoxResult) {});
}

void TodoApp::present_welcome() {
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Welcome to ckVision TODO";
    widgets::FieldDescriptor choice;
    choice.label = "&Start with";
    choice.kind = widgets::FieldKind::Radio;
    choice.options = {"Guided sample", "Empty board"};
    choice.initial_selection = 0;
    descriptor.fields.push_back(std::move(choice));
    descriptor.buttons.push_back({"&Start", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Quit", widgets::ButtonRole::Dismiss, nullptr});
    welcome_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    welcome_dialog_->set_completion_handler([this](widgets::DialogResult result) {
        if (!result.accepted) {
            app_.request_quit();
            return;
        }
        initialize_workspace(!result.selected.empty() && result.selected[0] == 1 ? InitialWorkspace::Empty
                                                                                 : InitialWorkspace::Guided);
    });
}

void TodoApp::initialize_workspace(InitialWorkspace initial) {
    const auto created = controller_.create(initial);
    if (!created) {
        show_error("Cannot create TODO workspace", created.error);
        return;
    }
    refresh_board();
    set_status("Workspace created and saved.");
}

void TodoApp::refresh_board() {
    const TodoWorkspace* workspace = controller_.workspace();
    if (workspace == nullptr || board_view_ == nullptr) return;
    BoardId board_id = workspace->snapshot().last_board_id;
    if (options_.initial_board_name) {
        if (const auto requested = board_named(*options_.initial_board_name)) board_id = *requested;
        options_.initial_board_name.reset();
    }
    if (!board_view_->set_board(*workspace, board_id)) return;
    board_view_->set_move_source(move_ ? std::optional<TaskId>(move_->task_id) : std::nullopt);
    board_view_->set_move_target(move_ ? board_view_->active_lane() : std::nullopt);
    if (const Board* board = workspace->find_board(board_id)) board_window_->set_title("TODO — " + board->name);
    if (menu_bar_ != nullptr) menu_bar_->set_menus(build_menus());
    refresh_status_items();
    if (app_.focused() == nullptr && board_view_->active_lane())
        app_.set_focus(board_view_->lane_view(*board_view_->active_lane()));
}

void TodoApp::poll_repository() {
    if (!controller_.is_open() || move_) return;
    refresh_status_items();
    const auto reloaded = controller_.reload_if_changed();
    if (!reloaded) {
        set_status("Reload failed: " + reloaded.error.diagnostic);
        return;
    }
    if (reloaded.value->changed) {
        refresh_board();
        set_status("Reloaded changes from another TODO instance.");
    }
}

TaskDraft TodoApp::draft_from(const Task& task) const {
    return TaskDraft{task.title, task.details, task.note, task.priority, task.due_date, task.color};
}

void TodoApp::present_task_dialog(bool editing) {
    const Task* task = editing ? selected_task() : nullptr;
    if (editing && task == nullptr) return;
    const TaskDraft initial = task ? draft_from(*task) : TaskDraft{};
    widgets::DialogDescriptor descriptor;
    descriptor.title = editing ? "Edit task" : "Add task";
    descriptor.resizable = true;
    descriptor.minimum_window_size = Size{46, 15};
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Title:", initial.title, [](const std::string& value) { return !value.empty() && value.size() <= TodoLimits::max_title_bytes; }});
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Details:", initial.details, [](const std::string& value) { return value.size() <= TodoLimits::max_details_bytes; }});
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Due (YYYY-MM-DD):", initial.due_date ? initial.due_date->value : std::string{}, [](const std::string& value) {
            return value.empty() || is_valid(IsoDate{value});
        }});
    descriptor.fields.push_back(combo_field("&Priority:", strings(kPriorityNames), static_cast<int>(initial.priority) - 1));
    descriptor.fields.push_back(combo_field("&Color:", strings(kColorNames), color_selection(initial.color)));
    descriptor.buttons.push_back({editing ? "&Apply" : "&Add", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    const std::optional<TaskId> task_id = task ? std::optional<TaskId>(task->id) : std::nullopt;
    const std::string note = initial.note;
    task_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    task_dialog_->set_completion_handler([this, editing, task_id, note](widgets::DialogResult result) {
        if (result.accepted) accept_task_dialog(editing, task_id, note, result);
    });
}

void TodoApp::accept_task_dialog(bool editing,
                                 std::optional<TaskId> task_id,
                                 std::string preserved_note,
                                 const widgets::DialogResult& result) {
    if (result.values.size() < 5 || result.selected.size() < 5) return;
    TaskDraft draft;
    draft.title = result.values[0];
    draft.details = result.values[1];
    draft.note = std::move(preserved_note);
    if (!result.values[2].empty()) draft.due_date = IsoDate{result.values[2]};
    const int priority = result.selected[3] >= 0 ? result.selected[3] : 1;
    draft.priority = static_cast<Priority>(std::clamp(priority + 1, 1, 4));
    draft.color = color_from_selection(result.selected[4]);

    if (editing && task_id) {
        const auto changed = controller_.edit_task(*task_id, std::move(draft));
        if (!changed) {
            show_error("Cannot edit task", changed.error);
            return;
        }
        refresh_board();
        board_view_->select_task(*task_id);
        set_status("Task updated and saved.");
        return;
    }
    const auto lane = active_lane();
    if (!lane) return;
    const auto added = controller_.add_task(*lane, std::move(draft));
    if (!added) {
        show_error("Cannot add task", added.error);
        return;
    }
    refresh_board();
    board_view_->select_task(*added.value);
    set_status("Task added and saved.");
}

void TodoApp::present_archive_confirmation() {
    const Task* task = selected_task();
    if (task == nullptr) return;
    const TaskId task_id = task->id;
    archive_confirmation_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Confirm, "Archive task", "Archive ‘" + task->title + "’?",
         widgets::MessageBoxButtons::YesNo});
    archive_confirmation_->set_completion_handler([this, task_id](widgets::MessageBoxResult result) {
        if (result != widgets::MessageBoxResult::Yes) return;
        const auto archived = controller_.archive_task(task_id);
        if (!archived) {
            show_error("Cannot archive task", archived.error);
            return;
        }
        refresh_board();
        set_status("Task archived before removal.");
    });
}

void TodoApp::present_delete_confirmation() {
    const Task* task = selected_task();
    if (task == nullptr) return;
    const TaskId task_id = task->id;
    delete_confirmation_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Warning, "Delete task permanently",
         "Permanently delete ‘" + task->title + "’? This does not create an archive.",
         widgets::MessageBoxButtons::YesNo});
    delete_confirmation_->set_completion_handler([this, task_id](widgets::MessageBoxResult result) {
        if (result != widgets::MessageBoxResult::Yes) return;
        const auto removed = controller_.delete_task(task_id);
        if (!removed) {
            show_error("Cannot delete task", removed.error);
            return;
        }
        refresh_board();
        set_status("Task permanently deleted.");
    });
}

void TodoApp::open_note_editor() {
    const Task* task = selected_task();
    if (task == nullptr) return;
    for (const auto& note : notes_) {
        if (note->task_id != task->id) continue;
        desktop_->activate(note->window);
        app_.set_focus(note->memo);
        return;
    }

    auto session = std::make_unique<NoteSession>();
    session->task_id = task->id;
    session->last_saved = task->note;
    NoteSession* observer = session.get();
    auto window = std::make_unique<widgets::Window>("Note — " + task->title);
    const int offset = static_cast<int>(notes_.size()) * 2;
    window->set_bounds(Rect{6 + offset, 4 + offset, 58, 16});
    window->set_min_size(Size{30, 8});
    window->set_footer("Saved");
    auto memo = std::make_unique<ObservedMemo>();
    memo->set_text(task->note);
    memo->set_help_context_key("todo.note");
    observer->memo = memo.get();
    memo->on_changed = [this, task_id = task->id] { schedule_note_save(task_id); };
    window->set_content(std::move(memo));
    window->close_request = [this, task_id = task->id] { return flush_note(task_id); };
    window->on_closed = [this, observer] { close_note_session(observer); };
    observer->window = desktop_->add_window(std::move(window));
    notes_.push_back(std::move(session));
    app_.set_focus(observer->memo);
}

void TodoApp::schedule_note_save(TaskId task_id) {
    const auto found = std::find_if(notes_.begin(), notes_.end(), [task_id](const auto& note) {
        return note->task_id == task_id;
    });
    if (found == notes_.end()) return;
    NoteSession& note = **found;
    if (note.timer != 0) app_.cancel_timer(note.timer);
    note.window->set_footer("Saving...");
    note.timer = app_.start_timer(kNoteSaveDelayNanos, false, [this, task_id] { flush_note(task_id); });
}

bool TodoApp::flush_note(TaskId task_id) {
    const auto found = std::find_if(notes_.begin(), notes_.end(), [task_id](const auto& note) {
        return note->task_id == task_id;
    });
    if (found == notes_.end()) return true;
    NoteSession& note = **found;
    if (note.timer != 0) {
        app_.cancel_timer(note.timer);
        note.timer = 0;
    }
    const Task* task = controller_.workspace() ? controller_.workspace()->find_task(task_id) : nullptr;
    if (task == nullptr) return true;
    const std::string current_text = note.memo->text();
    if (current_text == note.last_saved) {
        note.window->set_footer("Saved");
        return true;
    }
    TaskDraft draft = draft_from(*task);
    draft.note = current_text;
    const auto saved = controller_.edit_task(task_id, std::move(draft));
    if (!saved) {
        note.window->set_footer("Not saved: " + saved.error.diagnostic);
        set_status("Note not saved: " + saved.error.diagnostic);
        return false;
    }
    note.last_saved = current_text;
    note.window->set_footer("Saved");
    refresh_board();
    return true;
}

void TodoApp::close_note_session(NoteSession* session) {
    if (session == nullptr) return;
    if (session->timer != 0) app_.cancel_timer(session->timer);
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [session](const auto& item) {
                     return item.get() == session;
                 }),
                 notes_.end());
}

void TodoApp::present_lane_actions() {
    const Lane* lane = active_lane_record();
    if (lane == nullptr) return;
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Lane actions — " + lane->title;
    descriptor.fields.push_back(radio_field("&Action", {"Rename", "Color", "Sort", "Insert left",
                                                          "Insert right", "Merge into another lane", "Archive lane"},
                                                   0));
    descriptor.buttons.push_back({"&Continue", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_actions_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_actions_dialog_->set_completion_handler([this](widgets::DialogResult result) {
        if (!result.accepted || result.selected.empty()) return;
        switch (result.selected[0]) {
            case 0: present_lane_rename(); break;
            case 1: present_lane_color(); break;
            case 2: present_lane_sort(); break;
            case 3: present_lane_insert(true); break;
            case 4: present_lane_insert(false); break;
            case 5: present_lane_merge(); break;
            case 6: present_lane_archive(); break;
            default: break;
        }
    });
}

void TodoApp::present_lane_rename() {
    const Lane* lane = active_lane_record();
    if (lane == nullptr) return;
    const LaneId lane_id = lane->id;
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Rename lane";
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", lane->title,
        [](const std::string& value) { return !value.empty() && value.size() <= TodoLimits::max_name_bytes; }});
    descriptor.buttons.push_back({"&Rename", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_edit_dialog_->set_completion_handler([this, lane_id](widgets::DialogResult result) {
        if (!result.accepted || result.values.empty()) return;
        const auto renamed = controller_.rename_lane(lane_id, result.values[0]);
        if (!renamed) {
            show_error("Cannot rename lane", renamed.error);
            return;
        }
        refresh_board();
        set_status("Lane renamed and saved.");
    });
}

void TodoApp::present_lane_color() {
    const Lane* lane = active_lane_record();
    if (lane == nullptr) return;
    const LaneId lane_id = lane->id;
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Lane color — " + lane->title;
    descriptor.fields.push_back(combo_field("&Color:", strings(kColorNames), color_selection(lane->color)));
    descriptor.buttons.push_back({"&Apply", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_edit_dialog_->set_completion_handler([this, lane_id](widgets::DialogResult result) {
        if (!result.accepted || result.selected.empty()) return;
        const auto changed = controller_.set_lane_color(lane_id, color_from_selection(result.selected[0]));
        if (!changed) {
            show_error("Cannot change lane color", changed.error);
            return;
        }
        refresh_board();
        set_status("Lane color saved.");
    });
}

void TodoApp::present_lane_sort() {
    const Lane* lane = active_lane_record();
    if (lane == nullptr) return;
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Sort lane — " + lane->title;
    descriptor.fields.push_back(combo_field("&Sort by:", strings(kSortNames), static_cast<int>(lane->sort)));
    descriptor.buttons.push_back({"&Apply", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_edit_dialog_->set_completion_handler([this](widgets::DialogResult result) {
        if (!result.accepted || result.selected.empty() || result.selected[0] < 0 || result.selected[0] > 5) return;
        set_lane_sort(static_cast<SortMode>(result.selected[0]));
    });
}

void TodoApp::present_lane_insert(bool before_active) {
    const Board* board = active_board();
    const auto lane_id = active_lane();
    if (board == nullptr || !lane_id) return;
    std::optional<LaneId> before;
    const auto found = std::find_if(board->lanes.begin(), board->lanes.end(), [lane_id](const Lane& lane) {
        return lane.id == *lane_id;
    });
    if (found == board->lanes.end()) return;
    if (before_active) before = *lane_id;
    else if (std::next(found) != board->lanes.end()) before = std::next(found)->id;
    const BoardId board_id = board->id;

    widgets::DialogDescriptor descriptor;
    descriptor.title = before_active ? "Insert lane left" : "Insert lane right";
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", {},
        [](const std::string& value) { return !value.empty() && value.size() <= TodoLimits::max_name_bytes; }});
    descriptor.buttons.push_back({"&Insert", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_edit_dialog_->set_completion_handler([this, board_id, before](widgets::DialogResult result) {
        if (!result.accepted || result.values.empty()) return;
        const auto inserted = controller_.insert_lane(board_id, result.values[0], before);
        if (!inserted) {
            show_error("Cannot insert lane", inserted.error);
            return;
        }
        refresh_board();
        if (TodoLaneView* lane = board_view_->lane_view(*inserted.value)) app_.set_focus(lane);
        set_status("Lane inserted and saved.");
    });
}

void TodoApp::present_lane_merge() {
    const Board* board = active_board();
    const auto source_id = active_lane();
    if (board == nullptr || !source_id || board->lanes.size() < 2) return;
    std::vector<LaneId> targets;
    std::vector<std::string> labels;
    for (const Lane& lane : board->lanes) {
        if (lane.id == *source_id) continue;
        targets.push_back(lane.id);
        labels.push_back(lane.title + "  (#" + std::to_string(lane.id.value) + ")");
    }
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Merge lane";
    descriptor.fields.push_back(combo_field("Merge &into:", std::move(labels), 0));
    descriptor.buttons.push_back({"&Merge", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    lane_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    lane_edit_dialog_->set_completion_handler(
        [this, captured_source_id = *source_id,
         captured_targets = std::move(targets)](widgets::DialogResult result) {
            if (!result.accepted || result.selected.empty() || result.selected[0] < 0 ||
                static_cast<std::size_t>(result.selected[0]) >= captured_targets.size())
                return;
            const LaneId target_id = captured_targets[static_cast<std::size_t>(result.selected[0])];
            const auto merged = controller_.merge_lane(captured_source_id, target_id);
            if (!merged) {
                show_error("Cannot merge lane", merged.error);
                return;
            }
            refresh_board();
            if (TodoLaneView* lane = board_view_->lane_view(target_id)) app_.set_focus(lane);
            set_status("Lane merged and removed.");
        });
}

void TodoApp::present_lane_archive() {
    const Lane* lane = active_lane_record();
    if (lane == nullptr) return;
    const LaneId lane_id = lane->id;
    lane_confirmation_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Warning, "Archive lane",
         "Archive all " + std::to_string(lane->task_ids.size()) + " task(s) in ‘" + lane->title +
             "’ and remove the lane?",
         widgets::MessageBoxButtons::YesNo});
    lane_confirmation_->set_completion_handler([this, lane_id](widgets::MessageBoxResult result) {
        if (result != widgets::MessageBoxResult::Yes) return;
        const auto archived = controller_.archive_lane(lane_id);
        if (!archived) {
            show_error("Cannot archive lane", archived.error);
            return;
        }
        refresh_board();
        set_status("Lane tasks archived before the lane was removed.");
    });
}

void TodoApp::set_lane_sort(SortMode sort) {
    const auto lane_id = active_lane();
    if (!lane_id) return;
    const auto changed = controller_.set_lane_sort(*lane_id, sort);
    if (!changed) {
        show_error("Cannot change lane sorting", changed.error);
        return;
    }
    refresh_board();
    set_status("Lane sorting saved.");
}

void TodoApp::present_board_manager() {
    const TodoWorkspace* workspace = controller_.workspace();
    if (workspace == nullptr) return;
    std::vector<BoardId> boards;
    std::vector<std::string> labels;
    int selected = 0;
    for (std::size_t index = 0; index < workspace->snapshot().boards.size(); ++index) {
        const Board& board = workspace->snapshot().boards[index];
        boards.push_back(board.id);
        labels.push_back(board.name);
        if (board.id == workspace->snapshot().last_board_id) selected = static_cast<int>(index);
    }
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Board Manager";
    descriptor.fields.push_back(combo_field("&Board:", std::move(labels), selected));
    descriptor.fields.push_back(radio_field("&Action", {"Switch", "Create new", "Rename", "Merge into", "Archive"}, 0));
    descriptor.buttons.push_back({"&Continue", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    board_manager_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    board_manager_dialog_->set_completion_handler(
        [this, captured_boards = std::move(boards)](widgets::DialogResult result) {
            if (!result.accepted || result.selected.size() < 2 || result.selected[1] < 0) return;
            if (result.selected[1] == 1) {
                present_new_board();
                return;
            }
            if (result.selected[0] < 0 || static_cast<std::size_t>(result.selected[0]) >= captured_boards.size())
                return;
            const BoardId board_id = captured_boards[static_cast<std::size_t>(result.selected[0])];
            switch (result.selected[1]) {
                case 0: switch_board(board_id); break;
                case 2: present_board_rename(board_id); break;
                case 3: present_board_merge(board_id); break;
                case 4: present_board_archive(board_id); break;
                default: break;
            }
        });
}

void TodoApp::present_new_board() {
    widgets::DialogDescriptor descriptor;
    descriptor.title = "New Board";
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", {},
        [](const std::string& value) { return !value.empty() && value.size() <= TodoLimits::max_name_bytes; }});
    descriptor.buttons.push_back({"&Create", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    board_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    board_edit_dialog_->set_completion_handler([this](widgets::DialogResult result) {
        if (!result.accepted || result.values.empty()) return;
        const auto added = controller_.add_board(result.values[0]);
        if (!added) {
            show_error("Cannot create board", added.error);
            return;
        }
        switch_board(*added.value);
        set_status("Board created, selected, and saved.");
    });
}

void TodoApp::present_board_rename(BoardId board_id) {
    const TodoWorkspace* workspace = controller_.workspace();
    const Board* board = workspace != nullptr ? workspace->find_board(board_id) : nullptr;
    if (board == nullptr) return;
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Rename Board";
    descriptor.fields.push_back(widgets::FieldDescriptor{
        "&Name:", board->name,
        [](const std::string& value) { return !value.empty() && value.size() <= TodoLimits::max_name_bytes; }});
    descriptor.buttons.push_back({"&Rename", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    board_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    board_edit_dialog_->set_completion_handler([this, board_id](widgets::DialogResult result) {
        if (!result.accepted || result.values.empty()) return;
        const auto renamed = controller_.rename_board(board_id, result.values[0]);
        if (!renamed) {
            show_error("Cannot rename board", renamed.error);
            return;
        }
        refresh_board();
        set_status("Board renamed and saved.");
    });
}

void TodoApp::present_board_merge(BoardId source_id) {
    const TodoWorkspace* workspace = controller_.workspace();
    if (workspace == nullptr || workspace->snapshot().boards.size() < 2) return;
    std::vector<BoardId> targets;
    std::vector<std::string> labels;
    for (const Board& board : workspace->snapshot().boards) {
        if (board.id == source_id) continue;
        targets.push_back(board.id);
        labels.push_back(board.name);
    }
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Merge Board";
    descriptor.fields.push_back(combo_field("Merge &into:", std::move(labels), 0));
    descriptor.buttons.push_back({"&Merge", widgets::ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back({"&Cancel", widgets::ButtonRole::Dismiss, nullptr});
    board_edit_dialog_ = widgets::present_dialog(std::move(descriptor), app_, *desktop_, roles_);
    board_edit_dialog_->set_completion_handler(
        [this, source_id, captured_targets = std::move(targets)](widgets::DialogResult result) {
            if (!result.accepted || result.selected.empty() || result.selected[0] < 0 ||
                static_cast<std::size_t>(result.selected[0]) >= captured_targets.size())
                return;
            const auto merged =
                controller_.merge_board(source_id, captured_targets[static_cast<std::size_t>(result.selected[0])]);
            if (!merged) {
                show_error("Cannot merge board", merged.error);
                return;
            }
            refresh_board();
            set_status("Board merged and removed.");
        });
}

void TodoApp::present_board_archive(BoardId board_id) {
    const TodoWorkspace* workspace = controller_.workspace();
    const Board* board = workspace != nullptr ? workspace->find_board(board_id) : nullptr;
    if (board == nullptr) return;
    std::size_t task_count = 0;
    for (const Lane& lane : board->lanes) task_count += lane.task_ids.size();
    board_confirmation_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Warning, "Archive Board",
         "Archive all " + std::to_string(task_count) + " task(s) in ‘" + board->name + "’ and remove the board?",
         widgets::MessageBoxButtons::YesNo});
    board_confirmation_->set_completion_handler([this, board_id](widgets::MessageBoxResult result) {
        if (result != widgets::MessageBoxResult::Yes) return;
        const auto archived = controller_.archive_board(board_id);
        if (!archived) {
            show_error("Cannot archive board", archived.error);
            return;
        }
        refresh_board();
        set_status("Board tasks archived before the board was removed.");
    });
}

void TodoApp::switch_board(BoardId board_id) {
    const auto switched = controller_.switch_board(board_id);
    if (!switched) {
        show_error("Cannot switch board", switched.error);
        return;
    }
    refresh_board();
    set_status("Board selected and remembered.");
}

void TodoApp::toggle_move() {
    if (!move_) {
        const Task* task = selected_task();
        if (task == nullptr) return;
        const auto started = controller_.begin_task_move(task->id);
        if (!started) {
            show_error("Cannot move task", started.error);
            return;
        }
        move_ = *started.value;
        board_view_->set_move_source(task->id);
        board_view_->set_move_target(board_view_->active_lane());
        set_status("Move mode: arrows choose lane and position; Enter commits; Esc cancels.");
        return;
    }

    const auto lane = active_lane();
    if (!lane) return;
    std::optional<TaskId> before = board_view_->selected_task();
    if (before == move_->task_id) before = move_->original_before_task_id;
    const auto staged = controller_.stage_task_move(*move_, *lane, before);
    if (!staged) {
        show_error("Cannot stage task move", staged.error);
        return;
    }
    const TaskId task_id = move_->task_id;
    const auto committed = controller_.commit_task_move(*staged.value);
    if (!committed) {
        show_error("Cannot move task", committed.error);
        return;
    }
    move_.reset();
    refresh_board();
    board_view_->select_task(task_id);
    set_status(committed.value->changed ? "Task moved and saved." : "Task stayed in its original position.");
}

void TodoApp::cancel_move() {
    if (!move_) return;
    const TaskId task_id = move_->task_id;
    move_.reset();
    board_view_->set_move_source(std::nullopt);
    board_view_->set_move_target(std::nullopt);
    board_view_->select_task(task_id);
    set_status("Move cancelled; original position restored.");
}

void TodoApp::drop_task(TaskId task_id, LaneId target_lane_id, std::optional<TaskId> before_task_id) {
    const TodoWorkspace* workspace = controller_.workspace();
    const Lane* target = workspace != nullptr ? workspace->find_lane(target_lane_id) : nullptr;
    if (target == nullptr) return;
    const auto started = controller_.begin_task_move(task_id);
    if (!started) {
        show_error("Cannot drag task", started.error);
        return;
    }
    if (target->sort != SortMode::Manual) {
        if (started.value->source_lane_id == target_lane_id) {
            set_status("This lane is sorted automatically; choose Manual before reordering it.");
            return;
        }
        before_task_id.reset();
    }
    if (before_task_id == task_id) before_task_id = started.value->original_before_task_id;
    const auto staged = controller_.stage_task_move(*started.value, target_lane_id, before_task_id);
    if (!staged) {
        show_error("Cannot stage dragged task", staged.error);
        return;
    }
    const auto committed = controller_.commit_task_move(*staged.value);
    if (!committed) {
        show_error("Cannot drop task", committed.error);
        return;
    }
    refresh_board();
    board_view_->select_task(task_id);
    set_status(committed.value->changed ? "Task moved by drag and saved." : "Task stayed in its original position.");
}

void TodoApp::show_task_context_menu(Point screen_cell) {
    widgets::show_context_menu(
        {widgets::MenuItem::command(edit_task_command_), widgets::MenuItem::command(edit_note_command_),
         widgets::MenuItem::command(move_task_command_), widgets::MenuItem::separator(),
         widgets::MenuItem::command(archive_task_command_), widgets::MenuItem::command(delete_task_command_)},
        screen_cell, app_, *desktop_);
}

void TodoApp::show_lane_context_menu(Point screen_cell) {
    const SortMode active_sort = active_lane_record() != nullptr ? active_lane_record()->sort : SortMode::Manual;
    std::vector<widgets::MenuItem> sort_items;
    for (std::size_t index = 0; index < lane_sort_commands_.size(); ++index) {
        sort_items.push_back(widgets::MenuItem::command(lane_sort_commands_[index])
                                 .with_mark(static_cast<SortMode>(index) == active_sort
                                                ? widgets::MenuMark::RadioOn
                                                : widgets::MenuMark::RadioOff));
    }
    widgets::show_context_menu(
        {widgets::MenuItem::submenu("&Sort", std::move(sort_items)),
         widgets::MenuItem::command(lane_color_command_), widgets::MenuItem::command(lane_rename_command_),
         widgets::MenuItem::command(lane_insert_left_command_),
         widgets::MenuItem::command(lane_insert_right_command_), widgets::MenuItem::separator(),
         widgets::MenuItem::command(lane_merge_command_), widgets::MenuItem::command(lane_archive_command_)},
        screen_cell, app_, *desktop_);
}

void TodoApp::set_theme(TodoTheme theme) {
    theme_ = theme;
    switch (theme_) {
        case TodoTheme::Classic: app_.theme() = ui::make_classic_theme(app_.roles(), roles_); break;
        case TodoTheme::Dark: app_.theme() = ui::make_dark_theme(app_.roles(), roles_); break;
        case TodoTheme::Light: app_.theme() = ui::make_light_theme(app_.roles(), roles_); break;
        case TodoTheme::Mono: app_.theme() = ui::make_mono_theme(app_.roles(), roles_); break;
        case TodoTheme::HighContrast: app_.theme() = ui::make_high_contrast_theme(app_.roles(), roles_); break;
    }
    if (menu_bar_ != nullptr) menu_bar_->set_menus(build_menus());
    app_.invalidate_all();
}

void TodoApp::refresh_status_items() {
    if (status_line_ == nullptr) return;
    std::string board_label = "Board";
    if (const Board* board = active_board()) board_label = "Board: " + board->name;
    std::string calendar_label;
    if (const CalendarReadResult reading = calendar_.read(); reading) {
        calendar_label = reading.value->local_date.value;
        if (reading.value->utc_timestamp.value.size() >= 16)
            calendar_label += " " + reading.value->utc_timestamp.value.substr(11, 5) + "Z";
    }
    std::vector<widgets::StatusLineItem> items{
        widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().help}, 100},
        widgets::StatusLineItem{widgets::CommandPresentation{add_task_command_}, 95},
        widgets::StatusLineItem{widgets::CommandPresentation{edit_task_command_}, 70},
        widgets::StatusLineItem{widgets::CommandPresentation{edit_note_command_}, 60},
        widgets::StatusLineItem{widgets::CommandPresentation{archive_task_command_}, 50},
        widgets::StatusLineItem{widgets::CommandPresentation{move_task_command_}, 90},
        widgets::StatusLineItem{widgets::CommandPresentation{lane_actions_command_}, 80},
        widgets::StatusLineItem{std::move(board_label), board_manager_command_, 95},
    };
    if (!calendar_label.empty()) items.emplace_back(std::move(calendar_label), ui::kInvalidCommand, 5);
    items.emplace_back(widgets::CommandPresentation{app_.commands().standard().quit}, 100);
    status_line_->set_items(std::move(items));
}

void TodoApp::show_error(std::string title, const ControllerError& error) {
    set_status(error.diagnostic);
    message_box_ = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Error, std::move(title), error.diagnostic, widgets::MessageBoxButtons::Ok});
    message_box_->set_completion_handler([](widgets::MessageBoxResult) {});
}

void TodoApp::set_status(std::string message) {
    if (status_line_ == nullptr) return;
    if (status_timer_ != 0) {
        app_.cancel_timer(status_timer_);
        status_timer_ = 0;
    }
    status_line_->set_transient_hint(std::move(message));
    status_timer_ = app_.start_timer(kStatusHintNanos, false, [this] {
        status_timer_ = 0;
        if (status_line_ != nullptr) status_line_->set_transient_hint({});
    });
}

const Task* TodoApp::selected_task() const noexcept {
    if (controller_.workspace() == nullptr || board_view_ == nullptr) return nullptr;
    const auto selected = board_view_->selected_task();
    return selected ? controller_.workspace()->find_task(*selected) : nullptr;
}

std::optional<LaneId> TodoApp::active_lane() const noexcept {
    return board_view_ != nullptr ? board_view_->active_lane() : std::nullopt;
}

const Lane* TodoApp::active_lane_record() const noexcept {
    const TodoWorkspace* workspace = controller_.workspace();
    const auto lane_id = active_lane();
    return workspace != nullptr && lane_id ? workspace->find_lane(*lane_id) : nullptr;
}

const Board* TodoApp::active_board() const noexcept {
    const TodoWorkspace* workspace = controller_.workspace();
    return workspace != nullptr && board_view_ != nullptr ? workspace->find_board(board_view_->board_id()) : nullptr;
}

std::optional<BoardId> TodoApp::board_named(std::string_view name) const noexcept {
    if (controller_.workspace() == nullptr) return std::nullopt;
    for (const Board& board : controller_.workspace()->snapshot().boards)
        if (board.name == name) return board.id;
    return std::nullopt;
}

}  // namespace ckv::todo
