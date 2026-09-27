// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/dock.hpp"
#include "cvision/ui/grid.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/overlay.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/window.hpp"
#include "layouts_app.hpp"
#include "presented_frame.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::View;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::layouts::LayoutsApp layouts{app};
};

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// --- The M10 container-suite script ------------------------------------------
//
// tools/docgen/generate_layouts_goldens.cpp runs this same script to write the
// pinned dumps. Input enters only through the HeadlessTerminal: resizes to a
// wide 100x30 and a narrow 64x24, then back at 80x24 F5 zooms the window and
// Right twice moves the (focused) Splitter. The terminal is TrueColor so the
// presented display can be compared with the composed frame exactly.

enum class Stage { Initial, Wide, Narrow, Keyboard };

const char* stage_name(Stage stage) {
    switch (stage) {
        case Stage::Initial: return "initial";
        case Stage::Wide: return "wide";
        case Stage::Narrow: return "narrow";
        case Stage::Keyboard: return "keyboard";
    }
    return "";
}

// How far the window's content pane has grown from its 80x24 size (70x17) at
// each stage: the window keeps its distance to the desktop's right and bottom
// edges (DesktopGrowPolicy::AnchorEdges), and F5 zooms it to the content area.
ckv::Point pane_growth(Stage stage) {
    switch (stage) {
        case Stage::Initial: return {0, 0};
        case Stage::Wide: return {20, 6};
        case Stage::Narrow: return {-16, 0};
        case Stage::Keyboard: return {8, 3};
    }
    return {0, 0};
}

struct LayoutsScript {
    ckv::term::HeadlessTerminal term{Size{80, 24}, ckv::term::headless_no_graphics_profile()};
    ManualClock clock;
    Application app{term, clock};
    ckv::layouts::LayoutsApp layouts{app};

    void step() {
        app.step(0);
        CK_CHECK(cktest_support::presented_equals_composed(term, app));
    }
    void resize(Size size) {
        term.resize(size);
        step();
    }
    void press(Key key) {
        term.inject_event(ckv::KeyEvent{KeyChord{key, Modifier::None, ""}});
        step();
    }

    // Runs the whole script, handing each stage to `check` once it is on screen.
    void run(const std::function<void(Stage)>& check) {
        step();
        check(Stage::Initial);
        resize(Size{100, 30});
        check(Stage::Wide);
        resize(Size{64, 24});
        check(Stage::Narrow);
        resize(Size{80, 24});
        press(Key::F5);
        press(Key::Right);
        press(Key::Right);
        check(Stage::Keyboard);
    }

    // The composed frame's cells inside `view`'s absolute bounds, as a dump.
    std::string capture_region(const View& view) const {
        const Rect area = view.absolute_bounds();
        const ckv::scene::Surface& frame = app.composed_surface();
        ckv::scene::Surface region(Size{area.width, area.height});
        for (int y = 0; y < area.height; ++y)
            for (int x = 0; x < area.width; ++x)
                region.set_cell(ckv::Point{x, y}, frame.at(ckv::Point{area.x + x, area.y + y}),
                                frame.link_target(ckv::Point{area.x + x, area.y + y}));
        return ckv::golden::serialize(ckv::scene::capture(region));
    }

    // Compares `view`'s region with the family's pinned dump at the three
    // terminal sizes; the keyboard stage is pinned as a whole frame instead.
    void check_family_golden(const char* family, const View& view, Stage stage) const {
        if (stage == Stage::Keyboard) return;
        CK_CHECK(capture_region(view) == read_file(std::string("golden/layouts_") + family + "_" +
                                                   stage_name(stage) + ".dump"));
    }
};

// A family container's children in insertion order.
const View& child(const View& parent, std::size_t index) { return *parent.children().at(index); }
}  // namespace

