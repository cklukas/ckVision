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

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// Window-management default handlers (M9/WP-13, M10/WP-13 completion,
// D-029): on_attached() installs itself as kClose/kQuit/kZoom/
// kNextWindow/kPreviousWindow/kTile/kCascade's default handler — but
// ONLY for whichever of those nothing has claimed yet
// (CommandRegistry::has_handler), the same guarded-install/guarded-
// cleanup pattern MenuBar's own kMenu default established (M9/WP-13):
// an application that wires its own handler for any of these before
// attaching a Desktop is never overridden, and the destructor clears
// only the ones THIS instance actually installed, never one it found
// already claimed.
class Desktop : public ui::View {
public:
    struct WindowSnapshot {
        Window* window = nullptr;
        std::weak_ptr<void> liveness;
        Rect bounds;
        Rect restored_bounds;
        DesktopGrowPolicy grow_policy = DesktopGrowPolicy::None;
        bool active = false;
        bool zoomed = false;
    };

    struct Snapshot {
        std::vector<WindowSnapshot> windows;
    };

    explicit Desktop(Rect bounds = {});
    ~Desktop() override;

    // Adds `window`, activating it (raising to front, deactivating any
    // previously active window) and returning a non-owning observer.
    Window* add_window(std::unique_ptr<Window> window);

    // The generic View insertion/removal surface is safe to use with a
    // Desktop too.  A direct Window child is always registered exactly
    // as if it had been passed to add_window(), so no public attachment
    // path can bypass activation, cycling, resize, or lifecycle state.
    ui::View* add_child(std::unique_ptr<ui::View> child) override;
    std::unique_ptr<ui::View> remove_child(ui::View* child) override;

    // Explicit modeless standard-dialog presentation (D-038): attaches
    // through add_window (activation + z-order included) and focuses
    // handle.initial_focus, but leaves ordinary background input active.
    // Returns nullptr if a focus callback synchronously detaches or destroys
    // the new window before this operation returns.
    // Generic add_child is equally management-safe for a Window but
    // deliberately does not know WindowHandle's initial-focus choice.
    Window* present_modeless(WindowHandle handle, ui::Application& app);

    // Explicit non-blocking modal presentation (D-038): attaches and
    // focuses like present_modeless, but pushes the Window onto the
    // Application modal stack before returning. The scope ends when
    // the Window detaches; standard-dialog present_* helpers provide
    // typed completion rather than exposing this raw observer. Returns
    // nullptr if a focus callback synchronously detaches or destroys the
    // new window before this operation returns.
    Window* present_modal(WindowHandle handle, ui::Application& app);

    // Whether `child` should cast a shadow right now.
    //
    // A shadow says "this floats above that". When windows are tiled they
    // leave no desktop between them, so nothing is floating: the shadow one
    // casts on its neighbour is a dark smudge along a shared edge, and it
    // eats a row and a column of the window beneath. Resizable windows that
    // between them cover the whole content area therefore stop casting until
    // a gap reappears.
    //
    // Fixed-size and modal windows keep theirs regardless. A dialog IS above
    // the arrangement, whatever the arrangement happens to cover, and it is
    // the shadow that says so.
    bool child_casts_shadow(const ui::View& child) const;

    // The blocking sibling of present_modal (M9/WP-15, D-021's "blocking
    // convenience for applications that own the loop"): attaches and
    // focuses `handle` exactly like present_modal, then scopes event
    // routing to it (Application::push_modal) and pumps
    // Application::step() — never a nested native loop — until the
    // window is no longer among windows() (it has closed and self-
    // detached), popping the modal scope before returning. If a host
    // request_quit() interrupts that pump while the window is still
    // attached, it force-detaches the window instead of consulting its
    // vetoable user-close protocol; factory-level exec_* helpers then
    // return their documented cancellation fallback. If arbitrary pump
    // work removes this Desktop, the call returns after the detach path
    // has removed its exact modal scope; it never dereferences the former
    // Desktop or modal Window. Doesn't
    // interpret WHAT closed the window or produce any result of its
    // own: exec_message_box (and any future exec_dialog/
    // exec_file_dialog) captures its own typed result through the SAME
    // on_result-style callback its factory already accepts — into a
    // local read back after this call returns.
    void exec_modal(ui::Application& app, WindowHandle handle);

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

