// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <algorithm>
#include <cstdint>

#include "cvision/core/assert.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/terminal_report_dialog.hpp"
#include "cvision/widgets/window_list_dialog.hpp"

namespace ckv::widgets {

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
    }
    window_relationship_.reset();
    // Nothing may be reported from here on. A docked observer is one of the
    // children View's own destructor is about to destroy, and a notification
    // reaching it would be a call into a Desktop whose members no longer
    // exist.
    window_observers_.clear();

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

Window* Desktop::attach_window(std::unique_ptr<Window> window) {
    CKV_ASSERT(window != nullptr);
    auto* raw = static_cast<Window*>(ui::View::add_child(std::move(window)));
    raw->compositor_layer_id_ = next_compositor_layer_id_++;
    windows_.push_back(raw);  // stable insertion order — cycling order, not z-order
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
    // A drag may not carry the title bar out of the content area: under the
    // menu bar or past the footer there is nothing left to grab it by.
    raw->set_move_bounds(content_area());
    activate(raw);
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
    windows_.erase(it);

    // The relationship must disappear before View's detach sink can call
    // application/user code.  The Window may outlive the Desktop in the
    // unique_ptr returned below, but it can no longer reach this Desktop.
    window->clear_desktop_zoom_target();
    window->clear_gesture_observer();
    window->clear_title_observer();
    if (active_ == window) {
        active_ = nullptr;
        window->set_active(false);
    }
    std::unique_ptr<ui::View> detached = ui::View::remove_child(window);
    CKV_ASSERT(detached != nullptr);  // was in windows_, so it must have been a child
    request_layer_recompose();

    if (active_ == nullptr) {
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
                                                  window->zoomed_});
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
        window.set_bounds(entry.snapshot->bounds);
        if (entry.snapshot->active) active_ = &window;
    }

    for (const LiveEntry& entry : live) raise_layer_to_front(entry.window);
    reraise_popups();

    if (active_ != nullptr) active_->set_active(true);
    request_layer_recompose();
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
    if (active_gestures_ > 0 && --active_gestures_ == 0)
        for (Window* window : windows_) window->resume_rasters();
}

void Desktop::activate(Window* window) {
    CKV_ASSERT(std::find(windows_.begin(), windows_.end(), window) != windows_.end());
    if (window == active_) return;
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
    return Rect{0, top, bounds().width, std::max(0, bounds().height - top - bottom)};
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

    // ARCHITECTURE.md §5 "Sizing policy": on shrink, windows are
    // deterministically clamped and repositioned to remain reachable —
    // every Desktop resize (which includes docking new chrome, since
    // that shrinks the content area too) re-clamps every window,
    // regardless of grow policy: the policy above only decides
    // whether/how a window grows, this still enforces the
    // never-unreachable floor unconditionally.
    for (const LiveWindow& handle : windows) {
        if (desktop_liveness.expired()) return;
        if (!still_owned(handle)) continue;
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

bool Desktop::still_owned(const LiveWindow& handle) const {
    if (handle.liveness.expired()) return false;
    return std::find(windows_.begin(), windows_.end(), handle.window) != windows_.end();
}

void Desktop::tile_in_bands(BandAxis axis) {
    if (windows_.empty()) return;
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    const std::vector<LiveWindow> handles = live_windows();
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
}

void Desktop::tile() { tile_in_bands(BandAxis::Columns); }

void Desktop::tile_vertically() { tile_in_bands(BandAxis::Columns); }

void Desktop::tile_horizontally() { tile_in_bands(BandAxis::Rows); }

void Desktop::tile_grid() {
    if (windows_.empty()) return;
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    const std::vector<LiveWindow> handles = live_windows();
    const Rect area = content_area();
    const int n = static_cast<int>(handles.size());
    // ceil(sqrt(n)) by integer search rather than through std::sqrt: a
    // library that answers 2.9999999 for 9 would lay nine windows out in
    // four columns, and the arrangement would come out different on one
    // machine than on the next — which the determinism rule
    // (AGENTS.md, "identical inputs must yield byte-identical frames")
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
}

void Desktop::cascade() {
    if (windows_.empty()) return;
    const std::weak_ptr<void> desktop_liveness = lifetime_token();
    const std::vector<LiveWindow> handles = live_windows();
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
}

bool Desktop::tiling_participant(const Window& window) const {
    if (!window.visible()) return false;
    // A fixed-size window is an alert, an About box, a message: it sits ON
    // the arrangement, never in it, and it has no band to be resized into.
    if (!window.resizable()) return false;
    // Same reason for one that is resizable but modal — modality is the
    // strongest statement a window makes about being above everything else.
    if (context().app != nullptr && context().app->is_modal_root(window)) return false;
    return true;
}

std::vector<Desktop::TileFraction> Desktop::filled_tile_fractions() const {
    const Rect area = content_area();
    if (area.empty()) return {};

    std::vector<Window*> tiles;
    tiles.reserve(windows_.size());
    for (Window* window : windows_)
        if (tiling_participant(*window)) tiles.push_back(window);
    if (tiles.empty()) return {};

    // Three conditions, and no fourth: every window lies inside the content
    // area, no two of them overlap, and their areas sum to the content
    // area's. Together those are an exact partition — which is why this can
    // answer for an arrangement no tile command produced, and why it stops
    // answering the moment a reader drags one window a single cell off the
    // grid (the drag both uncovers a strip and overlaps a neighbour, and
    // either one alone is already disqualifying).
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
