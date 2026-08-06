// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Application (ARCHITECTURE.md §5 "There is no global state..."):
// owns the terminal, the focus, the command registry, the theme, and
// the embeddable step()/wake() loop (D-021). One Application per
// attached terminal; multiple Applications in one process are
// supported as long as each has its own terminal (a documented
// contract violation otherwise — not detectable from inside the
// library, so it is a caller obligation, not a runtime check here).
//
// Scope note (honest, not an oversight — mirrors docs/input-decoder.md
// and docs/text-width.md's discipline of documenting v1 gaps): the
// typed notification bus (parent-directed / subtree-broadcast state
// changes with per-application extension-type registration) is
// deferred to land alongside the first widgets that actually raise
// notifications — a bus with nothing to carry is unfalsifiable by
// test. Positional (mouse, with drag capture), focused (key/text,
// walking the focus chain then falling through to the command
// keymap), and Application::post() are implemented and tested here.
#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {

class Application {
public:
    Application(term::Terminal& terminal, Clock& clock);

    View& root() noexcept { return root_; }
    const View& root() const noexcept { return root_; }

    CommandRegistry& commands() noexcept { return commands_; }
    const CommandRegistry& commands() const noexcept { return commands_; }

    RoleRegistry& roles() noexcept { return roles_; }
    Theme& theme() noexcept { return theme_; }

    HistoryRegistry& history() noexcept { return history_; }

    // --- Internal clipboard (ARCHITECTURE.md §5, D-022) -------------------
    //
    // Always backs widget cut/copy/paste (in-app selection + export is
    // the primary copy path — mouse reporting captures drags). Writes
    // through to the terminal's system clipboard on a best-effort basis;
    // Terminal::write_clipboard is documented as a no-op when the
    // capability is absent, so this never branches on it itself. System
    // *import* only ever arrives as a TerminalEvent's TextEvent with
    // from_paste set, which dispatch() also mirrors into this clipboard
    // before routing it onward — never OSC 52 read (D-022).

    void set_clipboard_text(std::string text);
    const std::string& clipboard_text() const noexcept { return clipboard_text_; }

    // --- Focus (ARCHITECTURE.md §5 "Focus and traversal") ---------------

    View* focused() const noexcept { return focused_; }

    // Sets focus directly. `view` must be nullptr or focusable()
    // (CKV_ASSERT) — a caller wanting to focus a disabled/hidden view
    // must fix that view's state first, not silently no-op here.
    void set_focus(View* view);

    // Tab / Shift-Tab: moves to the next/previous focusable view in a
    // depth-first, declaration-order walk of the tree, wrapping around.
    // Returns false (no-op) when the tree has no focusable view at all.
    bool focus_next();
    bool focus_previous();

    // --- Commands --------------------------------------------------------

    // Registers the handler a bound key or menu item (M5) invokes.
    // `id` need not be pre-registered in commands() — a handler may be
    // attached before or after register_command() — but execute_command
    // on an id with neither a handler nor a registration is a no-op
    // (nothing to run, nothing to check enablement against).
    void set_command_handler(CommandId id, std::function<void()> handler);

    // Invokes id's handler if one is registered AND commands().is_enabled(id).
    // Returns true if the handler ran.
    bool execute_command(CommandId id);

    // --- Dispatch (ARCHITECTURE.md §5 "Events") --------------------------

    // Routes one event per the three explicit routes; returns true if
    // something consumed it. Safe to call directly (e.g. from a test
    // driving a scripted event list) without going through step().
    bool dispatch(const term::TerminalEvent& event);

    // --- Cross-thread posting --------------------------------------------

    // Thread-safe; queues `fn` to run on the owning thread during the
    // next step(). The only sanctioned way another thread touches this
    // Application (ARCHITECTURE.md §5, §9).
    void post(std::function<void()> fn);

    // --- The embeddable loop (D-021) --------------------------------------

    // Polls the terminal up to `deadline_nanos`, dispatches everything
    // received, then drains posted work. Returns true if any event was
    // dispatched or posted work ran (i.e. something may have
    // invalidated the tree) — callers drive their own paint/present
    // from that signal (M5's Window/Desktop wires paint; M4 has no
    // backing surface yet to paint into).
    bool step(std::int64_t deadline_nanos);

    // Best-effort: makes the NEXT step() call return promptly even if
    // its deadline has not elapsed, by clamping that call's effective
    // poll deadline to "now". Documented scope gap: this cannot
    // interrupt a poll() already in flight on another OS thread — true
    // mid-poll interruption needs backend-level cooperation (a
    // self-pipe or platform waitable handle) that the base Terminal
    // interface does not yet expose. Every current backend's poll()
    // already returns promptly at its own deadline, so the worst case
    // is bounded by that deadline, never unbounded.
    void wake() noexcept;

private:
    View* topmost_view_at(Point absolute_point) noexcept;
    static void collect_focusable(View& view, std::vector<View*>& out);
    std::vector<View*> focusable_views() const;

    term::Terminal& terminal_;
    Clock& clock_;
    View root_;
    CommandRegistry commands_;
    RoleRegistry roles_;
    Theme theme_{roles_};
    HistoryRegistry history_;
    std::string clipboard_text_;
    View* focused_ = nullptr;
    View* mouse_capture_ = nullptr;

    std::unordered_map<CommandId, std::function<void()>> command_handlers_;

    std::mutex post_mutex_;
    std::vector<std::function<void()>> posted_;
    bool wake_requested_ = false;
};

}  // namespace ckv::ui
