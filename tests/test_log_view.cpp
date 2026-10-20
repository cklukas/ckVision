// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "log_view_test_fixture.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/core/text.hpp"
#include <fstream>
#include <iterator>
#include <limits>

using namespace ckv;
using namespace ckv::widgets;

namespace {
KeyEvent log_key(Key key, Modifier modifiers = Modifier::None) { return {{key, modifiers, ""}}; }
struct Fixture {
    LogModel model{testing::log_view_limits()};
    LogView view{model};
    Fixture() {
        model.begin_source(1, 1);
        view.set_bounds({0, 0, 10, 4});
    }
    void append(std::string_view value) { CK_CHECK(model.append_record(value)); view.refresh(); }
};
// A coherent filtered snapshot, with logarithmic ID lookup and borrowed data.
struct Filtered : LogRecordProvider {
    const LogModel& model;
    std::vector<std::size_t> indices;
    std::uint64_t query = 0;
    mutable std::size_t reads = 0;
    explicit Filtered(const LogModel& value) : model(value) {}
    std::size_t size() const noexcept override { return indices.size(); }
    std::optional<LogRecord> at(std::size_t index) const noexcept override {
        ++reads; return index < indices.size() ? model.at(indices[index]) : std::nullopt;
    }
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept override {
        std::size_t first = 0, last = size();
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            if (model.at(indices[middle])->id.sequence < id.sequence) first = middle + 1;
            else last = middle;
        }
        return first < size() && model.at(indices[first])->id == id ? std::optional{first} : std::nullopt;
    }
    std::uint64_t revision() const noexcept override { return model.revision() + query; }
    std::uint64_t latest_sequence() const noexcept override { return model.latest_sequence(); }
    std::uint64_t evicted_count() const noexcept override { return model.evicted_count(); }
    std::size_t max_text_bytes() const noexcept override { return model.max_text_bytes(); }
};
std::string first_row(LogView& view) {
    scene::Surface surface({view.bounds().width, view.bounds().height}, Cell::from_grapheme(" ", {}));
    scene::Painter painter(surface, {0, 0, view.bounds().width, view.bounds().height});
    view.draw(painter);
    std::string value;
    for (int x = 0; x < view.bounds().width - 1; ++x) value += surface.at({x, 0}).grapheme();
    return value;
}
}

