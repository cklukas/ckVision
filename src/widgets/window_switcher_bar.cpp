// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window_switcher_bar.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {
namespace {

// How long a deferred width change waits before asking again, while a press
// is in flight. A press is bounded by the reader letting go, and nothing
// notifies this bar when that happens — the release is PagedStrip's and stops
// there — so this is a re-ask rather than a wait for the press to end. Short
// enough that a change comes due within a frame or two of the release, long
// enough that holding a taskbar button down is not a wake-up per tick.
constexpr std::int64_t kPressRetryNanos = 100'000'000;

// The status marks (U4-j), and the blank between a mark and the name it
// belongs to. They are the window FRAME's own controls, drawn on the bar:
// U+25A0 is the square a window wears in its close control, and U+005F is the
// line it wears in its minimize control. A row therefore shows the reader the
// same two characters the window it stands for shows, and there is one
// vocabulary of window chrome to learn rather than two.
//
// WHICH of the two a row draws says where the window is: on the desktop, or
// put away — flattened onto this bar by the very control the mark is copied
// from. Which window the reader is IN is said the way the desktop itself says
// it, in colour: Window::draw lights its controls on the active window and
// lets them fall back to the frame's own style on every other one, and a row
// does exactly that through Item::icon_role. Under the active row the strip
// also draws its selected highlight, and that is what carries the answer on a
// terminal with no colour at all.
//
// U+25A0 is East Asian Ambiguous, which D-019 resolves to one column here —
// the same resolution PagedStrip's own steering triangles rest on, and the
// same one the frame's close control has rested on since it was drawn. A host
// resolving Ambiguous the other way widens the frame control and this mark
// together; it cannot make the two disagree, which is the property this bar
// now trades a third distinct shape for.
constexpr std::string_view kOpenGlyph = "■";       // U+25A0, the frame's close control
constexpr std::string_view kMinimizedGlyph = "_";  // U+005F, the frame's minimize control
constexpr int kStatusGap = 1;

}  // namespace

// --- WindowSwitcherTarget -------------------------------------------------

WindowSwitcherTarget::WindowSwitcherTarget(Window& window, std::function<bool(Window*)> still_listed)
    : window_(&window), liveness_(window.lifetime_token()), still_listed_(std::move(still_listed)) {}

Window* WindowSwitcherTarget::window() const {
    // Two questions, and both have to be asked. The token settles identity: a
    // destroyed window can be replaced by a freshly allocated one at the same
    // address, and running a Close against that one would close a window the
    // reader never pointed at. The predicate settles membership: a window
    // detached from the set the row came from is no longer the row's window
    // even while the instance lives on in whoever took ownership of it.
    if (window_ == nullptr || liveness_.expired()) return nullptr;
    if (still_listed_ && !still_listed_(window_)) return nullptr;
    return window_;
}

std::function<void()> WindowSwitcherTarget::bind(std::function<void(Window&)> run) const {
    return [target = *this, held_run = std::move(run)] {
        if (!held_run) return;
        Window* const window = target.window();
        if (window == nullptr) return;
        held_run(*window);
    };
}

// --- WindowSwitcherBar ----------------------------------------------------

std::string_view WindowSwitcherBar::status_glyph(Status status) noexcept {
    switch (status) {
        case Status::Minimized:
            return kMinimizedGlyph;
        // A window in front and a window behind it are in the same PLACE, and
        // the mark says place. What tells them apart is the colour the row
        // draws the mark in and the highlight underneath it — see the glyph
        // table above, and strip_items for where the role is chosen.
        case Status::Active:
        case Status::Visible:
            break;
    }
    return kOpenGlyph;
}

