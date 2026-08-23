// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Desktop: background fill, owns z-ordered windows, activation,
// next/previous cycling, select-by-number, tile/tile-horizontally/
// tile-vertically/tile-grid/cascade and the filled-tiling query the
// tilings feed (the architecture §5 "Windows, popups, modality"). Window-list
// dialog and typed snapshot/restore are separate, larger pieces
// (dialog needs materialize_dialog + a List view that doesn't exist
// until M6; snapshot/restore is explicitly an application-owned-
// storage feature per the architecture's "Application services") —
// tracked as Desktop follow-ons, not folded in here.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cvision/ui/animation.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class MinimizedWindowStub;

// Window-management default handlers (M9/WP-13, M10/WP-13 completion,
// D-029): on_attached() installs itself as kClose/kQuit/kZoom/
// kMinimize/kNextWindow/kPreviousWindow/kTile/kTileHorizontally/
// kTileVertically/kTileGrid/kCascade's default handler — but
// ONLY for whichever of those nothing has claimed yet
// (CommandRegistry::has_handler), the same guarded-install/guarded-
// cleanup pattern MenuBar's own kMenu default established (M9/WP-13):
// an application that wires its own handler for any of these before
// attaching a Desktop is never overridden, and the destructor clears
// only the ones THIS instance actually installed, never one it found
// already claimed.
// Where a window goes when it is put away (D-064). The question every
// desktop has to answer and only a host can: `Desktop` draws the way back
// itself unless something else already does.
enum class MinimizedWindowPlacement {
    // The default. A minimized window leaves a MinimizedWindowStub — its own
    // top frame, one row tall — parked along the bottom of the desktop, and
    // comes back when that stub is clicked. Nothing is required of the
    // application: a window put away is on screen as the thing it is, which
    // is the only arrangement in which the `_` control is not a trap.
    Parked,
    // The host lists its windows itself — a WindowSwitcherBar, a session
    // picker, a window menu of its own — and does not want a second listing
    // under it. The window is hidden and nothing is drawn for it, which is
    // what this Desktop did before there was a choice to make.
    HostListed,
    // Windows are never put away at all. Every window's minimize control
    // disappears (Window::set_minimizable(false)), `minimize_active_window`
    // does nothing, anything already parked comes back, and a window that an
    // application minimizes directly is put straight back — an application
    // that turns minimizing off must not strand what was minimized while it
    // was on, and "no way to reach it" is not a promise that can be kept for
    // the reader's routes only.
    Disabled,
};

class Desktop : public ui::View {
public:
    // Long enough to read as a movement between two places, short enough
    // that a reader who minimizes a window and immediately reaches for the
    // next one is never waiting on it. Six frames at the default interval.
    static constexpr std::int64_t kDefaultMinimizeAnimationNanos = 180'000'000;

    struct WindowSnapshot {
        Window* window = nullptr;
        std::weak_ptr<void> liveness;
        Rect bounds;
        Rect restored_bounds;
        DesktopGrowPolicy grow_policy = DesktopGrowPolicy::None;
        bool active = false;
        bool zoomed = false;
        // Recorded beside the geometry, for the same reason: a layout in
        // which one window was parked on the switcher bar is not the same
        // layout with that window back on the desktop.
        bool minimized = false;
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
    //
    // Also clears the window's minimize control (Window::set_minimizable):
    // modality is this layer's concept, not Window's, and hiding the one
    // window that is accepting input would leave the reader an application
    // answering nothing, with the way back parked on a bar that the modal
    // scope will not let them click. exec_modal attaches the same way.
    Window* present_modal(WindowHandle handle, ui::Application& app);

