// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Unified window-frame scrollbars (D-166): verifies attach_frame_scrollbars,
// attach_vertical_frame_scrollbar, and attach_horizontal_frame_scrollbar across
// ckVision's scrollable views, including zero-interior-cost layout, corner
// resize-grip preservation, bottom-border coexistence with FrameText and
// Window::footer(), pointer-shape reporting, wheel scrolling over frame bars,
// EditorWindow integration, and lifetime teardown in both orders.
#include "cvision/widgets/frame_scrollbars.hpp"

#include <memory>
#include <string>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/filesystem.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/editor_window.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/frame_text.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/log_model.hpp"
#include "cvision/widgets/log_view.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/tree_view.hpp"

using namespace ckv;
using namespace ckv::widgets;

namespace {

void paint_subtree(ui::View& view, scene::Painter& painter) {
    if (!view.visible()) return;
    const Rect b = view.bounds();
    if (b.width <= 0 || b.height <= 0) return;
    scene::Painter local = painter.translated(Point{b.x, b.y}, Rect{0, 0, b.width, b.height});
    view.draw(local);
    for (const auto& child : view.children()) {
        paint_subtree(*child, local);
    }
}

struct UiHarness {
    ui::RoleRegistry registry;
    ui::StandardRoles roles = ui::intern_standard_roles(registry);
    ui::Theme theme = ui::make_classic_theme(registry, roles);

    void attach(Window& window, Rect bounds = Rect{0, 0, 24, 10}) {
        window.set_context(ui::Context{&theme, &registry, nullptr});
        window.set_bounds(bounds);
    }

    scene::Surface paint(Window& window) {
        const Size sz{window.bounds().width, window.bounds().height};
        scene::Surface surface(sz, Cell::from_grapheme(" ", {}));
        scene::Painter painter(surface, Rect{0, 0, sz.width, sz.height});
        paint_subtree(window, painter);
        return surface;
    }
};

}  // namespace

CK_TEST(scroll_viewport_mounts_bars_on_frame_and_gives_content_full_interior) {
    UiHarness h;
    Window window("Viewport");
    h.attach(window, Rect{0, 0, 20, 10});  // interior 18x8

    auto viewport = std::make_unique<ScrollViewport>();
    auto body = std::make_unique<ui::View>();
    body->set_preferred_size(Size{40, 30});
    ui::View* const body_ptr = body.get();
    viewport->set_content(std::move(body));
    ScrollViewport* const vp = viewport.get();
    window.set_content(std::move(viewport));

    const FrameScrollbars bars = attach_frame_scrollbars(window, *vp);
    CK_CHECK(bars.vertical != nullptr);
    CK_CHECK(bars.horizontal != nullptr);
    CK_CHECK(attach_vertical_frame_scrollbar(window, *vp) == nullptr);
    CK_CHECK(attach_horizontal_frame_scrollbar(window, *vp) == nullptr);

    // Content receives the full 40x30 preferred size while visible viewport is
    // the full 18x8 interior — zero columns or rows lost to scrollbars.
    CK_CHECK(vp->bounds() == (Rect{1, 1, 18, 8}));
    CK_CHECK(body_ptr->bounds() == (Rect{0, 0, 40, 30}));
    CK_CHECK(bars.vertical->visible());
    CK_CHECK(bars.horizontal->visible());
    CK_CHECK(bars.vertical->viewport_size() == 8);
    CK_CHECK(bars.horizontal->viewport_size() == 18);
    CK_CHECK(bars.vertical->bounds() == (Rect{19, 1, 1, 8}));
    // Bottom Fill leaves both 2-cell corner resize grips (0..1 and 18..19) free.
    CK_CHECK(bars.horizontal->bounds() == (Rect{2, 9, 16, 1}));

    bars.vertical->set_position(5);
    bars.horizontal->set_position(7);
    CK_CHECK(vp->scroll_x() == 7);
    CK_CHECK(vp->scroll_y() == 5);
    (void)h.paint(window);
}

