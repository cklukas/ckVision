// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// M2 benchmark smoke: exercises the compositor's compose stage, both a
// full initial composition and the steady-state (no damage) path the
// performance charter requires to be cheap (the architecture §8).
#include <cstdio>

#include "ckbench.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using namespace ckv;
using namespace ckv::scene;

bool run_scene_benchmarks() {
    constexpr int kWidth = 200;
    constexpr int kHeight = 60;

    Compositor compositor(Size{kWidth, kHeight});
    Surface background(Size{kWidth, kHeight}, Cell::from_grapheme(" ", Style{}));

    Surface window(Size{40, 15});
    Painter painter(window, Rect{0, 0, 40, 15});
    painter.draw_box(Rect{0, 0, 40, 15}, LineStyle::Single, Style{});
    painter.draw_text(Point{2, 0}, "Benchmark Window", Style{});
    std::vector<Layer> layers{{1, &window, Point{20, 10}, /*casts_shadow=*/true}};

    std::size_t sink = 0;

    ckbench::run("compose_initial_full_frame", 1, [&] {
        compositor.compose(layers, background, ShadowSpec{});
        sink += compositor.last_compose_cells_touched();
    });
    std::printf("  (touched %zu cells on the first, fully-damaged compose)\n",
                compositor.last_compose_cells_touched());

    bool budgets_hold = true;
    ckbench::run("compose_steady_state_no_damage", 1000, [&] {
        compositor.compose(layers, background, ShadowSpec{});
        sink += compositor.last_compose_cells_touched();
        if (compositor.last_compose_cells_touched() != 0) budgets_hold = false;
    });
    std::printf("  (touched %zu cells per steady-state compose — must be 0)\n",
                compositor.last_compose_cells_touched());

    bool use_x = true;
    ckbench::run("compose_single_cell_content_change", 500, [&] {
        window.set_cell(Point{5, 5}, Cell::from_grapheme(use_x ? "x" : "y", Style{}));
        use_x = !use_x;
        compositor.compose(layers, background, ShadowSpec{});
        sink += compositor.last_compose_cells_touched();
        if (compositor.last_compose_cells_touched() != 1) budgets_hold = false;
    });

    // This is the complete retained interaction rather than a synthetic
    // Layer move: the Window changes bounds, Desktop retains its local
    // backing, Application constrains compositor damage, and Presenter emits
    // the terminal diff. The two positions do not overlap, so two 30x10
    // rectangles plus two 2x1 shadows give the exact 696-cell upper bound.
    term::HeadlessTerminal terminal(Size{kWidth, kHeight});
    ManualClock clock;
    ui::Application app(terminal, clock);
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    app.theme() = ui::make_classic_theme(app.roles(), roles);
    auto desktop_owned = std::make_unique<widgets::Desktop>(Rect{});
    widgets::Desktop* desktop = desktop_owned.get();
    auto window_owned = std::make_unique<widgets::Window>("Benchmark Window");
    window_owned->set_bounds(Rect{10, 5, 30, 10});
    widgets::Window* retained_window = desktop->add_window(std::move(window_owned));
    app.root().add_child(std::move(desktop_owned));
    app.step(0);  // establish retained backing and Presenter history before measurement
    const std::size_t initial_repaints = retained_window->content_repaint_count();
    bool on_right = false;
    constexpr std::size_t kMoveCellsBudget = 696;
    // The cell budget is a geometric bound and is unaffected by chrome
    // detail; the byte budget is not. Each separately colored frame control
    // (close, and the maximize/restore control on a resizable window) adds a
    // truecolor SGR transition to the title row at BOTH the vacated and the
    // newly occupied position, so the emitted diff grew by a small constant
    // when the maximize control landed. This keeps the same "far below a full
    // repaint" guard — a full frame is an order of magnitude larger — with
    // enough headroom for those transitions rather than pinning the budget to
    // the exact byte count of one particular chrome layout.
    constexpr std::size_t kMoveBytesBudget = 6272;
    ckbench::run("retained_window_move", 500, [&] {
        retained_window->set_bounds(Rect{on_right ? 10 : 130, 5, 30, 10});
        on_right = !on_right;
        terminal.clear_written();
        app.step(0);
        sink += app.last_compose_cells_touched();
        if (retained_window->content_repaint_count() != initial_repaints ||
            desktop->last_content_repaints() != 0 || app.last_compose_cells_touched() > kMoveCellsBudget ||
            app.last_bytes_emitted() > kMoveBytesBudget)
            budgets_hold = false;
    });
    std::printf("  (retained move: %zu cells, %zu bytes — budgets %zu/%zu)\n",
                app.last_compose_cells_touched(), app.last_bytes_emitted(), kMoveCellsBudget,
                kMoveBytesBudget);

    std::printf("checksum %zu\n", sink);
    if (!budgets_hold)
        std::fputs("scene budget failure: damage scopes exceeded their checked limits\n", stderr);
    return budgets_hold;
}