WindowSwitcherBar::WindowSwitcherBar() {
    // The three seams between "a strip of items" and "a strip of WINDOWS".
    // Everything else the bar used to do itself — measuring the row, laying
    // it out, drawing it, tracking a press across a drag — is PagedStrip's,
    // and paging came with it.
    set_item_source([this] { return strip_items(); });
    // Every window is put away the same way unless a host says otherwise,
    // because minimizing is the window's own verb and needs no desktop —
    // unlike activating, which is a question about a set of windows and is
    // therefore left null until a constructor or a host answers it.
    minimize_action_ = [](Window& window) { window.set_minimized(true); };
    minimized_provider_ = [](Window& window) { return window.minimized(); };
    on_item_activated = [this](std::size_t index) {
        if (index >= entries_.size()) return;
        // Read out BEFORE either action runs. Both of them end in a desktop
        // notification, which comes straight back through refresh() and
        // rebuilds entries_ — so anything still pointing into that vector
        // afterwards would be pointing into freed storage.
        const Entry& entry = entries_[index];
        Window* const window = entry.window;
        const Status status = entry.status();
        if (window == nullptr) return;
        // The taskbar's three transitions, in the one place they are
        // decided — see set_minimize_action for why this is not left to
        // each host to work out from a click.
        if (status == Status::Active) {
            if (window->minimizable() && minimize_action_) minimize_action_(*window);
            return;
        }
        // Visible-but-behind and minimized are the same request: bring me
        // that one. Desktop::activate restores a hidden window on its way to
        // the front, so no second verb is needed for the difference.
        if (activate_action_) activate_action_(*window);
    };
    on_item_context_press = [this](std::size_t index, Point at) {
        return open_context_menu(index, at);
    };
}

WindowSwitcherBar::WindowSwitcherBar(Desktop& desktop) : WindowSwitcherBar() {
    Desktop* const bound = &desktop;
    // Every default holds the desktop weakly as well as by address. A bar can
    // be detached and kept alive by a host that then drops the desktop, and a
    // raw pointer alone would not say so — nor would it tell a later Desktop
    // allocated at that address apart from this one.
    const std::weak_ptr<void> liveness = desktop.lifetime_token();
    window_source_ = [bound, liveness]() -> std::vector<Window*> {
        if (liveness.expired()) return {};
        return bound->windows();
    };
    active_provider_ = [bound, liveness]() -> Window* {
        if (liveness.expired()) return nullptr;
        return bound->active_window();
    };
    activate_action_ = [bound, liveness](Window& window) {
        if (liveness.expired()) return;
        // activate() asserts against a window it does not own, and the row
        // was read before the click landed: a window closed in between has to
        // be a quiet no-op, not an assertion in the reader's face.
        const std::vector<Window*>& windows = bound->windows();
        if (std::find(windows.begin(), windows.end(), &window) == windows.end()) return;
        bound->activate(&window);
    };
    // Bound to this bar's own lifetime, so nothing has to cancel it: a docked
    // bar is destroyed as part of the Desktop that owns it, and a destructor
    // running at that point would be reaching into a Desktop whose own members
    // are already gone.
    desktop.subscribe_window_change([this](Desktop::WindowChange, Window&) { refresh(); },
                                    lifetime_token());
    refresh();
}

// No unsubscribe: see the subscription above. The damping wake-up is not
// cancelled here either, and for the same shape of reason: it is a one-shot
// holding a liveness token, so a bar destroyed before it fires makes it a
// no-op, and reaching for context().app from a destructor running as part of
// a teardown is reaching into whatever else that teardown has already freed.
WindowSwitcherBar::~WindowSwitcherBar() = default;

void WindowSwitcherBar::set_window_source(std::function<std::vector<Window*>()> source) {
    window_source_ = std::move(source);
    refresh();
}

void WindowSwitcherBar::set_label_provider(std::function<std::string(Window&)> provider) {
    label_provider_ = std::move(provider);
    refresh();
}

void WindowSwitcherBar::set_active_provider(std::function<Window*()> provider) {
    active_provider_ = std::move(provider);
    refresh();
}

void WindowSwitcherBar::set_activate_action(std::function<void(Window&)> action) {
    activate_action_ = std::move(action);
}

void WindowSwitcherBar::set_minimized_provider(std::function<bool(Window&)> provider) {
    minimized_provider_ = std::move(provider);
    refresh();
}