CK_TEST(text_editor_text_view_memo_and_log_view_use_full_interior_with_frame_bars) {
    UiHarness h;

    // TextEditor (unwrapped so both vertical and horizontal overflow).
    {
        Window window("Editor");
        h.attach(window, Rect{0, 0, 22, 8});  // interior 20x6
        auto doc = std::make_shared<EditorDocument>(
            "line 0 with enough horizontal text to exceed twenty columns\n"
            "line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\n");
        auto editor = std::make_unique<TextEditor>(doc);
        editor->set_wrap_mode(WrapMode::None);
        TextEditor* const ed = editor.get();
        window.set_content(std::move(editor));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *ed);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal != nullptr);
        CK_CHECK(bars.vertical->visible());
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.vertical->viewport_size() == 6);
        CK_CHECK(bars.horizontal->viewport_size() == 20);
        (void)h.paint(window);
    }

    // TextView.
    {
        Window window("Text");
        h.attach(window, Rect{0, 0, 16, 6});  // interior 14x4
        auto view = std::make_unique<TextView>();
        view->set_text("0123456789abcdefghij\nrow2\nrow3\nrow4\nrow5\nrow6");
        TextView* const tv = view.get();
        window.set_content(std::move(view));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal != nullptr);
        CK_CHECK(bars.vertical->visible());
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.vertical->viewport_size() == 4);
        CK_CHECK(bars.horizontal->viewport_size() == 14);
        bars.vertical->set_position(2);
        CK_CHECK(tv->top_line() == 2);
        (void)h.paint(window);
    }

    // Memo (vertical and horizontal scrollbars).
    {
        Window window("Memo");
        h.attach(window, Rect{0, 0, 14, 6});  // interior 12x4
        auto memo = std::make_unique<Memo>();
        memo->set_text("0123456789abcdefgh\nb\nc\nd\ne\nf");
        Memo* const m = memo.get();
        window.set_content(std::move(memo));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *m);
        CK_CHECK(bars.vertical != nullptr);
        CK_CHECK(bars.horizontal != nullptr);
        CK_CHECK(bars.vertical->visible());
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.vertical->viewport_size() == 4);
        CK_CHECK(bars.horizontal->viewport_size() == 12);
        (void)h.paint(window);
    }

    // LogView.
    {
        LogLimits limits;
        limits.records = 64;
        limits.record_bytes = 256;
        limits.retained_bytes = 16384;
        LogModel model(limits);
        model.begin_source(1, 1);
        for (int i = 0; i < 10; ++i) {
            CK_CHECK(model.append_record("record with wide payload " + std::to_string(i)));
        }
        Window window("Log");
        h.attach(window, Rect{0, 0, 18, 6});  // interior 16x4
        auto log = std::make_unique<LogView>(model);
        log->set_wrap_mode(WrapMode::None);
        LogView* const lv = log.get();
        window.set_content(std::move(log));
        lv->refresh();
        const FrameScrollbars bars = attach_frame_scrollbars(window, *lv);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal != nullptr);
        CK_CHECK(bars.vertical->visible());
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.vertical->viewport_size() == 4);
        CK_CHECK(bars.horizontal->viewport_size() == 16);
        (void)h.paint(window);
    }
}

CK_TEST(list_tree_table_and_flow_views_reclaim_rightmost_interior_column) {
    UiHarness h;

    // ListView: 10-char item fits completely inside a 10-wide interior once
    // its scrollbar moves onto the window frame.
    {
        Window window("List");
        h.attach(window, Rect{0, 0, 12, 6});  // interior 10x4
        auto list = std::make_unique<ListView>();
        list->set_items({"0123456789", "row1", "row2", "row3", "row4", "row5"});
        ListView* const lv = list.get();
        window.set_content(std::move(list));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *lv);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal == nullptr);
        CK_CHECK(bars.vertical->bounds() == (Rect{11, 1, 1, 4}));

        const scene::Surface surface = h.paint(window);
        // Interior column 9 (window x=10) shows the 10th character '9', while
        // window x=11 is the frame scrollbar.
        CK_CHECK(surface.at(Point{10, 1}).grapheme() == "9");
    }

    // TreeView.
    {
        Window window("Tree");
        h.attach(window, Rect{0, 0, 12, 6});  // interior 10x4
        auto tree = std::make_unique<TreeView>();
        TreeNode root;
        root.label = "3456789A";  // 2 branch cols ("  ") + 8 chars = 10 cols
        TreeView* const tv = tree.get();
        window.set_content(std::move(tree));
        tv->set_roots({std::move(root)});
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal == nullptr);

        const scene::Surface surface = h.paint(window);
        CK_CHECK(surface.at(Point{10, 1}).grapheme() == "A");
    }

    // Table.
    {
        Window window("Table");
        h.attach(window, Rect{0, 0, 12, 6});  // interior 10x4
        auto table = std::make_unique<Table>();
        table->set_columns({TableColumn{"Col", 10, 3}});
        table->set_rows({{"0123456789"}, {"r2"}, {"r3"}, {"r4"}, {"r5"}});
        Table* const tb = table.get();
        window.set_content(std::move(table));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tb);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal == nullptr);

        const scene::Surface surface = h.paint(window);
        // Data row 0 sits at window y=2 (y=1 is header); x=10 holds '9'.
        CK_CHECK(surface.at(Point{10, 2}).grapheme() == "9");
    }

    // FlowView: a 10-char word fits on 1 row in a 10-wide interior once the
    // scrollbar is moved to the frame (embedded Always would wrap it to 2 rows).
    {
        Window window("Flow");
        h.attach(window, Rect{0, 0, 12, 6});  // interior 10x4
        auto flow = std::make_unique<FlowView>();
        flow->set_document(FlowDocument{{FlowBlock{{FlowText{"0123456789", static_cast<Attr>(0), std::nullopt}}}}});
        FlowView* const fv = flow.get();
        window.set_content(std::move(flow));
        CK_CHECK(fv->line_count() == 2);  // before detaching: 9 content cols -> wraps
        const FrameScrollbars bars = attach_frame_scrollbars(window, *fv);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal == nullptr);
        CK_CHECK(fv->line_count() == 1);  // after detaching: 10 content cols -> 1 line
        (void)h.paint(window);
    }
}

