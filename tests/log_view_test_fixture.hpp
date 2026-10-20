// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/log_panel.hpp"

namespace ckv::testing {
inline LogLimits log_view_limits(std::size_t records = 16) {
    LogLimits limits;
    limits.records = records; limits.record_bytes = 128;
    limits.retained_bytes = kLogMetadataBytes + 16384;
    return limits;
}
inline void draw_log_tree(ui::View& view, scene::Painter& painter) {
    view.draw(painter);
    for (const auto& child : view.children()) if (child->visible()) {
        const auto box = child->bounds();
        auto local = painter.translated({box.x, box.y}, {0, 0, box.width, box.height});
        draw_log_tree(*child, local);
    }
}
inline constexpr std::array<std::string_view, 16> log_golden_names{
    "follow", "paused", "resumed", "wrapped", "selection", "evicted",
    "empty", "error", "resize", "dark", "light", "mono", "narrow",
    "selection_append", "selection_filtered", "selection_evicted"
};
struct LogGoldenProvider : widgets::LogRecordProvider {
    const widgets::LogModel& model;
    bool empty = false, skip_first = false;
    std::uint64_t query_revision = 0;
    explicit LogGoldenProvider(const widgets::LogModel& value) : model(value) {}
    std::size_t size() const noexcept override { return empty ? 0 : model.size() - (skip_first && model.size() ? 1U : 0U); }
    std::optional<widgets::LogRecord> at(std::size_t index) const noexcept override {
        return index < size() ? model.at(index + (skip_first ? 1U : 0U)) : std::nullopt;
    }
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept override {
        const auto index = model.index_of(id);
        if (empty || !index || (skip_first && *index == 0)) return std::nullopt;
        return *index - (skip_first ? 1U : 0U);
    }
    std::uint64_t revision() const noexcept override { return model.revision() + query_revision; }
    std::uint64_t latest_sequence() const noexcept override { return model.latest_sequence(); }
    std::uint64_t evicted_count() const noexcept override { return model.evicted_count(); }
    std::size_t max_text_bytes() const noexcept override { return model.max_text_bytes(); }
};
inline std::string log_view_golden(std::string_view scenario) {
    ui::RoleRegistry registry;
    const auto roles = ui::intern_standard_roles(registry);
    auto theme = scenario == "dark" ? ui::make_dark_theme(registry, roles) :
        scenario == "light" ? ui::make_light_theme(registry, roles) :
        scenario == "mono" ? ui::make_mono_theme(registry, roles) : ui::make_classic_theme(registry, roles);
    widgets::LogModel model(log_view_limits(scenario == "evicted" || scenario == "selection_evicted" ? 6 : 8));
    model.begin_source(1, 1);
    LogGoldenProvider provider(model);
    widgets::LogPanel panel(provider);
    const Size size = scenario == "narrow" ? Size{12, 6} : scenario == "resize" ? Size{25, 7} : Size{48, 8};
    panel.set_context({&theme, &registry, nullptr});
    panel.set_bounds({0, 0, 48, 8});
    panel.set_source_status({LogSourceState::Following, LogWatchMode::Native});
    for (const auto line : {"ready", "worker started", "a long message: alpha beta gamma delta epsilon zeta eta theta iota",
                            "warning: slow disk", "Unicode: 中 é", "tail"})
        model.append_record(line, {}, line == std::string_view{"warning: slow disk"} ? Attr::Bold : static_cast<Attr>(0));
    panel.refresh();
    if (scenario == "paused" || scenario == "resumed" || scenario == "evicted") {
        panel.view().set_top_record(0);
        model.append_record("new arrival");
        panel.refresh();
        if (scenario == "resumed") panel.view().resume();
    }
    if (scenario == "wrapped" || scenario == "resize") panel.view().set_wrap_mode(widgets::WrapMode::Word);
    if (scenario == "selection" || scenario == "selection_append" || scenario == "selection_filtered") {
        const auto first = model.at(2), second = model.at(3);
        panel.view().set_selection({{first->id, 16}, {second->id, 7}});
    }
    if (scenario == "selection_append") model.append_record("arrival after selection");
    if (scenario == "selection_filtered") { provider.skip_first = true; ++provider.query_revision; }
    if (scenario == "selection_evicted") {
        panel.view().set_top_record(0);
        const auto id = model.at(0)->id;
        panel.view().set_selection({{id, 0}, {id, 5}});
        model.append_record("entry replacing retention");
    }
    if (scenario == "empty") { provider.empty = true; ++provider.query_revision; }
    if (scenario == "error") panel.set_source_status({LogSourceState::Denied, LogWatchMode::Polling, 2048, 3, 13, true});
    panel.set_bounds({0, 0, size.width, size.height});
    panel.refresh();
    scene::Surface surface(size, Cell::from_grapheme(" ", Style{}));
    scene::Painter painter(surface, {0, 0, size.width, size.height});
    draw_log_tree(panel, painter);
    return golden::serialize(scene::capture(surface));
}
}  // namespace ckv::testing
