// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/desktop.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

#include "cvision/core/assert.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/terminal_report_dialog.hpp"
#include "cvision/widgets/window_list_dialog.hpp"

namespace ckv::widgets {
namespace {

// The ghost of a window frame, in flight between the window and the row it
// lives in. A view exactly the size of the rectangle it is describing, so it
// needs no transparency: every cell it owns is one it draws.
//
// It answers no input and takes no focus. That is not a detail — the desktop
// underneath it is ALREADY in its end state, so a decoration that swallowed a
// click would be stealing it from the window the reader can see, on the way
// to a state that had already arrived.
class MinimizeFlight final : public ui::View {
public:
    MinimizeFlight() = default;

    void draw(scene::Painter& painter) override {
        if (bounds().width <= 0 || bounds().height <= 0) return;
        const Style style = context().theme->resolve(role_);
        const Rect local{0, 0, bounds().width, bounds().height};
        // Under three cells in either direction there is no box to draw —
        // two borders and nothing between them — so the last frames of a
        // shrinking flight are a filled block rather than a smaller and
        // smaller mis-drawn frame.
        if (local.width < 3 || local.height < 3) {
            painter.fill(local, Cell::from_grapheme("░", style));
            return;
        }
        painter.draw_box(local, scene::LineStyle::Single, style);
    }

    void on_attached() override {
        // The window frame's own role: this is a window's ghost, so a theme
        // that retinted frames retints the flight with them rather than
        // leaving one of the two behind.
        if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.window.frame.active");
    }

