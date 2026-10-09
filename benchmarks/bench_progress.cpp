// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "ckbench.hpp"
#include "cvision/widgets/progress_tasks.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/scene/surface.hpp"
bool run_progress_benchmarks(const ckbench::Runner& bench) {
    using namespace ckv;
    ui::RoleRegistry roles; const auto standard = ui::intern_standard_roles(roles);
    auto theme = ui::make_classic_theme(roles, standard);
    widgets::ProgressModel model;
    const auto parent = model.add_task({.title = "Import", .aggregation = widgets::ProgressAggregation::WeightedChildren});
    for (int i = 0; i < 100; ++i) {
        const auto id = model.add_task({.title = "Files", .parent = parent, .total = 100});
        model.start(id); model.set_completed(id, i);
    }
    widgets::ProgressView view(model); view.set_max_rows(20);
    view.set_context({&theme, &roles, nullptr}); view.set_bounds({0, 0, 100, 20});
    scene::Surface surface({100, 20}, Cell{}); scene::Painter painter(surface, {0, 0, 100, 20});
    bench.run("progress_prepared_20_of_101_rows", 1000, [&] { view.draw(painter); });
    bool valid = true;
    bench.run("progress_weighted_100_children", 1000, [&] {
        const auto metrics = model.metrics(parent);
        valid &= metrics.fraction && *metrics.fraction > 0.49 && *metrics.fraction < 0.50;
    });
    return valid;
}