CK_TEST(log_view_follows_tail_and_pauses_without_stopping_admission) {
    Fixture f;
    for (const auto value : {"a", "b", "c", "d"}) f.append(value);
    CK_CHECK(first_row(f.view) == "b        ");
    f.view.on_key(log_key(Key::Up));
    const auto inspected = f.view.top();
    f.append("e"); f.append("f");
    CK_CHECK(f.view.top() == inspected);
    CK_CHECK(!f.view.state().following && f.view.state().unread == 2);
    CK_CHECK(f.model.size() == 6);
    f.view.on_key(log_key(Key::End));
    CK_CHECK(f.view.state().following && f.view.state().unread == 0);
    CK_CHECK(first_row(f.view) == "d        ");
}
CK_TEST(log_view_preserves_partial_identity_and_counts_new_records_once) {
    Fixture f;
    f.view.pause();
    f.model.append_bytes("one"); f.view.refresh();
    CK_CHECK(f.view.state().unread == 1);
    const auto id = f.model.at(0)->id;
    f.model.append_bytes(" more\n"); f.view.refresh();
    CK_CHECK(f.view.state().unread == 1);
    CK_CHECK(f.model.at(0)->id == id);
    CK_CHECK(first_row(f.view) == "one more ");
}
CK_TEST(log_view_wrap_and_resize_preserve_surviving_byte_anchor) {
    Fixture f;
    f.append("abcdefghijklmno");
    f.view.set_wrap_mode(WrapMode::Character);
    f.view.set_top_record(0);
    f.view.scroll_rows(1);
    CK_CHECK(f.view.top()->byte == 9);
    f.view.set_bounds({0, 0, 6, 4});
    CK_CHECK(f.view.top()->id == f.model.at(0)->id);
    CK_CHECK(f.view.top()->byte == 5);
    CK_CHECK(first_row(f.view) == "fghij");
    f.view.scroll_rows(-1);
    CK_CHECK(f.view.top()->byte == 0);
}
CK_TEST(log_view_multiline_and_wide_clusters_have_accessible_rows) {
    Fixture f;
    f.append("a\n\n中x\n");
    f.view.set_top_record(0);
    CK_CHECK(first_row(f.view) == "a        ");
    f.view.scroll_rows(2);
    CK_CHECK(first_row(f.view).starts_with("中x"));
    f.view.scroll_rows(1);
    CK_CHECK(first_row(f.view) == "         ");
}
CK_TEST(log_view_selection_is_grapheme_safe_and_copy_reports_truncation) {
    Fixture f;
    f.append("a中éz"); f.append("second");
    const auto first = f.model.at(0), last = f.model.at(1);
    CK_CHECK(f.view.set_selection({{first->id, 2}, {last->id, 3}}));
    CK_CHECK(f.view.selection()->anchor.byte == 1);
    CK_CHECK(f.view.selected_text().text == "中éz\nsec");
    const auto short_copy = f.view.selected_text(4);
    CK_CHECK(short_copy.state == LogCopyState::Truncated && short_copy.text == "中");
    CK_CHECK(f.view.set_selection({{last->id, 3}, {first->id, 1}}));
    CK_CHECK(f.view.selected_text().text == "中éz\nsec");
}
CK_TEST(log_view_inactive_selection_remains_visible_in_classic_theme) {
    Fixture f; f.append("abc");
    ui::RoleRegistry registry;
    const auto roles = ui::intern_standard_roles(registry);
    const auto theme = ui::make_classic_theme(registry, roles);
    f.view.set_context({&theme, &registry, nullptr});
    const auto id = f.model.at(0)->id;
    CK_CHECK(f.view.set_selection({{id, 0}, {id, 1}}));
    scene::Surface surface({10, 4}, Cell::from_grapheme(" ", {}));
    scene::Painter painter(surface, {0, 0, 10, 4});
    f.view.draw(painter);
    CK_CHECK(surface.at({0, 0}).style() != surface.at({1, 0}).style());
    CK_CHECK(has_attr(surface.at({0, 0}).style().attrs, Attr::Reverse));
}
CK_TEST(log_view_shift_selection_and_mouse_drag_use_absolute_coordinates) {
    Fixture f;
    f.append("abc中def");
    f.view.set_bounds({5, 6, 10, 4});
    CK_CHECK(f.view.on_mouse({.action = MouseAction::Down, .button = MouseButton::Left, .cell = {6, 6}}));
    CK_CHECK(f.view.on_mouse({.action = MouseAction::Move, .button = MouseButton::Left, .cell = {9, 6}}));
    CK_CHECK(f.view.on_mouse({.action = MouseAction::Up, .button = MouseButton::Left, .cell = {9, 6}}));
    CK_CHECK(f.view.selected_text().text == "bc");
    CK_CHECK(f.view.on_key(log_key(Key::Right, Modifier::Shift)));
    CK_CHECK(f.view.selected_text().text == "bc中");
}
CK_TEST(log_view_eviction_is_visible_and_never_reuses_selection_identity) {
    LogModel model(testing::log_view_limits(2)); model.begin_source(1, 1);
    model.append_record("old"); model.append_record("kept");
    LogView view(model); view.set_bounds({0, 0, 10, 4}); view.set_top_record(0);
    const auto old = model.at(0)->id;
    view.set_selection({{old, 0}, {old, 3}});
    model.append_record("new"); view.refresh();
    CK_CHECK(view.state().anchor_unavailable && view.state().selection_unavailable);
    CK_CHECK(!view.selection() && view.top()->id == model.at(0)->id);
    CK_CHECK(view.state().evicted == 1);
    view.acknowledge_notices();
    CK_CHECK(!view.state().anchor_unavailable && !view.state().selection_unavailable);
}
CK_TEST(log_view_filter_revision_preserves_eligible_anchors_and_selection) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1);
    for (const auto value : {"one", "two", "three", "four"}) model.append_record(value);
    Filtered provider(model); provider.indices = {0, 1, 2, 3};
    LogView view(provider); view.set_bounds({0, 0, 10, 4}); view.set_top_record(1);
    const auto id = model.at(1)->id;
    view.set_selection({{id, 0}, {id, 3}});
    provider.indices = {1, 3}; ++provider.query; view.refresh();
    CK_CHECK(view.top()->id == id && view.selection()->anchor.id == id);
    CK_CHECK(view.state().unread == 0);
    provider.indices = {3}; ++provider.query; view.refresh();
    CK_CHECK(view.state().selection_unavailable && !view.selection());
    CK_CHECK(view.top()->id == model.at(3)->id && view.state().anchor_unavailable);
    provider.indices.clear(); ++provider.query; view.refresh();
    CK_CHECK(!view.top() && first_row(view) == "         ");
}
CK_TEST(log_view_clear_view_is_separate_from_retention_and_source_stop) {
    Fixture f; f.append("before");
    f.view.clear_view();
    CK_CHECK(f.model.size() == 1 && !f.view.top());
    f.append("after");
    CK_CHECK(first_row(f.view) == "after    ");
    f.view.reset_view(); f.view.set_top_record(0);
    CK_CHECK(first_row(f.view) == "before   ");
}
CK_TEST(log_view_layout_and_paint_work_is_bounded_by_viewport_not_history) {
    auto limits = testing::log_view_limits(16000); limits.retained_bytes = 16 * 1024 * 1024;
    LogModel model(limits); model.begin_source(1, 1);
    for (std::size_t i = 0; i < 10000; ++i) model.append_record("short");
    Filtered provider(model);
    for (std::size_t i = 0; i < model.size(); ++i) provider.indices.push_back(i);
    LogView view(provider); view.set_bounds({0, 0, 20, 8});
    provider.reads = 0;
    view.resume(); first_row(view);
    CK_CHECK(provider.reads < 100);
    view.set_top_record(5000); provider.reads = 0;
    first_row(view);
    CK_CHECK(provider.reads <= 7);
}
CK_TEST(log_view_capacity_and_zero_size_are_explicit) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1);
    model.append_record("entry");
    LogView view(model, 2); view.set_bounds({0, 0, 10, 5});
    CK_CHECK(view.state().viewport_exceeded);
    view.set_bounds({0, 0, 0, 0});
    CK_CHECK(!view.state().viewport_exceeded);
    view.set_bounds({0, 0, 10, 3});
    CK_CHECK(first_row(view) == "entry    ");
}
CK_TEST(log_panel_query_commands_and_failure_status_are_composable) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_record("retained");
    auto panel = std::make_unique<LogPanel>(model);
    panel->set_bounds({0, 0, 30, 8});
    std::string query;
    panel->on_query_changed = [&](const std::string& value) { query = value; };
    panel->filter_box().set_query("needle"); CK_CHECK(query == "needle");
    int stops = 0;
    panel->on_stop = [&] { ++stops; };
    panel->on_clear_retention = [&] { model.clear(); };
    CK_CHECK(panel->invoke(LogPanelAction::Pause)); CK_CHECK(stops == 0 && model.size() == 1);
    CK_CHECK(panel->invoke(LogPanelAction::Stop)); CK_CHECK(stops == 1);
    CK_CHECK(panel->invoke(LogPanelAction::ClearRetention)); CK_CHECK(model.size() == 0);
    panel->set_source_status({LogSourceState::Denied, LogWatchMode::Polling, 100, 4, 13, true});
    panel->set_query_pending(true);
    CK_CHECK(panel->status_text().starts_with("Access denied"));
    CK_CHECK(panel->status_text().find("backlog 100 B") != std::string_view::npos);
    auto commands = panel->commands("app.logs", "log-monitor");
    CK_CHECK(commands.size() == 7 && commands[1].scope.contexts.front() == "log-monitor");
    commands[1].handler(); CK_CHECK(panel->view().state().following);
    panel.reset();
    for (auto& command : commands) command.handler();
    CK_CHECK(stops == 1);
}
CK_TEST(log_view_and_panel_frames_match_sixteen_theme_and_interaction_goldens) {
    for (const auto name : testing::log_golden_names) {
        const auto path = std::string(CKV_TEST_SOURCE_DIR) + "/golden/log_" + std::string(name) + ".dump";
        std::ifstream input(path, std::ios::binary);
        CK_CHECK(input.good());
        const std::string expected{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        CK_CHECK(testing::log_view_golden(name) == expected);
    }
}
CK_TEST(log_wrap_streaming_step_matches_grapheme_wrapper_for_all_modes) {
    for (const auto value : {"", "a b c", "longidentifier and words", "中 é 🙂 test", "  leading spaces"}) {
        std::vector<std::string> pieces;
        for (const auto cluster : text::split_graphemes(value)) pieces.emplace_back(cluster);
        for (const auto mode : {WrapMode::None, WrapMode::Word, WrapMode::Character}) for (int width = 1; width < 12; ++width) {
            const auto by_text = wrap_text(value, {width, mode, 1});
            const auto by_cluster = wrap_graphemes(pieces, {width, mode, 1});
            CK_CHECK(by_text.size() == by_cluster.size());
            for (std::size_t row = 0; row < by_text.size(); ++row) {
                std::string joined;
                for (auto i = by_cluster[row].begin; i < by_cluster[row].end; ++i) joined += pieces[i];
                CK_CHECK(joined == std::string_view(value).substr(by_text[row].begin, by_text[row].end - by_text[row].begin));
            }
        }
    }
}

CK_TEST(log_scrollbar_large_record_ranges_do_not_overflow_proportional_math) {
    ui::RoleRegistry registry;
    const auto roles = ui::intern_standard_roles(registry);
    const auto theme = ui::make_classic_theme(registry, roles);
    Scrollbar scrollbar(Orientation::Vertical);
    scrollbar.set_context({&theme, &registry, nullptr});
    scrollbar.set_bounds({0, 0, 1, 4096});
    scrollbar.set_range(1000000000, 4094);
    scrollbar.set_position(999000000);
    CK_CHECK(scrollbar.thumb_length() >= 1 && scrollbar.thumb_length() <= 4094);
    scene::Surface surface({1, 4096}, Cell::from_grapheme(" ", {}));
    scene::Painter painter(surface, {0, 0, 1, 4096});
    scrollbar.draw(painter);
}