void WindowSwitcherBar::set_minimize_action(std::function<void(Window&)> action) {
    minimize_action_ = std::move(action);
}

void WindowSwitcherBar::set_context_menu_provider(
    std::function<std::vector<MenuItem>(const WindowSwitcherTarget&)> provider) {
    context_menu_provider_ = std::move(provider);
}

void WindowSwitcherBar::set_width_damping(std::int64_t grow_delay_nanos,
                                          std::int64_t shrink_delay_nanos) {
    // Negatives are read as "off" rather than asserted on: this is a duration
    // a host computes from a setting, and the answer to a nonsensical one is
    // an undamped bar, not a dead application.
    grow_delay_nanos_ = std::max<std::int64_t>(0, grow_delay_nanos);
    shrink_delay_nanos_ = std::max<std::int64_t>(0, shrink_delay_nanos);
    // Whatever was being held is released at once. Turning damping off has to
    // mean the widths are right NOW; leaving a stale box in place until its
    // old delay elapsed would make the setting take effect at a moment the
    // host never chose.
    damped_.clear();
    refresh();
}

void WindowSwitcherBar::settle_width(Window& window) {
    // Erased rather than assigned: "no memory of this window" is already the
    // state that takes the next measurement at once, so there is no second
    // rule here to disagree with the one in damp_widths().
    if (damped_.erase(&window) == 0) return;
    refresh();
}

void WindowSwitcherBar::refresh() {
    std::vector<Entry> rebuilt;
    if (window_source_) {
        const std::vector<Window*> windows = window_source_();
        Window* const active = active_provider_ ? active_provider_() : nullptr;
        rebuilt.reserve(windows.size());
        for (Window* window : windows) {
            if (window == nullptr) continue;
            rebuilt.push_back(Entry{window,
                                    label_provider_ ? label_provider_(*window) : window->title(),
                                    window == active,
                                    minimized_provider_ && minimized_provider_(*window)});
        }
    }
    // What each box is given, which is its natural width unless a delay is
    // still holding an older one. Before the comparison below rather than
    // after it: a width that has just come due is news even under a label that
    // did not move, and it is the only news there is when the wake-up armed
    // here is what called this.
    damp_widths(rebuilt);
    // Repaint only on a real change. Window::set_title reports every
    // assignment, an identical one included, and an application that refreshes
    // a caption on a timer would otherwise leave this row permanently dirty.
    if (rebuilt == entries_) return;
    entries_ = std::move(rebuilt);
    // The window set moved, so the pages have to be recomputed and the current
    // one revalidated: a terminal closing must not leave the reader looking at
    // a page that no longer exists. PagedStrip owns that rule; this is where
    // the bar tells it the set changed.
    refresh_items();
}

int WindowSwitcherBar::natural_width(const Entry& entry) const {
    // Icon, blank, name. Measured rather than assumed to be two cells, so a
    // later table of wider glyphs is paid for here and both the damping and
    // the paging follow without being told.
    return text::text_width(status_glyph(entry.status())) + kStatusGap +
           text::text_width(entry.label);
}

