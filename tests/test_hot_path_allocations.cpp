// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-31 allocation gates. The process-wide replacement lives only in the
// test executable; library state remains instance-owned. It counts ordinary
// allocations while a narrow owning-thread scope is active, after each path
// has been warmed to its established scene/topology capacity.
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/image.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/option_group.hpp"

namespace {

std::atomic<bool> allocation_measurement_active{false};
std::atomic<std::size_t> measured_allocations{0};

void* counted_allocate(std::size_t size) {
    if (allocation_measurement_active.load(std::memory_order_relaxed))
        measured_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

class AllocationScope {
public:
    AllocationScope() {
        measured_allocations.store(0, std::memory_order_relaxed);
        allocation_measurement_active.store(true, std::memory_order_relaxed);
    }
    ~AllocationScope() { allocation_measurement_active.store(false, std::memory_order_relaxed); }

    std::size_t count() const noexcept { return measured_allocations.load(std::memory_order_relaxed); }
};

class InputProbe final : public ckv::ui::View {
public:
    bool on_key(const ckv::KeyEvent&) override { return true; }
    bool on_mouse(const ckv::MouseEvent&) override { return true; }
};

}  // namespace

namespace {

// The nothrow forms have to be replaced alongside the throwing ones, or the
// replacement set is only half applied. libstdc++'s std::get_temporary_buffer
// — reached from std::stable_sort, and in this tree only from
// Table::rebuild_order — allocates with ::operator new(n, std::nothrow) and
// releases through the sized ::operator delete. Replacing one side and not the
// other hands a sanitizer-owned pointer to std::free, which is exactly the
// alloc-dealloc-mismatch ASan reported. The std::align_val_t overloads are
// deliberately left alone: neither side is replaced, so they are already
// consistent, and replacing one of them would manufacture the same bug again.
void* counted_allocate_nothrow(std::size_t size) noexcept {
    if (allocation_measurement_active.load(std::memory_order_relaxed))
        measured_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

}  // namespace

void* operator new(std::size_t size) { return counted_allocate(size); }
void* operator new[](std::size_t size) { return counted_allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return counted_allocate_nothrow(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return counted_allocate_nothrow(size);
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }

CK_TEST(warmed_compositor_and_presenter_allocate_nothing_for_an_unchanged_frame) {
    ckv::scene::Compositor compositor(ckv::Size{8, 4});
    ckv::scene::Surface background(ckv::Size{8, 4});
    compositor.compose({}, background);
    {
        AllocationScope allocations;
        compositor.compose({}, background);
        CK_CHECK(allocations.count() == 0);
    }

    ckv::term::HeadlessTerminal terminal(ckv::Size{8, 4});
    ckv::term::Presenter presenter(terminal);
    presenter.present(compositor.frame().view(), ckv::CursorState{}, 0);
    terminal.clear_written();
    {
        AllocationScope allocations;
        presenter.present(compositor.frame().view(), ckv::CursorState{}, 0);
        CK_CHECK(allocations.count() == 0);
        CK_CHECK(presenter.last_bytes_emitted() == 0);
    }
}

CK_TEST(a_warmed_linked_frame_on_a_hyperlink_host_allocates_nothing_when_unchanged) {
    // D-088: the presenter keeps a copy of the last frame's link table, which
    // it refreshes into its own capacity; links cost nothing once warmed.
    ckv::scene::Compositor compositor(ckv::Size{8, 2});
    ckv::scene::Surface background(ckv::Size{8, 2});
    ckv::scene::Painter(background, ckv::Rect{0, 0, 8, 2})
        .draw_text(ckv::Point{0, 0}, "docs", ckv::Style{}, "https://example.test/documentation");
    ckv::scene::Surface window(ckv::Size{4, 1});
    ckv::scene::Painter(window, ckv::Rect{0, 0, 4, 1})
        .draw_text(ckv::Point{0, 0}, "home", ckv::Style{}, "https://example.test/home-page");
    const std::vector<ckv::scene::Layer> layers{{1, &window, ckv::Point{4, 1}, false}};
    compositor.compose(layers, background);

    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.hyperlinks = true;
    ckv::term::HeadlessTerminal terminal(ckv::Size{8, 2}, caps);
    ckv::term::Presenter presenter(terminal);
    presenter.present(compositor.frame().view(), ckv::CursorState{}, 0);
    presenter.present(compositor.frame().view(), ckv::CursorState{}, 0);
    terminal.clear_written();
    {
        AllocationScope allocations;
        compositor.compose(layers, background);
        presenter.present(compositor.frame().view(), ckv::CursorState{}, 0);
        CK_CHECK(allocations.count() == 0);
        CK_CHECK(presenter.last_bytes_emitted() == 0);
    }
}

CK_TEST(warmed_raster_layer_movement_updates_visibility_without_allocation) {
    ckv::scene::Compositor compositor(ckv::Size{16, 4});
    ckv::scene::Surface background(ckv::Size{16, 4});
    compositor.compose({}, background);  // consume initial background damage

    ckv::scene::Surface raster_layer(ckv::Size{4, 3});
    ckv::scene::Painter painter(raster_layer, ckv::Rect{0, 0, 4, 3});
    const auto image = std::make_shared<ckv::Image>(ckv::PixelSize{8, 6});
    painter.draw_image(ckv::Rect{0, 0, 2, 2}, 1, image, [](ckv::scene::Painter& fallback) {
        fallback.fill(ckv::Rect{0, 0, 2, 2}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    });
    std::vector<ckv::scene::Layer> layers{{1, &raster_layer, ckv::Point{0, 0}, false}};

    compositor.compose(layers, background);
    layers.front().position = ckv::Point{8, 0};
    compositor.compose(layers, background);  // grow and warm movement/raster-slice scratch
    layers.front().position = ckv::Point{0, 0};
    compositor.compose(layers, background);

    {
        AllocationScope allocations;
        layers.front().position = ckv::Point{8, 0};
        compositor.compose(layers, background);
        CK_CHECK(allocations.count() == 0);
    }
    CK_CHECK(compositor.last_compose_cells_touched() == 24);  // old + new 4x3 layer rects
    CK_CHECK(compositor.visible_rasters().size() == 1);
    CK_CHECK(compositor.visible_rasters().front().full_anchor == (ckv::Rect{8, 0, 2, 2}));
}

CK_TEST(warmed_application_dispatch_focus_and_post_drain_allocate_nothing) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    auto probe_owned = std::make_unique<InputProbe>();
    InputProbe* const probe = probe_owned.get();
    probe->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    app.root().add_child(std::move(probe_owned));
    app.step(0);
    app.set_focus(probe);

    const ckv::KeyEvent key{ckv::KeyChord{ckv::Key::Tab, ckv::Modifier::None, ""}};
    const ckv::MouseEvent mouse{.action = ckv::MouseAction::Down,
                                .button = ckv::MouseButton::Left,
                                .cell = ckv::Point{0, 0}};
    app.dispatch(key);       // route scratch warm-up
    app.dispatch(mouse);     // capture-route scratch warm-up
    app.focus_next();        // focus traversal scratch warm-up
    app.post([] {});
    app.step(0);             // posted-work scratch warm-up
    app.start_timer(1, false, [] {});
    clock.advance(1);
    app.step(clock.now_nanos());  // due-callback scratch warm-up

    app.post([] {});         // enqueue is intentionally outside the drain measurement
    app.start_timer(1, false, [] {});
    clock.advance(1);
    AllocationScope allocations;
    app.dispatch(key);
    app.dispatch(mouse);
    app.focus_next();
    app.step(clock.now_nanos());
    CK_CHECK(allocations.count() == 0);
}

CK_TEST(warmed_materialized_tree_draw_reuses_its_visible_entry_cache) {
    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    ckv::widgets::TreeNode root{.label = "large root", .expanded = true};
    root.children.reserve(2048);
    for (int index = 0; index < 2048; ++index)
        root.children.push_back(ckv::widgets::TreeNode{.label = "entry " + std::to_string(index)});

    ckv::widgets::TreeView tree;
    tree.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    tree.set_bounds(ckv::Rect{0, 0, 80, 4});
    tree.set_roots({std::move(root)});

    ckv::scene::Surface surface(ckv::Size{80, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 80, 4});
    tree.draw(painter);  // Build and warm the materialized visible-entry cache.

    {
        AllocationScope allocations;
        tree.draw(painter);
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(warmed_toolbar_presentations_reuse_captions_and_hit_geometry_without_allocating) {
    using namespace ckv;
    using namespace ckv::widgets;
    term::HeadlessTerminal terminal(Size{100, 8});
    ManualClock clock;
    ui::Application app(terminal, clock);
    const auto roles = ui::intern_standard_roles(app.roles());
    app.theme() = ui::make_classic_theme(app.roles(), roles);
    bool checked = false;
    const auto first = app.commands().declare({.key = "toolbar.allocation.first", .title = "&Long command caption 漢字 é", .handler = [] {}});
    const auto second = app.commands().declare({.key = "toolbar.allocation.second", .title = "&Another long command caption", .handler = [] {}});
    app.commands().set_checked_predicate(first, [&] { return checked; });
    auto* bar = static_cast<ToolBar*>(app.root().add_child(std::make_unique<ToolBar>()));
    bar->set_groups({{CommandPresentation{first, "", "Control+Shift+LongShortcut"}}, {CommandPresentation{second}}});
    bar->set_show_chords(true);
    bar->set_command_context("toolbar.document.context.with.a.long.stable.name");
    app.set_focus(bar);
    scene::Surface surface(Size{100, 3}, Cell::from_grapheme(" ", Style{}));
    scene::Painter painter(surface, Rect{0, 0, 100, 3});
    for (const auto presentation : {ToolBarPresentation::Compact, ToolBarPresentation::Padded, ToolBarPresentation::Framed}) {
        bar->set_presentation(presentation);
        for (const int width : {100, 68}) {
            bar->set_bounds(Rect{0, 0, width, 3});
            bar->draw(painter);
            (void)bar->pointer_shape_at(Point{1, 0});
            {
                AllocationScope allocations;
                for (int iteration = 0; iteration < 4; ++iteration) {
                    checked = !checked;
                    bar->draw(painter);
                    (void)bar->pointer_shape_at(Point{1, 0});
                    (void)bar->pointer_shape_at(Point{width - 1, 1});
                    bar->on_mouse(MouseEvent{.action = MouseAction::Move, .cell = Point{1, 0}});
                    (void)bar->horizontal_size_hint();
                    bar->on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
                    bar->on_key(KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});
                }
                CK_CHECK(allocations.count() == 0);
            }
        }
    }
}

CK_TEST(progress_presentations_and_percentage_paint_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Progress bar;
    bar.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    bar.set_bounds(ckv::Rect{0, 0, 40, 1});
    bar.set_label("long progress caption 漢é");
    bar.set_show_percentage(true);
    ckv::scene::Surface surface(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 40, 1});
    for (const auto presentation : {ckv::widgets::ProgressPresentation::Solid,
                                  ckv::widgets::ProgressPresentation::Block,
                                  ckv::widgets::ProgressPresentation::Segmented}) {
        bar.set_presentation(presentation);
        bar.draw(painter);
        AllocationScope allocations;
        for (int iteration = 0; iteration < 4; ++iteration) {
            bar.set_fraction(static_cast<double>(iteration) / 3);
            bar.draw(painter);
            bar.set_indeterminate(true);
            bar.set_pulse(iteration);
            bar.draw(painter);
            bar.set_indeterminate(false);
        }
        CK_CHECK(allocations.count() == 0);
    }
}


CK_TEST(input_and_search_presentations_paint_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::InputLine input;
    ckv::widgets::SearchBox search;
    input.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    search.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    input.set_bounds(ckv::Rect{0, 0, 40, 2});
    search.set_bounds(ckv::Rect{0, 0, 40, 2});
    input.set_text("a long Unicode input 漢é spanning the available columns");
    search.set_query("a long Unicode query 漢é");
    search.set_status("a long status that must be elided");
    ckv::scene::Surface surface(ckv::Size{40, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 40, 2});
    for (const auto presentation : {ckv::widgets::InputPresentation::Flat,
                                  ckv::widgets::InputPresentation::Padded,
                                  ckv::widgets::InputPresentation::Underlined}) {
        input.set_presentation(presentation);
        search.set_presentation(presentation);
        input.draw(painter);
        search.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            input.draw(painter);
            search.draw(painter);
            search.field().draw(painter);
        }
        CK_CHECK(allocations.count() == 0);
    }
}


CK_TEST(status_presentations_reuse_prepared_captions_hints_and_hit_geometry) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const auto roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    app.root().set_help_context_key("a long help context key");
    ckv::widgets::StatusLine status;
    status.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    status.set_bounds(ckv::Rect{0, 0, 80, 1});
    const auto run = app.commands().declare({.key = "allocation.run", .title = "Long command caption 漢é"});
    app.set_command_handler(run, [] {});
    ckv::widgets::StatusLineItem item{ckv::widgets::CommandPresentation{run, "", "Ctrl+Shift+F12"}};
    item.group_break_before = true;
    status.set_items({ckv::widgets::StatusLineItem{"Information"}, item});
    status.set_hint_provider([](const std::string&) { return std::string("A long contextual explanation prepared once"); });
    ckv::scene::Surface surface(ckv::Size{80, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 80, 1});
    for (const auto presentation : {ckv::widgets::StatusLinePresentation::Plain, ckv::widgets::StatusLinePresentation::Grouped}) {
        status.set_presentation(presentation);
        status.draw(painter);
        const ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{15, 0}, std::nullopt};
        const ckv::MouseEvent up{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{15, 0}, std::nullopt};
        status.on_mouse(down);
        status.on_mouse(up);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            status.draw(painter);
            (void)status.pointer_shape_at(ckv::Point{15, 0});
            status.on_mouse(down);
            status.on_mouse(up);
        }
        CK_CHECK(allocations.count() == 0);
    }
}


