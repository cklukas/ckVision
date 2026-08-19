// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window_switcher_bar.hpp"

#include <algorithm>
#include <utility>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

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
    return [target = *this, run = std::move(run)] {
        if (!run) return;
        Window* const window = target.window();
        if (window == nullptr) return;
        run(*window);
    };
}

// --- WindowSwitcherBar ----------------------------------------------------

WindowSwitcherBar::WindowSwitcherBar() {
    // The three seams between "a strip of items" and "a strip of WINDOWS".
    // Everything else the bar used to do itself — measuring the row, laying
    // it out, drawing it, tracking a press across a drag — is PagedStrip's,
    // and paging came with it.
    set_item_source([this] { return strip_items(); });
    on_item_activated = [this](std::size_t index) {
        if (index >= entries_.size()) return;
        Window* const window = entries_[index].window;
        if (window != nullptr && activate_action_) activate_action_(*window);
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

// No unsubscribe: see the subscription above.
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

void WindowSwitcherBar::set_context_menu_provider(
    std::function<std::vector<MenuItem>(const WindowSwitcherTarget&)> provider) {
    context_menu_provider_ = std::move(provider);
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
                                    window == active});
        }
    }
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
    return text::text_width(entry.label);
}

std::vector<PagedStrip::Item> WindowSwitcherBar::strip_items() const {
    std::vector<Item> items;
    items.reserve(entries_.size());
    for (const Entry& entry : entries_)
        items.push_back(Item{natural_width(entry), entry.label, entry.active});
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
