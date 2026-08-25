// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Focused TODO example benchmarks. The harness reports timing; correctness and
// deterministic scene budgets remain enforced by the test suite.
#include <cstdio>
#include <string>
#include <utility>

#include "ckbench.hpp"
#include "cvision/term/headless_terminal.hpp"

#include "memory_todo_repository.hpp"
#include "todo_app.hpp"
#include "todo_codec.hpp"

namespace {

using namespace ckv;
using namespace ckv::todo;

AuditStamp benchmark_stamp() {
    return {IsoTimestamp{"2026-08-25T12:00:00Z"}, "benchmark"};
}

TodoWorkspace thousand_task_workspace() {
    const auto guided = TodoWorkspace::guided(benchmark_stamp());
    TodoWorkspace workspace = guided ? *guided.value : TodoWorkspace::empty();
    for (int index = 3; index < 1'000; ++index) {
        TaskDraft task;
        task.title = "Benchmark task " + std::to_string(index + 1);
        task.details = "Stable details for refresh and rendering measurements";
        task.priority = static_cast<Priority>((index % 4) + 1);
        if (!workspace.add_task(LaneId{static_cast<std::uint64_t>((index % 3) + 1)},
                                std::move(task), benchmark_stamp()))
            break;
    }
    return workspace;
}

}  // namespace

int main() {
    const TodoWorkspace workspace = thousand_task_workspace();
    const auto encoded = encode_workspace(workspace);
    if (!encoded) {
        std::fprintf(stderr, "todo benchmark: workspace encoding failed\n");
        return 1;
    }

    std::size_t sink = 0;
    ckbench::run("todo_codec_encode_1000", 100, [&] {
        const auto result = encode_workspace(workspace);
        if (result) sink += result.value->size();
    });
    ckbench::run("todo_codec_decode_1000", 100, [&] {
        const auto result = decode_workspace(*encoded.value);
        if (result) sink += result.value->snapshot().tasks.size();
    });

    MemoryTodoRepository repository(workspace);
    ckbench::run("todo_repository_load_1000", 1'000, [&] {
        const auto result = repository.load();
        if (result) sink += result.value->workspace.snapshot().tasks.size();
    });

    term::HeadlessTerminal terminal(Size{140, 40}, term::headless_no_graphics_profile());
    ManualClock monotonic;
    ui::Application app(terminal, monotonic);
    FixedCalendarClock calendar(
        {IsoTimestamp{"2026-08-25T12:00:00Z"}, IsoDate{"2026-08-25"}});
    TodoApp todo(app, repository, calendar, "benchmark",
                 {.workspace_description = "in-memory benchmark workspace"});
    app.step(0);

    ckbench::run("todo_refresh_1000_tasks", 50, [&] {
        todo.refresh_board();
        sink += todo.board_view()->lane_count();
    });
    ckbench::run("todo_steady_board_render", 250, [&] {
        app.root().invalidate();
        app.step(monotonic.now_nanos());
        sink += app.composed_surface().size().width;
    });

    bool in_second_lane = false;
    ckbench::run("todo_lane_move_commit", 100, [&] {
        const auto pending = todo.controller().begin_task_move(TaskId{1});
        const LaneId target = in_second_lane ? LaneId{1} : LaneId{2};
        if (!pending) return;
        const auto staged = todo.controller().stage_task_move(*pending.value, target, std::nullopt);
        if (!staged) return;
        const auto committed = todo.controller().commit_task_move(*staged.value);
        if (!committed) return;
        in_second_lane = !in_second_lane;
        sink += committed.value->changed ? 1U : 0U;
    });

    ckbench::run("todo_unchanged_revision_poll", 5'000, [&] {
        todo.poll_repository();
        sink += todo.controller().workspace() != nullptr ? 1U : 0U;
    });

    std::printf("checksum %zu\n", sink);
    return sink == 0 ? 1 : 0;
}