CK_TEST(list_and_table_alternate_surfaces_borrow_materialized_text_during_paint) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::ListView list;
    ckv::widgets::Table table;
    list.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    table.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    list.set_bounds(ckv::Rect{0, 0, 80, 5});
    table.set_bounds(ckv::Rect{0, 0, 80, 5});
    const std::string long_text = "Long Unicode caption 漢é that exceeds small string storage";
    list.set_items({long_text, long_text, long_text});
    table.set_columns({ckv::widgets::TableColumn{long_text, 35, 5}, ckv::widgets::TableColumn{"Value", 35, 5}});
    table.set_rows({{long_text, long_text}, {long_text, long_text}, {long_text, long_text}});
    table.sort_by(0, true);
    ckv::scene::Surface surface(ckv::Size{80, 5});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 80, 5});
    for (const bool banded : {false, true}) {
        list.set_banded_rows(banded);
        table.set_banded_rows(banded);
        for (const bool divided : {false, true}) {
            table.set_column_dividers(divided);
            list.draw(painter);
            table.draw(painter);
            AllocationScope allocations;
            for (int i = 0; i < 4; ++i) {
                list.draw(painter);
                table.draw(painter);
            }
            CK_CHECK(allocations.count() == 0);
        }
    }
}


CK_TEST(splitter_presentations_paint_and_drag_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Splitter split(ckv::Rect{0, 0, 40, 10}, std::make_unique<ckv::ui::View>(), std::make_unique<ckv::ui::View>());
    split.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    ckv::scene::Surface surface(ckv::Size{40, 10});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 40, 10});
    for (const auto style : {ckv::widgets::SplitterPresentation::Line, ckv::widgets::SplitterPresentation::CentralGrip, ckv::widgets::SplitterPresentation::Gutter}) {
        split.set_presentation(style);
        split.set_split_position(15);
        split.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            split.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{15, 4}, std::nullopt});
            split.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{16, 4}, std::nullopt});
            split.draw(painter);
            (void)split.pointer_shape_at(ckv::Point{16, 4});
            split.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{16, 4}, std::nullopt});
            split.set_split_position(15);
        }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(slider_styles_ticks_and_readout_paint_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Slider slider;
    slider.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    slider.set_bounds(ckv::Rect{0, 0, 30, 2});
    slider.set_ticks({{0, "A long Unicode label 漢é"}, {50, "Middle"}, {100, "Maximum"}});
    slider.set_show_value(true);
    ckv::scene::Surface surface(ckv::Size{30, 2});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 2});
    for (auto style : {ckv::widgets::SliderPresentation::Line, ckv::widgets::SliderPresentation::Block, ckv::widgets::SliderPresentation::ProminentThumb}) {
        slider.set_presentation(style);
        slider.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) { slider.set_value(i * 25); slider.draw(painter); }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(button_presentations_paint_cached_mnemonics_without_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Button button("&Long Unicode caption 漢é with ample text");
    button.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    button.set_bounds(ckv::Rect{0, 0, 30, 3});
    ckv::scene::Surface surface(ckv::Size{30, 3});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 3});
    for (auto style : {ckv::widgets::ButtonPresentation::Classic, ckv::widgets::ButtonPresentation::Flat, ckv::widgets::ButtonPresentation::Padded, ckv::widgets::ButtonPresentation::Outlined}) {
        button.set_presentation(style);
        button.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) button.draw(painter);
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(breadcrumb_presentations_reuse_prepared_layout_without_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::BreadcrumbBar bar;
    bar.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    bar.set_segments({"Long Unicode root 漢é", "middle", "another", "Current location 漢é"});
    bar.set_bounds(ckv::Rect{0, 0, 30, 1});
    ckv::scene::Surface surface(ckv::Size{30, 1});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 1});
    for (auto style : {ckv::widgets::BreadcrumbPresentation::Plain, ckv::widgets::BreadcrumbPresentation::Padded, ckv::widgets::BreadcrumbPresentation::Connected}) {
        bar.set_presentation(style);
        bar.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            bar.draw(painter);
            (void)bar.pointer_shape_at(ckv::Point{1, 0});
            (void)bar.focused_segment();
            bar.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::None, ""}});
        }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(option_styles_paint_and_navigate_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::CheckGroup checks({"&Long Unicode label 漢é", "&Second long caption", "&Third"});
    ckv::widgets::RadioGroup radios({"&Long Unicode label 漢é", "&Second long caption", "&Third"});
    checks.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    radios.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    checks.set_bounds(ckv::Rect{0, 0, 40, 10});
    radios.set_bounds(ckv::Rect{0, 0, 40, 10});
    checks.set_columns(2);
    radios.set_columns(2);
    ckv::scene::Surface surface(ckv::Size{40, 10});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 40, 10});
    for (auto style : {ckv::widgets::OptionPresentation::Classic, ckv::widgets::OptionPresentation::BoxedRows, ckv::widgets::OptionPresentation::Buttons}) {
        checks.set_presentation(style);
        radios.set_presentation(style);
        checks.draw(painter);
        radios.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            checks.draw(painter);
            radios.draw(painter);
            checks.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::None, ""}});
            radios.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::None, ""}});
            (void)checks.pointer_shape_at(ckv::Point{1, 0});
        }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(spin_presentations_paint_long_entry_suffixes_without_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::term::HeadlessTerminal terminal{ckv::Size{20, 2}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::widgets::SpinBox spin;
    spin.set_context(ckv::ui::Context{&theme, &registry, &app});
    spin.set_bounds(ckv::Rect{0, 0, 20, 2});
    spin.set_editable(true);
    app.set_focus(&spin);
    spin.on_text(ckv::TextEvent{"Long Unicode entry 漢é with enough characters to scroll"});
    ckv::scene::Surface surface(ckv::Size{20, 2});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 20, 2});
    for (auto style : {ckv::widgets::SpinBoxPresentation::Compact, ckv::widgets::SpinBoxPresentation::Separate, ckv::widgets::SpinBoxPresentation::Stacked}) {
        spin.set_presentation(style);
        spin.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            spin.draw(painter);
            const auto caret = spin.cursor_state();
            CK_CHECK(caret.has_value());
            (void)spin.pointer_shape_at(ckv::Point{19, 0});
        }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(notification_presentations_paint_long_unicode_messages_without_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::NotificationCenter center;
    center.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    center.set_bounds(ckv::Rect{0, 0, 30, 8});
    center.add({ckv::widgets::NotificationSeverity::Info, "Long Unicode message 漢é that must be elided", false});
    center.add({ckv::widgets::NotificationSeverity::Error, "Persistent warning 漢é", true});
    ckv::scene::Surface surface(ckv::Size{30, 8});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 8});
    for (auto style : {ckv::widgets::NotificationPresentation::Lines, ckv::widgets::NotificationPresentation::Banners, ckv::widgets::NotificationPresentation::Framed}) {
        center.set_presentation(style);
        center.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) { center.draw(painter); (void)center.pointer_shape_at(ckv::Point{28, 1}); }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(property_presentations_paint_and_navigate_without_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::PropertyInspector view;
    view.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    view.set_bounds(ckv::Rect{0, 0, 30, 12});
    ckv::widgets::PropertyItem first{"Long Unicode name 漢é", "Long Unicode value 漢é that is clipped", false};
    first.group = "Long Unicode heading 漢é";
    ckv::widgets::PropertyItem second{"Read only", "true", false, ckv::widgets::PropertyKind::Bool};
    second.group = "State";
    view.set_items({first, second});
    view.set_banded_rows(true);
    ckv::scene::Surface surface(ckv::Size{30, 12});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 12});
    for (auto style : {ckv::widgets::PropertyPresentation::Plain, ckv::widgets::PropertyPresentation::Divided,
                       ckv::widgets::PropertyPresentation::Sectioned}) {
        view.set_presentation(style);
        view.draw(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            view.draw(painter);
            view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Down, ckv::Modifier::None, ""}});
            view.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Up, ckv::Modifier::None, ""}});
            (void)view.pointer_shape_at(ckv::Point{20, 1});
        }
        CK_CHECK(allocations.count() == 0);
    }
}

CK_TEST(wizard_presentations_render_and_navigate_without_steady_allocations) {
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    auto theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Wizard view;
    view.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    view.set_bounds(ckv::Rect{0, 0, 50, 14});
    view.set_pages({{"Long Unicode first title 漢é that needs clipping", {}}, {"Second Unicode title 漢é", {}}});
    ckv::scene::Surface surface(ckv::Size{50, 14});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 50, 14});
    for (auto presentation : {ckv::widgets::WizardPresentationStyle::Compact, ckv::widgets::WizardPresentationStyle::Bands,
                              ckv::widgets::WizardPresentationStyle::StepRail}) {
        view.set_presentation(presentation);
        view.draw(painter);
        view.paint_children(painter);
        AllocationScope allocations;
        for (int i = 0; i < 4; ++i) {
            view.draw(painter);
            view.paint_children(painter);
            (void)view.horizontal_size_hint();
            (void)view.vertical_size_hint();
            view.next();
            view.back();
        }
        CK_CHECK(allocations.count() == 0);
    }
}
