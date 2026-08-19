// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window_switcher_bar.hpp"

#include <algorithm>
#include <utility>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

namespace {
// One blank cell either side of a label. It belongs to the entry — a
// highlighted entry highlights its padding too, which is what makes the
// highlight read as one taskbar row rather than as recoloured words. It also
// accepts the click, exactly as its appearance promises.
constexpr int kEntryPadding = 1;
// The blank cell between two entries. Nothing is drawn there: the gap is the
// separation, the same answer StatusLine gives for the same question — a rule
// between every entry turns a bar into a table.
constexpr int kEntryGap = 1;
// The narrowest an entry may be squeezed to: both padding cells plus one cell
// of text, which is exactly enough to carry the elision marker and so exactly
// enough to still say "a window is here".
constexpr int kMinEntryWidth = 2 * kEntryPadding + 1;
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
    return [target = *this, run = std::move(run)] {
        if (!run) return;
        Window* const window = target.window();
        if (window == nullptr) return;
        run(*window);
    };
}

// --- WindowSwitcherBar ----------------------------------------------------

WindowSwitcherBar::WindowSwitcherBar() = default;

WindowSwitcherBar::WindowSwitcherBar(Desktop& desktop) {
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
    // A press whose entry has gone is a press on nothing.
    if (pressed_entry_.has_value() && *pressed_entry_ >= entries_.size()) {
        pressed_entry_.reset();
        pressed_visible_ = true;
    }
    invalidate();
}

int WindowSwitcherBar::natural_width(const Entry& entry) const {
    return 2 * kEntryPadding + text::text_width(entry.label);
}

std::vector<WindowSwitcherBar::DrawnEntry> WindowSwitcherBar::drawn_entries() const {
    const int width = bounds().width;
    const std::size_t count = entries_.size();
    if (count == 0 || width <= 0) return {};

    int natural_total = 0;
    for (const Entry& entry : entries_) natural_total += natural_width(entry);
    const int gaps = (static_cast<int>(count) - 1) * kEntryGap;

    std::vector<int> widths(count, 0);
    if (natural_total + gaps <= width) {
        for (std::size_t i = 0; i < count; ++i) widths[i] = natural_width(entries_[i]);
    } else {
        // Every window keeps a row for as long as one can be told apart.
        // Dropping entries the way a narrow status line drops its items would
        // make a window unreachable from the very bar that exists to reach it,
        // so the bar squeezes first: equal shares, with the remainder handed
        // to the leftmost entries so the row fills its width exactly instead
        // of ending ragged. Only when a share would be too narrow to carry
        // even an elision marker does it stop, and then it lays out as many
        // entries as the minimum allows — at that width there is nothing left
        // to show the rest with.
        const std::size_t affordable =
            static_cast<std::size_t>((width + kEntryGap) / (kMinEntryWidth + kEntryGap));
        const std::size_t shown = std::min(count, affordable);
        if (shown == 0) return {};
        const int space = width - (static_cast<int>(shown) - 1) * kEntryGap;
        const int share = space / static_cast<int>(shown);
        const int remainder = space % static_cast<int>(shown);
        for (std::size_t i = 0; i < shown; ++i)
            widths[i] = share + (static_cast<int>(i) < remainder ? 1 : 0);
    }

    std::vector<DrawnEntry> laid_out;
    laid_out.reserve(count);
    int x = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (widths[i] <= 0) break;
        const int label_columns = std::max(0, widths[i] - 2 * kEntryPadding);
        laid_out.push_back(
            DrawnEntry{i, x, widths[i], text::elide_to_width(entries_[i].label, label_columns)});
        x += widths[i] + kEntryGap;
    }
    return laid_out;
}

std::optional<std::size_t> WindowSwitcherBar::entry_at(Point cell) const {
    const Rect abs = absolute_bounds();
    if (cell.y != abs.y) return std::nullopt;
    const int local_x = cell.x - abs.x;
    for (const DrawnEntry& drawn : drawn_entries())
        if (local_x >= drawn.x && local_x < drawn.x + drawn.width) return drawn.index;
    return std::nullopt;
}

ui::SizeHint WindowSwitcherBar::horizontal_size_hint() const {
    return ui::SizeHint{0, 0, ui::kUnboundedExtent};
}

// Exactly one row, never more and never fewer. This is the number a host's
// ui::Column adds to its status line's own, and the sum is what
// Desktop::content_area() then keeps every window — maximized ones included —
// clear of.
ui::SizeHint WindowSwitcherBar::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void WindowSwitcherBar::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style active = context().theme->resolve(active_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));

    for (const DrawnEntry& drawn : drawn_entries()) {
        // A held entry wears the active entry's colours, because that is what
        // letting go will make it: a taskbar row pressed and a taskbar row
        // already in front are the same promise, and a third appearance would
        // be a distinction with nothing behind it.
        const bool held =
            pressed_entry_.has_value() && *pressed_entry_ == drawn.index && pressed_visible_;
        const Style style = (entries_[drawn.index].active || held) ? active : normal;
        painter.fill(Rect{drawn.x, 0, drawn.width, 1}, Cell::from_grapheme(" ", style));
        if (drawn.text.empty()) continue;
        painter.draw_text(Point{drawn.x + kEntryPadding, 0}, drawn.text, style);
    }
}

std::optional<PointerShape> WindowSwitcherBar::pointer_shape_at(Point local) const {
    const Rect abs = absolute_bounds();
    if (!entry_at(Point{abs.x + local.x, abs.y + local.y})) return std::nullopt;
    return PointerShape::Pointer;
}

void WindowSwitcherBar::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (active_role_ == ui::kInvalidRole)
        active_role_ = context().roles->find("ckv.statusline.selected");
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

bool WindowSwitcherBar::on_mouse(const MouseEvent& event) {
    const std::optional<std::size_t> hit = entry_at(event.cell);

    if (event.button == MouseButton::Right) {
        if (event.action != MouseAction::Down || !hit) return false;
        // The row is deliberately NOT activated by the right press. A reader
        // asking what can be done to a background window has not asked to be
        // taken to it, and taking them there would make every such question
        // reorder the desktop. Which is precisely why the menu's items are
        // bound to their own window instead of dispatched at whatever holds
        // focus — see WindowSwitcherTarget.
        return open_context_menu(*hit, event.cell);
    }

    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || !hit) return false;
        // Show the press before acting on it: a row that activates a window
        // with no visible acknowledgement leaves the reader unsure it was hit.
        pressed_entry_ = hit;
        pressed_visible_ = true;
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Move) {
        if (!pressed_entry_.has_value()) return false;
        const bool over = hit == pressed_entry_;
        if (over != pressed_visible_) {
            pressed_visible_ = over;
            invalidate();
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        const std::optional<std::size_t> pressed = pressed_entry_;
        pressed_entry_.reset();
        pressed_visible_ = true;
        invalidate();
        // Releasing away from the entry it started on takes the press back.
        if (!pressed.has_value() || hit != pressed) return pressed.has_value();
        Window* const window = entries_[*pressed].window;
        if (window != nullptr && activate_action_) activate_action_(*window);
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