    // Typed layout snapshot/restore for application-owned session state.
    // The snapshot records current windows by instance identity and liveness,
    // then restore ignores entries whose original Window is no longer owned.
    // It restores geometry, z-order, active state, zoom state, and grow policy;
    // it never creates or destroys windows.
    Snapshot snapshot() const;
    void restore(const Snapshot& snapshot);

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

    // The third member of that family: one view that fills content_area()
    // and is kept filling it across every resize, under any windows and
    // popups.
    //
    // Not every application's desktop is a place where windows float. A
    // viewer, a monitor, a dashboard is a fixed arrangement of panes over
    // one subject, and its desktop IS that arrangement — usually a
    // ui::Dock or a ui::Column composing the panes. Without this such an
    // application has to recompute the arrangement's bounds in its own
    // resize handler, which is exactly the hand-maintained geometry
    // dock_top()/dock_bottom() exist to remove for chrome; content is no
    // different, and gets the same treatment rather than a second
    // convention.
    //
    // Returns ownership of whatever content was there before (nullptr the
    // first time), so swapping the arrangement at runtime neither leaks
    // nor detaches something the caller still wanted.
    template <class T>
    T* set_content(std::unique_ptr<T> view) {
        return static_cast<T*>(set_content_impl(std::move(view)));
    }
    ui::View* content() const noexcept { return content_; }
    std::unique_ptr<ui::View> take_content();

    // The area available for windows: this Desktop's own bounds minus
    // whatever is currently docked top/bottom. tile()/cascade() and
    // reposition_within() (called on every window from on_resized())
    // all place windows within this rect, never under the docked
    // chrome.
    Rect content_area() const noexcept;

    void on_resized() override;
    void on_attached() override;
    // A docked view's own preferred height changing (M9/WP-16) — e.g. a
    // status line whose content now wraps to two rows — needs the same
    // full treatment as a Desktop resize: content_area() shrinks/grows
    // and every owned window re-clamps against it, not just the dock's
    // own two lines, so this reuses on_resized() wholesale rather than
    // duplicating a smaller slice of it.
    void on_child_size_hint_changed(ui::View& child) override;

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
    void draw_retained(scene::Painter& painter) override;

    // Polymorphic synonym for remove_child(), retained for callers that
    // only know their parent as a View.  remove_child() itself has the
    // same invariant, so either public removal entry point is safe.
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
    void paint_retained(const scene::Painter& own_painter,
                        std::vector<scene::Layer>& layers) override;