    // Whether `child` should cast a shadow right now.
    //
    // A shadow says "this floats above that". When windows are tiled they
    // leave no desktop between them, so nothing is floating: the shadow one
    // casts on its neighbour is a dark smudge along a shared edge, and it
    // eats a row and a column of the window beneath. So the answer is
    // filled_tile_fractions()' — while that reports a filled tiling, the
    // windows in it stop casting, and the moment one of them is dragged,
    // resized or closed out of the grid every shadow is back.
    //
    // One detection, not two: a second "are we tiled?" rule of its own here
    // would be a rule that could disagree with the one a layout report is
    // tagged with, and a reader looking at shadows would be looking at the
    // wrong answer.
    //
    // Fixed-size and modal windows keep theirs regardless. A dialog IS above
    // the arrangement, whatever the arrangement happens to cover, and it is
    // the shadow that says so — which is the same reason neither counts as
    // a participant in the tiling above.
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
    //
    // A MINIMIZED window is still in here. Being listed is what a hidden
    // window has left — it is how a switcher bar shows it and how the
    // reader asks for it back — so this list is the window set, not the
    // set of windows currently on screen. Everything that arranges or
    // cycles windows skips the hidden ones itself.
    const std::vector<Window*>& windows() const noexcept { return windows_; }

    Window* active_window() const noexcept { return active_; }

    // --- Observing the window set ----------------------------------
    //
    // What just changed about the windows this Desktop owns.
    enum class WindowChange {
        // Reported LAST in an addition, once the window is fully attached
        // and has taken activation — so the Activated its own arrival caused
        // arrives first. Every observer of this re-reads the list anyway, and
        // reporting Added earlier would hand one a window whose bindings and
        // move bounds were not set yet.
        Added,
        // Reported after the window has left windows() and whichever window
        // succeeded it is already active. The reference is still valid: the
        // detached window is alive in the ownership the removal is about to
        // hand back.
        Removed,
        Activated,
        TitleChanged,
        // A window hidden by its `_` control, or by set_minimized(true).
        // Reported LAST, once whichever window succeeded it is already
        // active — the same rule Added follows, and for the same reason:
        // an observer reading active_window() from here reads the answer,
        // never the gap. The window is still listed in windows(); that is
        // how a switcher bar offers it back.
        Minimized,
        // The same window returning: set_minimized(false), or activate()
        // being asked for a window that was hidden. Reported BEFORE the
        // Activated that an activating restore then produces, because the
        // window is on the desktop again before it is the one in front.
        // Restoring on its own does NOT activate — putting a window back
        // and choosing which window the reader is working in are two
        // requests, and every path a reader can take to make both of them
        // at once (a bar entry, a number, a menu) comes through activate().
        Restored,
    };

    using WindowObserverId = std::uint64_t;
    using WindowObserver = std::function<void(WindowChange, Window&)>;

    // Learn of windows opening, closing, being renamed, and changing
    // activation.
    //
    // A view that LISTS windows — a switcher bar, a taskbar, a navigator pane
    // — cannot get by on reading windows() whenever it happens to draw,
    // because nothing invalidates such a view when a window it does not
    // contain opens or is renamed. Its list would simply stop matching the
    // desktop until something unrelated forced a repaint. This is the signal
    // that tells it, and it exists so that no listing view has to sweep the
    // window set every frame to find out.
    //
    // An observer is a NOTIFICATION, not a place to change the window set: it
    // runs while this Desktop is part-way through the operation it is
    // reporting, and detaching or destroying a window from inside one leaves
    // that operation holding a pointer to something gone. Re-read, repaint,
    // record — and do anything that adds, removes or activates a window from
    // Application::post() instead. Unsubscribing from inside one IS safe.
    //
    // Multicast (subscribe/unsubscribe with an id, the shape EditorDocument
    // already uses) rather than one assignable callback: a Desktop routinely
    // carries a switcher bar AND an application's own bookkeeping, and a
    // single slot means whichever attaches second silently displaces the
    // first.
    WindowObserverId subscribe_window_change(WindowObserver observer);
    // The same, bound to `owner_lifetime`: once that token expires the
    // observer is dropped instead of called, and nothing has to cancel it.
    // The form a docked view needs — such a view is destroyed as part of the
    // Desktop that owns it, so a destructor cancelling by hand would be
    // reaching into a Desktop whose own members are already gone.
    WindowObserverId subscribe_window_change(WindowObserver observer,
                                             std::weak_ptr<void> owner_lifetime);
    void unsubscribe_window_change(WindowObserverId observer) noexcept;

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

