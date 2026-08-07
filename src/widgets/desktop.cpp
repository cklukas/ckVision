// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/scene/compositor.hpp"

namespace ckv::widgets {

Desktop::Desktop(Rect bounds, const ui::Theme& theme, ui::RoleId background_role)
    : View(bounds), theme_(theme), background_role_(background_role) {}

Window* Desktop::add_window(std::unique_ptr<Window> window) {
    CKV_ASSERT(window != nullptr);
    auto* raw = static_cast<Window*>(add_child(std::move(window)));
    windows_.push_back(raw);  // stable insertion order — cycling order, not z-order
    activate(raw);
    return raw;
}

std::unique_ptr<Window> Desktop::remove_window(Window* window) {
    auto it = std::find(windows_.begin(), windows_.end(), window);
    if (it == windows_.end()) return nullptr;
    windows_.erase(it);

    std::unique_ptr<ui::View> detached = remove_child(window);
    CKV_ASSERT(detached != nullptr);  // was in windows_, so it must have been a child

    if (active_ == window) {
        active_ = nullptr;
        // The new topmost WINDOW, not necessarily children().back() —
        // a popup may currently be topmost, and children() holds both.
        for (auto rit = children().rbegin(); rit != children().rend(); ++rit) {
            ui::View* candidate = rit->get();
            auto found = std::find_if(windows_.begin(), windows_.end(),
                                       [candidate](Window* w) { return static_cast<ui::View*>(w) == candidate; });
            if (found != windows_.end()) {
                activate(*found);
                break;
            }
        }
    }
    return std::unique_ptr<Window>(static_cast<Window*>(detached.release()));
}

void Desktop::on_descendant_mouse_down(ui::View& target) {
    for (ui::View* v = &target; v != nullptr; v = v->parent()) {
        auto it = std::find_if(windows_.begin(), windows_.end(),
                                [v](Window* w) { return static_cast<ui::View*>(w) == v; });
        if (it != windows_.end()) {
            activate(*it);
            return;
        }
    }
}

void Desktop::activate(Window* window) {
    CKV_ASSERT(std::find(windows_.begin(), windows_.end(), window) != windows_.end());
    if (window == active_) return;
    if (active_ != nullptr) active_->set_active(false);
    raise_to_front(window);
    reraise_popups();  // keep every open popup above the window that just moved to front
    active_ = window;
    active_->set_active(true);
}

ui::View* Desktop::add_popup(std::unique_ptr<ui::View> popup) {
    CKV_ASSERT(popup != nullptr);
    ui::View* raw = add_child(std::move(popup));
    popups_.push_back(raw);  // already topmost: add_child appends to the end of children_
    return raw;
}

std::unique_ptr<ui::View> Desktop::remove_popup(ui::View* popup) {
    auto it = std::find(popups_.begin(), popups_.end(), popup);
    if (it == popups_.end()) return nullptr;
    popups_.erase(it);
    return remove_child(popup);
}

std::unique_ptr<ui::View> Desktop::detach_child(ui::View* child) {
    if (auto* window = dynamic_cast<Window*>(child);
        window != nullptr && std::find(windows_.begin(), windows_.end(), window) != windows_.end())
        return remove_window(window);
    if (std::find(popups_.begin(), popups_.end(), child) != popups_.end()) return remove_popup(child);
    return remove_child(child);
}

void Desktop::reraise_popups() {
    for (ui::View* popup : popups_) raise_to_front(popup);
}

void Desktop::activate_next() {
    if (windows_.size() < 2 || active_ == nullptr) return;
    auto it = std::find(windows_.begin(), windows_.end(), active_);
    CKV_ASSERT(it != windows_.end());
    const std::size_t i = static_cast<std::size_t>(it - windows_.begin());
    activate(windows_[(i + 1) % windows_.size()]);
}

void Desktop::activate_previous() {
    if (windows_.size() < 2 || active_ == nullptr) return;
    auto it = std::find(windows_.begin(), windows_.end(), active_);
    CKV_ASSERT(it != windows_.end());
    const std::size_t i = static_cast<std::size_t>(it - windows_.begin());
    activate(windows_[(i + windows_.size() - 1) % windows_.size()]);
}

void Desktop::select_by_number(int n) {
    if (n < 1 || static_cast<std::size_t>(n) > windows_.size()) return;
    activate(windows_[static_cast<std::size_t>(n - 1)]);
}

ui::View* Desktop::dock_top(std::unique_ptr<ui::View> view) {
    CKV_ASSERT(view != nullptr);
    ui::View* raw = add_child(std::move(view));
    top_dock_ = raw;
    on_resized();  // position it immediately, not just on the next Desktop resize
    return raw;
}

ui::View* Desktop::dock_bottom(std::unique_ptr<ui::View> view) {
    CKV_ASSERT(view != nullptr);
    ui::View* raw = add_child(std::move(view));
    bottom_dock_ = raw;
    on_resized();
    return raw;
}

Rect Desktop::content_area() const noexcept {
    const int top = top_dock_ != nullptr ? std::max(1, top_dock_->vertical_size_hint().preferred) : 0;
    const int bottom = bottom_dock_ != nullptr ? std::max(1, bottom_dock_->vertical_size_hint().preferred) : 0;
    return Rect{0, top, bounds().width, std::max(0, bounds().height - top - bottom)};
}

void Desktop::on_resized() {
    if (top_dock_ != nullptr) {
        const int h = std::max(1, top_dock_->vertical_size_hint().preferred);
        top_dock_->set_bounds(Rect{0, 0, bounds().width, h});
    }
    if (bottom_dock_ != nullptr) {
        const int h = std::max(1, bottom_dock_->vertical_size_hint().preferred);
        bottom_dock_->set_bounds(Rect{0, bounds().height - h, bounds().width, h});
    }
    // ARCHITECTURE.md §5 "Sizing policy": on shrink, windows are
    // deterministically clamped and repositioned to remain reachable —
    // every Desktop resize (which includes docking new chrome, since
    // that shrinks the content area too) re-clamps every window.
    const Rect area = content_area();
    for (Window* w : windows_) w->reposition_within(area);
}

void Desktop::tile() {
    if (windows_.empty()) return;
    const Rect area = content_area();
    const int n = static_cast<int>(windows_.size());
    const int slice = area.width / n;
    int x = area.x;
    for (int i = 0; i < n; ++i) {
        const int width = (i == n - 1) ? (area.x + area.width - x) : slice;  // last slice absorbs any remainder
        windows_[static_cast<std::size_t>(i)]->set_bounds(Rect{x, area.y, width, area.height});
        x += width;
    }
}

void Desktop::cascade() {
    if (windows_.empty()) return;
    const Rect area = content_area();
    const int width = std::max(10, area.width * 2 / 3);
    const int height = std::max(4, area.height * 2 / 3);
    const int max_x = std::max(0, area.width - width);
    const int max_y = std::max(0, area.height - height);
    for (std::size_t i = 0; i < windows_.size(); ++i) {
        const int offset = static_cast<int>(i);
        const int x = area.x + (max_x == 0 ? 0 : (offset * 2) % (max_x + 1));
        const int y = area.y + (max_y == 0 ? 0 : (offset * 1) % (max_y + 1));
        windows_[i]->set_bounds(Rect{x, y, width, height});
    }
    // Cascade order should also become z-order: last window frontmost.
    for (Window* w : windows_) raise_to_front(w);
}

void Desktop::draw(scene::Painter& painter) {
    // U+2591 LIGHT SHADE — the classic windowed-desktop fill pattern
    // (distinct from a plain space so the desktop always reads as
    // "background texture," never mistakable for an unstyled dialog
    // panel even under a color scheme where the two happen to share a
    // hue).
    painter.fill(Rect{0, 0, bounds().width, bounds().height},
                  Cell::from_grapheme("░", theme_.resolve(background_role_)));
}

void Desktop::paint_children(const scene::Painter& own_painter) {
    // Shadows clip to content_area(), NOT the full desktop bounds: a
    // maximized window fills content_area() exactly (Window::toggle_zoom
    // is handed that same rect — see build_window()/menu handling in
    // every consumer), so its footprint's bottom strip would otherwise
    // fall exactly on the docked status line's row and dim it — a
    // window's shadow is a windows-on-windows effect, and docked chrome
    // is not a window.
    const Rect shadow_clip = content_area();
    for (auto& child_ptr : children()) {
        View& child = *child_ptr;
        if (!paint_one_child(child, own_painter)) continue;  // invisible: nothing to shadow either

        auto* window = dynamic_cast<Window*>(&child);
        if (window == nullptr || !window->casts_shadow()) continue;

        // Shadow is composited in DESKTOP-local space (own_painter's
        // own coordinate frame), not the window's translated space —
        // child.bounds() is already desktop-local since window is a
        // direct child of this Desktop.
        scene::Painter mutable_painter = own_painter;
        for (const Rect& footprint : scene::shadow_footprint(child.bounds(), scene::ShadowSpec{})) {
            const Rect clipped = footprint.intersected(shadow_clip);
            if (!clipped.empty()) mutable_painter.transform_style(clipped, &scene::default_dim);
        }
    }
}

}  // namespace ckv::widgets
