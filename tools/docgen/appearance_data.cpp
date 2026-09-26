// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Appearance specimens: data — the views that show a collection or a
// document: lists, trees, tables, cell grids, read-only text, and the editor.
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "appearance_matrix.hpp"
#include "cvision/widgets/cell_grid.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "cvision/widgets/editor_search.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/tree_view.hpp"

namespace ckv::docgen::appearance {

namespace {

// Every data specimen is the content of one document window of this size,
// on a screen with room for its shadow.
constexpr Size kScreen{30, 9};
constexpr Rect kWindow{1, 1, 27, 7};
// The same window, too narrow for the specimen's text.
constexpr Rect kNarrowWindow{1, 1, 10, 7};

// Shows `view` as the content of a document window and returns it.
template <class T>
T& shown(Stage& stage, std::string title, std::unique_ptr<T> view, Rect bounds = kWindow) {
    T& placed = *view;
    stage.window(bounds, std::move(title), std::move(view), false);
    return placed;
}

MouseEvent wheel_down(Point cell) {
    return MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, cell, std::nullopt};
}

// --- ListView -------------------------------------------------------------

std::vector<std::string> documents() {
    return {"boot-sequence.md", "capabilities.md", "dialogs.md", "editor.md", "fuzzing.md",
            "graphics.md",      "input.md",        "layout.md",  "themes.md", "widgets.md"};
}

widgets::ListView& list(Stage& stage, std::vector<std::string> items = documents(), bool multi_select = false,
                        Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::ListView>(multi_select);
    view->set_items(std::move(items));
    widgets::ListView& placed = shown(stage, "Documents", std::move(view), bounds);
    placed.set_cursor(1);
    return placed;
}

void add_list_view(Catalog& catalog) {
    Element& e = catalog.element("ListView", "include/cvision/widgets/list_view.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { list(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(list(s)); });
    state(e, "disabled", kScreen, [](Stage& s) { list(s).set_enabled(false); });
    state(e, "multi-select", kScreen, [](Stage& s) {
        widgets::ListView& view = list(s, documents(), true);
        view.set_selected(3, true);
        view.set_selected(4, true);
        s.focus(view);
    });
    state(e, "scrolled", kScreen, [](Stage& s) {
        widgets::ListView& view = list(s);
        view.set_cursor(8);
        s.focus(view);
    });
    state(e, "empty", kScreen, [](Stage& s) { s.focus(list(s, {})); });
    state(e, "wide", kScreen, [](Stage& s) {
        s.focus(list(s, {"notes.md", std::string(kWideText), "readme.md"}));
    });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(list(s, documents(), false, kNarrowWindow)); });
}

// --- TreeView -------------------------------------------------------------

constexpr std::uint64_t kIncludeNode = 2;
constexpr std::uint64_t kCoreNode = 3;

widgets::TreeNode node(std::string label, std::uint64_t id, std::vector<widgets::TreeNode> children = {},
                       bool expanded = false) {
    widgets::TreeNode made;
    made.label = std::move(label);
    made.id = id;
    made.children = std::move(children);
    made.expanded = expanded;
    return made;
}

// A project tree: a collapsed folder, an expanded one, and a file.
std::vector<widgets::TreeNode> project(std::string file = "README.md") {
    return {node("docs", 1, {node("guide.md", 5)}),
            node("include", kIncludeNode, {node("core", kCoreNode), node("widgets", 4)}, true),
            node(std::move(file), 6)};
}

widgets::TreeView& tree(Stage& stage, std::vector<widgets::TreeNode> roots = project(),
                        widgets::TreeConnectorStyle style = widgets::TreeConnectorStyle::Minimal,
                        Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::TreeView>();
    view->set_roots(std::move(roots));
    view->set_connector_style(style);
    widgets::TreeView& placed = shown(stage, "Project", std::move(view), bounds);
    placed.reveal_and_select(kCoreNode);
    return placed;
}

