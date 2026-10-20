// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "appearance_matrix.hpp"
#include "cvision/widgets/log_panel.hpp"

namespace ckv::docgen::appearance {
namespace {
LogLimits limits() {
    LogLimits value; value.records = 64; value.record_bytes = 512; value.retained_bytes = 64 * 1024; return value;
}
// Own the provider until its borrowing child has detached and been destroyed.
class LogSpecimen final : public ui::View {
public:
    widgets::LogModel model{limits()};
    widgets::LogFilter filter{model, {2, 64}};
    widgets::LogSearch search{filter};
    widgets::LogView* display = nullptr;
    widgets::LogPanel* panel = nullptr;
    explicit LogSpecimen(bool composite) {
        model.begin_source(1, 1);
        if (composite) {
            // ckvision-doc: logpanel
            panel = make<widgets::LogPanel>(filter);
            panel->set_search(&search);
            display = &panel->view();
            // ckvision-doc-end: logpanel
        } else {
            // ckvision-doc: logview
            display = make<widgets::LogView>(filter);
            display->set_highlighter(&search);
            // ckvision-doc-end: logview
        }
    }
    ~LogSpecimen() override { remove_child(panel ? static_cast<ui::View*>(panel) : display); }
    void on_resized() override {
        (panel ? static_cast<ui::View*>(panel) : display)->set_bounds({0, 0, bounds().width, bounds().height});
    }
};
void build(Stage& stage, bool composite, std::string_view kind) {
    auto owner = std::make_unique<LogSpecimen>(composite); auto* specimen = owner.get();
    const auto narrow = kind == "narrow";
    stage.window(narrow ? Rect{1, 1, 26, 12} : Rect{1, 1, 72, 14}, composite ? "LogPanel" : "LogView", std::move(owner), false);
    auto& view = *specimen->display;
    if (kind != "empty") {
        for (int index = 0; index < 24; ++index) {
            const auto text = "log " + std::to_string(index) + (kind == "wide" || narrow ? " 表示 🙂 été" : " ordinary record");
            specimen->model.append_record(text, {index % 5 == 0 ? LogSeverity::Warning : LogSeverity::Info, {}, {}});
        }
        if (kind == "wrapped") specimen->model.append_record("a multiline record\nwith a long explanatory line that wraps across a narrow viewport and preserves its identity");
        if (kind == "partial") specimen->model.append_bytes("partial UTF-8 record e\xcc");
    }
    if (kind == "filtered" || kind == "query-pending") specimen->filter.set_include("log 2");
    if (kind != "query-pending") while (specimen->filter.step()) {}
    view.refresh();
    if (kind == "find") specimen->search.set_query("log");
    if (kind == "wrapped") { view.set_wrap_mode(widgets::WrapMode::Word); specimen->set_bounds({0, 0, 28, 10}); }
    if (kind == "paused" || kind == "selection" || kind == "focused" || kind == "normal") view.set_top_record(0);
    if ((kind == "selection" || kind == "focused" || kind == "normal") && specimen->filter.size()) {
        const auto id = specimen->filter.at(0)->id; view.set_selection({{id, 0}, {id, 4}});
    }
    if (specimen->panel) {
        specimen->panel->set_source_status({.state = kind == "denied" ? LogSourceState::Denied : LogSourceState::Following,
            .watch = LogWatchMode::Native, .backlog_bytes = kind == "overload" ? 4096U : 0U,
            .dropped = kind == "overload" ? 250U : 0U, .native_error = kind == "denied" ? 5U : 0U,
            .uncertain = kind == "overload", .dropped_bytes = kind == "overload" ? 16384U : 0U,
            .dropped_batches = kind == "overload" ? 8U : 0U, .lag_nanos = kind == "overload" ? 500'000'000 : 0});
        specimen->panel->set_query_pending(kind == "query-pending");
        if (kind == "find") { specimen->panel->show_find(true); specimen->panel->find_box().set_query("log"); }
        specimen->panel->refresh();
        if (kind == "focused") stage.focus(specimen->panel->filter_box().field());
    } else if (kind == "focused") stage.focus(view);
}
}
void add_log_specimens(Catalog& catalog) {
    for (const auto composite : {false, true}) {
        auto& element = catalog.element(composite ? "LogPanel" : "LogView",
            composite ? "include/cvision/widgets/log_panel.hpp" : "include/cvision/widgets/log_view.hpp",
            Traits{.focusable = !composite, .text = true});
        for (const auto kind : {"normal", "focused", "wide", "narrow", "empty", "paused", "selection", "wrapped", "partial", "filtered", "query-pending", "overload", "denied", "find"})
            state(element, kind, {80, 18}, [composite, kind](Stage& stage) { build(stage, composite, kind); });
    }
}
} // namespace ckv::docgen::appearance
