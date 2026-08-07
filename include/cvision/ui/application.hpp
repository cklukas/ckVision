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

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {

// run()'s default per-frame deadline (M9/WP-14) — the same 200ms
// every example's own hand-rolled step() loop used before run()
// existed.
inline constexpr std::int64_t kDefaultFrameIntervalNanos = 200'000'000;

class Application {
public:
    Application(term::Terminal& terminal, Clock& clock);

    View& root() noexcept { return root_; }
    const View& root() const noexcept { return root_; }

    // The most recently composed frame (whatever paint_and_present()
    // last produced via step()) — a non-owning view into Application's
    // own off-screen state. For tooling that wants to render what's
    // actually on screen outside of the terminal byte stream itself
    // (e.g. a documentation screenshot generator driving a
    // HeadlessTerminal): NOT part of the interactive rendering path,
    // which goes through Presenter/Terminal exclusively.
    FrameView current_frame() const noexcept { return compositor_.frame().view(); }

    // The full composed Surface (raster regions included) and cursor
    // state behind current_frame() — for golden-dump testing
    // (scene::capture() needs a Surface, not just a FrameView) and the
    // same documentation-tooling use case current_frame() itself
    // exists for. Not part of the interactive rendering path.
    const scene::Surface& composed_surface() const noexcept { return compositor_.frame(); }
    CursorState current_cursor() const noexcept { return compositor_.cursor(); }

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

    // --- Timers (part of the loop's "drain input, dispatch, run due
    // timers" batch, ARCHITECTURE.md §5) --------------------------------

    using TimerId = std::uint64_t;

    // Fires `callback` once `interval_nanos` (on the injected Clock,
    // never wall-clock) has elapsed; if `repeating`, reschedules for
    // `interval_nanos` after the fire time (not "after now" — so a
    // late `step()` call does not accumulate drift across fires).
    TimerId start_timer(std::int64_t interval_nanos, bool repeating, std::function<void()> callback);
    void cancel_timer(TimerId id);

    // --- Context help (ARCHITECTURE.md §5 "Commands and help", D-027) ---
    //
    // F1 is std_commands::kHelp's default chord (M9/WP-12); its default
    // handler (installed by this constructor) resolves the focused
    // view's nearest help-context key and hands it to this provider —
    // a no-op if none resolves. No provider installed, or F1 consumed
    // earlier in the focus chain: same as any other command whose
    // handler declines to do anything visible. An application wanting
    // completely different F1 behavior replaces the default with its
    // own commands().set_handler(std_commands::kHelp, ...).
    void set_help_provider(std::function<void(const std::string&)> provider);

    // --- Mouse input capture -------------------------------------------
    //
    // While set, EVERY MouseEvent routes directly to `view`, bypassing
    // topmost_view_at() entirely (and taking priority even over an
    // in-flight drag's mouse_capture_) — the mechanism a popup (a menu
    // dropdown, M5) uses for light-dismiss: it captures on open, and its
    // own on_mouse checks whether a click landed inside or outside its
    // bounds, dismissing itself on the latter. Cleared automatically
    // (like focused_/mouse_capture_) if `view` is removed or destroyed
    // while captured.
    void set_input_capture(View* view) noexcept { input_capture_ = view; }
    void clear_input_capture() noexcept { input_capture_ = nullptr; }
    View* input_capture() const noexcept { return input_capture_; }

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

    // Forwards to commands().set_handler()/execute() (M9/WP-10 moved
    // handler storage into CommandRegistry itself, so
    // CommandRegistry::add()'s .handler field has somewhere to land
    // without giving the registry an Application dependency) — kept
    // here because callers already have an Application& in scope
    // everywhere dispatch happens. `id` need not be pre-registered —
    // a handler may be attached before or after register_command()/
    // add() — but execute_command on an id with neither a handler nor
    // a registration is a no-op.
    void set_command_handler(CommandId id, std::function<void()> handler);
    bool execute_command(CommandId id);

    // --- Dispatch (ARCHITECTURE.md §5 "Events") --------------------------

    // Routes one event per the three explicit routes; returns true if
    // something consumed it. Safe to call directly (e.g. from a test
    // driving a scripted event list) without going through step().
    //
    // Safe to remove or destroy the CURRENTLY focused or mouse-captured
    // view (or any of its ancestors/descendants) from ANY handler —
    // View's detach notification clears focused_/mouse_capture_
    // immediately, so the NEXT dispatch() call never touches a freed
    // view. Known residual gap: destroying an ANCESTOR of the view
    // currently being walked, from WITHIN that same key/text
    // ancestor-chain walk (i.e. mid-dispatch, not merely mid-handler),
    // is not yet hardened — the walk was already holding raw parent()
    // pointers into that subtree before the handler ran. A handler that
    // needs to tear down its own ancestry should defer the actual
    // removal via post() rather than doing it inline.
    bool dispatch(const term::TerminalEvent& event);

    // --- Cross-thread posting --------------------------------------------

    // Thread-safe; queues `fn` to run on the owning thread during the
    // next step(). The only sanctioned way another thread touches this
    // Application (ARCHITECTURE.md §5, §9).
    void post(std::function<void()> fn);

    // --- The embeddable loop (D-021) --------------------------------------

    // Polls the terminal up to `deadline_nanos` (clamped earlier if a
    // timer is due sooner), dispatches everything received, runs every
    // due timer, drains posted work, and — only if any of that
    // invalidated the tree — paints the whole tree into an off-screen
    // Surface and presents it (ARCHITECTURE.md §5's "one frame per
    // batch"). Returns true if any event was dispatched, any timer
    // fired, or posted work ran.
    //
    // v1 scope note: painting is whole-tree, not damage-rect-limited —
    // every View under an invalidated subtree redraws every frame that
    // has ANY damage anywhere. Presenter's own cell-by-cell diff still
    // guarantees the terminal only ever receives bytes for cells that
    // actually changed, so this does not cost extra terminal I/O; it
    // costs extra Painter calls into the off-screen Surface, which is
    // measured (ckvision_bench) rather than assumed acceptable.
    // Damage-rect-scoped painting is a real follow-on, not a
    // correctness gap.
    bool step(std::int64_t deadline_nanos);

    // Forces the next paint to redraw and re-present the whole frame
    // (e.g. after a runtime theme switch, which invalidates every
    // cell's style without any single View's bounds changing).
    void invalidate_all();

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

    // --- Standalone run loop (E8, D-021's convenience, M9/WP-14) ---------

    // The "just run this as a terminal program" convenience: steps
    // repeatedly, each call's deadline kDefaultFrameIntervalNanos out
    // (via the injected Clock), until request_quit() has been called.
    // Installs its own SIGINT handler for the duration — restoring
    // whatever was previously installed when it returns — so Ctrl+C
    // requests a clean quit and main() returns normally, letting
    // Terminal's destructor restore the terminal's original mode,
    // rather than the process dying mid-raw-mode under SIGINT's
    // default disposition. An application embedding ckVision in its
    // own event loop uses step()/wake() directly instead — those stay
    // exactly as they are, with no signal-handling side effect; run()
    // is a separate, optional entry point layered on top of them, not
    // a replacement. Defined in terms of run_until() below (a constant-
    // false predicate — only quit_requested() ever ends it).
    void run();

    // Steps repeatedly, at run()'s own cadence, until either `done()`
    // returns true or quit_requested() does — whichever comes first —
    // and returns done()'s own final value, so a caller can tell
    // "completed normally" from "interrupted by a quit request" (e.g.
    // Ctrl+C arriving while blocked). This is the shared primitive
    // run() itself is built on (done = []{ return false; }, so only
    // quit_requested() ends it) and that every widgets::exec_* modal
    // convenience (M9/WP-15, D-021) uses to block without a nested
    // native loop — neither duplicates run()'s own step-cadence logic,
    // and neither hangs forever if a quit is requested while blocked.
    // Nothing stops an application from calling this directly for its
    // own blocking-wait needs.
    bool run_until(const std::function<bool()>& done);

    // --- Modality (M9/WP-15, D-021) --------------------------------------
    //
    // A modal push scopes event routing to `modal_root`'s own subtree
    // until the matching pop_modal(): mouse hit-testing (topmost_view_at)
    // searches only within it — a click outside is simply not delivered,
    // never falls through to whatever is underneath; Tab/Shift-Tab
    // traversal only cycles views within it; and the keyboard-
    // accelerator command-keymap fallback is suppressed entirely (only
    // the focus chain's own on_key delivery still runs, and only up
    // through modal_root itself — e.g. Window::on_key's accept_request/
    // cancel_request wiring — never past it to whatever it's nested
    // inside). A stack, not a single slot, so a modal opening a second
    // modal on top of it (VISION.md's "nested modality") scopes
    // correctly to the innermost one without either side needing to
    // know about the other.
    //
    // widgets::exec_window (and every exec_* built on it) is the
    // sanctioned way most applications reach this — push/pop is public
    // because D-021 makes the modal push itself, not just its blocking
    // wrapper, part of the decided design; an application with its own
    // non-blocking modal need (scoping input without wanting to block
    // the calling code) can call it directly.
    void push_modal(View& modal_root);

    // Pops the most recently pushed modal. Takes no argument and never
    // dereferences the popped View: by the time an exec_* wrapper is
    // ready to pop, the modal's own Window may already be destroyed —
    // a dialog's standard "close, then self-detach via post()" sequence
    // runs to completion inside the very step() call that produces the
    // result exec_window was waiting on — so popping is by stack
    // position, never by identity. CKV_ASSERT if the stack is empty.
    void pop_modal();

    bool is_modal() const noexcept { return !modal_stack_.empty(); }

    // Makes run()'s next loop check return promptly. Also callable
    // directly regardless of whether run() is in use at all — e.g.
    // from a Quit command's own handler — it is a plain flag, not
    // coupled to run() having been called. Backed by std::atomic
    // (relaxed ordering — the flag needs no synchronization with any
    // other state) because run()'s SIGINT handler calls this on the
    // signal-handler stack: writing/reading a plain bool there
    // concurrently with the rest of the program is undefined
    // behavior, atomic<bool> is the standard-sanctioned way to do it.
    void request_quit() noexcept { quit_requested_.store(true, std::memory_order_relaxed); }
    bool quit_requested() const noexcept { return quit_requested_.load(std::memory_order_relaxed); }

private:
    View* topmost_view_at(Point absolute_point) noexcept;
    static void collect_focusable(View& view, std::vector<View*>& out);
    std::vector<View*> focusable_views() const;
    // The innermost active modal, or nullptr if none — the single spot
    // dispatch()/topmost_view_at()/focusable_views() all consult to
    // scope routing (see push_modal's own doc comment above).
    View* modal_root() const noexcept {
        return modal_stack_.empty() ? nullptr : modal_stack_.back();
    }
    void paint_and_present();

    term::Terminal& terminal_;
    Clock& clock_;
    // Declared (and therefore constructed) before root_, and — the
    // reason this order matters, not just convention — destroyed
    // AFTER it: every widget in the tree holds a Context pointing at
    // roles_/theme_ (D-028) and can reach commands_ via context().app
    // (M9/WP-9…WP-13, e.g. MenuBar's destructor clearing its own
    // std_commands::kMenu handler). A widget's destructor firing
    // during root_'s own teardown must find these still alive — with
    // commands_/roles_/theme_ declared AFTER root_, they would already
    // be destroyed by the time root_'s children (transitively) run
    // their own destructors, a real dangling-reference bug found via
    // ASan while wiring WP-13's MenuBar-clears-its-own-handler cleanup.
    RoleRegistry roles_;
    Theme theme_{roles_};
    CommandRegistry commands_;
    View root_;
    scene::Surface surface_;
    // v1 renders through ONE shared Surface (Window baseline draws
    // through the ordinary View/Painter tree, no per-window backing
    // store yet — see widgets/window.hpp's own scope note), but
    // Compositor is still what turns a Surface's raster regions
    // (Sixel/kitty image content) into the RasterSlices Presenter
    // needs — its occlusion slicing is trivial with a single "layer"
    // covering the whole frame, but reusing the real, tested Compositor
    // path is strictly better than re-deriving that math ad hoc here.
    // `background_layer_` exists only because compose() requires a
    // background distinct from its layers; it is never drawn into.
    scene::Surface background_layer_;
    scene::Compositor compositor_;
    term::Presenter presenter_;
    bool dirty_ = true;  // forces the very first frame to paint+present
    std::atomic<bool> quit_requested_{false};
    HistoryRegistry history_;
    std::string clipboard_text_;
    View* focused_ = nullptr;
    View* mouse_capture_ = nullptr;
    View* input_capture_ = nullptr;
    std::vector<View*> modal_stack_;

    std::function<void(const std::string&)> help_provider_;

    struct Timer {
        TimerId id;
        std::int64_t next_fire_nanos;
        std::int64_t interval_nanos;
        bool repeating;
        std::function<void()> callback;
    };
    std::vector<Timer> timers_;
    TimerId next_timer_id_ = 1;

    std::mutex post_mutex_;
    std::vector<std::function<void()>> posted_;
    bool wake_requested_ = false;
};

}  // namespace ckv::ui