    // Number of retained child backing stores repainted in the most recent
    // retained Desktop pass. A translated Window leaves this at zero;
    // resizing or changing a window's content increments only that window.
    std::size_t last_content_repaints() const noexcept { return last_content_repaints_; }

private:
    struct WindowRelationshipToken {};
    ui::View* dock_top_impl(std::unique_ptr<ui::View> view);
    ui::View* dock_bottom_impl(std::unique_ptr<ui::View> view);
    ui::View* set_content_impl(std::unique_ptr<ui::View> view);
    ui::View* add_popup_impl(std::unique_ptr<ui::View> popup);
    void reraise_popups();
    // Installs `handler` as `id`'s default IFF nothing has claimed `id`
    // yet, and records that this instance did so (installed_default_
    // handlers_) — the shared guarded-install step on_attached() calls
    // once per window-management command; the destructor walks that
    // same list to clear only what it actually installed.
    void install_default_handler(ui::CommandId id, std::function<void()> handler);
    // kClose's default handler: closes the active window (vetoable,
    // exactly what clicking its own close control already does) — a
    // no-op with no active window.
    void close_active_window();
    // kQuit's default handler: sweeps every window through the same
    // vetoable close() protocol clicking each one's own close control
    // would (ARCHITECTURE.md §5 "application quit sweeps all windows
    // through the same protocol"), front-to-back so a topmost "are you
    // sure" dialog a window's own close_request opens gets seen first;
    // the FIRST veto stops the sweep entirely (quit is cancelled — not
    // "close everything that didn't veto, then quit anyway"). Only
    // calls Application::request_quit() if every starting window's close()
    // returned true. A user close callback may detach/destroy/reuse storage,
    // so the implementation snapshots per-instance lifetime identity before
    // iterating rather than assuming windows_ remains stable. A recursive
    // default kQuit execution from one of those callbacks is a no-op: only
    // the outer request may observe later vetoes or request shutdown.
    void quit_sweep();
    // kWindowList's default handler: presents make_window_list_dialog over
    // this Desktop's own windows(), which is the same list, in the same
    // cycling order, that select_by_number and activate_next() already walk.
    // A Desktop is the only thing that knows that order, so an application
    // that had to supply this handler itself could only reach for the same
    // dialog over the same list — which is why it is here, beside kTile and
    // kCascade, rather than copied into every application. Presenting it is a
    // no-op while one is already open, and while this Desktop has no
    // Application (nothing to present into).
    void show_window_list();
    // kTerminalReport's default handler: presents make_terminal_report_dialog
    // over this Desktop. What the report reads lives on the Application —
    // capabilities, cell grid, mouse-dispatch counters — but presenting it
    // needs a surface, and this Desktop is the surface every dialog handler
    // here presents into. An application whose terminal can count decoded SGR
    // reports (a POSIX host) presents the dialog itself and passes the probe;
    // this handler shows the report without that one line. Presenting is a
    // no-op while one is already open, and while this Desktop has no
    // Application.
    void show_terminal_report();
    // kZoom's default handler: toggles the ACTIVE window's own zoom,
    // the same call its own zoom-control click already makes — a
    // no-op with no active window.
    void zoom_active_window();
    void on_invalidated(Rect, ui::InvalidationKind) override;
    void on_descendant_invalidated(const ui::View& source, Rect,
                                   ui::InvalidationKind kind) override;
    static bool is_descendant_of(const ui::View& view, const ui::View* ancestor) noexcept;
    void request_layer_recompose();
    void raise_layer_to_front(ui::View* child);
    // One window's move or resize rests every window's pictures — see the
    // binding in add_window. Counted rather than boolean out of caution;
    // a single pointer means it is 0 or 1 in practice.
    void window_gesture_changed(bool active);

    ui::RoleId background_role_ = ui::kInvalidRole;
    std::vector<Window*> windows_;  // parallel to View::children_, same order
    Window* active_ = nullptr;
    int active_gestures_ = 0;
    std::vector<ui::View*> popups_;
    struct PopupBacking {
        std::optional<scene::Surface> surface;
        int compositor_layer_id = 0;
        bool dirty = true;
    };
    std::unordered_map<ui::View*, PopupBacking> popup_backings_;
    int next_compositor_layer_id_ = 1;
    std::size_t last_content_repaints_ = 0;
    bool retained_base_dirty_ = true;
    bool structural_invalidation_ = false;
    ui::View* top_dock_ = nullptr;
    ui::View* content_ = nullptr;
    ui::View* bottom_dock_ = nullptr;
    ui::Application* app_ = nullptr;
    std::vector<ui::CommandId> installed_default_handlers_;
    // Whether a Desktop-installed dialog handler (kWindowList,
    // kTerminalReport) already has its dialog up. Shared, like the quit sweep
    // below, so the dialog's completion can clear it even when what completed
    // the dialog was this Desktop's destruction.
    struct StandardDialogState {
        bool open = false;
    };
    std::shared_ptr<StandardDialogState> window_list_state_ =
        std::make_shared<StandardDialogState>();
    std::shared_ptr<StandardDialogState> terminal_report_state_ =
        std::make_shared<StandardDialogState>();
    struct QuitSweepState {
        bool in_progress = false;
    };
    std::shared_ptr<QuitSweepState> quit_sweep_state_ = std::make_shared<QuitSweepState>();
    // content_area() as of the end of the previous on_resized() pass —
    // DesktopGrowPolicy::AnchorEdges needs the delta between the old
    // and new area to preserve each such window's right/bottom margin
    // (initialized in the constructor body, since content_area()
    // depends on top_dock_/bottom_dock_, set after this member in
    // declaration order).
    Rect last_content_area_{};
    // Capability held weakly by every attached Window's private zoom
    // target.  It makes destruction and detachment safe even if a
    // callback is accidentally invoked after the Window leaves us.
    std::shared_ptr<WindowRelationshipToken> window_relationship_ =
        std::make_shared<WindowRelationshipToken>();
};

}  // namespace ckv::widgets
