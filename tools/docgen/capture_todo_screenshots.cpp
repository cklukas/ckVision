// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Deterministic figures for the TODO example. Every scene runs the shipped
// TodoApp against the same public headless terminal path as the interactive
// executable, with fixed repository, identity, and calendar services.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/menu.hpp"

#include "frame_svg.hpp"
#include "memory_todo_repository.hpp"
#include "todo_app.hpp"

namespace {

using namespace ckv;
using namespace ckv::todo;

[[noreturn]] void fail(std::string_view message) {
    std::fprintf(stderr, "todo screenshot capture: %.*s\n", static_cast<int>(message.size()), message.data());
    std::exit(1);
}

CalendarReading fixed_reading() {
    return {IsoTimestamp{"2026-08-25T12:00:00Z"}, IsoDate{"2026-08-25"}};
}

AuditStamp external_stamp() { return {IsoTimestamp{"2026-08-25T12:01:00Z"}, "external-instance"}; }

TodoWorkspace guided_workspace() {
    const auto guided = TodoWorkspace::guided({IsoTimestamp{"2026-08-25T11:00:00Z"}, "demo"});
    if (!guided) fail("could not create the guided workspace");
    return *guided.value;
}

struct Scene {
    term::HeadlessTerminal terminal;
    ManualClock monotonic;
    ui::Application app;
    MemoryTodoRepository repository;
    FixedCalendarClock calendar;
    TodoApp todo;

