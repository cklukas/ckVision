// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "fuzz_common.hpp"
#include "cvision/widgets/progress_tasks.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/scene/surface.hpp"
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ckv;
    widgets::ProgressModel model;
    std::vector<widgets::ProgressTaskId> ids;
    std::int64_t now = 0;
    for (std::size_t i = 0; i + 2 < size && i < 384; i += 3) {
        const auto id = ids.empty() ? 0 : ids[data[i + 1] % ids.size()];
        switch (data[i] % 9) {
        case 0: ids.push_back(model.add_task({.title = std::string(reinterpret_cast<const char*>(data + i), 3), .parent = id, .total = static_cast<double>(data[i + 2])})); break;
        case 1: model.start(id); break;
        case 2: model.set_completed(id, data[i + 2]); break;
        case 3: model.set_parent(id, ids.empty() ? 0 : ids[data[i + 2] % ids.size()]); break;
        case 4: model.complete(id); break;
        case 5: model.cancel(id); break;
        case 6: model.set_aggregation(id, widgets::ProgressAggregation::WeightedChildren); break;
        case 7: model.remove(id); break;
        default: now += data[i + 2] * 1'000'000LL; model.set_time(now); break;
        }
        if (model.find(id)) { const auto m = model.metrics(id); fuzz::require(!m.fraction || (*m.fraction >= 0 && *m.fraction <= 1)); }
    }
    ui::RoleRegistry roles; const auto standard = ui::intern_standard_roles(roles);
    auto theme = ui::make_classic_theme(roles, standard);
    widgets::ProgressView view(model); widgets::ActivityIndicator indicator;
    view.set_context({&theme, &roles, nullptr}); indicator.set_context({&theme, &roles, nullptr});
    const int width = size ? data[0] % 80 + 1 : 1;
    view.set_bounds({0, 0, width, 8}); indicator.set_bounds({0, 0, width, 1});
    indicator.set_frames({std::string(reinterpret_cast<const char*>(data), size)});
    scene::Surface surface({width, 8}, Cell{}); scene::Painter painter(surface, {0, 0, width, 8});
    view.draw(painter); indicator.draw(painter);
    return 0;
}