CK_TEST(layouts_about_dialog_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find(
                 "Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

CK_TEST(layouts_example_renders_each_layout_family) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("Layouts") != std::string::npos);
    CK_CHECK(bytes.find("Row") != std::string::npos);
    CK_CHECK(bytes.find("Column") != std::string::npos);
    CK_CHECK(bytes.find("Grid") != std::string::npos);
    CK_CHECK(bytes.find("Dock") != std::string::npos);
    CK_CHECK(bytes.find("Overlay") != std::string::npos);
    CK_CHECK(bytes.find("Anchored") != std::string::npos);
}

CK_TEST(layouts_splitter_is_keyboard_adjustable_through_the_example_graph) {
    Fixture f;
    f.app.step(0);
    const int before = f.layouts.splitter()->split_position();
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    CK_CHECK(f.layouts.splitter()->split_position() == before + 1);
}

CK_TEST(layouts_example_reflows_docked_chrome_and_anchored_content_after_terminal_resize) {
    Fixture f;
    f.app.step(0);
    const ckv::Rect anchored_before = f.layouts.anchored_label()->bounds();

    f.term.resize(ckv::Size{100, 30});
    CK_CHECK(f.app.step(0));

    CK_CHECK(f.layouts.desktop().top_dock()->bounds() == (ckv::Rect{0, 0, 100, 1}));
    CK_CHECK(f.layouts.desktop().bottom_dock()->bounds() == (ckv::Rect{0, 29, 100, 1}));
    CK_CHECK(f.layouts.anchored_label()->bounds().x > anchored_before.x);
}

CK_TEST(alt_x_quits_from_the_layouts_example) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(!f.app.quit_requested());
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(f.app.quit_requested());
}

CK_TEST(layouts_anchor_pane_keeps_each_container_on_its_anchored_edges_through_resizes_and_keyboard_zoom) {
    LayoutsScript script;
    const ckv::layouts::LayoutsApp& layouts = script.layouts;
    script.run([&](Stage stage) {
        const ckv::Point grow = pane_growth(stage);
        const int dw = grow.x;
        const int dh = grow.y;
        CK_CHECK(layouts.window()->zoomed() == (stage == Stage::Keyboard));
        CK_CHECK(layouts.window()->content_rect().width == 70 + dw);
        CK_CHECK(layouts.window()->content_rect().height == 17 + dh);
        // Left+right anchored containers stretch; right-only ones move; a
        // bottom anchor moves (or, with top, stretches) by the height change.
        CK_CHECK(layouts.row()->bounds() == (Rect{1, 1, 30 + dw, 3}));
        CK_CHECK(layouts.column()->bounds() == (Rect{33 + dw, 1, 18, 7}));
        CK_CHECK(layouts.grid()->bounds() == (Rect{1, 5, 30 + dw, 5}));
        CK_CHECK(layouts.dock()->bounds() == (Rect{1, 11 + dh, 30 + dw, 5}));
        CK_CHECK(layouts.overlay()->bounds() == (Rect{33 + dw, 9 + dh, 18, 5}));
        CK_CHECK(layouts.splitter()->bounds() == (Rect{52 + dw, 1, 17, 13 + dh}));
        CK_CHECK(layouts.anchored_label()->bounds() == (Rect{58 + dw, 15 + dh, 8, 1}));
        script.check_family_golden("anchor", *layouts.window(), stage);
    });
}

CK_TEST(layouts_keyboard_zoom_and_splitter_keys_reach_the_example_through_the_terminal) {
    LayoutsScript script;
    int split_before_keys = 0;
    script.run([&](Stage stage) {
        if (stage == Stage::Narrow) split_before_keys = script.layouts.splitter()->split_position();
    });
    // F5 zoomed the active window to the desktop's content area, and Right
    // twice moved the focused Splitter's divider two cells.
    CK_CHECK(script.layouts.window()->bounds() == script.layouts.desktop().content_area());
    CK_CHECK(script.layouts.splitter()->split_position() == split_before_keys + 2);
    const std::string frame =
        ckv::golden::serialize(ckv::scene::capture(script.app.composed_surface(), script.app.current_cursor()));
    CK_CHECK(frame == read_file("golden/layouts_keyboard.dump"));
}

