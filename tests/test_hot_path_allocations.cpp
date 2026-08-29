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

CK_TEST(warmed_raster_layer_movement_updates_visibility_without_allocation) {
    ckv::scene::Compositor compositor(ckv::Size{16, 4});
    ckv::scene::Surface background(ckv::Size{16, 4});
    compositor.compose({}, background);  // consume initial background damage

    ckv::scene::Surface raster_layer(ckv::Size{4, 3});
    ckv::scene::Painter painter(raster_layer, ckv::Rect{0, 0, 4, 3});
    const auto image = std::make_shared<ckv::Image>(8, 6);
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
