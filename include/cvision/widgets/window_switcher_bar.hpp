// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// WindowSwitcherBar (U4-a): one row listing every open window by title,
// left-click to activate and raise it, right-click for a host-supplied
// context menu — the arrangement a desktop taskbar uses.
//
// Nothing about WHICH windows are listed, what they are CALLED, what a
// click DOES, or what the menu OFFERS is fixed here. Each is a provider the
// host installs, and the Desktop-bound constructor fills all four in for
// the common case (that desktop's windows, labelled by title, activated and
// raised on click). An application whose notion of a window is its own — a
// session manager listing remote terminals, a shell listing documents
// rather than frames — replaces the providers and keeps the row, the
// layout, the elision, the input handling and the liveness rules.
//
// Paging (U4-l) is not this widget's. It comes from PagedStrip, which the
// bar derives from: the row of variable-width items, the page controls, the
// page index and the collapse toggle are a general one-row strip, and this
// bar is that strip's first consumer exactly as ckmux was this bar's. What
// stays here is the only thing the strip does not know — that an item is a
// WINDOW. The bar supplies each item's width and label, and installs the
// activate and context-menu behaviour on the strip's item callbacks.
//
// Docking: Desktop::dock_bottom holds exactly ONE view per edge, so a bar
// above an application's existing status line is not a second dock. The
// host composes them:
//
//     auto stack = std::make_unique<ui::Column>();
//     stack->add_item(std::make_unique<WindowSwitcherBar>(desktop));
//     stack->add_item(std::make_unique<StatusLine>());
//     desktop.dock_bottom(std::move(stack));
//
// ui::Column's vertical hint sums its children, so Desktop::content_area()
// — and therefore the bounds a maximized window is zoomed into — already
// excludes both rows. Nothing here makes the bar uncoverable; the
// arithmetic that was already there does.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/paged_strip.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// The window one row stands for, plus the only sanctioned way to run
// something against THAT window later.
//
// Why it exists at all: command dispatch has no target. CommandRegistry::
// execute(id) runs one handler, and every window-management handler a
// Desktop installs reads active_window() — so a "Close" chosen from a
// BACKGROUND window's row, wired as MenuItem::command(standard().close),
// closes whichever window happens to be active. That is the wrong window,
// and destructively so. A window's own frame controls never had the problem
// because the close control calls close() on the window it is drawn on;
// this carries that same seam to a menu built somewhere else:
//
//     MenuItem::action("&Close", target.bind([](Window& w) { w.close(); }))
//     MenuItem::action("Ma&ximize", target.bind([&desktop](Window& w) {
//         w.toggle_zoom(desktop.content_area());
//     }))
//
// The bound callback is checked twice before it runs, because the menu
// outlives the click that opened it: the Window instance must still exist
// (a per-instance liveness token, so a later window allocated at the same
// address is not mistaken for this one), and it must still be one of the
// windows the bar draws from. A window closed by a timer, by its own child,
// or by the reader's other hand while the menu stood open therefore ends as
// a no-op rather than as a reference to freed storage.
class WindowSwitcherTarget {
public:
    // `still_listed` answers whether a window is still part of the set the
    // row came from. WindowSwitcherBar supplies one that holds a copy of its
    // window source rather than the bar itself — a host that swaps its
    // chrome while a menu is up must not leave the chosen item calling
    // through a destroyed bar.
    WindowSwitcherTarget(Window& window, std::function<bool(Window*)> still_listed);

    // The window this row was built for, or nullptr once it has been
    // destroyed or has left the listed set.
    Window* window() const;

    // `run`, wrapped as the callback MenuItem::action carries. Never invokes
    // `run` at all once window() would answer nullptr.
    std::function<void()> bind(std::function<void(Window&)> run) const;

private:
    Window* window_ = nullptr;
    std::weak_ptr<void> liveness_;
    std::function<bool(Window*)> still_listed_;
};

// Theme roles, the one-row size hint, the layout, the paging chrome, the
// press-and-release input and the pointer shape all come from PagedStrip —
// including "ckv.statusline.normal"/"ckv.statusline.selected" (D-028) and the
// reasoning behind borrowing the status line's own roles. Everything this
// class adds is about windows.
//
// Keyboard: none, on purpose. Windows are already cycled by the standard
// next/previous-window commands (F6 / Shift+F6) and selected by number; a
// focus stop here would be a second, competing way to walk the same list,
// and Tab would land the reader in the chrome rather than in their work.
class WindowSwitcherBar : public PagedStrip {
public:
    // One row, as the bar currently understands it.
    struct Entry {
        Window* window = nullptr;
        std::string label;
        bool active = false;

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    // One row as it is actually laid out at the current width — exposed so a
    // test can read the text and the columns it occupies without scraping
    // rendered cells. This is PagedStrip::Placement in the bar's own words,
    // and like it, it describes the CURRENT PAGE: a window on another page has
    // no columns to describe.
    struct DrawnEntry {
        std::size_t index = 0;  // into entries()
        int x = 0;              // local column of the entry's first cell
        int width = 0;          // cells the entry occupies, padding included
        // The label as it will be drawn. Elided only where PagedStrip still
        // elides — an entry alone on a page and wider than the whole item
        // area. Every other entry takes its natural width and a window that
        // does not fit moves to the next page rather than losing its name.
        std::string text;
    };

