// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

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

void Desktop::tile() {
    if (windows_.empty()) return;
    const int n = static_cast<int>(windows_.size());
    const int total_width = bounds().width;
    const int slice = total_width / n;
    int x = 0;
    for (int i = 0; i < n; ++i) {
        const int width = (i == n - 1) ? (total_width - x) : slice;  // last slice absorbs any remainder
        windows_[static_cast<std::size_t>(i)]->set_bounds(Rect{x, 0, width, bounds().height});
        x += width;
    }
}

void Desktop::cascade() {
    if (windows_.empty()) return;
    const int width = std::max(10, bounds().width * 2 / 3);
    const int height = std::max(4, bounds().height * 2 / 3);
    const int max_x = std::max(0, bounds().width - width);
    const int max_y = std::max(0, bounds().height - height);
    for (std::size_t i = 0; i < windows_.size(); ++i) {
        const int offset = static_cast<int>(i);
        const int x = max_x == 0 ? 0 : (offset * 2) % (max_x + 1);
        const int y = max_y == 0 ? 0 : (offset * 1) % (max_y + 1);
        windows_[i]->set_bounds(Rect{x, y, width, height});
    }
    // Cascade order should also become z-order: last window frontmost.
    for (Window* w : windows_) raise_to_front(w);
}

void Desktop::draw(scene::Painter& painter) {
    painter.fill(Rect{0, 0, bounds().width, bounds().height},
                  Cell::from_grapheme(" ", theme_.resolve(background_role_)));
}

}  // namespace ckv::widgets