    // --- Where put-away windows go (D-064) ---------------------------
    //
    // `Parked` by default, so an application that has thought about none of
    // this still cannot lose a window: the `_` control leads somewhere
    // visible. A host with its own listing sets `HostListed` and gets the
    // bare hiding it already builds on; a host with no use for minimizing at
    // all sets `Disabled` and the control is not offered.
    //
    // Changing this settles the windows that are already here: switching
    // away from `Parked` takes the stubs down, switching to it puts one up
    // for every window that is currently minimized, and `Disabled` restores
    // them — see the enum.
    void set_minimized_window_placement(MinimizedWindowPlacement placement);
    MinimizedWindowPlacement minimized_window_placement() const noexcept {
        return minimized_placement_;
    }

    // The stubs on screen, in the order their windows were put away. Empty
    // unless the placement is `Parked`. A host reads this to lay something
    // out around them; a test reads it to name what a reader can see.
    const std::vector<MinimizedWindowStub*>& parked_windows() const noexcept {
        return parked_stubs_;
    }

    // --- The minimize flight (U4-k) ---------------------------------
    //
    // Where a minimized window goes, so that hiding one can be SHOWN rather
    // than merely done: the frame shrinks and flies to the row it will live
    // in, and back out of it on restore.
    //
    // A provider rather than a fixed destination, because this Desktop does
    // not know what lists its windows. A taskbar knows where a window's
    // entry is; a window-list dialog, a session picker or an application with
    // no listing at all each answer differently, and one of the answers is
    // `nullopt` — "nowhere to fly to", which is also what every application
    // that never installs a provider says. **No provider, no flight**, so
    // nothing changes for a host that has not asked for one.
    //
    // The rect is in this Desktop's own coordinates (the same frame
    // `Window::bounds()` is in), and is asked for at the moment the flight
    // starts rather than remembered — a taskbar row moves as its neighbours
    // come and go, and a remembered rectangle is how an effect flies to
    // where a button used to be.
    void set_minimize_target_provider(std::function<std::optional<Rect>(Window&)> provider);

    // How long the flight lasts. **Zero disables it** — which is the whole
    // of "animations off" for this effect, and is why no call site carries a
    // branch for it (ui::Animation ends a zero-duration run immediately,
    // having drawn nothing).
    void set_minimize_animation_duration(std::int64_t nanos) noexcept;
    std::int64_t minimize_animation_duration() const noexcept {
        return minimize_animation_nanos_;
    }

    // Ends any flight now. The desktop is already in its end state — that is
    // the invariant the whole effect is built on (ui::Animation, D-060) — so
    // this only stops a decoration being drawn; there is nothing to resolve.
    //
    // Exposed because "interruptible" is the host's judgement, not this
    // widget's: an application that wants a keystroke to cut the flight short
    // calls this from wherever it sees keystrokes. A Desktop cannot do it for
    // them, since a key pressed into a focused window never reaches here.
    void finish_minimize_animation();

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

    // --- A world larger than the view of it (U7-a) --------------------
    //
    // A `Desktop` is its own viewport by default: windows live in its bounds
    // and there is nowhere else for them to be. An EXTENT makes the world it
    // contains bigger than the hole it is seen through, and a PAN says which
    // part of that world the hole is over.
    //
    // Two vocabularies, deliberately, because every call site belongs to one
    // of them and the bugs live where they are confused:
    //
    //   * WORLD — `extent()`, `content_area()`, every window's `bounds()`,
    //     the tilings, the cascade, `filled_tile_fractions()` and the
    //     remembered arrangement (D-058). An arrangement is a statement about
    //     the world and does not change because somebody looked elsewhere.
    //   * VIEW — this desktop's own `bounds()`, where things are drawn, and
    //     where a click lands. That is what `pan()` moves.
    //
    // The extent defaults to the desktop's own size and follows it on resize
    // while nobody has set one, so a `Desktop` that never calls this behaves
    // exactly as it always has — which is the regression bar for the whole
    // feature: with a viewport equal to the extent, every existing test must
    // be unable to tell the difference.
    //
    // Docked chrome does NOT pan. A menu bar that scrolled away from the top
    // of the screen would not be a menu bar, and `content_area()` is the seam
    // where that is decided: the docks belong to the view, the content to the
    // world.
    void set_extent(Size extent);
    Size extent() const noexcept { return extent_; }
    // Where the view sits over the world, clamped so it can never show a
    // region the world does not have. A pan of (0,0) is the top-left corner,
    // which is where every desktop that has never panned already is.
    void set_pan(Point pan);
    Point pan() const noexcept { return pan_; }
    // Moves the view the least distance that brings `world` fully into it —
    // nothing if it is already there. What a host calls when a reader focuses
    // a window that is off-screen: they meant to see it.
    void pan_to_show(Rect world);