CK_TEST(layouts_row_and_column_apply_per_child_cross_axis_alignment_and_margins_through_resizes) {
    LayoutsScript script;
    const ckv::layouts::LayoutsApp& layouts = script.layouts;
    script.run([&](Stage stage) {
        const int row_width = 30 + pane_growth(stage).x;
        // Row (3 rows tall): the Fixed label is centred on the cross axis;
        // the Expanding text keeps its one-row margin above and takes the
        // rest of the width after the one-cell spacing.
        CK_CHECK(child(*layouts.row(), 0).bounds() == (Rect{0, 1, 3, 1}));
        CK_CHECK(child(*layouts.row(), 1).bounds() == (Rect{4, 1, row_width - 4, 2}));
        // Column (18 wide): the label is end-aligned on the cross axis, the
        // paragraph keeps a one-column margin either side and fills the
        // height left after the label and the spacing.
        CK_CHECK(child(*layouts.column(), 0).bounds() == (Rect{12, 0, 6, 1}));
        CK_CHECK(child(*layouts.column(), 1).bounds() == (Rect{1, 2, 16, 5}));
        script.check_family_golden("row", *layouts.row(), stage);
        script.check_family_golden("column", *layouts.column(), stage);
    });
}

CK_TEST(layouts_grid_divides_its_width_evenly_and_places_spans_and_centred_cells_through_resizes) {
    LayoutsScript script;
    const ckv::layouts::LayoutsApp& layouts = script.layouts;
    script.run([&](Stage stage) {
        // 2 rows x 3 columns with one cell of spacing: the rows are 2 tall
        // (row 1 starts at 3); the columns share width - 2, any remainder to
        // the first columns.
        struct Columns {
            int width;
            int first_two;    // columns 0 and 1 with the gap between them
            int third_start;  // column 2's offset
            int third_width;  // column 2's extent
        };
        const Columns columns = stage == Stage::Initial ? Columns{30, 20, 21, 9}
                                : stage == Stage::Wide  ? Columns{50, 33, 34, 16}
                                : stage == Stage::Narrow ? Columns{14, 9, 10, 4}
                                                         : Columns{38, 25, 26, 12};
        CK_CHECK(layouts.grid()->bounds().width == columns.width);
        // "Grid" spans two columns and fills them; "A" is centred in column
        // 2; "span" is centred across all three columns of row 1.
        CK_CHECK(child(*layouts.grid(), 0).bounds() == (Rect{0, 0, columns.first_two, 2}));
        CK_CHECK(child(*layouts.grid(), 1).bounds() ==
                 (Rect{columns.third_start + (columns.third_width - 1) / 2, 0, 1, 2}));
        CK_CHECK(child(*layouts.grid(), 2).bounds() == (Rect{(columns.width - 4) / 2, 3, 4, 2}));
        script.check_family_golden("grid", *layouts.grid(), stage);
    });
}

CK_TEST(layouts_dock_carves_top_then_left_and_fills_the_centre_through_resizes) {
    LayoutsScript script;
    const ckv::layouts::LayoutsApp& layouts = script.layouts;
    script.run([&](Stage stage) {
        const int width = 30 + pane_growth(stage).x;
        CK_CHECK(child(*layouts.dock(), 0).bounds() == (Rect{0, 0, width, 1}));
        CK_CHECK(child(*layouts.dock(), 1).bounds() == (Rect{0, 1, 4, 4}));
        CK_CHECK(child(*layouts.dock(), 2).bounds() == (Rect{4, 1, width - 4, 4}));
        script.check_family_golden("dock", *layouts.dock(), stage);
    });
}

CK_TEST(layouts_overlay_keeps_its_fill_base_and_manual_badge_as_it_moves_through_resizes) {
    LayoutsScript script;
    const ckv::layouts::LayoutsApp& layouts = script.layouts;
    script.run([&](Stage stage) {
        CK_CHECK(child(*layouts.overlay(), 0).bounds() == (Rect{0, 0, 18, 5}));
        CK_CHECK(child(*layouts.overlay(), 1).bounds() == (Rect{11, 1, 5, 1}));
        script.check_family_golden("overlay", *layouts.overlay(), stage);
    });
}