void add_tree_view(Catalog& catalog) {
    Element& e = catalog.element("TreeView", "include/cvision/widgets/tree_view.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { tree(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(tree(s)); });
    state(e, "disabled", kScreen, [](Stage& s) { tree(s).set_enabled(false); });
    state(e, "collapsed", kScreen, [](Stage& s) {
        widgets::TreeView& view = tree(s);
        s.focus(view);
        s.app().dispatch(KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});
        s.app().dispatch(KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});
    });
    state(e, "outline", kScreen, [](Stage& s) {
        s.focus(tree(s, project(), widgets::TreeConnectorStyle::Outline));
    });
    state(e, "box-drawing", kScreen, [](Stage& s) {
        s.focus(tree(s, project(), widgets::TreeConnectorStyle::BoxDrawing));
    });
    state(e, "wide", kScreen, [](Stage& s) { s.focus(tree(s, project(std::string(kWideText)))); });
    state(e, "narrow", kScreen, [](Stage& s) {
        s.focus(tree(s, project("CHANGELOG-2026.md"), widgets::TreeConnectorStyle::Minimal, kNarrowWindow));
    });
}

// --- Table ----------------------------------------------------------------

std::vector<std::vector<std::string>> suites() {
    return {{"layout", "148", "core"}, {"editor", "96", "text"}, {"frame_svg", "12", "docs"},
            {"table", "54", "data"}};
}

widgets::Table& table(Stage& stage, std::vector<std::vector<std::string>> rows = suites(), Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::Table>();
    view->set_columns({
        widgets::TableColumn{"Suite", 9, 5, widgets::TableCellType::Text, false},
        widgets::TableColumn{"Cases", 7, 4, widgets::TableCellType::Integer, false},
        widgets::TableColumn{"Owner", 6, 4, widgets::TableCellType::Text, true},
    });
    view->set_rows(std::move(rows));
    view->set_selected_cell(widgets::TableCellRef{2, 0});
    return shown(stage, "Tests", std::move(view), bounds);
}

void add_table(Catalog& catalog) {
    Element& e = catalog.element("Table", "include/cvision/widgets/table.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { table(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(table(s)); });
    state(e, "disabled", kScreen, [](Stage& s) { table(s).set_enabled(false); });
    state(e, "sorted", kScreen, [](Stage& s) {
        widgets::Table& view = table(s);
        view.sort_by(0, false);
        s.focus(view);
    });
    state(e, "editing", kScreen, [](Stage& s) {
        widgets::Table& view = table(s);
        view.set_selected_cell(widgets::TableCellRef{2, 2});
        s.focus(view);
        view.begin_edit();
        // The reader replaces the owner "text" with "ui" in place.
        for (int i = 0; i < 4; ++i) s.app().dispatch(KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}});
        s.app().dispatch(TextEvent{"ui"});
    });
    state(e, "wide", kScreen, [](Stage& s) {
        std::vector<std::vector<std::string>> rows = suites();
        rows[1][0] = std::string(kWideText);
        s.focus(table(s, std::move(rows)));
    });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(table(s, suites(), kNarrowWindow)); });
}

// --- CellGrid -------------------------------------------------------------

// The provider a Sheet owns. A base declared ahead of the grid is built
// before the grid borrows it and destroyed after the grid is gone, which is
// the lifetime CellGrid::set_model() asks of its caller.
struct SheetProvider {
    widgets::MaterializedCellGridModel provider;
};

// A cell grid that owns its provider, so that a specimen can hand the whole
// of it to a window.
class Sheet final : private SheetProvider, public widgets::CellGrid {
public:
    explicit Sheet(std::vector<std::vector<std::string>> rows) {
        provider.set_cells(std::move(rows));
        provider.set_column_widths({7, 5, 5, 6});
        for (widgets::GridIndex row = 1; row < provider.row_count(); ++row)
            for (widgets::GridIndex column = 1; column < provider.column_count(); ++column) {
                widgets::GridCell figure = provider.cell_at({row, column});
                figure.alignment = widgets::CellAlignment::End;
                provider.set_cell({row, column}, figure);
            }
        set_model(provider);
    }

    widgets::MaterializedCellGridModel& sheet() noexcept { return provider; }
};