    void on_resized() override;

    // Puts every window where the pan says it should be drawn. Called
    // whenever the pan or the extent moves; docked chrome is skipped, which
    // is the whole of "the docks belong to the view".
    void apply_pan_to_windows();
    // The pan, held inside what the world actually has to show.
    Point clamped_pan(Point wanted) const noexcept;
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
    //
    // A MINIMIZED window is restored first. Activation means "this is the
    // window the reader is working in", and a hidden window cannot be
    // that; the alternative — refusing, or activating something invisible
    // — would put every caller in charge of a rule that has only one
    // right answer. It is also what keeps the invariant in one place:
    // select_by_number, a click on a switcher-bar entry and a host's own
    // activate() all come through here, so no path can leave activation
    // pointing at something nobody can see.
    void activate(Window* window);

    // Classic "next/previous window" commands: steps activation to the
    // adjacent window in windows()' stable insertion order, wrapping
    // around. No-op with 0 or 1 windows.
    //
    // Minimized windows are STEPPED OVER rather than restored: cycling
    // moves between the windows on the desktop, and a reader pressing it
    // repeatedly is looking for one of those, not asking to unpack the
    // ones they parked. (Naming a window — by number, or by its entry in
    // a bar — is the other kind of request, and that one does restore.)
    // A full lap that finds nothing else visible leaves activation where
    // it is.
    void activate_next();
    void activate_previous();

    // 1-based index into windows()' insertion order (matching the
    // classic "Alt+1".."Alt+9" window-select convention). Out-of-range
    // n is a harmless no-op, not an error — a stale keybinding from a
    // since-closed window must not crash the application. The numbering
    // counts minimized windows, because it counts windows() and that is
    // the list a reader is looking at; naming a hidden one restores it,
    // per activate().
    void select_by_number(int n);

    // Arranges every window to fill an equal vertical slice of the
    // desktop's content area, left to right, in windows()' insertion
    // order — the same arrangement tile_vertically() names. A no-op with
    // zero windows.
    //
    // "Every window" means every window ON the desktop: a hidden one —
    // minimized to a switcher bar, or hidden by its own application —
    // gets no band, and the division is by the count of the rest. Giving
    // it one would divide the area by a window nobody can see and leave
    // the visible ones a gap exactly where its band would have been,
    // which costs the arrangement its filled_tile_fractions() verdict as
    // surely as any other gap.
    void tile();

    // The three explicitly named tilings, in the sense a desktop
    // taskbar's own "Tile Windows" commands use — spelled out here
    // because the two axis words are used inconsistently across
    // platforms and a caller deserves to know which arrangement it is
    // asking for:
    //
    //   tile_horizontally() — full-WIDTH bands stacked top to bottom.
    //   tile_vertically()   — full-HEIGHT bands side by side.
    //   tile_grid()         — a near-square grid: ceil(sqrt(n)) columns,
    //                         filled row by row, the last row holding
    //                         whatever is left and stretching across the
    //                         full width rather than stopping short.
    //
    // tile_vertically() produces exactly the arrangement tile() has
    // always produced; the two are not merged because kTile is a
    // standard command applications already bind, and renaming or
    // re-pointing it would change behavior under callers that never
    // asked for a change.
    //
    // All three fill content_area() exactly: the last band, the last row
    // and the last window in each row absorb whatever the integer
    // division left over, so no gap row or column is handed back to the
    // desktop. That is not cosmetic — a one-cell gap, or a short last
    // grid row, makes the arrangement fail filled_tile_fractions()' own
    // coverage test, which is what would silently cost the arrangement
    // its proportional-restore tag and its shadow suppression.
    //
    // No-ops with zero windows.
    void tile_horizontally();
    void tile_vertically();
    void tile_grid();