    explicit Scene(Size size = {100, 30}, TodoWorkspace workspace = guided_workspace())
        : terminal(size, term::headless_no_graphics_profile()),
          app(terminal, monotonic),
          repository(std::move(workspace)),
          calendar(fixed_reading()),
          todo(app, repository, calendar, "capture", {.workspace_description = "in-memory demo workspace"}) {}
};

bool execute(Scene& scene, std::string_view key) {
    const auto command = scene.app.commands().id_for(key);
    return command.has_value() && scene.app.execute_command(*command);
}

KeyEvent key(Key value) { return KeyEvent{KeyChord{value, Modifier::None, {}}}; }

void write_scene(const std::filesystem::path& screenshot_directory,
                 const std::optional<std::filesystem::path>& golden_directory,
                 std::string_view name,
                 Scene& scene,
                 bool golden) {
    scene.app.step(scene.monotonic.now_nanos());
    const std::filesystem::path svg_path = screenshot_directory / (std::string(name) + ".svg");
    std::ofstream svg(svg_path);
    if (!svg) fail("could not open an output SVG");
    svg << docgen::render_virtual_display_svg(scene.terminal.display());
    if (!svg) fail("could not write an output SVG");
    std::fprintf(stderr, "wrote %s\n", svg_path.string().c_str());

    if (!golden || !golden_directory) return;
    const std::filesystem::path dump_path = *golden_directory / (std::string(name) + ".dump");
    std::ofstream dump(dump_path);
    if (!dump) fail("could not open an output golden dump");
    dump << golden::serialize(scene::capture(scene.app.composed_surface(), scene.app.current_cursor()));
    if (!dump) fail("could not write an output golden dump");
    std::fprintf(stderr, "wrote %s\n", dump_path.string().c_str());
}

void select_theme(Scene& scene, TodoTheme theme) {
    const auto& menus = scene.todo.menu_bar()->menus();
    const auto view = std::find_if(menus.begin(), menus.end(), [](const widgets::MenuBarItem& menu) {
        return menu.label == "&View";
    });
    if (view == menus.end() || view->items.empty()) fail("View theme menu was unavailable");
    const auto& themes = view->items.front().children();
    const std::size_t index = static_cast<std::size_t>(theme);
    if (index >= themes.size() || !themes[index].action()) fail("requested theme action was unavailable");
    themes[index].action()();
}

void commit_external_title(Scene& scene, TaskId task_id, std::string title) {
    auto loaded = scene.repository.load();
    if (!loaded) fail("could not load the external workspace");
    const Task* task = loaded.value->workspace.find_task(task_id);
    if (task == nullptr) fail("external task was unavailable");
    TaskDraft draft{task->title, task->details, task->note, task->priority, task->due_date, task->color};
    draft.title = std::move(title);
    if (!loaded.value->workspace.edit_task(task_id, std::move(draft), external_stamp()))
        fail("could not edit the external task");
    if (!scene.repository.commit(loaded.value->revision, loaded.value->workspace, fixed_reading().local_date))
        fail("could not commit the external task");
}

TodoWorkspace many_lane_workspace() {
    TodoWorkspace workspace = guided_workspace();
    for (std::string name : {"Review", "Blocked", "Later"}) {
        const auto lane = workspace.insert_lane(BoardId{1}, std::move(name));
        if (!lane) fail("could not insert a capture lane");
        TaskDraft task;
        task.title = "A task in this lane";
        task.details = "Horizontal scrolling keeps every lane useful";
        if (!workspace.add_task(*lane.value, std::move(task), external_stamp()))
            fail("could not add a capture task");
    }
    return workspace;
}

void capture_product_scenes(const std::filesystem::path& screenshot_directory,
                            const std::optional<std::filesystem::path>& golden_directory) {
    Scene guided;
    write_scene(screenshot_directory, golden_directory, "todo-guided", guided, true);

    Scene task_editor;
    if (!execute(task_editor, TodoApp::kEditTaskKey)) fail("Edit task command was unavailable");
    write_scene(screenshot_directory, golden_directory, "todo-task-editor", task_editor, true);

    Scene moving;
    if (!execute(moving, TodoApp::kMoveTaskKey)) fail("Move command was unavailable");
    if (!moving.app.dispatch(key(Key::Right))) fail("Move target could not change lanes");
    write_scene(screenshot_directory, golden_directory, "todo-move-mode", moving, true);

    Scene lane_actions;
    if (!execute(lane_actions, TodoApp::kLaneActionsKey)) fail("Lane actions command was unavailable");
    write_scene(screenshot_directory, golden_directory, "todo-lane-actions", lane_actions, true);

    Scene board_manager;
    if (!execute(board_manager, TodoApp::kBoardManagerKey)) fail("Board Manager command was unavailable");
    write_scene(screenshot_directory, golden_directory, "todo-board-manager", board_manager, true);

    Scene note;
    if (!execute(note, TodoApp::kEditNoteKey)) fail("Note command was unavailable");
    if (!note.app.dispatch(TextEvent{"A modeless ckVision TextEditor note.\nUndo, find, clipboard, and wrap stay available.",
                                     false, false}))
        fail("note text was not accepted");
    write_scene(screenshot_directory, golden_directory, "todo-note-editor", note, true);

    Scene narrow({72, 22}, many_lane_workspace());
    write_scene(screenshot_directory, golden_directory, "todo-narrow-many-lanes", narrow, true);

    Scene conflict;
    commit_external_title(conflict, TaskId{1}, "Changed in another instance");
    if (!execute(conflict, TodoApp::kDeleteTaskKey)) fail("Delete command was unavailable");
    if (!conflict.app.dispatch(key(Key::Enter))) fail("Delete confirmation could not be accepted");
    conflict.app.step(conflict.monotonic.now_nanos());
    write_scene(screenshot_directory, golden_directory, "todo-conflict-resolution", conflict, true);
}

void capture_themes(const std::filesystem::path& screenshot_directory) {
    for (const auto [theme, name] : {std::pair{TodoTheme::Dark, "todo-theme-dark"},
                                    std::pair{TodoTheme::Light, "todo-theme-light"},
                                    std::pair{TodoTheme::Mono, "todo-theme-mono"},
                                    std::pair{TodoTheme::HighContrast, "todo-theme-high-contrast"}}) {
        Scene scene;
        select_theme(scene, theme);
        write_scene(screenshot_directory, std::nullopt, name, scene, false);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "usage: %s <screenshot-directory> [golden-directory]\n", argv[0]);
        return 1;
    }
    const std::filesystem::path screenshot_directory = argv[1];
    std::filesystem::create_directories(screenshot_directory);
    std::optional<std::filesystem::path> golden_directory;
    if (argc == 3) {
        golden_directory = std::filesystem::path(argv[2]);
        std::filesystem::create_directories(*golden_directory);
    }
    capture_product_scenes(screenshot_directory, golden_directory);
    capture_themes(screenshot_directory);
    return 0;
}
