// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// M2 benchmark smoke: exercises the compositor's compose stage, both a
// full initial composition and the steady-state (no damage) path the
// performance charter requires to be cheap (ARCHITECTURE.md §8).
#include <cstdio>

#include "ckbench.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"

using namespace ckv;
using namespace ckv::scene;

void run_scene_benchmarks() {
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

    ckbench::run("compose_steady_state_no_damage", 1000, [&] {
        compositor.compose(layers, background, ShadowSpec{});
        sink += compositor.last_compose_cells_touched();
    });
    std::printf("  (touched %zu cells per steady-state compose — must be 0)\n",
                compositor.last_compose_cells_touched());

    ckbench::run("compose_single_cell_content_change", 500, [&] {
        window.set_cell(Point{5, 5}, Cell::from_grapheme("x", Style{}));
        compositor.compose(layers, background, ShadowSpec{});
        sink += compositor.last_compose_cells_touched();
    });

    std::printf("checksum %zu\n", sink);
}