std::vector<std::vector<std::string>> quarters() {
    return {{"Region", "Q1", "Q2", "Total"},  {"North", "1240", "1310", "2550"},
            {"South", "980", "1105", "2085"}, {"East", "760", "840", "1600"},
            {"West", "1530", "1490", "3020"}, {"Sum", "4510", "4745", "9255"}};
}

// The cursor is placed once the grid is laid out: the provider scrolls to
// keep it in view, and before layout its viewport is a single cell.
Sheet& grid(Stage& stage, std::vector<std::vector<std::string>> rows = quarters(), Rect bounds = kWindow) {
    Sheet& placed = shown(stage, "Quarterly", std::make_unique<Sheet>(std::move(rows)), bounds);
    placed.sheet().place_cursor({1, 1}, false);
    placed.model_changed();
    return placed;
}

void add_cell_grid(Catalog& catalog) {
    Element& e = catalog.element("CellGrid", "include/cvision/widgets/cell_grid.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { grid(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(grid(s)); });
    state(e, "selection", kScreen, [](Stage& s) {
        Sheet& view = grid(s);
        view.sheet().navigate(widgets::GridMove::Right, true);
        view.sheet().navigate(widgets::GridMove::Down, true);
        view.model_changed();
        s.focus(view);
    });
    state(e, "scrolled", kScreen, [](Stage& s) {
        Sheet& view = grid(s);
        view.sheet().set_frozen(1, 1);
        s.step();
        view.sheet().navigate(widgets::GridMove::GridEnd, false);
        view.model_changed();
        s.focus(view);
    });
    state(e, "cell-colours", kScreen, [](Stage& s) {
        Sheet& view = grid(s);
        widgets::MaterializedCellGridModel& sheet = view.sheet();
        for (widgets::GridIndex column = 0; column < sheet.column_count(); ++column) {
            widgets::GridCell title = sheet.cell_at({0, column});
            title.style.attributes = Attr::Bold;
            sheet.set_cell({0, column}, title);
        }
        widgets::GridCell alert = sheet.cell_at({2, 1});
        alert.style.foreground = Color::rgb(255, 255, 255);
        alert.style.background = Color::rgb(170, 0, 0);
        sheet.set_cell({2, 1}, alert);
        sheet.place_cursor({2, 1}, false);
        view.model_changed();
        s.focus(view);
    });
    state(e, "merged", kScreen, [](Stage& s) {
        std::vector<std::vector<std::string>> rows = quarters();
        rows[0] = {"Revenue by region", "", "", ""};
        Sheet& view = grid(s, std::move(rows));
        view.sheet().set_spans({widgets::GridRange{{0, 0}, {0, 3}}});
        view.model_changed();
        s.focus(view);
    });
    state(e, "wide", kScreen, [](Stage& s) {
        std::vector<std::vector<std::string>> rows = quarters();
        rows[1][0] = std::string(kWideText);
        s.focus(grid(s, std::move(rows)));
    });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(grid(s, quarters(), kNarrowWindow)); });
}

// --- TextView -------------------------------------------------------------

std::vector<widgets::TextSpan> notes(std::string lead = "TextView shows text the reader cannot edit.") {
    return {widgets::TextSpan{std::move(lead) + "\n", Attr{}, std::nullopt},
            widgets::TextSpan{"See the ", Attr{}, std::nullopt},
            widgets::TextSpan{"guide", Attr{}, std::string("guide.md")},
            widgets::TextSpan{" or the ", Attr{}, std::nullopt},
            widgets::TextSpan{"index", Attr{}, std::string("index.md")},
            widgets::TextSpan{".", Attr{}, std::nullopt}};
}

widgets::TextView& text_view(Stage& stage, std::vector<widgets::TextSpan> spans = notes(),
                             widgets::WrapMode wrap = widgets::WrapMode::Word, Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::TextView>();
    view->set_wrap_mode(wrap);
    view->set_spans(std::move(spans));
    return shown(stage, "Notes", std::move(view), bounds);
}

