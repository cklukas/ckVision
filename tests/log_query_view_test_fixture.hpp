// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "log_view_test_fixture.hpp"

namespace ckv::testing {
inline constexpr std::array<std::string_view, 7> log_query_golden_names{
    "filter_pending", "filter_ready", "filter_empty", "find_highlight", "find_selected", "query_error", "query_narrow"
};
inline std::string log_query_view_golden(std::string_view scenario) {
    ui::RoleRegistry registry;
    const auto roles = ui::intern_standard_roles(registry);
    auto theme = ui::make_classic_theme(registry, roles);
    widgets::LogModel model(log_view_limits()); model.begin_source(1, 1);
    for (const auto value : {"ready", "warning: disk latency", "info: worker", "warning: network", "Unicode é 中 tail"}) model.append_record(value);
    widgets::LogFilter filter(model, {1, 4});
    widgets::LogSearch search(filter, {1, 4});
    widgets::LogPanel panel(filter); panel.set_search(&search);
    const Size size = scenario == "query_narrow" ? Size{15, 8} : Size{56, 9};
    panel.set_context({&theme, &registry, nullptr}); panel.set_bounds({0, 0, size.width, size.height});
    panel.set_source_status({LogSourceState::Following, LogWatchMode::Native});
    panel.filter_box().set_query(scenario == "filter_empty" ? "missing" : "warning");
    if (scenario == "filter_pending") filter.step();
    else while (filter.step()) {}
    if (scenario == "find_highlight" || scenario == "find_selected" || scenario == "query_narrow") {
        panel.show_find(true); panel.find_box().set_query("disk");
        if (scenario == "find_selected") {
            panel.find(widgets::LogSearchDirection::Next);
            while (search.step()) {}
        }
    }
    if (scenario == "query_error") panel.filter_box().set_query(std::string(1025, 'x'));
    panel.refresh();
    scene::Surface surface(size, Cell::from_grapheme(" ", {}));
    scene::Painter painter(surface, {0, 0, size.width, size.height});
    draw_log_tree(panel, painter);
    return golden::serialize(scene::capture(surface));
}
}  // namespace ckv::testing