    // Arranges every window at the same size (roughly 2/3 of the
    // desktop, clamped to fit), offset diagonally so each remains
    // partially visible, topmost window last (frontmost).
    void cascade();

    // One window's share of a filled tiling, as a fraction of
    // content_area() — see filled_tile_fractions().
    struct TileFraction {
        Window* window = nullptr;
        // Offsets are measured from content_area()'s own origin, so all
        // four numbers lie in [0, 1] and x + width <= 1, y + height <= 1.
        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 0.0;
    };

    // The current arrangement expressed proportionally, IF it is a
    // filled, non-overlapping tiling of content_area() right now; an
    // empty vector otherwise. Entries are in windows()' stable insertion
    // order, the same order every other Desktop enumeration walks.
    //
    // What this is for: a proportional restore. A 50/50 split reported
    // as {0, 0, 0.5, 1} and {0.5, 0, 0.5, 1} is still a 50/50 split when
    // it is laid back down on a desktop of a different size, which
    // replaying the absolute cell rects would not be.
    //
    // The test is GEOMETRIC — every participating window is inside the
    // content area, no two overlap, and their areas sum to the content
    // area's — never a flag the tile commands set. A flag would keep
    // reporting "filled" after a reader dragged one window off the grid,
    // handing a restore the wrong fractions to lay down; and it would
    // never report a reader who arranged the same tiling by hand. Both
    // cases are answered correctly by measuring what is actually there.
    //
    // A filled tiling SURVIVES a resize of the desktop, in both directions:
    // the arrangement a reader stated is remembered and re-divided
    // proportionally whenever the content area changes, so the answer here
    // after a terminal grows or shrinks is the same arrangement at the new
    // size rather than an empty vector. It is the proportions that survive,
    // not the cell sizes — a band of a 23-row desktop is one row shorter
    // than the same band of a 24-row one — and returning to an area the
    // arrangement was already laid out in returns exactly the cells it had
    // there. The arrangement is lost, and this goes empty for good, in
    // exactly three cases: one of its OWN windows leaves the desktop, a
    // reader moves or resizes one of them out of its cell, or the content
    // area becomes too small to give every cell at least one row and column.
    // A window merely opening over the arrangement — a dialog, a new document
    // — empties this for as long as it covers part of the grid, and the
    // arrangement is reported again once it closes.
    //
    // Two kinds of window are deliberately not participants, because
    // neither is part of the arrangement: one that is not on the desktop
    // — hidden by its application, or minimized to a switcher bar — and a
    // fixed-size or modal window, which is ABOVE whatever the
    // arrangement covers, and including it would both break the tiling's
    // coverage test and put the dialog into a restore that has no place
    // for it. The first test is the same one the tile commands ask before
    // handing out a band, so what is measured here and what was arranged
    // there cannot drift apart.
    std::vector<TileFraction> filled_tile_fractions() const;