void WindowSwitcherBar::damp_widths(std::vector<Entry>& rebuilt) {
    const bool damping = grow_delay_nanos_ > 0 || shrink_delay_nanos_ > 0;
    ui::Application* const app = context().app;
    // Undamped, or attached to nothing that can tell the time: every box takes
    // its natural width. Not an assertion — a bar is measured and laid out
    // before it is ever attached, and a widget that refused to size itself
    // until it had a clock would have no size to report at that point.
    if (!damping || app == nullptr) {
        for (Entry& entry : rebuilt) entry.width = natural_width(entry);
        damped_.clear();
        return;
    }

    // Not while a reader's finger is down. PagedStrip resolves a click by
    // INDEX — the press remembers one and the release fires for whatever item
    // now sits at it — so a box that changed width between the two either
    // moves the item out from under the pointer, and the click does nothing,
    // or shifts the columns so the release names a window the reader never
    // pointed at. Every other thing that re-lays this row out is an event the
    // reader or the host caused; a damping wake-up is the one that can land in
    // the middle of a click with nothing else happening at all, which is why
    // it is the one that has to hold still. Every remembered width is carried
    // over untouched and the wake-up is re-armed for the moment the press is
    // expected to be over.
    if (press_in_flight()) {
        for (Entry& entry : rebuilt) {
            const auto remembered = damped_.find(entry.window);
            entry.width = remembered == damped_.end() ? natural_width(entry) : remembered->second.shown;
        }
        arm_damping_wake(app->clock().now_nanos() + kPressRetryNanos);
        return;
    }

    const std::int64_t now = app->clock().now_nanos();
    // The earliest moment a deferred change becomes due, over every entry.
    // One wake-up for the bar rather than one per window: what a due change
    // costs is a re-layout of the whole row, so waking once and re-reading
    // everything is both cheaper and the only order in which the widths that
    // come due together are applied together.
    std::int64_t next_wake = 0;

    std::unordered_map<const Window*, DampedWidth> kept;
    kept.reserve(rebuilt.size());
    for (Entry& entry : rebuilt) {
        const int natural = natural_width(entry);
        const auto remembered = damped_.find(entry.window);
        // A window this bar has not measured before takes its natural width
        // now. There is no previous width for it to flicker between, and
        // growing a new row into its own name would BE the flicker.
        if (remembered == damped_.end()) {
            entry.width = natural;
            kept.emplace(entry.window, DampedWidth{natural, now});
            continue;
        }

        DampedWidth held = remembered->second;
        if (natural != held.shown) {
            // One stamp for both directions, deliberately. It is what makes
            // the promise the plain one — at most one change per grow delay,
            // and at most one narrowing per shrink delay — rather than two
            // clocks that can both come due at once and step a box twice.
            const std::int64_t delay = natural > held.shown ? grow_delay_nanos_ : shrink_delay_nanos_;
            const std::int64_t due = held.settled_nanos + delay;
            if (now >= due) {
                held.shown = natural;
                held.settled_nanos = now;
            } else if (next_wake == 0 || due < next_wake) {
                next_wake = due;
            }
        }
        entry.width = held.shown;
        kept.emplace(entry.window, held);
    }
    // Rebuilt rather than erased-from: a window that has closed is gone from
    // `rebuilt`, and carrying its measurement forever would make this map grow
    // for the whole life of a long session.
    damped_ = std::move(kept);
    if (next_wake != 0) arm_damping_wake(next_wake);
}

void WindowSwitcherBar::arm_damping_wake(std::int64_t deadline_nanos) {
    ui::Application* const app = context().app;
    if (app == nullptr) return;
    // An armed wake-up that would come sooner already covers this one. The
    // one that comes LATER is replaced, because a change that is due at once
    // must not sit behind a shrink that is thirty seconds out.
    if (damping_timer_ != 0 && damping_wake_nanos_ <= deadline_nanos) return;
    if (damping_timer_ != 0) app->cancel_timer(damping_timer_);
    const std::int64_t delay = std::max<std::int64_t>(0, deadline_nanos - app->clock().now_nanos());
    const std::weak_ptr<void> liveness = lifetime_token();
    damping_wake_nanos_ = deadline_nanos;
    damping_timer_ = app->start_timer(delay, /*repeating=*/false, [this, liveness] {
        // One-shot, so nothing has to be cancelled on the way out — but the
        // bar can be destroyed between arming and firing, and a callback that
        // read `this` then would be reading freed storage (View's own
        // per-instance liveness token, the same guard the desktop
        // subscription above uses).
        if (liveness.expired()) return;
        damping_timer_ = 0;
        damping_wake_nanos_ = 0;
        // Straight back through refresh(), which re-reads the providers and
        // re-runs the damping: by now more than one width may have come due,
        // and a wake-up that applied only the one it was armed for would need
        // a second wake-up to apply the rest.
        refresh();
    });
}

