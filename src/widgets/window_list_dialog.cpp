// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window_list_dialog.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/ascii.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/list_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// Whether `title` contains `filter`, ASCII letters compared without case and
// every other byte exactly. No locale is consulted: a filter has to match the
// same windows on every machine.
bool title_matches(std::string_view title, std::string_view filter) {
    if (filter.empty()) return true;
    return ascii_lower(title).find(ascii_lower(filter)) != std::string::npos;
}

// The desktop's windows as the list shows them: every window but the dialog
// itself, in the desktop's insertion order — the order select_by_number and
// the cycling commands walk — narrowed to the titles the filter matches.
//
// A window keeps one item id for as long as it lives, so the list's cursor
// stays on the same window across a rename, a filter change, or a neighbour
// closing. Identity is the window's lifetime token as well as its address,
// which a window opened after another one closed may reuse.
class WindowListModel final : public ListModel {
public:
    WindowListModel(const Desktop& desktop, const Window& dialog) : desktop_(desktop), dialog_(dialog) {}

    void rebuild(std::string_view filter) {
        // Forget windows that have gone, so the table does not grow with
        // every window a long session ever opened.
        known_.erase(std::remove_if(known_.begin(), known_.end(),
                                    [](const Known& known) { return known.liveness.expired(); }),
                     known_.end());
        entries_.clear();
        for (Window* window : desktop_.windows()) {
            if (window == &dialog_ || !title_matches(window->title(), filter)) continue;
            entries_.push_back(Entry{window, id_for(*window)});
        }
    }

    // The window an item id names, or nullptr once it has left the desktop.
    Window* window_for(ListItemId id) const {
        const std::optional<std::size_t> index = index_of(id);
        if (!index) return nullptr;
        Window* const window = entries_[*index].window;
        const auto& windows = desktop_.windows();
        return std::find(windows.begin(), windows.end(), window) != windows.end() ? window : nullptr;
    }

    std::size_t item_count() const override { return entries_.size(); }
    ListItem item_at(std::size_t index) const override {
        const Entry& entry = entries_.at(index);
        return ListItem{entry.id, entry.window->title(), std::nullopt};
    }
    std::optional<std::size_t> index_of(ListItemId id) const override {
        const auto entry =
            std::find_if(entries_.begin(), entries_.end(), [id](const Entry& e) { return e.id == id; });
        if (entry == entries_.end()) return std::nullopt;
        return static_cast<std::size_t>(entry - entries_.begin());
    }

private:
    struct Entry {
        Window* window = nullptr;
        ListItemId id = kInvalidListItemId;
    };
    struct Known {
        const Window* window = nullptr;
        std::weak_ptr<void> liveness;
        ListItemId id = kInvalidListItemId;
    };

    ListItemId id_for(const Window& window) {
        for (const Known& known : known_)
            if (known.window == &window && !known.liveness.expired()) return known.id;
        known_.push_back(Known{&window, window.lifetime_token(), next_id_++});
        return known_.back().id;
    }

    const Desktop& desktop_;
    const Window& dialog_;
    std::vector<Entry> entries_;
    std::vector<Known> known_;
    ListItemId next_id_ = 1;
};

// The list of windows, its type-ahead turned into a filter: a character typed
// over the list goes to the filter line above it instead of moving the
// cursor, Backspace takes one back, and Escape clears a filter before it
// dismisses anything. Delete closes the window under the cursor. Everything
// else is the ordinary list.
class WindowListView final : public ListView {
public:
    WindowListView(const Desktop& desktop, const Window& dialog, SearchBox& filter)
        : ListView(false), model_(desktop, dialog), filter_(filter) {
        model_.rebuild({});
        set_model(model_);
    }
    ~WindowListView() override { clear_model(); }

    // Re-reads the desktop through the current filter, keeping the cursor on
    // the window it was on while that window is still listed.
    void refresh() {
        model_.rebuild(filter_.query());
        model_changed();
        if (on_refreshed) on_refreshed();
    }

    // The window under the cursor, or nullptr when nothing is listed.
    Window* cursor_window() const {
        const std::optional<ListItemId> id = cursor_id();
        return id ? model_.window_for(*id) : nullptr;
    }

    // Runs after every refresh, so the dialog can say what the list now
    // holds.
    std::function<void()> on_refreshed;
    // Delete pressed over the list.
    std::function<void()> on_close_request;

    bool on_key(const KeyEvent& event) override {
        const Modifier modifiers = event.chord.modifiers;
        const bool chord = has_modifier(modifiers, Modifier::Alt) || has_modifier(modifiers, Modifier::Ctrl) ||
                           has_modifier(modifiers, Modifier::Super);
        switch (event.chord.key) {
            case Key::Char:
                // A letter with a command modifier is a chord — a button's
                // mnemonic, an accelerator — and not part of the filter.
                if (!chord) return filter_.field().on_key(event);
                break;
            case Key::Backspace:
                if (!filter_.query().empty()) return filter_.field().on_key(event);
                break;
            case Key::Escape:
                // A filter is cleared before anything is dismissed: a reader
                // who typed a few letters and wants the whole list back must
                // not lose the dialog doing it.
                if (!filter_.query().empty()) {
                    filter_.clear();
                    return true;
                }
                break;
            case Key::Delete:
                if (event.action == KeyAction::Press && on_close_request) on_close_request();
                return true;
            default: break;
        }
        return ListView::on_key(event);
    }

    bool on_text(const TextEvent& event) override { return filter_.field().on_text(event); }

private:
    WindowListModel model_;
    SearchBox& filter_;
};

}  // namespace

