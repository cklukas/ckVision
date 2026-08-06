// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Desktop: background fill, owns z-ordered windows, activation,
// next/previous cycling, select-by-number, tile/cascade
// (ARCHITECTURE.md §5 "Windows, popups, modality"). Window-list
// dialog and typed snapshot/restore are separate, larger pieces
// (dialog needs materialize_dialog + a List view that doesn't exist
// until M6; snapshot/restore is explicitly an application-owned-
// storage feature per ARCHITECTURE.md's "Application services") —
// tracked as Desktop follow-ons, not folded in here.
#pragma once

#include <memory>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop : public ui::View {
public:
    Desktop(Rect bounds, const ui::Theme& theme, ui::RoleId background_role);

    // Adds `window`, activating it (raising to front, deactivating any
    // previously active window) and returning a non-owning observer.
    Window* add_window(std::unique_ptr<Window> window);

    // Detaches and returns ownership (nullptr if `window` is not one of
    // this desktop's windows). If it was the active window, activates
    // the new topmost remaining window, if any.
    std::unique_ptr<Window> remove_window(Window* window);

    // Every window currently owned, in stable INSERTION order — this is
    // the cycling order activate_next()/activate_previous() and
    // select_by_number() use, deliberately independent of z-order (a
    // z-order-relative "next" would only ever reach the top two
    // windows once activation itself perturbs z-order — see
    // desktop.cpp). Current z-order/topmost is a View::children()
    // concern, not this list's.
    const std::vector<Window*>& windows() const noexcept { return windows_; }

    Window* active_window() const noexcept { return active_; }

    // Raises `window` to the front and marks it active, deactivating
    // whichever window was active before (a no-op if `window` is
    // already the active one). CKV_ASSERT if `window` is not owned by
    // this desktop.
    void activate(Window* window);

    // Classic "next/previous window" commands: steps activation to the
    // adjacent window in windows()' stable insertion order, wrapping
    // around. No-op with 0 or 1 windows.
    void activate_next();
    void activate_previous();

    // 1-based index into windows()' insertion order (matching the
    // classic "Alt+1".."Alt+9" window-select convention). Out-of-range
    // n is a harmless no-op, not an error — a stale keybinding from a
    // since-closed window must not crash the application.
    void select_by_number(int n);

    // Arranges every window to fill an equal vertical slice of the
    // desktop's content area, left to right, in z-order. A no-op with
    // zero windows.
    void tile();

    // Arranges every window at the same size (roughly 2/3 of the
    // desktop, clamped to fit), offset diagonally so each remains
    // partially visible, topmost window last (frontmost).
    void cascade();

    void draw(scene::Painter& painter) override;

private:
    const ui::Theme& theme_;
    ui::RoleId background_role_;
    std::vector<Window*> windows_;  // parallel to View::children_, same order
    Window* active_ = nullptr;
};

}  // namespace ckv::widgets