    // --- Maximize-follows-on-open (opt-in, default off) ------------
    //
    // When enabled, a window added while the currently active window is
    // maximized opens maximized too, instead of at whatever bounds it
    // was constructed with. Opt-in rather than default because it is a
    // choice about an application's own window discipline: an
    // application whose windows are dialogs and palettes wants a new
    // window at its own size even when a document happens to be
    // maximized, and every ckVision application built before this
    // existed is one of those by construction.
    //
    // It reuses Window's zoom (toggle_zoom/zoomed()) rather than
    // introducing a second notion of "maximized", so the zoom control on
    // the new window's frame restores it exactly like any other zoomed
    // window. A window whose bounds are still empty when it is added is
    // given its natural centered placement FIRST and zoomed after, so
    // the geometry the reader restores to is a real rectangle rather
    // than the 0x0 one an unplaced window would otherwise record.
    //
    // Standard-dialog presentation (present_modeless/present_modal/
    // exec_modal) is deliberately outside the policy: a dialog has
    // already been sized to what it has to say, and a message box blown
    // up to the full desktop because the document behind it was
    // maximized is not what any host is asking for here.
    void set_maximize_follows_active(bool enabled) noexcept {
        maximize_follows_active_ = enabled;
    }
    bool maximize_follows_active() const noexcept { return maximize_follows_active_; }

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
    // gotten right (the architecture §5 "moving a window costs
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
    // A window plus the means to tell whether it is still THE window that
    // was recorded. Every arrangement pass (tile, cascade, resize reflow)
    // calls set_bounds in a loop, set_bounds runs on_resized, and an
    // application's on_resized may detach and destroy any window — its own
    // or a sibling. A raw pointer alone cannot survive that: a destroyed
    // window can be replaced by a freshly allocated one at the same
    // address. The weak token settles identity; the windows_ lookup
    // settles ownership.
    struct LiveWindow {
        Window* window = nullptr;
        std::weak_ptr<void> liveness;
    };
    std::vector<LiveWindow> live_windows() const;
    // The same, narrowed to the windows currently ON the desktop — what
    // every arrangement command lays out.
    std::vector<LiveWindow> live_shown_windows() const;
    bool still_owned(const LiveWindow& handle) const;
    // Whether `window` is on the desktop at all: not minimized to a switcher
    // bar, and not hidden by its own application. One question, asked by
    // every arrangement, by the cycle step and by the filled-tiling query,
    // so that no two of them can hold different opinions about which windows
    // are there to be arranged.
    static bool shown(const Window& window) noexcept;
    // Gives activation to the topmost window that is shown, or leaves this
    // desktop with none if every window is hidden. The successor rule, in
    // one place: a window leaving — by closing or by being minimized — must
    // not hand activation to something the reader cannot see.
    void activate_topmost_shown();
    // Steps activation one place along windows()' insertion order, skipping
    // hidden windows — see activate_next/activate_previous.
    void activate_step(bool forward);
    // A window minimizing or restoring, reported by the window itself (the
    // binding in attach_window). Hands activation on when the window that
    // just went away was the active one, then reports the change to whoever
    // lists windows.
    void window_minimize_changed(Window& window);
    // Starts the flight for a window that has JUST changed its minimized
    // state — the state is already applied when this runs, which is what
    // makes the effect skippable at no risk (Window::set_minimized notifies
    // after set_visible, deliberately).
    void begin_minimize_flight(Window& window);
    // The parking row: one stub per put-away window, laid left to right
    // along the bottom of the desktop and wrapped upwards when the row runs
    // out. Called whenever the set changes or the desktop is resized, and
    // it is the ONLY thing that positions a stub — so there is one answer
    // to where a parked window is, not one per caller.
    void layout_parked_stubs();
    // Puts `window`'s stub up / takes it down. Both are no-ops unless the
    // placement is `Parked`, and both are safe to call for a window that is
    // already in the state they describe.
    void park_window(Window& window);
    void unpark_window(Window& window);
    MinimizedWindowStub* stub_for(const Window& window) const noexcept;
    // Takes the minimize control off a window and records that this Desktop
    // is the one who did, so `Disabled` can be switched back off without
    // handing the control to a window that never had it.
    void gate_minimize(Window& window);
    // Reports `change` to every live observer — see subscribe_window_change.
    // Iterates a copy of the list, so an observer may unsubscribe itself (or
    // another) from inside the call without invalidating the walk.
    void notify_window_change(WindowChange change, Window& window);
    struct WindowObserverEntry {
        WindowObserverId id = 0;
        WindowObserver observer;
        std::weak_ptr<void> owner_lifetime;
        // Whether owner_lifetime means anything. An unbound subscription
        // carries an empty weak_ptr, and an empty weak_ptr is expired — so
        // without this flag the caller who asked to manage its own
        // subscription would be dropped before its first notification.
        bool lifetime_bound = false;
    };
    // Everything add_window does except the maximize-follows-on-open
    // policy: registration, the zoom/gesture relationships, move bounds
    // and activation. present_modeless/present_modal/exec_modal attach
    // through THIS rather than through add_window, because a dialog has
    // already been given the size it means to open at (see
    // place_unpositioned_window) and a message box that opened maximized
    // because a document behind it happened to be maximized is not a
    // policy any host asked for.
    Window* attach_window(std::unique_ptr<Window> window);
    // Which way the bands run in a tiling.
    enum class BandAxis {
        Rows,     // full-width bands stacked top to bottom (Tile Horizontally)
        Columns,  // full-height bands side by side (Tile Vertically, and tile())
    };
    void tile_in_bands(BandAxis axis);
    // Whether `window` is part of the arrangement filled_tile_fractions()
    // measures — see that function for why a dialog is not.
    bool tiling_participant(const Window& window) const;
    // The participants that exactly partition `area` right now — every one
    // of them inside it, no two overlapping, their areas summing to its own
    // — in windows()' insertion order; empty when what is on the desktop is
    // not a partition of that rect.
    //
    // Parameterised on the rect because two callers need the same answer
    // about two different rects: filled_tile_fractions() asks about
    // content_area() as it is now, and the arrangement reference below asks
    // whether what a reader just made is a partition of the area they made
    // it in. One test rather than two, so a resize can never re-lay an
    // arrangement the filled-tiling verdict would deny, nor deny one it
    // re-laid.
    std::vector<Window*> partition_of(Rect area) const;
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
    // would (the architecture §5 "application quit sweeps all windows
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
    // kMinimize's default handler: puts the ACTIVE window away, the same
    // call its own `_` control makes — a no-op with no active window, and
    // a no-op for one whose minimizable() says it is not a window a reader
    // parks. That gate is the whole point of routing through this rather
    // than calling set_minimized(true) at the command site: a modal, an
    // alert or a fixed-size About box draws no `_` control, and a menu item
    // that hid one anyway would leave an application answering nothing.
    void minimize_active_window();
    // Maximize-follows-on-open, applied to a window that has just been
    // added — see set_maximize_follows_active(). Whether the PREVIOUSLY
    // active window was maximized is decided by add_window before
    // activation moves, not here: asked afterwards the question could only
    // ever be about the new window and would answer nothing.
    void open_maximized(Window& window);
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