void add_text_view(Catalog& catalog) {
    Element& e = catalog.element("TextView", "include/cvision/widgets/text_view.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { text_view(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(text_view(s)); });
    state(e, "current-link", kScreen, [](Stage& s) {
        widgets::TextView& view = text_view(s);
        view.set_current_link(1);
        s.focus(view);
    });
    state(e, "styled", kScreen, [](Stage& s) {
        s.focus(text_view(s, {widgets::TextSpan{"Bold", Attr::Bold, std::nullopt},
                              widgets::TextSpan{", ", Attr{}, std::nullopt},
                              widgets::TextSpan{"italic", Attr::Italic, std::nullopt},
                              widgets::TextSpan{" and ", Attr{}, std::nullopt},
                              widgets::TextSpan{"underlined", Attr::Underline, std::nullopt},
                              widgets::TextSpan{" runs.", Attr{}, std::nullopt}}));
    });
    state(e, "scrolled", kScreen, [](Stage& s) {
        std::vector<widgets::TextSpan> log;
        for (int line = 1; line <= 12; ++line)
            log.push_back(widgets::TextSpan{"build step " + std::to_string(line) + " passed\n", Attr{},
                                            std::nullopt});
        widgets::TextView& view = text_view(s, std::move(log));
        s.step();
        const Rect at = view.absolute_bounds();
        s.app().dispatch(wheel_down(Point{at.x + 2, at.y + 1}));
    });
    state(e, "wide", kScreen, [](Stage& s) { s.focus(text_view(s, notes(std::string(kWideText)))); });
    state(e, "narrow", kScreen, [](Stage& s) {
        s.focus(text_view(s, notes(), widgets::WrapMode::None, kNarrowWindow));
    });
}

// --- TextEditor -----------------------------------------------------------

constexpr std::string_view kPackage =
    "{\n"
    "  \"name\": \"ckvision\",\n"
    "  \"version\": \"0.4.0\",\n"
    "  \"headless\": true\n"
    "}\n";

// An editor on its own document. Without a registry of its own the editor
// registers the standard syntax profiles, and the file name picks one.
widgets::TextEditor& editor(Stage& stage, std::string text = std::string(kPackage),
                            std::string file = "package.json", Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::TextEditor>(std::make_shared<widgets::EditorDocument>(std::move(text)));
    view->set_file_name(file);
    view->set_show_line_numbers(true);
    return shown(stage, std::move(file), std::move(view), bounds);
}

void add_text_editor(Catalog& catalog) {
    Element& e = catalog.element("TextEditor", "include/cvision/widgets/text_editor.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { editor(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(editor(s)); });
    state(e, "plain", kScreen, [](Stage& s) {
        widgets::TextEditor& view = editor(s, "Plain text has no syntax\ncolouring and no gutter.\n", "notes.txt");
        view.set_show_line_numbers(false);
        s.focus(view);
    });
    state(e, "selection", kScreen, [](Stage& s) {
        widgets::TextEditor& view = editor(s);
        const widgets::EditorDocument& document = *view.document();
        view.set_selection(widgets::DocumentRange{*document.position_at_line_column(1, 2),
                                                  *document.position_at_line_column(2, 12)});
        s.focus(view);
    });
    state(e, "search", kScreen, [](Stage& s) {
        widgets::TextEditor& view = editor(s);
        view.set_search_query(widgets::EditorSearchQuery{"e", true, false});
        s.focus(view);
    });
    state(e, "wrapped", kScreen, [](Stage& s) {
        widgets::TextEditor& view =
            editor(s, "A line too long for the window reflows onto display rows of its own.\n", "notes.txt");
        view.set_wrap_mode(widgets::WrapMode::Word);
        s.focus(view);
    });
    state(e, "wide", kScreen, [](Stage& s) {
        s.focus(editor(s, "{\n  \"title\": \"" + std::string(kWideText) + "\"\n}\n"));
    });
    state(e, "narrow", kScreen, [](Stage& s) {
        s.focus(editor(s, std::string(kPackage), "package.json", kNarrowWindow));
    });
}

}  // namespace

void add_data_specimens(Catalog& catalog) {
    add_list_view(catalog);
    add_tree_view(catalog);
    add_table(catalog);
    add_cell_grid(catalog);
    add_text_view(catalog);
    add_text_editor(catalog);
}

}  // namespace ckv::docgen::appearance