CK_TEST(bottom_fill_scrollbar_coexists_with_status_readouts_and_footer_in_any_order) {
    UiHarness h;

    // Order A: horizontal bar attached BEFORE FrameText at Bottom/End.
    {
        Window window("CoexistA");
        h.attach(window, Rect{0, 0, 40, 10});
        auto view = std::make_unique<TextView>();
        view->set_text("01234567890123456789012345678901234567890123456789");
        TextView* const tv = view.get();
        window.set_content(std::move(view));

        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        auto* status = window.add_frame_overlay(std::make_unique<FrameText>("Ln 1, Col 1"),
                                                FrameSlot{Edge::Bottom, ui::Alignment::End});
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.horizontal->bounds().x == 2);
        // Bar stops at least 1 border cell short of the status readout, and
        // status stops short of the 2-cell bottom-right resize grip (38..39).
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);
        CK_CHECK(status->bounds().x + status->bounds().width <= 38);

        // Live growth of status text dynamically shrinks the horizontal bar.
        const int width_before = bars.horizontal->bounds().width;
        status->set_text("Ln 128, Col 64 OVR *");
        CK_CHECK(bars.horizontal->bounds().width < width_before);
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);

        // Adding a footer on the left shrinks the bar from the left as well.
        window.set_footer("UTF-8");
        CK_CHECK(bars.horizontal->bounds().x > 2);
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);

        // Narrowing the window until less than 3 cells remain for the Fill bar
        // cleanly suppresses the sliver rather than overlapping text or grips.
        window.set_bounds(Rect{0, 0, 28, 10});
        CK_CHECK(bars.horizontal->bounds().width == 0);
    }

    // Order B: FrameText at Bottom/End added BEFORE horizontal bar.
    {
        Window window("CoexistB");
        h.attach(window, Rect{0, 0, 40, 10});
        auto* status = window.add_frame_overlay(std::make_unique<FrameText>("Ln 1, Col 1"),
                                                FrameSlot{Edge::Bottom, ui::Alignment::End});
        auto view = std::make_unique<TextView>();
        view->set_text("01234567890123456789012345678901234567890123456789");
        TextView* const tv = view.get();
        window.set_content(std::move(view));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        CK_CHECK(bars.horizontal->bounds().x == 2);
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);
    }
}