    // --- The arrangement a reader stated, replayed across a resize -----
    //
    // A tiling is a statement about PROPORTIONS, and a content area that
    // changes size has to be re-divided by them. Clamping each window
    // independently — which is all a resize used to do — cannot: it pulls a
    // band up without shortening it, so two neighbours that shared a seam
    // start overlapping on it, and because clamping only ever shrinks,
    // growing back never repairs the damage. One row off the terminal and
    // the arrangement was gone for good, taking filled_tile_fractions()'
    // verdict with it, and with that the shadow suppression and every host's
    // proportional-restore tag.
    //
    // So the arrangement is REMEMBERED: the cells as stated, and the content
    // area they were stated in. Every reflow maps from that ORIGINAL, never
    // from the previous reflow's output. That is the whole design and it is
    // not an optimization: the rounding map is not an involution, so a
    // 24 -> 23 -> 24 round trip re-derived at each step hands two neighbours
    // each other's heights and keeps them. Mapped from the original,
    // returning to the original area is the identity by construction rather
    // than by luck.
    struct TilingCell {
        Window* window = nullptr;
        // The cell as stated, in the stated area's own coordinates.
        Rect cell;
        // What the last mapping actually wrote. A window is validated
        // against THIS and never against `cell`: a reader who drags or
        // resizes a window is not asking to be snapped back to where a tile
        // command once put it, and comparing against where this Desktop last
        // put the window is the only way to notice that they moved it.
        Rect placed;
    };
    struct TilingReference {
        Rect area;
        std::vector<TilingCell> cells;  // insertion order, as partition_of reports
    };
    // Records whatever is on the desktop right now as the arrangement to
    // replay, or forgets the recorded one when there is no arrangement to
    // record. Called at the end of every command that lays windows out, and
    // when a reader's own move/resize gesture ends.
    //
    // A participant that is zoomed, or that carries a grow policy of its
    // own, vetoes the whole capture: such a window already has an authority
    // that sizes it on every resize (refresh_zoom_area, fill), and a second
    // one writing raw cells over the top would take Window::clamp_size — and
    // with it min_size()/max_size() — out of the loop for exactly the window
    // whose whole policy is about how it is sized. Note that this is not a
    // corner case: a single window covering the area IS a one-cell
    // partition, so an ordinary maximized window would be captured here
    // without the veto.
    void capture_tiling_reference(Rect area);
    // Whether every remembered window is still exactly where the last
    // mapping put it. Anything else means somebody other than this reflow
    // moved it, and a remembered arrangement that no longer describes the
    // desktop is not one to replay.
    bool tiling_reference_is_intact() const;
    // Re-lays the remembered arrangement into `area`, returning the windows
    // it placed — empty when there was nothing to replay, in which case the
    // ordinary per-window clamp applies to everything exactly as before.
    //
    // Edges are mapped, not rectangles: a seam shared by two neighbours is
    // ONE coordinate, so it maps to one number and the partition stays exact
    // by construction, rather than by two independently rounded rectangles
    // happening to agree along it. If any cell would map to nothing — the
    // floor is a content extent below the number of bands across it, not
    // twice that — nothing is re-laid and the reference is forgotten: the
    // arrangement is honestly lost, and the ordinary clamp resumes, rather
    // than the reader being handed a row of empty windows.
    std::vector<Window*> reflow_tiling_reference(Rect area);
    // A reader's own move/resize gesture has ended. If it left a tiling this
    // Desktop is not already remembering, that is a stated arrangement and
    // is remembered from here — the hand-made half of what the tile commands
    // get for free, and the one moment at which a bounds change is known to
    // have come from the reader rather than from a host's own layout code.
    void note_reader_arrangement();