    bool on_mouse(const MouseEvent&) override { return false; }
    std::optional<PointerShape> pointer_shape_at(Point) const override { return std::nullopt; }

private:
    ui::RoleId role_ = ui::kInvalidRole;
};

// One edge of the flight, at `progress`. Rounded rather than truncated so
// that the last frame before the end sits ON the destination rather than one
// cell short of it, and so the first frame has already left the source.
int flight_edge(int from, int to, double progress) {
    return from + static_cast<int>(std::llround(static_cast<double>(to - from) * progress));
}

Rect flight_rect(const Rect& from, const Rect& to, double progress) {
    return Rect{flight_edge(from.x, to.x, progress), flight_edge(from.y, to.y, progress),
                std::max(1, flight_edge(from.width, to.width, progress)),
                std::max(1, flight_edge(from.height, to.height, progress))};
}

}  // namespace


namespace {

// The width an auto-sized window opens at: the one it asks for, widened
// only as far as widening actually buys it.
//
// A window asks for the width its content reads best at, which is not the
// same as the width its content fits in — wrapping text asks for a
// paragraph measure however much of it there is. Widening is the answer
// the window has when that leaves it taller than the desktop, because text
// given more columns needs fewer rows. What it aims at is the desktop's
// height, or — when not even the full width reaches that — the shortest
// the window can be made, since content that overflows whatever happens
// should at least overflow by as little as possible. Either way the
// SMALLEST width that reaches the target is the one taken, so a window
// never takes columns that buy it nothing: content whose height does not
// depend on its width (a picture, a fixed table) opens exactly as wide as
// it asked to, overflow or not.
//
// The bisection holds because wrapping makes height_for_width
// non-increasing in width: no view answers a wider question with more
// rows.
int fitting_width(const Window& window, int preferred, Rect available) {
    const int widest = std::max(0, available.width);
    const int wanted = std::clamp(preferred, 0, widest);
    const int target = std::max(available.height, window.height_for_width(widest));
    if (window.height_for_width(wanted) <= target) return wanted;

    int narrow = wanted;
    int wide = widest;
    while (narrow + 1 < wide) {
        const int candidate = narrow + (wide - narrow) / 2;
        if (window.height_for_width(candidate) <= target)
            wide = candidate;
        else
            narrow = candidate;
    }
    return wide;
}

void place_unpositioned_window(Window& window, Rect available) {
    const Rect current = window.bounds();
    if (current.width > 0 && current.height > 0) return;

    const ui::SizeHint horizontal = window.horizontal_size_hint();
    const int width = fitting_width(window, horizontal.preferred, available);
    const int measured_height = window.height_for_width(width);
    const ui::SizeHint vertical = window.vertical_size_hint();
    const int height = std::clamp(std::max(vertical.preferred, measured_height), 0, std::max(0, available.height));
    const int x = available.x + std::max(0, (available.width - width) / 2);
    const int y = available.y + std::max(0, (available.height - height) / 2);
    window.set_bounds(Rect{x, y, width, height});
}

// One coordinate of a remembered arrangement, re-expressed in a content area
// of a different size: `edge` lies in [origin, origin + extent] and comes
// back somewhere in [new_origin, new_origin + new_extent], proportionally and
// rounded to nearest.
//
// Two properties are load-bearing, and both are exact rather than
// approximate. Either end of the span maps to the corresponding end of the
// new one, so an arrangement that filled its area still fills the new one.
// And the two spans being identical makes this the identity — which is what
// makes a resize round trip give a reader back the arrangement they had,
// instead of a rounded neighbour of it. (Rounding to nearest is not an
// involution: re-deriving the reference from each intermediate size would
// walk a seam a row at a time and never walk it back. That is why callers map
// from the ORIGINAL cells every time rather than from the last result.)
//
// int64 arithmetic because a coordinate times an extent is a product of two
// terminal dimensions; nothing in a cell grid comes near overflowing it, but
// the multiplication should not be the place that decides.
int map_edge(int edge, int origin, int extent, int new_origin, int new_extent) noexcept {
    CKV_ASSERT(extent > 0);
    const std::int64_t offset = edge - origin;
    const std::int64_t scaled =
        (offset * new_extent + extent / 2) / extent;  // + extent/2: round to nearest
    return new_origin + static_cast<int>(scaled);
}

}  // namespace

Desktop::Desktop(Rect bounds) : View(bounds) {
    last_content_area_ = content_area();
}

Desktop::~Desktop() {
    // Base View destruction happens only after this destructor body.  Clear
    // every Window's Desktop-only callback while both objects are still
    // alive, before a detach observer or retained Window can use it.
    for (Window* window : windows_) {
        window->clear_desktop_zoom_target();
        window->clear_gesture_observer();
        window->clear_title_observer();
        window->clear_minimize_observer();
    }
    window_relationship_.reset();
    // Nothing may be reported from here on. A docked observer is one of the
    // children View's own destructor is about to destroy, and a notification
    // reaching it would be a call into a Desktop whose members no longer
    // exist.
    window_observers_.clear();

    // Children are torn down HERE, not by the base View destructor that runs
    // after this body. By then popups_, popup_backings_ and windows_ are gone,
    // and a child that talks to its Desktop on the way out reads members that
    // no longer exist: ~DropdownMenu dismisses itself into remove_popup(), and
    // ~MenuBar closes its dropdown the same way. Declaration order cannot fix
    // this the way Application orders commands_ and modal_stack_ before root_
    // — children_ lives in the BASE, and a base subobject is always destroyed
    // after every derived member. remove_child() is the per-kind teardown
    // (window, popup, dock); discarding the owner it hands back destroys the
    // child while everything it might call into is still alive.
    while (!children().empty()) {
        ui::View* const child = children().back().get();
        std::unique_ptr<ui::View> owned = remove_child(child);
        // A specialised path that declined the child must still leave the list
        // shorter, or this loop would not terminate.
        if (owned == nullptr) owned = ui::View::remove_child(child);
    }

    if (app_ != nullptr)
        for (ui::CommandId id : installed_default_handlers_) app_->commands().set_handler(id, nullptr);
}

Desktop::WindowObserverId Desktop::subscribe_window_change(WindowObserver observer) {
    const WindowObserverId identifier = next_window_observer_id_++;
    window_observers_.push_back(WindowObserverEntry{identifier, std::move(observer), {}, false});
    return identifier;
}

Desktop::WindowObserverId Desktop::subscribe_window_change(WindowObserver observer,
                                                           std::weak_ptr<void> owner_lifetime) {
    const WindowObserverId identifier = next_window_observer_id_++;
    window_observers_.push_back(
        WindowObserverEntry{identifier, std::move(observer), std::move(owner_lifetime), true});
    return identifier;
}

void Desktop::unsubscribe_window_change(WindowObserverId observer) noexcept {
    window_observers_.erase(std::remove_if(window_observers_.begin(), window_observers_.end(),
                                            [observer](const WindowObserverEntry& candidate) {
                                                return candidate.id == observer;
                                            }),
                            window_observers_.end());
}

void Desktop::notify_window_change(WindowChange change, Window& window) {
    if (window_observers_.empty()) return;
    // An expired owner is dropped rather than called, which is the whole
    // reason the lifetime-bound form needs no cancelling.
    window_observers_.erase(std::remove_if(window_observers_.begin(), window_observers_.end(),
                                            [](const WindowObserverEntry& candidate) {
                                                return candidate.lifetime_bound &&
                                                       candidate.owner_lifetime.expired();
                                            }),
                            window_observers_.end());
    // A copy, so an observer may unsubscribe from inside its own call without
    // invalidating the walk. The observers themselves must not change the
    // window set — see subscribe_window_change — so this Desktop's own state
    // is stable across the loop.
    const std::vector<WindowObserverEntry> observers = window_observers_;
    for (const WindowObserverEntry& entry : observers) {
        if (entry.lifetime_bound && entry.owner_lifetime.expired()) continue;
        if (entry.observer) entry.observer(change, window);
    }
}

void Desktop::install_default_handler(ui::CommandId id, std::function<void()> handler) {
    if (app_->commands().has_handler(id)) return;
    app_->commands().set_handler(id, std::move(handler));
    installed_default_handlers_.push_back(id);
}

void Desktop::on_attached() {
    if (background_role_ == ui::kInvalidRole)
        background_role_ = context().roles->find("ckv.desktop.background");
    app_ = context().app;
    if (app_ == nullptr) return;
    const ui::StandardCommands& standard = app_->commands().standard();
    install_default_handler(standard.close, [this] { close_active_window(); });
    install_default_handler(standard.quit, [this] { quit_sweep(); });
    install_default_handler(standard.zoom, [this] { zoom_active_window(); });
    install_default_handler(standard.minimize, [this] { minimize_active_window(); });
    install_default_handler(standard.next_window, [this] { activate_next(); });
    install_default_handler(standard.previous_window, [this] { activate_previous(); });
    install_default_handler(standard.tile, [this] { tile(); });
    install_default_handler(standard.tile_horizontally, [this] { tile_horizontally(); });
    install_default_handler(standard.tile_vertically, [this] { tile_vertically(); });
    install_default_handler(standard.tile_grid, [this] { tile_grid(); });
    install_default_handler(standard.cascade, [this] { cascade(); });
    install_default_handler(standard.window_list, [this] { show_window_list(); });
    install_default_handler(standard.terminal_report, [this] { show_terminal_report(); });
}

void Desktop::show_window_list() {
    if (app_ == nullptr) return;
    // One at a time. The command stays reachable by its key while the list is
    // up, and a second modal list over the first would hide the very thing it
    // was asked to show.
    const std::shared_ptr<StandardDialogState> state = window_list_state_;
    if (state->open) return;
    state->open = true;
    // Held by the completion handler rather than by this Desktop: the dialog
    // is a child of ours, so our own destruction detaches it and completes the
    // presentation — at a point where `this` is already partly gone.
    auto presentation = std::make_shared<WindowListDialogPresentation>(
        present_window_list_dialog(*this, *app_, ui::intern_standard_roles(app_->roles())));
    presentation->set_completion_handler(
        [state, presentation](WindowListDialogResult) { state->open = false; });
}

void Desktop::show_terminal_report() {
    if (app_ == nullptr) return;
    // One at a time, for the same reason as the window list: a second modal
    // report over the first would hide the evidence it was asked to show.
    const std::shared_ptr<StandardDialogState> state = terminal_report_state_;
    if (state->open) return;
    state->open = true;
    // Held by the completion handler rather than by this Desktop — see
    // show_window_list above.
    auto presentation = std::make_shared<TerminalReportDialogPresentation>(
        present_terminal_report_dialog(*this, *app_, ui::intern_standard_roles(app_->roles())));
    presentation->set_completion_handler(
        [state, presentation](TerminalReportDialogResult) { state->open = false; });
}

void Desktop::on_child_size_hint_changed(ui::View& child) {
    if (&child == top_dock_ || &child == bottom_dock_) on_resized();
}

void Desktop::close_active_window() {
    if (active_ != nullptr) active_->close();
}

void Desktop::quit_sweep() {
    // close() enters application-defined code. A callback may execute kQuit
    // again; restarting here would re-close the current window recursively
    // and could let an inner sweep request shutdown before the outer request
    // reaches a later veto. One active sweep is the complete user request.
    const std::shared_ptr<QuitSweepState> sweep_state = quit_sweep_state_;
    if (sweep_state->in_progress) return;
    sweep_state->in_progress = true;
    struct ResetSweepGuard {
        std::shared_ptr<QuitSweepState> state;
        ~ResetSweepGuard() { state->in_progress = false; }
    } reset_sweep{sweep_state};

    // The close callback may detach and destroy this Desktop itself. Retain a
    // liveness observation separately from the object so no following loop
    // iteration or shutdown request dereferences a departed owner.
    const std::weak_ptr<void> desktop_liveness = lifetime_token();

    // close() is user-extensible: its callback can synchronously detach and
    // destroy this Window (or another one). Iterate a stable front-to-back
    // snapshot rather than windows_ itself. A raw pointer alone is not a
    // sufficient identity: a callback can destroy an unvisited Window and
    // allocate a new one at that exact address. Pair it with View's
    // per-instance liveness token, then confirm the original still remains
    // attached before touching it. Windows opened by a close callback were
    // not present when the quit request began and are left for the host's
    // imminent shutdown rather than being unexpectedly swept as re-entrant
    // work.
    struct WindowAtStart {
        Window* window = nullptr;
        std::weak_ptr<void> liveness;
    };
    std::vector<WindowAtStart> windows_at_start;
    windows_at_start.reserve(windows_.size());
    for (Window* window : windows_)
        windows_at_start.push_back(WindowAtStart{window, window->lifetime_token()});
    for (auto it = windows_at_start.rbegin(); it != windows_at_start.rend(); ++it) {
        if (it->liveness.expired()) continue;
        Window* window = it->window;
        if (std::find(windows_.begin(), windows_.end(), window) == windows_.end()) continue;
        if (!window->close()) return;  // vetoed: the sweep stops here, quit is cancelled
        if (desktop_liveness.expired()) return;
    }
    if (desktop_liveness.expired()) return;
    if (app_ != nullptr) app_->request_quit();
}

void Desktop::zoom_active_window() {
    if (active_ != nullptr) active_->toggle_zoom(content_area());
}

void Desktop::minimize_active_window() {
    if (active_ == nullptr || !active_->minimizable()) return;
    // Straight to the window, exactly as the `_` control does. Everything
    // that follows — activation moving to whatever is still shown, the
    // Minimized notification a switcher bar redraws on — is
    // window_minimize_changed's, which the window reports into itself.
    active_->set_minimized(true);
}

Window* Desktop::attach_window(std::unique_ptr<Window> window) {
    CKV_ASSERT(window != nullptr);
    // Nothing happens to the remembered arrangement here, deliberately. A
    // window arriving is not one of its cells: the windows in the arrangement
    // still hold exactly the cells they were given, and the newcomer floats
    // over them like any other window that is not part of it. Forgetting the
    // arrangement on every addition would mean a reader lost their tiling to
    // opening a message box — including the two dialogs this Desktop presents
    // itself — which is a high price for a rule that buys nothing. (The
    // filled-tiling VERDICT does go while a resizable newcomer covers part of
    // the grid, and comes back when it closes; that is a different question,
    // about what is on the desktop now rather than what was arranged.)
    auto* raw = static_cast<Window*>(ui::View::add_child(std::move(window)));
    raw->compositor_layer_id_ = next_compositor_layer_id_++;
    windows_.push_back(raw);  // stable insertion order — cycling order, not z-order
    // A window opened while the view is panned belongs to the world like every
    // other, so it is drawn where the pan says rather than where an unpanned
    // desktop would have put it (U7-a). Zero for every desktop that has never
    // panned, which is all of them until a host asks.
    raw->set_paint_offset(Point{-pan_.x, -pan_.y});
    // Window never exposes a Desktop callback publicly.  This private,
    // lifetime-bound capability is cleared before either participant can
    // detach, so a retained Window cannot call into a former Desktop.
    raw->bind_desktop_zoom_target([this] { return content_area(); }, window_relationship_);
    // A window on the move churns the pictures of everything it passes
    // over: each position re-slices what it occludes, and every slice is a
    // replacement the host pays to decode. So one window's gesture rests
    // every window's pictures, and the drop brings them all back at once.
    raw->bind_gesture_observer([this](bool active) { window_gesture_changed(active); },
                               window_relationship_);
    // A rename is news for whoever LISTS windows; this window's own
    // invalidate reaches only its own frame.
    raw->bind_title_observer(
        [this, raw] { notify_window_change(WindowChange::TitleChanged, *raw); },
        window_relationship_);
    // Minimizing is news this Desktop must ACT on rather than merely pass
    // along: the window that just went away may have been the active one.
    raw->bind_minimize_observer([this, raw] { window_minimize_changed(*raw); },
                                window_relationship_);
    // A drag may not carry the title bar out of the content area: under the
    // menu bar or past the footer there is nothing left to grab it by.
    raw->set_move_bounds(content_area());
    // A window that arrives already minimized stays minimized. Activating it
    // would restore it (see activate), which turns "open this in the
    // background" into "open this in front" — and an application that hands
    // us a hidden window has said which of the two it meant.
    if (shown(*raw)) activate(raw);
    // Last, so an observer is handed a window that is completely attached —
    // see WindowChange::Added for why the Activated above precedes it.
    notify_window_change(WindowChange::Added, *raw);
    return raw;
}

Window* Desktop::add_window(std::unique_ptr<Window> window) {
    // Asked here, before the new window takes activation away from it. The
    // window the reader was looking at when they opened this one is the
    // whole question; active_ read afterwards can only ever be the new
    // window, which answers nothing.
    const bool follow_maximized =
        maximize_follows_active_ && active_ != nullptr && active_->maximized();
    Window* const raw = attach_window(std::move(window));
    if (follow_maximized) open_maximized(*raw);
    return raw;
}

void Desktop::open_maximized(Window& window) {
    // Already maximized — by its own zoom or by a KeepFilling grow policy.
    // Toggling here would un-zoom the first and give the second a
    // restore rectangle it never asked for.
    if (window.maximized()) return;
    // A window still at its default empty rect has no geometry to restore
    // to. Zoomed straight from there it would record 0x0 as its restored
    // bounds, and the reader's first click on the zoom control would make
    // the window disappear instead of shrinking it. Give it the centered
    // placement present_modeless() would have given it, then zoom that.
    if (window.bounds().width <= 0 || window.bounds().height <= 0)
        place_unpositioned_window(window, content_area());
    window.toggle_zoom(content_area());
}

ui::View* Desktop::add_child(std::unique_ptr<ui::View> child) {
    CKV_ASSERT(child != nullptr);
    if (dynamic_cast<Window*>(child.get()) != nullptr) {
        std::unique_ptr<Window> owned_window(static_cast<Window*>(child.release()));
        return add_window(std::move(owned_window));
    }
    return ui::View::add_child(std::move(child));
}

Window* Desktop::present_modeless(WindowHandle handle, ui::Application& app) {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    Window* window = handle.window.get();
    CKV_ASSERT(window != nullptr);
    const std::weak_ptr<void> window_liveness = window->lifetime_token();
    place_unpositioned_window(*window, content_area());
    attach_window(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    if (desktop_liveness.expired() || window_liveness.expired()) return nullptr;
    if (std::find(windows_.begin(), windows_.end(), window) == windows_.end()) return nullptr;
    return window;
}

Window* Desktop::present_modal(WindowHandle handle, ui::Application& app) {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    Window* window = handle.window.get();
    CKV_ASSERT(window != nullptr);
    const std::weak_ptr<void> window_liveness = window->lifetime_token();
    place_unpositioned_window(*window, content_area());
    // Modality is this layer's concept, not Window's. A modal window with a
    // minimize control offers the reader a way to hide the only window that
    // is accepting input — and then refuses them the bar they would have to
    // click to get it back.
    window->set_minimizable(false);
    attach_window(std::move(handle.window));
    app.push_modal(*window);
    app.set_focus(handle.initial_focus);
    if (desktop_liveness.expired() || window_liveness.expired()) return nullptr;
    if (std::find(windows_.begin(), windows_.end(), window) == windows_.end()) return nullptr;
    return window;
}

void Desktop::exec_modal(ui::Application& app, WindowHandle handle) {
    // A handler/post/timer is already executing within Application::step;
    // pumping here would recursively dispatch and make lifetime/routing
    // order unprovable. Callers in that context use present_modal instead.
    CKV_ASSERT(app.can_run_blocking());
    // The blocking pump crosses arbitrary application callbacks. A callback
    // may detach and destroy either this Desktop or its modal Window, so retain
    // their independent identity observations before entering the loop.
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    Window* window = handle.window.get();
    CKV_ASSERT(window != nullptr);
    const std::weak_ptr<void> window_liveness = window->lifetime_token();
    place_unpositioned_window(*window, content_area());
    window->set_minimizable(false);  // modal: see present_modal
    attach_window(std::move(handle.window));
    const ui::Application::ModalScopeId modal_scope = app.push_modal(*window);
    app.set_focus(handle.initial_focus);
    const bool window_detached = app.run_until([this, desktop_liveness, window, window_liveness] {
        if (desktop_liveness.expired() || window_liveness.expired()) return true;
        return std::find(windows_.begin(), windows_.end(), window) == windows_.end();
    });
    // A host-requested shutdown is not a user close request: it has already
    // selected loop termination, so a still-open blocking dialog must not
    // remain attached and silently become modeless after this function
    // returns. Keep the removed window alive until after detachment callbacks
    // finish; those callbacks may inspect the exact modal-scope state.
    std::unique_ptr<Window> shutdown_detached;
    if (!window_detached && !desktop_liveness.expired() && !window_liveness.expired())
        shutdown_detached = remove_window(window);
    // The Application detach sink removes a modal scope as soon as its
    // Window detaches. Pop exactly this scope only if an unusual caller-owned
    // detachment path left it present; never pop an independently opened
    // inner modal.
    app.pop_modal(modal_scope);
}

std::unique_ptr<Window> Desktop::remove_window(Window* window) {
    auto it = std::find(windows_.begin(), windows_.end(), window);
    if (it == windows_.end()) return nullptr;
    // A window that HELD a cell takes it with it and leaves a hole no
    // proportion describes, so the arrangement goes with it — which is also
    // what guarantees a remembered cell can never outlive the window it
    // names, since a cell's window is only ever removed through here. One
    // that held no cell was never in the arrangement and leaves it untouched:
    // a dialog closing gives the reader their tiling back rather than
    // finishing it off. Forgotten before anything below runs application
    // code, so that a successor's activation or an observer re-arranging what
    // is left has the last word.
    if (tiling_reference_) {
        const auto& cells = tiling_reference_->cells;
        if (std::any_of(cells.begin(), cells.end(),
                        [window](const TilingCell& cell) { return cell.window == window; }))
            tiling_reference_.reset();
    }
    windows_.erase(it);

    // The relationship must disappear before View's detach sink can call
    // application/user code.  The Window may outlive the Desktop in the
    // unique_ptr returned below, but it can no longer reach this Desktop.
    window->clear_desktop_zoom_target();
    window->clear_gesture_observer();
    window->clear_title_observer();
    window->clear_minimize_observer();
    if (active_ == window) {
        active_ = nullptr;
        window->set_active(false);
    }
    std::unique_ptr<ui::View> detached = ui::View::remove_child(window);
    CKV_ASSERT(detached != nullptr);  // was in windows_, so it must have been a child
    request_layer_recompose();

    if (active_ == nullptr) activate_topmost_shown();
    // After the successor is active, so an observer reading active_window()
    // from here never sees the gap between the two. `window` is still alive:
    // `detached` owns it until this function hands that ownership on.
    notify_window_change(WindowChange::Removed, *window);
    return std::unique_ptr<Window>(static_cast<Window*>(detached.release()));
}

Desktop::Snapshot Desktop::snapshot() const {
    Snapshot snapshot;
    snapshot.windows.reserve(windows_.size());
    for (const auto& child : children()) {
        auto* window = dynamic_cast<Window*>(child.get());
        if (window == nullptr) continue;
        if (std::find(windows_.begin(), windows_.end(), window) == windows_.end()) continue;
        snapshot.windows.push_back(WindowSnapshot{window,
                                                  window->lifetime_token(),
                                                  window->bounds(),
                                                  window->restored_bounds_,
                                                  window->grow_policy_,
                                                  window == active_,
                                                  window->zoomed_,
                                                  window->minimized_});
    }
    return snapshot;
}

void Desktop::restore(const Snapshot& snapshot) {
    struct LiveEntry {
        const WindowSnapshot* snapshot = nullptr;
        Window* window = nullptr;
    };
    std::vector<LiveEntry> live;
    live.reserve(snapshot.windows.size());
    for (const WindowSnapshot& entry : snapshot.windows) {
        if (entry.window == nullptr || entry.liveness.expired()) continue;
        if (std::find(windows_.begin(), windows_.end(), entry.window) == windows_.end()) continue;
        live.push_back(LiveEntry{&entry, entry.window});
    }

    if (active_ != nullptr) {
        active_->set_active(false);
        active_ = nullptr;
    }

    for (const LiveEntry& entry : live) {
        Window& window = *entry.window;
        window.restored_bounds_ = entry.snapshot->restored_bounds;
        window.grow_policy_ = entry.snapshot->grow_policy;
        window.zoomed_ = entry.snapshot->zoomed;
        // Through the setter, not the member: hiding a window is what makes
        // it stop painting and stop answering the pointer, and a restored
        // layout in which a parked window is marked minimized but still on
        // screen is not the layout that was recorded.
        window.set_minimized(entry.snapshot->minimized);
        window.set_bounds(entry.snapshot->bounds);
        // The one other place activation is assigned, and it keeps the same
        // invariant activate() does: never at a window that is not on the
        // desktop. A snapshot this Desktop took cannot claim both at once;
        // one assembled by hand can, and the visible half wins.
        if (entry.snapshot->active && shown(window)) active_ = &window;
    }

    for (const LiveEntry& entry : live) raise_layer_to_front(entry.window);
    reraise_popups();

    if (active_ != nullptr) active_->set_active(true);
    request_layer_recompose();
    // A snapshot laid back down is a stated arrangement like any tiling
    // command's: if what it restored is a tiling, a later resize re-divides it
    // by these proportions rather than clamping the bands apart. Recorded
    // against the area the windows have just been placed in, which is this
    // desktop's own, not whatever size the desktop had when the snapshot was
    // taken — the snapshot replays absolute rectangles, and this remembers
    // the arrangement those rectangles turned out to make here.
    capture_tiling_reference(content_area());
    // A restore moves activation without going through activate(), and a view
    // listing windows has no other way to hear that the window in front is a
    // different one than it was before the snapshot was laid back down.
    if (active_ != nullptr) notify_window_change(WindowChange::Activated, *active_);
}

std::unique_ptr<ui::View> Desktop::remove_child(ui::View* child) {
    if (auto* window = dynamic_cast<Window*>(child)) {
        std::unique_ptr<Window> detached = remove_window(window);
        return std::unique_ptr<ui::View>(detached.release());
    }
    if (std::find(popups_.begin(), popups_.end(), child) != popups_.end()) return remove_popup(child);
    // Docks participate in content_area(); clear their observers before the
    // generic detach can enter user callbacks or destroy the dock.
    if (child == top_dock_) top_dock_ = nullptr;
    if (child == bottom_dock_) bottom_dock_ = nullptr;
    return ui::View::remove_child(child);
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

void Desktop::window_gesture_changed(bool active) {
    if (active) {
        if (++active_gestures_ == 1)
            for (Window* window : windows_) window->suspend_rasters();
        return;
    }
    if (active_gestures_ > 0 && --active_gestures_ == 0) {
        for (Window* window : windows_) window->resume_rasters();
        // The drop, and the only moment this Desktop is told that a bounds
        // change came from the READER — a drag, a corner resize, or the
        // keyboard move/resize modes, all of which bracket themselves with
        // this gesture. A reader who has just built a tiling by hand has
        // stated an arrangement exactly as a tile command would have, and
        // gets the same proportional treatment across a resize; one who has
        // just dragged a window out of its cell has stated that it is no
        // longer part of one, and is never snapped back.
        note_reader_arrangement();
    }
}

bool Desktop::shown(const Window& window) noexcept { return window.visible(); }

void Desktop::activate_topmost_shown() {
    // The topmost WINDOW, not necessarily children().back() — a popup may
    // currently be topmost, and children() holds both. Hidden windows are
    // passed over: handing activation to one would leave the reader with a
    // desktop whose active window is nowhere on it.
    for (auto rit = children().rbegin(); rit != children().rend(); ++rit) {
        ui::View* candidate = rit->get();
        auto found = std::find_if(windows_.begin(), windows_.end(), [candidate](Window* w) {
            return static_cast<ui::View*>(w) == candidate;
        });
        if (found == windows_.end() || !shown(**found)) continue;
        activate(*found);
        return;
    }
}

void Desktop::window_minimize_changed(Window& window) {
    if (window.minimized() && active_ == &window) {
        // The window the reader was working in has left the desktop. Some
        // other window has to have their attention, or the next keystroke
        // has nowhere to go; a desktop whose windows are ALL minimized
        // legitimately has no active window at all, which is the one case
        // this loop finds nothing for.
        active_ = nullptr;
        window.set_active(false);
        activate_topmost_shown();
    }
    // The effect, before the notification rather than after it: an observer
    // is allowed to minimize something else, and a flight that started after
    // that would be the second one describing the first one's window.
    //
    // Nothing downstream depends on it. `Window::set_minimized` has already
    // applied the whole end state — hidden, invisible, its bounds untouched —
    // and this only draws where it went.
    begin_minimize_flight(window);
    // Last, with activation already settled — see WindowChange::Minimized.
    notify_window_change(window.minimized() ? WindowChange::Minimized : WindowChange::Restored,
                         window);
}

void Desktop::set_minimize_target_provider(std::function<std::optional<Rect>(Window&)> provider) {
    minimize_target_provider_ = std::move(provider);
    // Whatever is in the air was flying to an answer this Desktop no longer
    // gives. Ended rather than redirected: a decoration mid-flight between two
    // superseded places is not worth the arithmetic to rescue.
    finish_minimize_animation();
}

void Desktop::set_minimize_animation_duration(std::int64_t nanos) noexcept {
    minimize_animation_nanos_ = std::max<std::int64_t>(0, nanos);
}

void Desktop::finish_minimize_animation() { minimize_animation_.finish(); }

void Desktop::begin_minimize_flight(Window& window) {
    // No provider is the ordinary case and the quiet one: an application that
    // has not said where its hidden windows go gets no effect, not a guess.
    if (!minimize_target_provider_ || minimize_animation_nanos_ <= 0) return;
    ui::Application* const app = context().app;
    if (app == nullptr) return;  // measured before attachment; nothing to animate on
    const std::optional<Rect> target = minimize_target_provider_(window);
    if (!target || target->width <= 0 || target->height <= 0) return;
    const Rect frame = window.bounds();
    if (frame.width <= 0 || frame.height <= 0) return;

    // Out to the row on the way in, back out of it on the way out. Read from
    // the window rather than remembered, because this runs after the state
    // changed and the state is the only thing that knows which way it went.
    //
    // This asks ONCE and flies to the answer, which is only safe because a
    // window changing state does not move its own row. That is not this
    // widget's property to guarantee — it belongs to whatever lists the
    // windows — and in this library `WindowSwitcherBar` holds it deliberately:
    // its three status glyphs are each one cell (D-059), so a minimize does
    // not re-flow the row the flight is aimed at. A host whose listing DOES
    // re-flow on a state change would see the decoration end somewhere the
    // window is not, which reads as a rendering glitch rather than as the
    // layout change it is. `every_status_glyph_is_one_cell_so_a_state_change_
    // never_re_flows_the_row` in test_window_switcher_bar.cpp is what keeps
    // the bar honest about it.
    const Rect from = window.minimized() ? frame : *target;
    const Rect to = window.minimized() ? *target : frame;

    // One decoration at a time. Starting a run ends the previous one, whose
    // `on_finished` removes its popup — so this is the whole of "a second
    // minimize during the flight".
    minimize_animation_.finish();
    auto flight = std::make_unique<MinimizeFlight>();
    flight->set_bounds(from);
    minimize_flight_ = add_popup(std::move(flight));

    const std::weak_ptr<void> alive = lifetime_token();
    minimize_animation_.start(
        *app, minimize_animation_nanos_,
        [this, alive, from, to](double progress) {
            if (alive.expired() || minimize_flight_ == nullptr) return;
            minimize_flight_->set_bounds(flight_rect(from, to, progress));
            minimize_flight_->invalidate();
        },
        [this, alive] {
            // The one place the decoration goes, for every ending there is —
            // finished, cut short, or a duration of zero that drew nothing.
            if (alive.expired() || minimize_flight_ == nullptr) return;
            remove_popup(minimize_flight_).reset();
            minimize_flight_ = nullptr;
        });
}

void Desktop::activate(Window* window) {
    CKV_ASSERT(std::find(windows_.begin(), windows_.end(), window) != windows_.end());
    if (window == active_) return;
    // Naming a window is asking for it: a hidden one comes back rather than
    // becoming an active window the reader cannot see. This is the only
    // place that rule lives, so select_by_number, a click on a switcher-bar
    // entry and an application's own activate() all get it right.
    if (window->minimized()) window->set_minimized(false);
    if (active_ != nullptr) active_->set_active(false);
    raise_layer_to_front(window);
    reraise_popups();  // keep every open popup above the window that just moved to front
    active_ = window;
    active_->set_active(true);
    // Last, with this Desktop's own activation state already settled, so an
    // observer that reads active_window() reads the answer this call made.
    notify_window_change(WindowChange::Activated, *window);
}

ui::View* Desktop::add_popup_impl(std::unique_ptr<ui::View> popup) {
    CKV_ASSERT(popup != nullptr);
    ui::View* raw = ui::View::add_child(std::move(popup));
    popups_.push_back(raw);  // already topmost: add_child appends to the end of children_
    popup_backings_.emplace(raw, PopupBacking{std::nullopt, next_compositor_layer_id_++});
    request_layer_recompose();
    return raw;
}

std::unique_ptr<ui::View> Desktop::remove_popup(ui::View* popup) {
    auto it = std::find(popups_.begin(), popups_.end(), popup);
    if (it == popups_.end()) return nullptr;
    popups_.erase(it);
    popup_backings_.erase(popup);
    std::unique_ptr<ui::View> detached = ui::View::remove_child(popup);
    request_layer_recompose();
    return detached;
}

std::unique_ptr<ui::View> Desktop::detach_child(ui::View* child) {
    return remove_child(child);
}

void Desktop::reraise_popups() {
    for (ui::View* popup : popups_) raise_layer_to_front(popup);
}

void Desktop::activate_step(bool forward) {
    if (windows_.size() < 2 || active_ == nullptr) return;
    auto it = std::find(windows_.begin(), windows_.end(), active_);
    CKV_ASSERT(it != windows_.end());
    const std::size_t count = windows_.size();
    const std::size_t stride = forward ? 1 : count - 1;  // one place back, without a signed modulo
    std::size_t i = static_cast<std::size_t>(it - windows_.begin());
    // At most one full lap, and never back to where it started: a hidden
    // window is stepped over, and a desktop where every OTHER window is
    // hidden leaves activation exactly where it is rather than re-activating
    // the window it never left.
    for (std::size_t step = 0; step + 1 < count; ++step) {
        i = (i + stride) % count;
        if (!shown(*windows_[i])) continue;
        activate(windows_[i]);
        return;
    }
}

void Desktop::activate_next() { activate_step(true); }

void Desktop::activate_previous() { activate_step(false); }

void Desktop::select_by_number(int n) {
    if (n < 1 || static_cast<std::size_t>(n) > windows_.size()) return;
    activate(windows_[static_cast<std::size_t>(n - 1)]);
}

ui::View* Desktop::dock_top_impl(std::unique_ptr<ui::View> view) {
    CKV_ASSERT(view != nullptr);
    ui::View* raw = ui::View::add_child(std::move(view));
    top_dock_ = raw;
    on_resized();  // position it immediately, not just on the next Desktop resize
    return raw;
}

ui::View* Desktop::dock_bottom_impl(std::unique_ptr<ui::View> view) {
    CKV_ASSERT(view != nullptr);
    ui::View* raw = ui::View::add_child(std::move(view));
    bottom_dock_ = raw;
    on_resized();
    return raw;
}

ui::View* Desktop::set_content_impl(std::unique_ptr<ui::View> view) {
    CKV_ASSERT(view != nullptr);
    // The previous content leaves first, so the new arrangement is never
    // briefly stacked on top of the old one.
    std::unique_ptr<ui::View> previous = take_content();
    (void)previous;
    ui::View* raw = ui::View::add_child(std::move(view));
    content_ = raw;
    // Content sits under everything else: windows float above the
    // arrangement, and a popup above them both.
    lower_to_back(raw);
    on_resized(); // fill immediately, not just on the next Desktop resize
    return raw;
}

std::unique_ptr<ui::View> Desktop::take_content() {
    if (content_ == nullptr) return nullptr;
    ui::View* const previous = content_;
    content_ = nullptr;
    return ui::View::remove_child(previous);
}

Rect Desktop::content_area() const noexcept {
    const int top = top_dock_ != nullptr ? std::max(1, top_dock_->vertical_size_hint().preferred) : 0;
    const int bottom = bottom_dock_ != nullptr ? std::max(1, bottom_dock_->vertical_size_hint().preferred) : 0;
    // The WORLD's content area, not the view's (U7-a). Everything that
    // arranges windows is expressed in it — the tilings, the cascade, a
    // maximized window's rect, the remembered arrangement — and none of those
    // may change because a reader scrolled. With no extent set, the world IS
    // this desktop's bounds and the answer is what it has always been.
    const Size world = extent_.width > 0 && extent_.height > 0
                           ? extent_
                           : Size{bounds().width, bounds().height};
    return Rect{0, top, world.width, std::max(0, world.height - top - bottom)};
}

void Desktop::set_extent(Size extent) {
    const Size wanted{std::max(0, extent.width), std::max(0, extent.height)};
    if (wanted.width == extent_.width && wanted.height == extent_.height) return;
    extent_ = wanted;
    // A world that just shrank may leave the view looking past its edge, and a
    // world that grew may let it look further; either way the pan is re-held
    // inside what there is to show before anything is drawn with it.
    pan_ = clamped_pan(pan_);
    apply_pan_to_windows();
    // The content area has changed shape, which is the same event a resize is
    // as far as every window and dock is concerned.
    on_resized();
}

Point Desktop::clamped_pan(Point wanted) const noexcept {
    const Rect world = content_area();
    const int top = world.y;
    const int view_height = std::max(0, bounds().height - top -
                                            (bottom_dock_ != nullptr
                                                 ? std::max(1, bottom_dock_->vertical_size_hint().preferred)
                                                 : 0));
    // Never past the edge, and never negative: a pan is which part of the
    // world the hole is over, and there is no world left of zero.
    const int max_x = std::max(0, world.width - bounds().width);
    const int max_y = std::max(0, world.height - view_height);
    return Point{std::clamp(wanted.x, 0, max_x), std::clamp(wanted.y, 0, max_y)};
}

void Desktop::set_pan(Point pan) {
    const Point held = clamped_pan(pan);
    if (held.x == pan_.x && held.y == pan_.y) return;
    pan_ = held;
    apply_pan_to_windows();
    invalidate();
}

void Desktop::pan_to_show(Rect world) {
    const Rect area = content_area();
    const int view_width = bounds().width;
    const int view_height = std::max(0, bounds().height - area.y -
                                            (bottom_dock_ != nullptr
                                                 ? std::max(1, bottom_dock_->vertical_size_hint().preferred)
                                                 : 0));
    Point wanted = pan_;
    // The least movement that brings it in: a reader who focused an off-screen
    // window means to see it, not to have their view thrown somewhere new.
    if (world.x < pan_.x) wanted.x = world.x;
    else if (world.right() > pan_.x + view_width) wanted.x = world.right() - view_width;
    const int world_top = world.y - area.y;
    const int world_bottom = world_top + world.height;
    if (world_top < pan_.y) wanted.y = world_top;
    else if (world_bottom > pan_.y + view_height) wanted.y = world_bottom - view_height;
    set_pan(wanted);
}

void Desktop::apply_pan_to_windows() {
    // Windows move; docked chrome does not. A menu bar that scrolled off the
    // top of the screen would not be a menu bar — the docks belong to the
    // view, the windows to the world.
    const Point offset{-pan_.x, -pan_.y};
    for (Window* window : windows_) window->set_paint_offset(offset);
    // Popups are the view's too, and stay where they were put: a dropdown that
    // slid away from the bar it hangs from is not a dropdown, and a context
    // menu was opened at a place on the SCREEN.
}

void Desktop::on_resized() {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    if (top_dock_ != nullptr) {
        const int h = std::max(1, top_dock_->vertical_size_hint().preferred);
        top_dock_->set_bounds(Rect{0, 0, bounds().width, h});
        if (desktop_liveness.expired()) return;
    }
    if (bottom_dock_ != nullptr) {
        const int h = std::max(1, bottom_dock_->vertical_size_hint().preferred);
        bottom_dock_->set_bounds(Rect{0, bounds().height - h, bounds().width, h});
        if (desktop_liveness.expired()) return;
    }
    const Rect area = content_area();
    if (content_ != nullptr) {
        content_->set_bounds(area);
        if (desktop_liveness.expired()) return;
    }
    // The docks may have changed height, so the area a window must stay
    // reachable within has moved with them.
    for (Window* window : windows_)
        if (window != nullptr) window->set_move_bounds(area);

    // Desktop growth tracking (M8 WP-4): a zoomed window keeps filling
    // the (possibly now larger or smaller) content area rather than
    // staying pinned at whatever size it had when it was first zoomed;
    // KeepFilling behaves the same way permanently, without needing
    // zoomed() at all; AnchorEdges keeps each such window's current
    // distance to the right/bottom edges, using the area from the
    // PREVIOUS pass to know what that distance was.
    const std::vector<LiveWindow> windows = live_windows();
    for (const LiveWindow& handle : windows) {
        if (desktop_liveness.expired()) return;
        if (!still_owned(handle)) continue;
        Window* const w = handle.window;
        if (w->zoomed()) {
            w->refresh_zoom_area(area);
        } else if (w->grow_policy() == DesktopGrowPolicy::KeepFilling) {
            w->fill(area);
        } else if (w->grow_policy() == DesktopGrowPolicy::AnchorEdges) {
            const Rect b = w->bounds();
            const int right_margin = (last_content_area_.x + last_content_area_.width) - (b.x + b.width);
            const int bottom_margin = (last_content_area_.y + last_content_area_.height) - (b.y + b.height);
            w->fill(Rect{b.x, b.y, (area.x + area.width) - right_margin - b.x,
                         (area.y + area.height) - bottom_margin - b.y});
        }
    }

    // The arrangement a reader stated, re-divided by the same proportions in
    // the new area (see capture_tiling_reference). After the grow policies
    // above, because AnchorEdges reads each window's bounds against the
    // PREVIOUS area to recover its margins and must not be shown a window
    // this pass has already moved. Before the clamp below, because for the
    // windows it places it IS the clamp: every cell it writes lies inside
    // `area` by construction, and re-clamping them afterwards would let one
    // window's own minimum size push it back over its neighbour — the exact
    // overlap the arrangement is being replayed to prevent.
    const std::vector<Window*> reflowed = reflow_tiling_reference(area);
    if (desktop_liveness.expired()) return;

    // The architecture §5 "Sizing policy": on shrink, windows are
    // deterministically clamped and repositioned to remain reachable —
    // every Desktop resize (which includes docking new chrome, since
    // that shrinks the content area too) re-clamps every window,
    // regardless of grow policy: the policy above only decides
    // whether/how a window grows, this still enforces the
    // never-unreachable floor unconditionally.
    //
    // Minimized windows included, and deliberately: a hidden window whose
    // geometry was frozen would come back off the edge of a desktop that
    // shrank while it was away, which is the very unreachability this pass
    // exists to prevent. Hiding withholds a window's presentation, not its
    // owner's sizing policy. A window the arrangement reflow just placed is
    // the one exception, and is already inside the area by construction.
    for (const LiveWindow& handle : windows) {
        if (desktop_liveness.expired()) return;
        if (!still_owned(handle)) continue;
        if (std::find(reflowed.begin(), reflowed.end(), handle.window) != reflowed.end()) continue;
        handle.window->reposition_within(area);
    }
    if (desktop_liveness.expired()) return;
    last_content_area_ = area;
}

std::vector<Desktop::LiveWindow> Desktop::live_windows() const {
    std::vector<LiveWindow> handles;
    handles.reserve(windows_.size());
    for (Window* window : windows_)
        handles.push_back(LiveWindow{window, window->lifetime_token()});
    return handles;
}

std::vector<Desktop::LiveWindow> Desktop::live_shown_windows() const {
    std::vector<LiveWindow> handles;
    handles.reserve(windows_.size());
    for (Window* window : windows_)
        if (shown(*window)) handles.push_back(LiveWindow{window, window->lifetime_token()});
    return handles;
}

bool Desktop::still_owned(const LiveWindow& handle) const {
    if (handle.liveness.expired()) return false;
    return std::find(windows_.begin(), windows_.end(), handle.window) != windows_.end();
}

void Desktop::tile_in_bands(BandAxis axis) {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    // The windows on the desktop, and the count the area is divided by. Both
    // from the same list: dividing by every window while handing bands only
    // to the shown ones would leave a hidden window's share of the desktop
    // showing through, which is the one thing every tiling promises not to
    // do — and it would cost the arrangement its filled_tile_fractions()
    // verdict along with the shadow suppression that follows from it.
    const std::vector<LiveWindow> handles = live_shown_windows();
    if (handles.empty()) return;
    const Rect area = content_area();
    const int n = static_cast<int>(handles.size());
    // One axis is divided into bands; the other is kept whole, which is what
    // makes every band full-width or full-height.
    const int origin = axis == BandAxis::Rows ? area.y : area.x;
    const int divided = axis == BandAxis::Rows ? area.height : area.width;
    const int band = divided / n;
    int at = origin;
    for (int i = 0; i < n; ++i) {
        // The last band absorbs whatever the integer division dropped.
        // Leaving it out would leave a gap row or column down the
        // arrangement — visible desktop, and enough on its own to cost the
        // whole tiling its filled_tile_fractions() verdict, and with it the
        // proportional restore and the shadow suppression that follow from
        // it.
        const int extent = (i == n - 1) ? (origin + divided - at) : band;
        const LiveWindow& handle = handles[static_cast<std::size_t>(i)];
        if (desktop_liveness.expired()) return;
        // set_bounds enters the window's own on_resized, which is
        // application-extensible and may have destroyed or detached this
        // window — or this whole Desktop — since the loop began.
        if (still_owned(handle))
            handle.window->set_bounds(axis == BandAxis::Rows
                                          ? Rect{area.x, at, area.width, extent}
                                          : Rect{at, area.y, extent, area.height});
        at += extent;
    }
    if (desktop_liveness.expired()) return;
    // A reader has just stated an arrangement. Remembered here, as laid down
    // and in the area it was laid down in, so that a later resize can
    // re-divide it by these proportions instead of clamping each band on its
    // own and leaving two of them sharing a row.
    capture_tiling_reference(area);
}

void Desktop::tile() { tile_in_bands(BandAxis::Columns); }

// The axis names what the windows are laid out ALONG, not the direction the
// dividers between them run. Tiling horizontally puts them in a row, side by
// side across the desktop; tiling vertically stacks them down it. Both
// readings exist in the wild — a divider-named convention would swap these —
// and this is the one a reader of ckmux reported expecting, which settles it
// for a pair of commands whose whole job is to be predictable. `tile()` keeps
// its historical arrangement, which is now the horizontal one.
void Desktop::tile_horizontally() { tile_in_bands(BandAxis::Columns); }

void Desktop::tile_vertically() { tile_in_bands(BandAxis::Rows); }

void Desktop::tile_grid() {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    // Shown windows only, and the grid's own shape derived from their count
    // — see tile_in_bands. Four windows with one minimized is a two-window
    // grid, not a four-window grid with a hole in it.
    const std::vector<LiveWindow> handles = live_shown_windows();
    if (handles.empty()) return;
    const Rect area = content_area();
    const int n = static_cast<int>(handles.size());
    // ceil(sqrt(n)) by integer search rather than through std::sqrt: a
    // library that answers 2.9999999 for 9 would lay nine windows out in
    // four columns, and the arrangement would come out different on one
    // machine than on the next — which the determinism rule
    // (the engineering standard, "identical inputs must yield byte-identical frames")
    // forbids outright.
    int columns = 1;
    while (columns * columns < n) ++columns;
    const int rows = (n + columns - 1) / columns;
    const int row_height = area.height / rows;
    int y = area.y;
    for (int row = 0; row < rows; ++row) {
        const int height = (row == rows - 1) ? (area.y + area.height - y) : row_height;
        const int first = row * columns;
        const int end = std::min(first + columns, n);  // a short last row is normal
        // A short last row spreads across the WHOLE width instead of
        // stopping where a full row would have. Three windows leave a
        // half-empty bottom row otherwise, and that gap is not merely ugly:
        // it is desktop showing through, so the arrangement stops being a
        // filled tiling and loses both the proportional restore and the
        // shadow suppression that depend on it.
        const int column_width = area.width / (end - first);
        int x = area.x;
        for (int i = first; i < end; ++i) {
            const int width = (i == end - 1) ? (area.x + area.width - x) : column_width;
            const LiveWindow& handle = handles[static_cast<std::size_t>(i)];
            if (desktop_liveness.expired()) return;
            // set_bounds enters application-extensible code that may destroy
            // this window or the whole Desktop — see tile_in_bands.
            if (still_owned(handle)) handle.window->set_bounds(Rect{x, y, width, height});
            x += width;
        }
        y += height;
    }
    if (desktop_liveness.expired()) return;
    capture_tiling_reference(area);  // a stated arrangement — see tile_in_bands
}

void Desktop::cascade() {
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    // A cascade is an arrangement like any other: a hidden window gets no
    // step in the diagonal, and — since the cascade order becomes z-order —
    // no place in the restacking either. Offsetting one nobody can see would
    // leave a gap in the run of title bars the whole arrangement is for.
    const std::vector<LiveWindow> handles = live_shown_windows();
    if (handles.empty()) return;
    const Rect area = content_area();
    const int width = std::max(10, area.width * 2 / 3);
    const int height = std::max(4, area.height * 2 / 3);
    const int max_x = std::max(0, area.width - width);
    const int max_y = std::max(0, area.height - height);
    for (std::size_t i = 0; i < handles.size(); ++i) {
        const int offset = static_cast<int>(i);
        const int x = area.x + (max_x == 0 ? 0 : (offset * 2) % (max_x + 1));
        const int y = area.y + (max_y == 0 ? 0 : (offset * 1) % (max_y + 1));
        if (desktop_liveness.expired()) return;
        if (still_owned(handles[i])) handles[i].window->set_bounds(Rect{x, y, width, height});
    }
    // Cascade order should also become z-order: last window frontmost.
    for (const LiveWindow& handle : handles) {
        if (desktop_liveness.expired()) return;
        if (still_owned(handle)) raise_to_front(handle.window);
    }
    if (desktop_liveness.expired()) return;
    // Every command that lays windows out ends by stating what it made, this
    // one included — even though a cascade is an overlapping arrangement and
    // almost never a partition. Saying so is the point: a cascade supersedes
    // whatever tiling preceded it, and leaving the old one remembered would
    // have the next resize re-lay an arrangement the reader has replaced.
    capture_tiling_reference(area);
}

bool Desktop::tiling_participant(const Window& window) const {
    // Not on the desktop, so not in the arrangement: a window minimized to a
    // switcher bar, or one its application hid, has no band to be measured
    // and no share to be restored. The SAME test the tile commands ask
    // before handing out a band, so the measurement and the arrangement
    // cannot come to different conclusions about who is in it.
    if (!shown(window)) return false;
    // A fixed-size window is an alert, an About box, a message: it sits ON
    // the arrangement, never in it, and it has no band to be resized into.
    if (!window.resizable()) return false;
    // Same reason for one that is resizable but modal — modality is the
    // strongest statement a window makes about being above everything else.
    if (context().app != nullptr && context().app->is_modal_root(window)) return false;
    return true;
}

std::vector<Window*> Desktop::partition_of(Rect area) const {
    if (area.empty()) return {};

    std::vector<Window*> tiles;
    tiles.reserve(windows_.size());
    for (Window* window : windows_)
        if (tiling_participant(*window)) tiles.push_back(window);
    if (tiles.empty()) return {};

    // Three conditions, and no fourth: every window lies inside `area`, no
    // two of them overlap, and their areas sum to its own. Together those are
    // an exact partition — which is why this can answer for an arrangement no
    // tile command produced, and why it stops answering the moment a reader
    // drags one window a single cell off the grid (the drag both uncovers a
    // strip and overlaps a neighbour, and either one alone is already
    // disqualifying).
    std::int64_t covered = 0;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const Rect bounds = tiles[i]->bounds();
        if (bounds.empty()) return {};
        if (!(bounds.intersected(area) == bounds)) return {};  // reaches outside the area
        for (std::size_t j = 0; j < i; ++j)
            if (!bounds.intersected(tiles[j]->bounds()).empty()) return {};
        covered += static_cast<std::int64_t>(bounds.width) * bounds.height;
    }
    if (covered != static_cast<std::int64_t>(area.width) * area.height) return {};
    return tiles;
}

std::vector<Desktop::TileFraction> Desktop::filled_tile_fractions() const {
    const Rect area = content_area();
    // The partition test itself lives in one place, so what a resize replays
    // and what this reports can never be two different opinions about the
    // same desktop.
    const std::vector<Window*> tiles = partition_of(area);
    if (tiles.empty()) return {};

    std::vector<TileFraction> fractions;
    fractions.reserve(tiles.size());
    const double area_width = area.width;
    const double area_height = area.height;
    for (Window* window : tiles) {
        const Rect bounds = window->bounds();
        fractions.push_back(TileFraction{window, (bounds.x - area.x) / area_width,
                                         (bounds.y - area.y) / area_height,
                                         bounds.width / area_width,
                                         bounds.height / area_height});
    }
    return fractions;
}

void Desktop::capture_tiling_reference(Rect area) {
    // Unconditionally, first: a command that laid windows out and did not
    // leave a tiling has superseded whatever was remembered just as surely as
    // one that did. A cascade is the ordinary case of that.
    tiling_reference_.reset();

    const std::vector<Window*> tiles = partition_of(area);
    if (tiles.empty()) return;
    // Another authority already sizes such a window on every resize — see
    // this function's declaration in desktop.hpp for why that authority keeps
    // it rather than sharing it.
    for (Window* window : tiles)
        if (window->zoomed() || window->grow_policy() != DesktopGrowPolicy::None) return;

    TilingReference reference;
    reference.area = area;
    reference.cells.reserve(tiles.size());
    // Membership is settled HERE and then frozen. A window that is later
    // minimized keeps its cell: it left the screen, not the arrangement, and
    // it is coming back into the cell it left — which is why nothing below
    // re-asks shown(), even though the question that chose these windows did.
    for (Window* window : tiles)
        reference.cells.push_back(TilingCell{window, window->bounds(), window->bounds()});
    tiling_reference_ = std::move(reference);
}

bool Desktop::tiling_reference_is_intact() const {
    if (!tiling_reference_) return false;
    // Every cell's window is still owned: adding or removing one forgets the
    // reference outright, which is what makes these pointers safe to read.
    for (const TilingCell& entry : tiling_reference_->cells)
        if (entry.window->bounds() != entry.placed) return false;
    return true;
}

void Desktop::note_reader_arrangement() {
    // An intact reference is left exactly as it is. Re-capturing here would
    // replace the original cells and the original area with the reflowed ones
    // — which is the one thing this design exists not to do, since mapping
    // from a reflow rather than from the original is what makes a resize
    // round trip lose a row rather than give it back.
    if (tiling_reference_is_intact()) return;
    capture_tiling_reference(content_area());
}

std::vector<Window*> Desktop::reflow_tiling_reference(Rect area) {
    if (!tiling_reference_is_intact()) {
        tiling_reference_.reset();
        return {};
    }
    const Rect from = tiling_reference_->area;
    if (from.empty()) {  // nothing to map proportions out of
        tiling_reference_.reset();
        return {};
    }

    // Planned in full before a single window is touched, for two reasons: the
    // degenerate floor below has to be able to decline the whole reflow, and
    // set_bounds re-enters application code that may detach or destroy any
    // window in the middle of the walk.
    struct Placement {
        LiveWindow handle;
        Rect cell;
    };
    std::vector<Placement> plan;
    plan.reserve(tiling_reference_->cells.size());
    for (const TilingCell& entry : tiling_reference_->cells) {
        const Rect cell = entry.cell;
        // Edges, not rectangles. Two neighbours share a seam, the seam is one
        // coordinate, and one coordinate maps to one number — so the mapped
        // cells still meet exactly, with no rounding to reconcile.
        const int left = map_edge(cell.x, from.x, from.width, area.x, area.width);
        const int right = map_edge(cell.x + cell.width, from.x, from.width, area.x, area.width);
        const int top = map_edge(cell.y, from.y, from.height, area.y, area.height);
        const int bottom = map_edge(cell.y + cell.height, from.y, from.height, area.y, area.height);
        const Rect mapped{left, top, right - left, bottom - top};
        // The floor: a content area too small to give every band a row (or
        // every column a cell) cannot hold this arrangement at all. Declining
        // is the honest answer — the ordinary clamp takes over and the
        // arrangement is gone, rather than the reader being handed windows
        // with nothing in them.
        if (mapped.empty()) {
            tiling_reference_.reset();
            return {};
        }
        plan.push_back(Placement{LiveWindow{entry.window, entry.window->lifetime_token()}, mapped});
    }

    // Recorded before the writing, not after: a window's own on_resized may
    // destroy this Desktop, and what a surviving reference must hold is where
    // this pass MEANT to put each window. If application code moves one
    // afterwards, the next pass sees the difference and forgets the
    // arrangement — which is exactly the intended reading of "somebody else
    // moved it".
    for (std::size_t i = 0; i < plan.size(); ++i) tiling_reference_->cells[i].placed = plan[i].cell;

    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    std::vector<Window*> placed;
    placed.reserve(plan.size());
    for (const Placement& step : plan) {
        if (desktop_liveness.expired()) return placed;
        // Raw set_bounds, exactly as tile_in_bands writes its own bands: a
        // cell is the arrangement's answer, and routing it through the
        // window's own size clamp would let one window's minimum silently
        // reopen the overlap this whole mechanism exists to prevent. Same
        // liveness guard, and for the same reason — set_bounds is
        // application-extensible.
        if (!still_owned(step.handle)) continue;
        placed.push_back(step.handle.window);
        step.handle.window->set_bounds(step.cell);
    }
    return placed;
}

void Desktop::draw(scene::Painter& painter) {
    // U+2591 LIGHT SHADE — the classic windowed-desktop fill pattern
    // (distinct from a plain space so the desktop always reads as
    // "background texture," never mistakable for an unstyled dialog
    // panel even under a color scheme where the two happen to share a
    // hue).
    painter.fill(Rect{0, 0, bounds().width, bounds().height},
                  Cell::from_grapheme("░", context().theme->resolve(background_role_)));
}

void Desktop::draw_retained(scene::Painter& painter) {
    if (!retained_base_dirty_) return;
    draw(painter);
    retained_base_dirty_ = false;
}

bool Desktop::is_descendant_of(const ui::View& view, const ui::View* ancestor) noexcept {
    for (const ui::View* current = &view; current != nullptr; current = current->parent())
        if (current == ancestor) return true;
    return false;
}

void Desktop::on_invalidated(Rect, ui::InvalidationKind) {
    if (!structural_invalidation_) retained_base_dirty_ = true;
}

void Desktop::request_layer_recompose() {
    structural_invalidation_ = true;
    invalidate();
    structural_invalidation_ = false;
}

void Desktop::raise_layer_to_front(ui::View* child) {
    structural_invalidation_ = true;
    raise_to_front(child);
    structural_invalidation_ = false;
}

void Desktop::on_descendant_invalidated(const ui::View& source, Rect, ui::InvalidationKind kind) {
    // Windows and popups have independent retained surfaces. Docked chrome
    // is composed into the base surface, so only it (or its descendants)
    // invalidates that surface.
    if (is_descendant_of(source, top_dock_) || is_descendant_of(source, bottom_dock_)) {
        retained_base_dirty_ = true;
        return;
    }
    // A popup's own geometry invalidation does not imply its local backing
    // changed: a same-size translation is a layer-position change. Its child
    // invalidations, on the other hand, change popup content and must repaint
    // that one backing before the next compose.
    for (ui::View* popup : popups_) {
        if (!is_descendant_of(source, popup)) continue;
        if (&source != popup || kind == ui::InvalidationKind::Content)
            popup_backings_.at(popup).dirty = true;
        return;
    }
}

bool Desktop::child_casts_shadow(const ui::View& child) const {
    if (!child.casts_shadow()) return false;
    const auto* window = dynamic_cast<const Window*>(&child);
    if (window == nullptr || !window->resizable()) return true;
    // A modal window is above the arrangement by definition.
    if (context().app != nullptr && context().app->is_modal_root(child)) return true;

    // Is this window part of a tiling that leaves no desktop between the
    // windows? One shared detection answers that and the layout query both,
    // rather than a shadow rule of its own that could disagree with the tag
    // a layout report carries.
    return filled_tile_fractions().empty();
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
    // Docked chrome paints last, for the same reason the retained path bounds
    // its window layers: a window belongs under the furniture.
    for (auto& child_ptr : children()) {
        View& child = *child_ptr;
        if (&child == top_dock_ || &child == bottom_dock_) continue;
        if (!paint_one_child(child, own_painter)) continue;  // invisible: nothing to shadow either

        if (!child_casts_shadow(child)) continue;

        // Shadow is composited in DESKTOP-local space (own_painter's
        // own coordinate frame), not the window's translated space —
        // child.bounds() is already desktop-local since window is a
        // direct child of this Desktop.
        scene::Painter mutable_painter = own_painter;
        for (const Rect& footprint : scene::shadow_footprint(child.bounds(), scene::ShadowSpec{})) {
            const Rect clipped = footprint.intersected(shadow_clip);
            if (!clipped.empty()) mutable_painter.apply_shadow(clipped, &scene::default_dim);
        }
    }
    for (ui::View* dock : {top_dock_, bottom_dock_})
        if (dock != nullptr) (void)paint_one_child(*dock, own_painter);
}

void Desktop::paint_retained(const scene::Painter& own_painter,
                             std::vector<scene::Layer>& layers) {
    last_content_repaints_ = 0;
    const Rect desktop_bounds = absolute_bounds();
    const Rect content = content_area();
    const Rect shadow_clip{desktop_bounds.x + content.x, desktop_bounds.y + content.y,
                           content.width, content.height};
    for (auto& child_ptr : children()) {
        ui::View& child = *child_ptr;
        if (!child.visible()) continue;

        if (auto* window = dynamic_cast<Window*>(&child)) {
            if (window->repaint_backing_if_needed()) ++last_content_repaints_;
            // Windows are bounded to the content area, so one dragged upward
            // slides under the menu bar rather than over it. Docked chrome is
            // the desktop's furniture: it is always there, and a window that
            // can cover it can hide the way out of whatever it is covering.
            layers.push_back(scene::Layer{window->compositor_layer_id_, &window->backing_surface(),
                                          Point{window->absolute_bounds().x,
                                                window->absolute_bounds().y},
                                          child_casts_shadow(*window), shadow_clip, shadow_clip});
            continue;
        }

        const auto popup = popup_backings_.find(&child);
        if (popup != popup_backings_.end()) {
            const Size size{child.bounds().width, child.bounds().height};
            if (!popup->second.surface || popup->second.surface->size() != size) {
                popup->second.surface.emplace(size);
                popup->second.dirty = true;
            }
            scene::Surface& backing = *popup->second.surface;
            if (popup->second.dirty) {
                backing.clear_raster_regions();
                scene::Painter popup_painter(backing, Rect{0, 0, size.width, size.height});
                child.draw(popup_painter);
                child.paint_children(popup_painter);
                popup->second.dirty = false;
                ++last_content_repaints_;
            }
            layers.push_back(scene::Layer{popup->second.compositor_layer_id, &backing,
                                          Point{child.absolute_bounds().x, child.absolute_bounds().y},
                                          child.casts_shadow(), shadow_clip});
            continue;
        }

        const Rect local_bounds{0, 0, child.bounds().width, child.bounds().height};
        scene::Painter child_painter = own_painter.isolated().translated(
            Point{child.bounds().x, child.bounds().y}, local_bounds);
        child.draw_retained(child_painter);
        child.paint_retained(child_painter, layers);
    }
}

}  // namespace ckv::widgets