WindowHandle make_window_list_dialog(Desktop& desktop, const ui::StandardRoles& roles, ui::Application& app,
                                      ui::View* restore_focus_to, const StandardStrings& strings) {
    auto window = std::make_unique<Window>(strings.window_list_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    window->set_resizable(false);
    // A column of air on each side: a list whose text touches the frame reads
    // as clipped even when every title is whole, and the last button's shadow
    // would run into it. Window margins are budgeted in its size hints, so the
    // dialog opens wide enough to keep them.
    window->set_content_margins(1, 0, 1, 0);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    // The filter line. It is edited by typing over the list, so it is never a
    // stop of its own: the keyboard stays on the windows it narrows, and the
    // arrows, Enter and Delete keep meaning what they mean there. A click on
    // its clear control still clears it.
    auto filter = std::make_unique<SearchBox>();
    filter->field().set_focus_policy(ui::FocusPolicy::None);
    auto* filter_ptr = static_cast<SearchBox*>(column->add_item(std::move(filter), LayoutSpec{SizePolicy::Fixed, 1}));

    auto list = std::make_unique<WindowListView>(desktop, *window_ptr, *filter_ptr);
    // A bar that cannot scroll is a dead control, and a reader reads its
    // presence as "there is more" (the same rule the help viewer's link list
    // follows). With three windows open there is not.
    list->set_scrollbar_policy(ScrollbarPolicy::Auto);
    auto* list_ptr =
        static_cast<WindowListView*>(column->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(1);
    auto switch_button = std::make_unique<Button>(strings.switch_to_window);
    switch_button->set_default(true);
    auto* switch_ptr =
        static_cast<Button*>(button_row->add_item(std::move(switch_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto* close_window_ptr = static_cast<Button*>(button_row->add_item(
        std::make_unique<Button>(strings.close_window), LayoutSpec{SizePolicy::Fixed, 1}));
    auto* cancel_ptr = static_cast<Button*>(
        button_row->add_item(std::make_unique<Button>(strings.cancel), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    // Switching activates the window under the cursor and dismisses the list:
    // Enter or a double click on a row, the default button, or Enter anywhere
    // else in the dialog. The list closes first: closing hands the focus back
    // to where the reader was, and the switch is the later word -- the
    // activation carries the focus into the chosen window (D-107), deferred
    // by the Application until the list's modal scope has ended.
    Desktop* desktop_ptr = &desktop;
    const auto switch_to_cursor = [desktop_ptr, window_ptr, list_ptr]() {
        Window* const target = list_ptr->cursor_window();
        if (target == nullptr) return;
        const std::weak_ptr<void> target_liveness = target->lifetime_token();
        window_ptr->close();
        // Closing ran the dialog's own callbacks; the chosen window is only
        // activated if it is still one of this desktop's.
        const std::vector<Window*>& owned = desktop_ptr->windows();
        if (!target_liveness.expired() && std::find(owned.begin(), owned.end(), target) != owned.end())
            desktop_ptr->activate(target);
    };
    list_ptr->on_activate_id = [switch_to_cursor](ListItemId) { switch_to_cursor(); };
    switch_ptr->on_press = switch_to_cursor;
    window->accept_request = switch_to_cursor;

    // Closing a listed window asks that window, through the same vetoable
    // close() its own close control uses: an editor with unsaved work gets to
    // refuse, or to ask, exactly as it would from its frame. The list stays
    // up and follows the desktop — the window leaves the list when it leaves
    // the desktop, whenever that turns out to be.
    const auto close_cursor_window = [list_ptr]() {
        if (Window* const target = list_ptr->cursor_window()) (void)target->close();
    };
    list_ptr->on_close_request = close_cursor_window;
    close_window_ptr->on_press = close_cursor_window;

    cancel_ptr->on_press = [window_ptr]() { window_ptr->close(); };
    window->cancel_request = [window_ptr]() { window_ptr->close(); };

    filter_ptr->on_change = [list_ptr](const std::string&) { list_ptr->refresh(); };
    // The two buttons that act on a listed window are live only while one is
    // listed: an empty desktop, or a filter nothing matches, leaves them
    // nothing to act on, and a control that would do nothing says so.
    list_ptr->on_refreshed = [list_ptr, switch_ptr, close_window_ptr]() {
        const bool any = list_ptr->cursor_window() != nullptr;
        switch_ptr->set_enabled(any);
        close_window_ptr->set_enabled(any);
    };
    list_ptr->refresh();

    // The list follows the desktop while it is up: a window closed from the
    // list, or opened, closed or renamed by the application underneath it,
    // is reflected at once. Bound to the dialog's own lifetime, so a dialog
    // that has gone is never called.
    desktop.subscribe_window_change(
        [list_ptr](Desktop::WindowChange change, Window&) {
            switch (change) {
                case Desktop::WindowChange::Added:
                case Desktop::WindowChange::Removed:
                case Desktop::WindowChange::TitleChanged: list_ptr->refresh(); break;
                case Desktop::WindowChange::Activated:
                case Desktop::WindowChange::Minimized:
                case Desktop::WindowChange::Restored: break;
            }
        },
        window_liveness);

    window->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    return WindowHandle{std::move(window), list_ptr};
}

WindowListDialogPresentation present_modal_window_list_dialog(Desktop& desktop, ui::Application& app,
                                                         const ui::StandardRoles& roles,
                                                         const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<WindowListDialogResult>;
    auto parts = Access::make();
    auto handle = make_window_list_dialog(desktop, roles, app, app.focused(), strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, WindowListDialogResult::Closed);
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