    // Provider-driven: lists nothing until a window source is installed.
    WindowSwitcherBar();

    // The common case, wired to `desktop`: its windows(), their title(), its
    // active_window(), and activate() — which raises to front and marks
    // active in one call — as the click action. Every one of them stays
    // replaceable through the setters below. No context menu until the host
    // supplies one: what belongs on it is the application's vocabulary, not
    // this library's.
    //
    // Also subscribes to that desktop's window-change notification, so the
    // row tracks windows opening, closing, being renamed and changing
    // activation without reading anything per frame. The subscription is
    // bound to this bar's own lifetime and needs no cancelling.
    explicit WindowSwitcherBar(Desktop& desktop);

    ~WindowSwitcherBar() override;

    // --- what the bar is made of ------------------------------------
    // Each setter re-reads the list immediately, so a bar configured after
    // construction is correct without a separate refresh() call.

    void set_window_source(std::function<std::vector<Window*>()> source);
    void set_label_provider(std::function<std::string(Window&)> provider);
    void set_active_provider(std::function<Window*()> provider);
    // What a left-click does. Bound to a Desktop this is activate(), which
    // is both "activate" and "raise"; a host that means something else by
    // clicking a window's name says so here.
    void set_activate_action(std::function<void(Window&)> action);
    // What a right-click offers, for the window that row stands for. An
    // empty result opens no menu. Build items with MenuItem::action and
    // WindowSwitcherTarget::bind — see WindowSwitcherTarget for why
    // MenuItem::command would act on the wrong window.
    void set_context_menu_provider(
        std::function<std::vector<MenuItem>(const WindowSwitcherTarget&)> provider);

    // Re-reads the providers and repaints if the answer changed. Called for
    // the host by the Desktop subscription and by every setter above; a bar
    // driven by a window model of the host's own calls it when that model
    // moves.
    void refresh();

    const std::vector<Entry>& entries() const noexcept { return entries_; }

    // How the entries of the current page fall at the current width — see
    // DrawnEntry. PagedStrip::placed_items() is the same answer in the
    // library's own vocabulary.
    std::vector<DrawnEntry> drawn_entries() const;

    // The entry under an ABSOLUTE cell (a MouseEvent's own coordinates), or
    // nullopt over the bar's empty space, its paging chrome, or a window that
    // is not on the current page.
    std::optional<std::size_t> entry_at(Point cell) const;

    void on_attached() override;

private:
    // The predicate WindowSwitcherTarget checks, built from a copy of the
    // current window source — see WindowSwitcherTarget's constructor.
    WindowSwitcherTarget target_for(Window& window) const;
    // True only if a menu actually opened. A right press the host has nothing
    // to answer with is left unconsumed rather than swallowed, so an
    // application with a desktop-wide context menu of its own still gets it.
    bool open_context_menu(std::size_t index, Point at);
    // The cells one entry's own content needs — today its label, and the one
    // place a leading status glyph would be paid for. This is the answer
    // PagedStrip::Item::width carries, so an entry that grows an icon grows
    // here and the layout follows without being told twice. The strip adds
    // its own padding either side, which is what makes a highlighted entry
    // highlight its margins and read as one row rather than as recoloured
    // words.
    int natural_width(const Entry& entry) const;
    // The item set the strip pages, built from entries_.
    std::vector<Item> strip_items() const;

    std::function<std::vector<Window*>()> window_source_;
    std::function<std::string(Window&)> label_provider_;
    std::function<Window*()> active_provider_;
    std::function<void(Window&)> activate_action_;
    std::function<std::vector<MenuItem>(const WindowSwitcherTarget&)> context_menu_provider_;

    std::vector<Entry> entries_;

    // Where a context-menu popup goes. Found by walking the parent chain at
    // attach, exactly as MenuBar finds the Desktop it drops menus onto:
    // "which desktop owns my popups" is a question about where this view
    // sits, and stays separate from "where do my windows come from", which
    // the providers answer.
    Desktop* popup_host_ = nullptr;
};

}  // namespace ckv::widgets