std::vector<PagedStrip::Item> WindowSwitcherBar::strip_items() const {
    std::vector<Item> items;
    items.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        const Status status = entry.status();
        // The icon is part of the item's TEXT rather than something the
        // strip knows about: PagedStrip pages words of a given width, and
        // what those words say is the consumer's business. It also puts the
        // glyph on the left, where an elision that had to cut a lone
        // over-wide entry takes the tail of the name and leaves the state
        // legible.
        std::string text;
        text.reserve(status_glyph(status).size() + 1U + entry.label.size());
        text.append(status_glyph(status));
        text.append(kStatusGap, ' ');
        text.append(entry.label);
        // Selected is the ACTIVE row alone. A minimized window is not the
        // one the reader is in, whatever a host's active provider says —
        // status() has already settled that, and the highlight follows it
        // rather than asking again.
        // The mark's own style, and the one place the frame's rule is
        // copied: the ACTIVE row's mark wears the control colour, every
        // other row lets it fall back to the row's own — which is exactly
        // what Window::draw does with its controls, and why a desktop of
        // windows shows one set of live controls rather than several
        // competing for the eye.
        items.push_back(Item{entry.width, std::move(text), status == Status::Active,
                             text::text_width(status_glyph(status)),
                             status == Status::Active ? control_role_ : ui::kInvalidRole});
    }
    return items;
}

std::vector<WindowSwitcherBar::DrawnEntry> WindowSwitcherBar::drawn_entries() const {
    const std::vector<Placement> placed = placed_items();
    std::vector<DrawnEntry> laid_out;
    laid_out.reserve(placed.size());
    for (const Placement& item : placed)
        laid_out.push_back(DrawnEntry{item.index, item.x, item.width, item.text});
    return laid_out;
}

std::optional<std::size_t> WindowSwitcherBar::entry_at(Point cell) const { return item_at(cell); }

void WindowSwitcherBar::on_attached() {
    // Theme roles and the first item read are the strip's.
    PagedStrip::on_attached();
    // Except one: the mark on the active row is a window CONTROL, and it is
    // resolved from the window family's own role rather than the status
    // line's, by the same name Window::on_attached resolves it by. A theme
    // that retints window controls therefore retints the bar's marks with
    // them, which is the whole point of drawing the frame's characters here.
    if (control_role_ == ui::kInvalidRole)
        control_role_ = context().roles->find("ckv.window.control");
    // Where a context menu goes, found the way MenuBar finds the desktop it
    // drops its own menus onto. Separate from wherever the windows come from:
    // one is a fact about where this view sits, the other is the host's model.
    for (ui::View* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
        if (auto* desktop = dynamic_cast<Desktop*>(ancestor)) {
            popup_host_ = desktop;
            break;
        }
    }
    refresh();
}

WindowSwitcherTarget WindowSwitcherBar::target_for(Window& window) const {
    // The predicate holds a COPY of the window source rather than this bar. A
    // menu outlives the click that opened it, and a host that swaps its chrome
    // while one is up would otherwise leave the chosen item asking a destroyed
    // bar whether its window is still listed.
    return WindowSwitcherTarget(window, [source = window_source_](Window* candidate) {
        if (!source) return false;
        const std::vector<Window*> windows = source();
        return std::find(windows.begin(), windows.end(), candidate) != windows.end();
    });
}

bool WindowSwitcherBar::open_context_menu(std::size_t index, Point at) {
    if (!context_menu_provider_ || popup_host_ == nullptr) return false;
    ui::Application* const app = context().app;
    if (app == nullptr) return false;
    if (index >= entries_.size()) return false;
    Window* const window = entries_[index].window;
    if (window == nullptr) return false;
    std::vector<MenuItem> items = context_menu_provider_(target_for(*window));
    // A host with nothing to offer for this window offers no menu at all,
    // rather than an empty popup the reader then has to dismiss.
    if (items.empty()) return false;
    show_context_menu(std::move(items), at, *app, *popup_host_);
    return true;
}

}  // namespace ckv::widgets
