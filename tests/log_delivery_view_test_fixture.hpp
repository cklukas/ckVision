// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "log_view_test_fixture.hpp"
#include "cvision/widgets/log_controller.hpp"

namespace ckv::testing {
inline constexpr std::array<std::string_view, 3> log_delivery_golden_names{
    "overload_follow", "overload_paused", "overload_narrow"
};
inline std::string log_delivery_view_golden(std::string_view scenario) {
    ui::RoleRegistry registry; const auto roles = ui::intern_standard_roles(registry);
    auto theme = ui::make_classic_theme(registry, roles);
    widgets::LogModel model(log_view_limits()); model.begin_source(1, 1);
    widgets::LogFilter filter(model); widgets::LogPanel panel(filter);
    const Size size = scenario == "overload_narrow" ? Size{24, 8} : Size{100, 10};
    panel.set_context({&theme, &registry, nullptr}); panel.set_bounds({0, 0, size.width, size.height});
    LogMailbox mailbox(1, 1, {4, 128, LogOverflowPolicy::DropOldest});
    widgets::LogStreamReceiver receiver(model, mailbox, {32, 2});
    widgets::LogController controller({1, 1, 1, 10});
    controller.attach({&model, &filter, nullptr, &panel, nullptr, &receiver});
    model.append_record("earlier retained context"); model.append_record("inspect this entry");
    controller.step(0);
    if (scenario == "overload_paused") panel.view().set_top_record(0);
    LogMessage message; message.source = 1; message.epoch = 1;
    message.set_record("latest burst message 中", {LogSeverity::Info, {}, {}});
    for (int batch = 0; batch < 12; ++batch) mailbox.offer(message);
    controller.step(100'000'000);
    scene::Surface surface(size, Cell::from_grapheme(" ", {}));
    scene::Painter painter(surface, {0, 0, size.width, size.height});
    draw_log_tree(panel, painter);
    return golden::serialize(scene::capture(surface));
}
}  // namespace ckv::testing