CK_TEST(corner_grips_resize_window_and_pointer_shapes_and_wheel_work_on_frame_bars) {
    term::HeadlessTerminal term(Size{80, 24});
    ManualClock test_clock;
    ui::Application app(term, test_clock);
    app.theme() = ui::make_classic_theme(app.roles(), app.standard_roles());

    auto desktop = std::make_unique<Desktop>(app.root().bounds());
    Desktop* const desk = desktop.get();
    app.root().add_child(std::move(desktop));

    auto win = std::make_unique<Window>("FrameResize");
    win->set_bounds(Rect{2, 2, 24, 10});  // interior 22x8; absolute bottom-right corner at (25, 11)
    auto view = std::make_unique<TextView>();
    view->set_text(
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "r1\nr2\nr3\nr4\nr5\nr6\nr7\nr8\nr9\nr10\nr11\nr12");
    TextView* const tv = view.get();
    win->set_content(std::move(view));
    const FrameScrollbars bars = attach_frame_scrollbars(*win, *tv);
    Window* const window = desk->add_window(std::move(win));
    app.step(0);

    CK_CHECK(bars.vertical->visible());
    CK_CHECK(bars.horizontal->visible());

    // Hovering over the vertical or horizontal frame scrollbar reports Pointer
    // (not Window's ResizeEastWest / ResizeNorthSouth), whereas the uncovered
    // bottom-right corner grip still reports ResizeNorthWestSouthEast.
    term.inject_event(MouseEvent{MouseAction::Move, MouseButton::None, Point{25, 5}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(app.pointer_shape() == PointerShape::Pointer);

    term.inject_event(MouseEvent{MouseAction::Move, MouseButton::None, Point{10, 11}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(app.pointer_shape() == PointerShape::Pointer);

    term.inject_event(MouseEvent{MouseAction::Move, MouseButton::None, Point{25, 11}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(app.pointer_shape() == PointerShape::ResizeNorthWestSouthEast);

    // Wheeling directly over the vertical frame scrollbar scrolls vertically.
    CK_CHECK(tv->top_line() == 0);
    term.inject_event(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{25, 5}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(tv->top_line() > 0);

    // Wheeling directly over the horizontal frame scrollbar scrolls horizontally.
    CK_CHECK(tv->left_column() == 0);
    term.inject_event(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{10, 11}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(tv->left_column() > 0);

    // Dragging the uncovered bottom-right corner grip (25, 11) resizes the
    // window even while both frame scrollbars are visible.
    term.inject_event(MouseEvent{MouseAction::Down, MouseButton::Left, Point{25, 11}, std::nullopt, Modifier::None});
    term.inject_event(MouseEvent{MouseAction::Move, MouseButton::Left, Point{30, 14}, std::nullopt, Modifier::None});
    term.inject_event(MouseEvent{MouseAction::Up, MouseButton::Left, Point{30, 14}, std::nullopt, Modifier::None});
    app.step(0);
    CK_CHECK(window->bounds() == (Rect{2, 2, 29, 13}));
    CK_CHECK(bars.vertical->bounds() == (Rect{28, 1, 1, 11}));
    CK_CHECK(bars.horizontal->bounds() == (Rect{2, 12, 25, 1}));
}

CK_TEST(lifetime_guards_survive_destroying_content_or_removing_frame_overlay_in_either_order) {
    UiHarness h;

    // Case 1: content view destroyed while frame scrollbars remain on Window.
    {
        Window window("ViewDiesFirst");
        h.attach(window, Rect{0, 0, 20, 8});
        auto view = std::make_unique<TextView>();
        view->set_text("012345678901234567890123456789\n1\n2\n3\n4\n5\n6\n7\n8");
        TextView* const tv = view.get();
        window.set_content(std::move(view));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        CK_CHECK(bars.vertical->visible());

        // Destroy the TextView; the frame bars hide cleanly and moving them
        // afterward invokes no dangling callback.
        window.set_content(nullptr);
        CK_CHECK(!bars.vertical->visible());
        CK_CHECK(!bars.horizontal->visible());
        bars.vertical->set_position(2);
        bars.horizontal->set_position(3);
        (void)h.paint(window);
    }

    // Case 2: frame scrollbars removed from Window while content view lives on.
    {
        Window window("BarsDieFirst");
        h.attach(window, Rect{0, 0, 20, 8});
        auto view = std::make_unique<TextView>();
        view->set_text("012345678901234567890123456789\n1\n2\n3\n4\n5\n6\n7\n8");
        TextView* const tv = view.get();
        window.set_content(std::move(view));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *tv);
        CK_CHECK(window.remove_frame_overlay(bars.vertical) != nullptr);
        CK_CHECK(window.remove_frame_overlay(bars.horizontal) != nullptr);

        // Resizing, mutating text, link activation, and keyboard/wheel scrolling
        // all continue to work after the external frame bars are removed.
        window.set_bounds(Rect{0, 0, 24, 10});
        std::string activated;
        tv->on_link_activate = [&activated](const std::string& target) { activated = target; };
        tv->set_spans({TextSpan{"go\n", static_cast<Attr>(0), std::string("topic:1")},
                       TextSpan{"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11", static_cast<Attr>(0), std::nullopt}});
        CK_CHECK(tv->on_key(KeyEvent{{Key::Enter, Modifier::None, ""}}));
        CK_CHECK(activated == "topic:1");
        CK_CHECK(tv->on_key(KeyEvent{{Key::Down, Modifier::None, ""}}));
        CK_CHECK(tv->top_line() == 1);
        tv->set_top_line(3);
        CK_CHECK(tv->top_line() == 3);
        (void)h.paint(window);
    }

    // Case 3: TerminalView destroyed while its frame scrollbar remains on Window,
    // and vice versa, plus idempotent second attach and preserved observer.
    {
        term::TerminalEmulator session{[] {
            term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
            profile.cells = Size{10, 4};
            return profile;
        }()};
        Window window("TermLifetime");
        h.attach(window, Rect{0, 0, 12, 6});
        auto tv = std::make_unique<TerminalView>(session);
        TerminalView* const raw_tv = tv.get();
        int observer_calls = 0;
        raw_tv->on_scroll_state_changed = [&observer_calls] { ++observer_calls; };
        window.set_content(std::move(tv));
        Scrollbar* const bar = attach_frame_scrollbars(window, *raw_tv).vertical;
        CK_CHECK(bar != nullptr);
        CK_CHECK(attach_frame_scrollbars(window, *raw_tv).vertical == nullptr);
        for (int i = 0; i < 8; ++i) session.feed_output("line\r\n");
        window.notify_terminal_subsession_changed(session);
        CK_CHECK(bar->visible());
        CK_CHECK(observer_calls > 0);

        window.set_content(nullptr);
        // Destroying TerminalView immediately hides its frame scrollbar.
        CK_CHECK(!bar->visible());
        bar->set_position(0);
        CK_CHECK(!bar->visible());
    }

    // Case 4: attach_frame_scrollbars called BEFORE set_content (reverse child
    // destruction order when Window tears down).
    {
        Window window("BarsAttachedBeforeContent");
        h.attach(window, Rect{0, 0, 20, 8});
        auto view = std::make_unique<TextView>();
        view->set_text("012345678901234567890123456789\n1\n2\n3\n4\n5\n6\n7\n8");
        const FrameScrollbars bars = attach_frame_scrollbars(window, *view);
        CK_CHECK(bars.vertical != nullptr && bars.horizontal != nullptr);
        window.set_content(std::move(view));
        (void)h.paint(window);
    }
}

CK_TEST(widgets_retain_full_scroll_and_input_semantics_after_frame_bars_are_removed) {
    UiHarness h;

    // Memo: mouse click caret placement, wheel, and vertical/horizontal reveal.
    {
        Window window("MemoNoBars");
        h.attach(window, Rect{0, 0, 14, 6});  // interior 12x4
        auto memo = std::make_unique<Memo>();
        memo->set_wrap_mode(WrapMode::None);
        memo->set_text("0123456789abcdefghij\nl1\nl2\nl3\nl4\nl5");
        Memo* const m = memo.get();
        window.set_content(std::move(memo));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *m);
        CK_CHECK(window.remove_frame_overlay(bars.vertical) != nullptr);
        CK_CHECK(window.remove_frame_overlay(bars.horizontal) != nullptr);

        CK_CHECK(m->on_key(KeyEvent{{Key::End, Modifier::None, ""}}));
        CK_CHECK(m->left_column() > 0);
        CK_CHECK(m->on_mouse(MouseEvent{MouseAction::Down, MouseButton::Left, Point{2, 3}, std::nullopt, Modifier::None}));
        CK_CHECK(m->on_mouse(MouseEvent{MouseAction::Up, MouseButton::Left, Point{2, 3}, std::nullopt, Modifier::None}));
        CK_CHECK(m->cursor().line == 2);
        CK_CHECK(m->on_mouse(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{2, 2}, std::nullopt, Modifier::None}));
        (void)h.paint(window);
    }

    // TextEditor: horizontal scroll reveal without horizontal bar.
    {
        Window window("EditorNoBars");
        h.attach(window, Rect{0, 0, 14, 6});  // interior 12x4
        auto doc = std::make_shared<EditorDocument>("0123456789abcdefghijklmnop\n2\n3\n4\n5\n");
        auto editor = std::make_unique<TextEditor>(doc);
        editor->set_wrap_mode(WrapMode::None);
        TextEditor* const ed = editor.get();
        window.set_content(std::move(editor));
        const FrameScrollbars bars = attach_frame_scrollbars(window, *ed);
        CK_CHECK(window.remove_frame_overlay(bars.vertical) != nullptr);
        CK_CHECK(window.remove_frame_overlay(bars.horizontal) != nullptr);

        CK_CHECK(ed->on_key(KeyEvent{{Key::End, Modifier::None, ""}}));
        CK_CHECK(ed->left_column() > 0);
        (void)h.paint(window);
    }

    // ScrollViewport, ListView, TreeView, Table, and FlowView.
    {
        Window window("ViewportNoBars");
        h.attach(window, Rect{0, 0, 14, 6});  // interior 12x4
        auto vp = std::make_unique<ScrollViewport>(ui::FocusPolicy::TabStop);
        auto body = std::make_unique<ui::View>();
        body->set_preferred_size(Size{30, 20});
        vp->set_content(std::move(body));
        ScrollViewport* const raw_vp = vp.get();
        window.set_content(std::move(vp));
        const FrameScrollbars vp_bars = attach_frame_scrollbars(window, *raw_vp);
        CK_CHECK(window.remove_frame_overlay(vp_bars.vertical) != nullptr);
        CK_CHECK(window.remove_frame_overlay(vp_bars.horizontal) != nullptr);
        raw_vp->set_scroll(4, 6);
        CK_CHECK(raw_vp->scroll_x() == 4 && raw_vp->scroll_y() == 6);
        CK_CHECK(raw_vp->on_key(KeyEvent{{Key::Down, Modifier::None, ""}}));
        CK_CHECK(raw_vp->scroll_y() == 7);

        auto list = std::make_unique<ListView>();
        list->set_items({"0", "1", "2", "3", "4", "5"});
        ListView* const raw_list = list.get();
        window.set_content(std::move(list));
        CK_CHECK(window.remove_frame_overlay(attach_frame_scrollbars(window, *raw_list).vertical) != nullptr);
        (void)h.paint(window);
        CK_CHECK(raw_list->on_mouse(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{2, 2}, std::nullopt, Modifier::None}));

        auto tree = std::make_unique<TreeView>();
        std::vector<TreeNode> roots(6);
        for (int i = 0; i < 6; ++i) roots[static_cast<std::size_t>(i)].label = std::to_string(i);
        tree->set_roots(std::move(roots));
        TreeView* const raw_tree = tree.get();
        window.set_content(std::move(tree));
        CK_CHECK(window.remove_frame_overlay(attach_frame_scrollbars(window, *raw_tree).vertical) != nullptr);
        (void)h.paint(window);
        CK_CHECK(raw_tree->on_mouse(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{2, 2}, std::nullopt, Modifier::None}));

        auto table = std::make_unique<Table>();
        table->set_columns({TableColumn{"C", 6, 3}});
        table->set_rows({{"0"}, {"1"}, {"2"}, {"3"}, {"4"}, {"5"}});
        Table* const raw_table = table.get();
        window.set_content(std::move(table));
        CK_CHECK(window.remove_frame_overlay(attach_frame_scrollbars(window, *raw_table).vertical) != nullptr);
        CK_CHECK(raw_table->on_mouse(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{2, 2}, std::nullopt, Modifier::None}));

        auto flow = std::make_unique<FlowView>();
        flow->set_document(FlowDocument{{FlowBlock{{FlowText{"a\nb\nc\nd\ne\nf\ng", static_cast<Attr>(0), std::nullopt}}}}});
        FlowView* const raw_flow = flow.get();
        window.set_content(std::move(flow));
        CK_CHECK(window.remove_frame_overlay(attach_frame_scrollbars(window, *raw_flow).vertical) != nullptr);
        CK_CHECK(raw_flow->on_key(KeyEvent{{Key::Down, Modifier::None, ""}}));
        CK_CHECK(raw_flow->top_line() == 1);
    }
}

CK_TEST(replacing_or_detaching_window_content_replaces_superseded_bars_and_severs_detached_views) {
    UiHarness h;
    Window window("ReplaceContent");
    h.attach(window, Rect{0, 0, 20, 8});

    auto first = std::make_unique<TextView>();
    first->set_text("short");
    TextView* const raw_first = first.get();
    window.set_content(std::move(first));
    const FrameScrollbars first_bars = attach_frame_scrollbars(window, *raw_first);
    CK_CHECK(!first_bars.vertical->visible());

    // Detach `first` WITHOUT destroying it; mutating `first` afterward must NOT
    // show or move `window`'s frame scrollbars.
    std::unique_ptr<ui::View> saved = window.set_content(nullptr);
    raw_first->set_text("012345678901234567890123456789\n1\n2\n3\n4\n5\n6\n7\n8\n9");
    CK_CHECK(!first_bars.vertical->visible());
    CK_CHECK(!first_bars.horizontal->visible());
    CK_CHECK(raw_first->on_key(KeyEvent{{Key::Down, Modifier::None, ""}}));
    CK_CHECK(raw_first->top_line() == 1);

    // Mount a second scrollable view on the same Window without manual overlay
    // removal; superseded Scrollbar overlays are cleanly replaced.
    auto second = std::make_unique<TextView>();
    second->set_text("012345678901234567890123456789\n1\n2\n3\n4\n5\n6\n7\n8\n9");
    TextView* const raw_second = second.get();
    window.set_content(std::move(second));
    const FrameScrollbars second_bars = attach_frame_scrollbars(window, *raw_second);
    CK_CHECK(second_bars.vertical != nullptr && second_bars.horizontal != nullptr);
    CK_CHECK(second_bars.vertical->visible());
    CK_CHECK(second_bars.horizontal->visible());

    // Replace again with a TerminalView on the same Window.
    term::TerminalEmulator session{[] {
        term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
        profile.cells = Size{18, 6};
        return profile;
    }()};
    auto term_view = std::make_unique<TerminalView>(session);
    TerminalView* const raw_term = term_view.get();
    window.set_content(std::move(term_view));
    const FrameScrollbars term_bars = attach_frame_scrollbars(window, *raw_term);
    CK_CHECK(term_bars.vertical != nullptr);
    (void)h.paint(window);
}

CK_TEST(disabling_hiding_thumb_drag_pointer_move_size_exit_and_dynamic_wrap_transitions_behave_cleanly) {
    term::HeadlessTerminal term(Size{80, 24});
    ManualClock test_clock;
    ui::Application app(term, test_clock);
    app.theme() = ui::make_classic_theme(app.roles(), app.standard_roles());

    auto desktop = std::make_unique<Desktop>(app.root().bounds());
    Desktop* const desk = desktop.get();
    app.root().add_child(std::move(desktop));

    // Split window: disabling or hiding the right pane updates its frame bar.
    {
        auto win = std::make_unique<Window>("SplitPane");
        win->set_bounds(Rect{2, 2, 30, 10});  // right border at x=31
        auto left = std::make_unique<TreeView>();
        auto right = std::make_unique<ListView>();
        right->set_items({"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});
        ListView* const raw_right = right.get();
        auto split = std::make_unique<Splitter>(win->content_rect(), std::move(left), std::move(right));
        win->set_content(std::move(split));
        Scrollbar* const bar = attach_vertical_frame_scrollbar(*win, *raw_right);
        Window* const window = desk->add_window(std::move(win));
        app.set_focus(raw_right);
        app.step(0);
        CK_CHECK(bar->visible());

        // Disabling `raw_right` makes its frame scrollbar report NotAllowed and
        // refuse wheel/click scrolling without turning into a Window edge resize.
        raw_right->set_enabled(false);
        term.inject_event(MouseEvent{MouseAction::Move, MouseButton::None, Point{31, 5}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(app.pointer_shape() == PointerShape::NotAllowed);
        const int pos_before = bar->position();
        term.inject_event(MouseEvent{MouseAction::Wheel, MouseButton::WheelDown, Point{31, 5}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(bar->position() == pos_before);
        raw_right->set_enabled(true);

        // Hiding `raw_right` hides its frame scrollbar; showing restores it.
        raw_right->set_visible(false);
        CK_CHECK(!bar->visible());
        raw_right->set_visible(true);
        CK_CHECK(bar->visible());

        // Clicking a frame scrollbar while Window is in keyboard move/size mode
        // exits move/size mode and restores focus to the pane.
        app.set_focus(raw_right);
        window->enter_move_size_mode();
        CK_CHECK(window->in_move_size_mode());
        term.inject_event(MouseEvent{MouseAction::Down, MouseButton::Left, Point{31, 9}, std::nullopt, Modifier::None});
        term.inject_event(MouseEvent{MouseAction::Up, MouseButton::Left, Point{31, 9}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(!window->in_move_size_mode());
        CK_CHECK(app.focused() == raw_right);

        // Thumb drag keeps PointerShape::Pointer even when pointer moves onto
        // Window's corner grip (31, 11) or title bar (31, 2).
        bar->set_position(0);
        const int thumb_y = window->bounds().y + bar->bounds().y + 1;
        term.inject_event(MouseEvent{MouseAction::Down, MouseButton::Left, Point{31, thumb_y}, std::nullopt, Modifier::None});
        term.inject_event(MouseEvent{MouseAction::Move, MouseButton::Left, Point{31, 11}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(app.pointer_shape() == PointerShape::Pointer);
        term.inject_event(MouseEvent{MouseAction::Move, MouseButton::Left, Point{31, 2}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(app.pointer_shape() == PointerShape::Pointer);
        term.inject_event(MouseEvent{MouseAction::Up, MouseButton::Left, Point{31, 2}, std::nullopt, Modifier::None});
        app.step(0);
        desk->remove_child(window);
    }

    // Dynamic wrap toggle + narrow/widen resize with FrameText readout.
    {
        auto doc = std::make_shared<EditorDocument>(
            "0123456789 abcdefghij klmnopqrst uvwxyzABCD EFGHIJKLMN OPQRSTUVWXYZ\n"
            "line2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\n");
        auto win = std::make_unique<Window>("DynamicEditor");
        win->set_bounds(Rect{2, 2, 42, 10});
        auto ed = std::make_unique<TextEditor>(doc);
        ed->set_wrap_mode(WrapMode::Word);
        TextEditor* const editor = ed.get();
        win->set_content(std::move(ed));
        const FrameScrollbars bars = attach_frame_scrollbars(*win, *editor);
        auto* status = win->add_frame_overlay(std::make_unique<FrameText>("Ln 1, Col 1 INS"),
                                              FrameSlot{Edge::Bottom, ui::Alignment::End});
        Window* const window = desk->add_window(std::move(win));
        app.step(0);

        CK_CHECK(bars.vertical->visible());
        CK_CHECK(!bars.horizontal->visible());

        editor->set_wrap_mode(WrapMode::None);
        app.step(0);
        CK_CHECK(bars.horizontal->visible());
        CK_CHECK(bars.horizontal->bounds().width >= 3);
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);

        // Shrink window until bottom Fill span drops below 3 cells, then widen back.
        window->set_bounds(Rect{2, 2, 22, 10});
        app.step(0);
        CK_CHECK(bars.horizontal->bounds().width == 0);
        // Bottom-right corner grip (23, 11) still hit-tests to Window resize.
        term.inject_event(MouseEvent{MouseAction::Move, MouseButton::None, Point{23, 11}, std::nullopt, Modifier::None});
        app.step(0);
        CK_CHECK(app.pointer_shape() == PointerShape::ResizeNorthWestSouthEast);

        window->set_bounds(Rect{2, 2, 44, 10});
        app.step(0);
        CK_CHECK(bars.horizontal->bounds().width >= 3);
        CK_CHECK(bars.horizontal->bounds().x + bars.horizontal->bounds().width + 1 <= status->bounds().x);
    }
}

CK_TEST(editor_window_mounts_scrollbars_on_frame_beside_cursor_status) {
    UiHarness h;
    MemoryFileSystem fs;
    auto doc = std::make_shared<EditorDocument>(
        "wide line 0123456789012345678901234567890123456789\n"
        "2\n3\n4\n5\n6\n7\n8\n9\n10\n");
    EditorWindow window("Untitled", doc, fs);
    window.editor().set_wrap_mode(WrapMode::None);
    h.attach(window, Rect{0, 0, 40, 10});
    (void)h.paint(window);
    // EditorWindow's content is the full 38x8 interior, and its editor has no
    // remaining embedded Scrollbar children because both were moved to the frame.
    CK_CHECK(window.editor().bounds() == (Rect{1, 1, 38, 8}));
    CK_CHECK(window.editor().children().empty());
}
