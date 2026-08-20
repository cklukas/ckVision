// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WindowSwitcherBar (U4-a): one row listing every open window by title and
// by state, left-click to bring it forward or put it away, right-click for
// a host-supplied context menu — the arrangement a desktop taskbar uses,
// including what a taskbar's buttons do (U4-j: see Status and the click
// semantics beside set_minimize_action).
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
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cvision/ui/application.hpp"
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
    // What one row says about its window, in the taskbar's three states
    // (U4-j). Each is drawn as a glyph one space left of the title — see
    // status_glyph, which is the single table that maps the three.
    //
    // The state is also what a left-click MEANS, which is why the two live
    // together: a taskbar entry does not do one thing, it does the thing its
    // icon says is available. See the click semantics below.
    enum class Status {
        Active,     // the window the reader is working in
        Visible,    // on the desktop, behind something else
        Minimized,  // put away, and reachable only from this row
    };

    // The glyph one row draws for each state — the ONE place the three are
    // spelled, so a menu legend, a help page or a host's own listing names
    // the same shapes this bar draws rather than copying literals that then
    // drift apart.
    //
    // All three were checked against ckv::text — this library's own width
    // authority — before being chosen, the check PagedStrip's own steering
    // glyphs record: U+25AE, U+25AF and U+2584 are neither East Asian Wide
    // nor Extended_Pictographic, so no host widens them and no variation
    // selector can turn one into a two-cell emoji. That is why they are
    // these and not the BLACK/WHITE SQUARE U+25A0/U+25A1 the request was
    // first written with: those two are East Asian **Ambiguous**, which
    // D-019 resolves to one column here while a host rendering a CJK font
    // gives them two — and a taskbar whose every entry is one cell wider
    // than the layout believes shears the whole row.
    static std::string_view status_glyph(Status status) noexcept;

    // One row, as the bar currently understands it.
    struct Entry {
        Window* window = nullptr;
        std::string label;
        bool active = false;
        // Whether this window is put away. Kept beside `active` rather than
        // folded into one state member because the two are separately
        // sourced — activity from the active provider, hiddenness from the
        // minimized provider — and status() is where they are resolved into
        // the one answer everything else reads.
        bool minimized = false;
        // The content cells this row's box is CURRENTLY given — which is
        // `natural_width(entry)` unless damping is holding it at an older
        // measurement (see set_width_damping). It is part of the row's
        // identity, and therefore of `operator==`, because a box that changed
        // width is a row that has to be laid out again even when every label
        // on the bar reads exactly as it did.
        int width = 0;

        // The two flags as one answer. Minimized wins over active, and the
        // precedence is stated rather than left to a host's providers to get
        // right: a Desktop never leaves a hidden window active (it moves
        // activation on as the window goes), but a host driving this bar
        // from a model of its own can answer both questions "yes", and a row
        // that drew itself as the reader's current window while the window
        // was nowhere on screen would be the one lie a taskbar must not
        // tell.
        Status status() const noexcept {
            if (minimized) return Status::Minimized;
            return active ? Status::Active : Status::Visible;
        }

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
    // Whether a listed window is put away — the second half of what a row's
    // status icon says, and of what a click on it does. Defaults to
    // Window::minimized(), which is the answer for anything the framework
    // itself hides; a host whose windows are put away by some model of its
    // own (a session that is detached, a document that is closed but
    // listed) says so here, exactly as it says what a window is CALLED
    // through the label provider.
    void set_minimized_provider(std::function<bool(Window&)> provider);

    // --- What a click does, which is what the icon promises -----------
    //
    // The three transitions of a taskbar (U4-j), decided here from the row's
    // own Status so that no host has to re-derive them:
    //
    //   * an ACTIVE row minimizes its window — the reader is pointing at
    //     what they are already in, and the only thing left to ask for is
    //     to put it away;
    //   * a VISIBLE row activates and raises — the plain "bring me that
    //     one";
    //   * a MINIMIZED row comes back, which is the same activate: naming a
    //     hidden window is asking for it, and Desktop::activate restores it
    //     on the way to the front.
    //
    // Hence two verbs rather than one action with a state test inside it.
    // The activate action is the one that was always here and is unchanged;
    // the minimize action is new, and a host that installs neither gets both
    // defaults. A host that installs only its own activate action — to move
    // the keyboard with it, say — keeps the taskbar's semantics for free
    // rather than re-deriving which of the three it is looking at.
    //
    // A window whose minimizable() is false is never minimized by a click on
    // its row: it draws no `_` control on its own frame, and a taskbar that
    // put it away anyway would be a second, hidden route past the gate the
    // window set. Clicking the active row of such a window does nothing,
    // which is what its own frame does too.
    void set_minimize_action(std::function<void(Window&)> action);

    // --- Damped widths ------------------------------------------------
    //
    // How long an entry's box must hold still before it may change width
    // again: `grow_delay_nanos` before it may get wider, `shrink_delay_nanos`
    // before it may get narrower. Both measured on the injected Clock from
    // the last width change of that entry, whichever direction it went in.
    // Zero for either — the default — is no damping at all, and every row
    // takes its natural width the instant its label changes.
    //
    // It exists because a window's title is not a stable string. A shell
    // rewrites its caption at every prompt, a build tool writes its progress
    // into one, an editor appends and removes a dirty marker; each of those
    // is a new measurement, and an undamped taskbar re-sizes the button and
    // re-flows every button beside it each time. The row becomes unreadable
    // long before the titles do, and a reader aiming at a button hits the one
    // that took its place.
    //
    // The two directions get different delays because they are not equally
    // urgent. A row that is too NARROW is showing an elided name, so it
    // should widen soon; a row that is too WIDE is showing the whole name in
    // a box with slack in it, which costs the reader nothing and is worth
    // holding on to — a title that just got shorter is very often about to
    // get longer again.
    //
    // What is damped is the LAYOUT width, not a drawn one. That is the point:
    // the reader's complaint is that the button jitters, and a button's length
    // is the width the strip lays it out at. So page composition becomes a
    // function of time as well as of the labels — as it already is, since a
    // title change re-pages today. Damping does not make a pure function
    // history-dependent; it lengthens the interval over which that function
    // holds still, from "until the next title change" to "at most one width
    // change per delay". Within that interval, paging forward and back returns
    // the reader to exactly the set they came from.
    //
    // A window this bar has not measured before takes its natural width at
    // once. A row that has just appeared has no previous width to flicker
    // between, and starting every new window at zero and growing into its name
    // would be the flicker this exists to remove.
    void set_width_damping(std::int64_t grow_delay_nanos, std::int64_t shrink_delay_nanos);
    std::int64_t grow_delay_nanos() const noexcept { return grow_delay_nanos_; }
    std::int64_t shrink_delay_nanos() const noexcept { return shrink_delay_nanos_; }

    // Forgets one window's damping memory, so its next measurement is taken
    // at once and the delays start again from there — the state a window the
    // bar has never seen is already in.
    //
    // For the change a READER made. Damping exists to absorb a caption a
    // program is rewriting; a reader who has just renamed a window is not
    // flicker to be absorbed, and a rename that visibly took effect a second
    // later — or, for a shorter name, half a minute later — reads as a
    // command that did not work. The host calls this where it knows the
    // difference, which is the one thing this widget cannot know.
    void settle_width(Window& window);

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
    // place a leading status glyph would be paid for. The measurement damping
    // is computed FROM; what the strip is actually given is `Entry::width`,
    // which is this unless a delay is still holding an older number. An entry
    // that grows an icon grows here and both the damping and the layout follow
    // without being told twice. The strip adds its own padding either side,
    // which is what makes a highlighted entry highlight its margins and read
    // as one row rather than as recoloured words.
    int natural_width(const Entry& entry) const;
    // Fills in each rebuilt entry's `width` — carrying the remembered one over
    // where a delay has not elapsed — and arms a wake-up for the earliest
    // deferred change there is. Runs BEFORE the "did anything change?"
    // comparison in refresh(), because a box that is due to widen under a
    // label that did not move is still news for the layout.
    void damp_widths(std::vector<Entry>& rebuilt);
    // Schedules one refresh() at `deadline_nanos`, replacing an armed wake-up
    // that would come later. Nothing else re-reads the widths: a title that
    // changes once and then holds still gives the bar no second chance to
    // apply the growth it deferred, so the bar has to wake itself.
    void arm_damping_wake(std::int64_t deadline_nanos);
    // The item set the strip pages, built from entries_.
    std::vector<Item> strip_items() const;

    std::function<std::vector<Window*>()> window_source_;
    std::function<std::string(Window&)> label_provider_;
    std::function<Window*()> active_provider_;
    std::function<void(Window&)> activate_action_;
    std::function<void(Window&)> minimize_action_;
    std::function<bool(Window&)> minimized_provider_;
    std::function<std::vector<MenuItem>(const WindowSwitcherTarget&)> context_menu_provider_;

    std::vector<Entry> entries_;

    // What one window's box is currently given, and when it was last given it.
    // Keyed by window rather than carried on the Entry alone because entries_
    // is rebuilt from the providers on every refresh, and the whole of damping
    // is a memory of what the width was BEFORE that rebuild. Pruned in the
    // same pass that rebuilds, so a closed window's measurement goes with it.
    struct DampedWidth {
        int shown = 0;
        std::int64_t settled_nanos = 0;
    };
    std::unordered_map<const Window*, DampedWidth> damped_;
    std::int64_t grow_delay_nanos_ = 0;
    std::int64_t shrink_delay_nanos_ = 0;
    // The wake-up that will apply a deferred change, and when it is due. The
    // deadline is kept beside the id so that a change which becomes due
    // SOONER than the armed one replaces it rather than waiting behind it.
    ui::Application::TimerId damping_timer_ = 0;
    std::int64_t damping_wake_nanos_ = 0;

    // Where a context-menu popup goes. Found by walking the parent chain at
    // attach, exactly as MenuBar finds the Desktop it drops menus onto:
    // "which desktop owns my popups" is a question about where this view
    // sits, and stays separate from "where do my windows come from", which
    // the providers answer.
    Desktop* popup_host_ = nullptr;
};

}  // namespace ckv::widgets
