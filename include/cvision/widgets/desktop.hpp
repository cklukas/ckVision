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

#include "cvision/ui/application.hpp"
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

    // The one sanctioned way to present a standard-dialog handle
    // (message box, file/save dialog, directory picker, window list,
    // help viewer — anything returned as a widgets::WindowHandle) on
    // this desktop (M8 WP-5): attaches handle.window via add_window
    // (activation + z-order included, unlike attaching it through
    // plain add_child, which skips activation entirely — a dialog
    // attached that way renders with inactive-frame styling while
    // still holding focus, and sits outside window cycling) and
    // focuses handle.initial_focus. add_window itself remains the
    // right call for an application's OWN windows, which don't come
    // with a pre-computed initial_focus to hand to set_focus.
    Window* show_dialog(WindowHandle handle, ui::Application& app);

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

    // --- Docked chrome (menu bar / status line) --------------------
    //
    // Takes ownership of `view` (like add_window/add_popup), adds it as
    // a child, and docks it to the top or bottom edge, full width, at
    // its own vertical_size_hint().preferred height — kept there across
    // every subsequent Desktop resize via on_resized(), so an
    // application never has to recompute a menu bar's or status line's
    // bounds by hand in its own resize handler (the gap that left
    // examples/gallery's chrome stale after a resize before this
    // existed). Returns a non-owning observer, same convention as
    // add_window. Docking a second view to the same edge replaces the
    // first as far as auto-positioning goes, but does NOT remove it —
    // the caller still owns that decision (mirrors set_content()'s
    // "replaces and returns ownership" for the rare case an application
    // wants to swap a status line at runtime, but most never will).
    // Typed insertion (M9/WP-9): returns the docked view back as T*,
    // not ui::View* — no static_cast at the call site.
    template <class T>
    T* dock_top(std::unique_ptr<T> view) {
        return static_cast<T*>(dock_top_impl(std::move(view)));
    }
    template <class T>
    T* dock_bottom(std::unique_ptr<T> view) {
        return static_cast<T*>(dock_bottom_impl(std::move(view)));
    }
    ui::View* top_dock() const noexcept { return top_dock_; }
    ui::View* bottom_dock() const noexcept { return bottom_dock_; }

    // The area available for windows: this Desktop's own bounds minus
    // whatever is currently docked top/bottom. tile()/cascade() and
    // reposition_within() (called on every window from on_resized())
    // all place windows within this rect, never under the docked
    // chrome.
    Rect content_area() const noexcept;

    void on_resized() override;

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

    // Popups (menu dropdowns, M5): appended after every window in
    // z-order — always topmost regardless of window activation — with
    // their own independent lifetime, not tied to any window. Multiple
    // popups may be open at once (nested submenus, a future feature);
    // each stays above every window and below no other popup opened
    // after it. A window activate() happening while a popup is open
    // re-raises every popup above the newly-activated window, so a
    // popup never gets visually buried by window activation.
    // Typed insertion (M9/WP-9): returns the popup back as T*, not
    // ui::View* — no static_cast at the call site.
    template <class T>
    T* add_popup(std::unique_ptr<T> popup) {
        return static_cast<T*>(add_popup_impl(std::move(popup)));
    }
    std::unique_ptr<ui::View> remove_popup(ui::View* popup);
    const std::vector<ui::View*>& popups() const noexcept { return popups_; }

    // Activates+raises whichever of windows_ is `target` or an
    // ancestor of it (M8 WP-3) — the mechanism behind click-to-raise
    // for clicks anywhere inside a window, not just its title bar. A
    // no-op if `target` is not inside any owned window (a click on a
    // popup, or on the desktop's own background).
    void on_descendant_mouse_down(ui::View& target) override;

    void draw(scene::Painter& painter) override;

    // Routes to remove_window()/remove_popup() when `child` is one of
    // those (keeping windows_/popups_/active_ consistent — plain
    // View::remove_child would silently desync them, leaving a
    // dangling pointer in windows_ or popups_), falling back to plain
    // child removal for anything else (docked chrome, or a window/
    // popup attached via add_child() directly rather than add_window/
    // add_popup, both of which remain fully supported).
    std::unique_ptr<ui::View> detach_child(ui::View* child) override;

    // Overridden (not just the default per-child walk) so a window's
    // shadow can be composited immediately after that window paints,
    // in the SAME z-order pass — the only place "dim exactly what's
    // below me, leave alone whatever paints above me next" can be
    // gotten right (ARCHITECTURE.md §5 "moving a window costs
    // composition only" / D-... shadow declaration). See
    // ui::View::paint_children's doc comment for why a separate
    // post-pass over the whole tree cannot do this correctly.
    void paint_children(const scene::Painter& own_painter) override;

private:
    ui::View* dock_top_impl(std::unique_ptr<ui::View> view);
    ui::View* dock_bottom_impl(std::unique_ptr<ui::View> view);
    ui::View* add_popup_impl(std::unique_ptr<ui::View> popup);
    void reraise_popups();

    const ui::Theme& theme_;
    ui::RoleId background_role_;
    std::vector<Window*> windows_;  // parallel to View::children_, same order
    Window* active_ = nullptr;
    std::vector<ui::View*> popups_;
    ui::View* top_dock_ = nullptr;
    ui::View* bottom_dock_ = nullptr;
    // content_area() as of the end of the previous on_resized() pass —
    // DesktopGrowPolicy::AnchorEdges needs the delta between the old
    // and new area to preserve each such window's right/bottom margin
    // (initialized in the constructor body, since content_area()
    // depends on top_dock_/bottom_dock_, set after this member in
    // declaration order).
    Rect last_content_area_{};
};

}  // namespace ckv::widgets