    ui::RoleId background_role_ = ui::kInvalidRole;
    std::vector<Window*> windows_;  // parallel to View::children_, same order
    std::vector<WindowObserverEntry> window_observers_;
    WindowObserverId next_window_observer_id_ = 1;
    Window* active_ = nullptr;
    int active_gestures_ = 0;
    std::vector<ui::View*> popups_;
    // The minimize flight: where a host says hidden windows go, how long the
    // effect lasts, the run itself, and the decoration it draws.
    //
    // The decoration is a popup like any other — topmost, its own compositor
    // layer — because that is already this Desktop's word for "above every
    // window". It is added when a flight starts and removed when it ends, so
    // a desktop with nothing in flight carries no extra layer at all.
    std::function<std::optional<Rect>(Window&)> minimize_target_provider_;
    std::int64_t minimize_animation_nanos_ = kDefaultMinimizeAnimationNanos;
    ui::Animation minimize_animation_;
    ui::View* minimize_flight_ = nullptr;
    MinimizedWindowPlacement minimized_placement_ = MinimizedWindowPlacement::Parked;
    // Parking order, which is minimize order — not window order. A reader
    // who puts three windows away reads them back in the order they left.
    std::vector<MinimizedWindowStub*> parked_stubs_;
    std::vector<std::pair<Window*, std::weak_ptr<void>>> minimize_gated_;
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
    bool maximize_follows_active_ = false;
    // Zero height or width means "no extent of my own": the world is whatever
    // this desktop's bounds are, which is what every consumer written before
    // U7-a assumes and continues to get.
    Size extent_{0, 0};
    Point pan_{0, 0};
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
    // The arrangement to replay across a resize — see
    // capture_tiling_reference(). Empty whenever there is nothing to replay.
    // A cell's Window* cannot dangle: the only way a window leaves this
    // Desktop is remove_window(), which forgets the arrangement whenever what
    // is leaving is one of these.
    std::optional<TilingReference> tiling_reference_;
    // Capability held weakly by every attached Window's private zoom
    // target.  It makes destruction and detachment safe even if a
    // callback is accidentally invoked after the Window leaves us.
    std::shared_ptr<WindowRelationshipToken> window_relationship_ =
        std::make_shared<WindowRelationshipToken>();
};

}  // namespace ckv::widgets
