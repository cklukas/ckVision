// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "log_query_view_test_fixture.hpp"
#include "cvision/testing/cktest.hpp"
#include <fstream>
#include <iterator>

using namespace ckv;
using namespace ckv::widgets;
namespace {
void complete(LogFilter& filter) { std::size_t count = 0; while (filter.step()) CK_CHECK(++count < 10000); }
void complete(LogSearch& search) { std::size_t count = 0; while (search.step()) CK_CHECK(++count < 10000); }
}
CK_TEST(log_panel_filter_field_binds_the_shipped_engine_and_tags_old_results) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_bytes("one\nneedle\nthree\n");
    LogFilter filter(model, {1, 1}); LogPanel panel(filter); panel.set_bounds({0, 0, 40, 8});
    panel.filter_box().set_query("needle"); panel.refresh();
    CK_CHECK(filter.status().query_pending && panel.status_text().find("query 1 rebuilding 2") != std::string_view::npos);
    complete(filter); panel.refresh();
    CK_CHECK(filter.size() == 1 && panel.status_text().find("retained 3 matches 1") != std::string_view::npos);
    panel.filter_box().set_query(std::string(1025, 'x')); panel.refresh();
    CK_CHECK(panel.status_text().find("query too long") != std::string_view::npos && filter.size() == 1);
}
CK_TEST(log_panel_find_is_separate_from_filter_and_standard_close_remains_available) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_bytes("keep one\nother\nkeep two\n");
    LogFilter filter(model); LogSearch search(filter, {1, 2}); LogPanel panel(filter); panel.set_search(&search);
    panel.set_bounds({0, 0, 40, 9}); panel.filter_box().set_query("keep"); complete(filter); panel.refresh();
    CK_CHECK(panel.on_key({{Key::Char, Modifier::Ctrl, "f"}})); panel.find_box().set_query("two");
    CK_CHECK(panel.on_key({{Key::F3, Modifier::None, ""}}));
    complete(search); panel.refresh();
    CK_CHECK(filter.size() == 2 && panel.view().selected_text().text == "two");
    CK_CHECK(!panel.view().state().following);
    CK_CHECK(!panel.on_key({{Key::F3, Modifier::Alt, ""}}));
    const auto commands = panel.commands("app.log", "log"); CK_CHECK(commands.size() == 10);
    panel.find_box().clear(); CK_CHECK(!panel.find_box().visible() && search.query().empty() && filter.size() == 2);
}
CK_TEST(log_view_search_reveal_expands_partial_cluster_matches_and_scrolls_horizontally) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_record("longprefix é suffix");
    LogView view(model); view.set_bounds({0, 0, 6, 3});
    const auto id = model.at(0)->id;
    CK_CHECK(view.reveal(id, {11, 12}));
    CK_CHECK(view.selected_text().text == "é" && view.left_column() == 11);
    CK_CHECK(!view.reveal(id, {50, 51}));
}
CK_TEST(log_view_highlights_whole_touched_graphemes_and_explicit_newline_cells) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_record("é\nnext");
    LogSearch search(model); search.set_query("e");
    LogView view(model); view.set_bounds({0, 0, 10, 4}); view.set_highlighter(&search);
    scene::Surface surface({10, 4}, Cell::from_grapheme(" ", {})); scene::Painter painter(surface, {0, 0, 10, 4});
    view.draw(painter);
    CK_CHECK(surface.at({0, 0}).grapheme() == "é" && has_attr(surface.at({0, 0}).style().attrs, Attr::Underline));
    search.set_query("\n"); view.draw(painter);
    CK_CHECK(has_attr(surface.at({1, 0}).style().attrs, Attr::Underline));
}
CK_TEST(log_filter_publication_preserves_surviving_view_selection) {
    LogModel model(testing::log_view_limits()); model.begin_source(1, 1); model.append_bytes("keep one\nhide\nkeep two\n");
    LogFilter filter(model); LogView view(filter); view.set_bounds({0, 0, 12, 4}); view.set_top_record(0);
    const auto id = model.at(0)->id; view.set_selection({{id, 0}, {id, 4}});
    filter.set_include("keep"); complete(filter); view.refresh();
    CK_CHECK(view.selection()->anchor.id == id && view.selected_text().text == "keep");
    model.append_record("keep latest"); complete(filter); view.refresh();
    CK_CHECK(view.selection()->anchor.id == id && view.top()->id == id);
}
CK_TEST(log_query_view_seven_frames_match_pending_search_error_and_narrow_goldens) {
    for (const auto name : testing::log_query_golden_names) {
        std::ifstream input(std::string(CKV_TEST_SOURCE_DIR) + "/golden/log_" + std::string(name) + ".dump", std::ios::binary);
        CK_CHECK(input.good());
        const std::string expected{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        CK_CHECK(testing::log_query_view_golden(name) == expected);
    }
}
