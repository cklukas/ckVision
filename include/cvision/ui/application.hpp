// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Application (the architecture §5 "There is no global state..."):
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
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/clipboard.hpp"
#include "cvision/core/diagnostics.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/capability_report.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/term/terminal_subsession.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {

// run()'s default per-frame deadline (M9/WP-14) — the same 200ms
// every example's own hand-rolled step() loop used before run()
// existed.
inline constexpr std::int64_t kDefaultFrameIntervalNanos = 200'000'000;

// How long an unanswered frame is waited for before it is written off, on a
// host that has never answered one. Short enough that a terminal without the
// facility costs an application a fraction of a second once, rather than a
// stall it has to be restarted out of.
//
// A host that HAS answered is waited for far longer than this — see
// kFrameCompletionPatienceFactor. The two cases look identical for the first
// 250 ms and could not be more different afterwards: one has no answer to
// give, the other is busy drawing the very frame being asked about.
inline constexpr std::int64_t kFrameCompletionTimeoutNanos = 250'000'000;
// How many times the longest answer yet seen a host is given before its
// silence is treated as a lost reply. A terminal whose decoder takes a
// quarter of a second for a large picture must not be mistaken for one that
// does not implement the query — writing off a slow answer removes the very
// back-pressure that would have kept the picture whole.
inline constexpr int kFrameCompletionPatienceFactor = 4;
// Consecutive write-offs after which a host that has NEVER answered is taken
// at its word.
inline constexpr int kFrameCompletionGiveUpCount = 3;

// The double-click rule (MouseEvent::click_count). A press of the same button
// on the same cell no more than this long after the one before it, on the
// injected Clock, is the second press of a double click. No terminal protocol
// reports double clicks, so Application counts them from ordinary presses for
// every view; the injected clock keeps the count deterministic.
inline constexpr std::int64_t kDoubleClickIntervalNanos = 500'000'000;

// The architecture §5 "Sizing policy" (M10/WP-21). Below kMinFullChromeSize,
// nothing new triggers — Window::reposition_within (M8 WP-1/WP-4) already
// clamps every window to fit and stay reachable as root() shrinks, and
// that existing clamped rendering IS the "degraded chrome" the policy
// names; there is no separate degraded visual mode to build. Below
// kHardFloorSize, Application::paint_and_present() instead shows a
// deterministic "terminal too small" message in place of the normal
// View tree — a partial, garbled render (a window frame's own
// box-drawing corners alone need at least 2x2) is worse than a clear,
// honest refusal to render. Recovery is automatic and needs nothing
// undone on either side: the very next frame root() is at least
// kHardFloorSize again, painting resumes normally.
//
// Both sizes are in cells (columns x rows): 80x24 for full chrome, 20x6
// as the floor below which nothing but the message is drawn.
inline constexpr Size kMinFullChromeSize{80, 24};
inline constexpr Size kHardFloorSize{20, 6};

// The one owner of a terminal UI session: the root of the view tree, focus and input routing,
// the command registry, the theme, timers, and the step()/run() loop that turns invalidation into
// presented frames. Not thread-safe apart from post(), wake() and request_quit(); everything else
// runs on the owning thread.
class Application {
public:
    // Builds the application over `terminal` and `clock`, both borrowed and required to outlive
    // it, with a clipboard bridge of its own that exports through the terminal. The root view is
    // sized to the terminal's current size, and the standard help and focus-traversal handlers
    // are installed.
    Application(term::Terminal& terminal, Clock& clock);
    // Explicit host composition: the injected bridge receives portable
    // clipboard exports. Its lifetime must cover this Application. The
    // two-argument convenience constructor instead owns a terminal-backed
    // bridge for this Application instance.
    Application(term::Terminal& terminal, Clock& clock, ClipboardWriter& clipboard_writer);
    // Settles outstanding frame-completion replies, restores the terminal, writes the buffered
    // diagnostics after that restore, and then destroys the root's children while every service
    // their destructors may reach (commands, roles, theme, timers) is still alive.
    ~Application();

    // The root of the view tree, sized to the terminal and resized with it. Applications add
    // their top-level views (a Desktop, a full-screen surface) as its children; children whose
    // View::fills_root() is true track its size. The root itself is owned by the Application.
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

    // Whether root()'s CURRENT size is strictly below kHardFloorSize on
    // either axis — the same condition paint_and_present() itself
    // checks to decide whether to show the "terminal too small" state
    // instead of the normal View tree (see kHardFloorSize's own doc
    // comment). kHardFloorSize itself is still large enough to render.
    bool terminal_too_small() const noexcept;

    // The composed Surface and cursor state behind current_frame() — for
    // golden-dump testing (scene::capture() needs a Surface, not just a
    // FrameView) and the same documentation-tooling use case
    // current_frame() itself exists for. Not part of the interactive
    // rendering path. The surface holds the frame's cells only: the
    // pictures in the frame are compositor().visible_rasters().
    const scene::Surface& composed_surface() const noexcept { return compositor_.frame(); }
    CursorState current_cursor() const noexcept { return compositor_.cursor(); }
    // The compositor that produced composed_surface(), for the same tooling:
    // scene::capture_frame(app.compositor(), app.current_cursor()) is the
    // frame's symbolic golden dump with a `raster` record for every visible,
    // occlusion-sliced picture, which a capture of composed_surface() alone
    // cannot contain. Not part of the interactive rendering path.
    const scene::Compositor& compositor() const noexcept { return compositor_; }

    // Deterministic render-cost counters for the most recently presented
    // frame. They are application-local by construction and support the
    // machine-independent performance gates in the architecture §8.
    std::size_t last_compose_cells_touched() const noexcept {
        return compositor_.last_compose_cells_touched();
    }
    std::size_t last_bytes_emitted() const noexcept { return presenter_.last_bytes_emitted(); }

    // --- Frame completion (the architecture §4, §8) ----------------------
    //
    // Presenting a frame proves that its bytes were written, and nothing
    // more. A terminal that accepts them faster than it draws them leaves
    // an application free to produce frames the reader will never see, at
    // the cost of the responsiveness of every one that follows.
    //
    // Enabled, each presented frame carries a Device Status Report after
    // it; a terminal reads its input in order, so the reply says that
    // frame has been taken in. Nothing waits: the count below is a fact an
    // application may consult, exactly like the cost counters above. The
    // running loop uses the same fact to coalesce expensive frames: every
    // raster frame and every text frame of at least
    // term::kLargeTextFrameBytes must finish before another is sent, while
    // small text interactions remain latency-first.
    //
    // A terminal that never answers must not stall anything, so an
    // unanswered frame is written off after kFrameCompletionTimeoutNanos,
    // and a host that fails to answer kFrameCompletionGiveUpCount times in
    // a row is taken at its word: tracking turns itself off, a diagnostic
    // records it, and frames_awaiting_terminal() reads zero from then on.
    void set_frame_completion_tracking(bool enabled);
    bool frame_completion_tracking() const noexcept { return presenter_.frame_completion_tracking(); }

    // Frames presented that this terminal has not yet reported finishing.
    // Zero means it has caught up — and means nothing at all when tracking
    // is off, which is its value then.
    std::size_t frames_awaiting_terminal() const noexcept {
        const std::size_t marked = presenter_.frames_marked();
        return marked > frames_settled_ ? marked - frames_settled_ : 0;
    }

    // How long the most recently acknowledged frame took to come back,
    // measured on the injected Clock. Negative until one has.
    std::int64_t last_terminal_round_trip_nanos() const noexcept {
        return last_terminal_round_trip_nanos_;
    }

    // Collects the answers to the questions already asked. A session that
    // stops reading with a marked frame outstanding does not cancel that
    // reply — restoring a terminal hands its input queue on rather than
    // discarding it, so `CSI 0 n` is delivered to whatever inherits the
    // queue, and a shell shows the tail of it as typed input. Asking the
    // terminal something therefore obliges this session to stay for the
    // answer.
    //
    // The destructor does this before it restores, which covers an
    // ordinary exit; call it directly when handing the terminal back while
    // this Application stays alive. Terminal events arriving meanwhile are
    // dropped: this is the end of the session's reading, and there is
    // nothing left to deliver them to.
    //
    // Only a host that has answered before is waited for, and only as long
    // as its own slowest answer suggests it needs — the same patience the
    // running loop extends it. Silence is not evidence that a reply is on
    // its way, and an exit is the worst place to spend a timeout
    // rediscovering that.
    void settle_frame_completion();

    // This application's command registry: the standard set, the keymap, handlers, and pushed
    // contexts. Owned by the Application and alive until after every view is destroyed.
    CommandRegistry& commands() noexcept { return commands_; }
    const CommandRegistry& commands() const noexcept { return commands_; }

    // The role registry every view interns its role names into (through Context::roles). The
    // Application does not intern the standard roles itself; call intern_standard_roles() on it.
    RoleRegistry& roles() noexcept { return roles_; }
    // The active theme, built over roles(). Starts empty, so every role resolves to its fallback
    // until a scheme is installed with set_theme().
    Theme& theme() noexcept { return theme_; }
    const Theme& theme() const noexcept { return theme_; }
    // Replace the active scheme and repaint retained views on the next step.
    // `theme` must be built over roles(): it resolves unset roles through
    // whichever registry it was constructed with.
    void set_theme(Theme theme);

    // The application-wide named history lists input lines and other widgets share by key.
    HistoryRegistry& history() noexcept { return history_; }

    // The injected clock every timer, animation and frame deadline is measured on.
    Clock& clock() noexcept { return clock_; }
    const Clock& clock() const noexcept { return clock_; }

    // The outer terminal's current pixel geometry. Views that host a private
    // terminal session use this only to size that child endpoint; child bytes
    // still have no route to the outer Terminal writer.
    PixelSize terminal_cell_pixels() const noexcept { return terminal_.capabilities().cell_pixels; }
    // What this terminal reported and what was concluded from it — the
    // evidence behind graphics placement and pixel-mouse conversion. An
    // application shows or exports this so a misbehaving host can be
    // diagnosed from facts instead of from symptoms.
    term::Capabilities terminal_capabilities() const noexcept { return terminal_.capabilities(); }
    // The same evidence as a built table and as its plain-text export.
    // Fronted here — the ui layer legitimately faces cvision/term — so the
    // widgets layer's terminal report dialog can show terminal evidence
    // without reaching into cvision/term itself (the include-direction rule
    // The architecture §1 enforces).
    std::vector<term::CapabilityReportEntry> terminal_capability_report() const {
        return term::capability_report(terminal_.capabilities(), terminal_.size());
    }
    std::string terminal_capability_report_text() const {
        return term::capability_report_text(terminal_.capabilities(), terminal_.size());
    }
    // How many mouse events have reached dispatch, and the last one. The
    // difference between "the terminal never sent it", "it was decoded to
    // the wrong cell", and "it arrived and nothing wanted it" is otherwise
    // invisible from inside an application — and those three have entirely
    // different causes.
    std::size_t mouse_events_dispatched() const noexcept { return mouse_events_dispatched_; }
    const std::optional<MouseEvent>& last_mouse_event() const noexcept { return last_mouse_event_; }
    // The terminal's current size in cells (columns x rows), as the terminal reports it now; root()
    // follows it once the corresponding resize event has been dispatched.
    Size terminal_cell_grid() const noexcept { return terminal_.size(); }

    // The single deepest view the pointer is over, or nullptr when it is
    // over nothing — outside the root, or on a terminal that never reports
    // where it is. Exposed for the same reason as last_mouse_event():
    // "the pointer is over the wrong view" and "the view under the pointer
    // does not react" are different faults with the same symptom.
    View* hovered_view() const noexcept { return hovered_; }

    // The shape the host's pointer is being asked to take, after this
    // terminal's own degradations have been applied. The value handed to
    // the Presenter on the next frame.
    PointerShape pointer_shape() const noexcept;

    // Whether this terminal can actually show a raster image right now.
    // A caller that would otherwise reserve cells for a picture asks first,
    // rather than leaving a hole where the picture would have been.
    bool terminal_shows_graphics() const noexcept {
        // Only whether it draws images at all. An unknown cell metric is not
        // a refusal — Canvas assumes a cell rather than dropping the image.
        return terminal_.capabilities().sixel_graphics;
    }

    // Launches a contained child session owned by this application.  A
    // TerminalView borrows the returned object; the application therefore
    // guarantees the session outlives all views in its tree.
    term::TerminalSubsession& launch_terminal_subsession(
        term::TerminalLaunchSpec spec, term::TerminalSubsessionOptions options = {});

    // Adopts a child session this application did not launch — a mirror of a
    // session running somewhere else, a recording played back, a fake in a
    // test. From here it is drained, polled and notified exactly as a
    // launched one is, because it joins the same vector each of those loops
    // walks.
    //
    // That sameness is the entire point. Everything a private child session
    // needs from its host is in `step()`: it is drained under a byte budget,
    // its `wait_handles()` are folded into the outer terminal's combined
    // wait, and every view is told when it changed. A caller that could only
    // hand over a launch spec had to reproduce all of that to host a session
    // it built itself, and the part most easily got wrong — the second drain
    // after the poll, without which a child that woke the wait supplies no
    // outer event and its output waits a whole frame — is invisible until it
    // is missing.
    //
    // Ownership transfers, as it does for a launched session, and the return
    // is the same borrowable reference: `TerminalView` borrows it identically
    // and the lifetime rule ("the session outlives every view that borrows
    // it") stays in one place. A caller that kept ownership would be stating
    // that rule a second time, differently.
    term::TerminalSubsession& adopt_terminal_subsession(
        std::unique_ptr<term::TerminalSubsession> session);

    // Hands a session back — launched or adopted — and returns it, or
    // nullptr if this application does not own it.
    //
    // It exists because adoption without it is a one-way door, and the
    // sessions worth adopting are the ones that come and go: a mirror whose
    // far end closed is not a session any more, and without a way out it
    // would be drained on every step and polled on every wait for as long as
    // the application runs. That was equally true of launched sessions, whose
    // closed children accumulated for the life of the process.
    //
    // Safe to call from inside a `notify_terminal_subsession_changed`
    // callback, which is exactly where a host learns that a session has
    // ended: the slot is emptied now and the vector is compacted at the top
    // of the next `step()`, so no loop ever has the ground moved under it.
    // The caller owns the returned session afterwards, and must not release
    // one that a live view still borrows.
    //
    // Takes the core seam rather than the term one, because that is what a
    // change notification carries: `on_terminal_subsession_changed` hands over
    // a `core::TerminalSubsession&`, and a host acting on it should not have to
    // downcast to say "this one is finished".
    std::unique_ptr<term::TerminalSubsession> release_terminal_subsession(
        const core::TerminalSubsession& session);

    // --- Diagnostics (the architecture §11) -------------------------------
    //
    // Every Application owns an in-memory diagnostic buffer. Applications
    // may additionally inject an observing sink (transferring ownership) for
    // structured host logging; messages are always retained in the owned
    // buffer too, then emitted through Terminal only after this Application
    // has restored its session during destruction. This avoids stderr output
    // corrupting an alternate-screen UI while preserving a no-global,
    // per-application observation seam.
    DiagnosticsSink& diagnostics() noexcept;
    void set_diagnostics_sink(std::unique_ptr<DiagnosticsSink> sink);

    // Where this Application's graphics path reports what it does (D-077): its Presenter's
    // refused pictures, encodes, and frame timings, and every owned terminal session's
    // decodes, graphics queries, and resizes. Off by default. The sink and clock are the
    // host's, borrowed until replaced or this Application is destroyed; they are deliberately
    // not the diagnostics buffer above, because a trace is read while the program runs, not
    // after it has restored the terminal. Sessions adopted later receive it too; a released
    // session has it withdrawn.
    void set_graphics_trace(GraphicsTrace trace) noexcept;
    GraphicsTrace graphics_trace() const noexcept { return graphics_trace_; }

    // --- Internal clipboard (the architecture §5, D-022) -------------------
    //
    // Always backs widget cut/copy/paste (in-app selection + export is
    // the primary copy path — mouse reporting captures drags). Writes
    // through to the injected ClipboardWriter on a best-effort basis;
    // the bridge itself decides whether its host supports export. System
    // *import* only ever arrives as a TerminalEvent's TextEvent with
    // from_paste set, which dispatch() also mirrors into this clipboard
    // before routing it onward — never OSC 52 read (D-022).

    // The internal clipboard's text. Setting it replaces the text and writes it through to the
    // ClipboardWriter at once; a paste arriving from the terminal replaces it too.
    void set_clipboard_text(std::string text);
    const std::string& clipboard_text() const noexcept { return clipboard_text_; }

    // --- Timers (part of the loop's "drain input, dispatch, run due
    // timers" batch, the architecture §5) --------------------------------

    // Identifies one start_timer() call. Assigned from 1 upward and never reused, so 0 is free
    // for callers to mean "no timer".
    using TimerId = std::uint64_t;

    // Fires `callback` once `interval_nanos` (on the injected Clock,
    // never wall-clock) has elapsed; if `repeating`, reschedules for
    // `interval_nanos` after the fire time (not "after now" — so a
    // late `step()` call does not accumulate drift across fires).
    // `interval_nanos` must be positive (asserted). Callbacks run on the
    // owning thread inside step(); a repeating timer that fell several
    // intervals behind fires once and keeps its original phase.
    TimerId start_timer(std::int64_t interval_nanos, bool repeating, std::function<void()> callback);
    // Removes the timer; a no-op for an unknown, cancelled or already fired one-shot id. A timer
    // that was already due in the step now running still fires once in that step, even when an
    // earlier callback of the same batch cancels it.
    void cancel_timer(TimerId id);

    // --- Context help (the architecture §5 "Commands and help", D-027) ---
    //
    // F1 is the standard help command's default chord (M9/WP-12); its
    // handler (installed by this constructor) resolves the focused
    // view's nearest help-context key and hands it to this provider —
    // a no-op if none resolves. No provider installed, or F1 consumed
    // earlier in the focus chain: same as any other command whose
    // handler declines to do anything visible. An application wanting
    // completely different F1 behavior replaces the default with its
    // own commands().set_handler(commands().standard().help, ...).
    void set_help_provider(std::function<void(const std::string&)> provider);

    // --- Host capability changes -----------------------------------------
    //
    // A `CapabilityChangedEvent` from the terminal already forces a full
    // repaint on its own (dispatch()); this is for a host that needs to know
    // too, not just redraw. The evidence that resolves a probe (DA1, an
    // XTSMGRAPHICS reply, a DECRQM answer) arrives asynchronously and after
    // this application, and the terminal it wraps, already exist — so a host
    // that reads `terminal.capabilities()` once at construction reads
    // whatever was true before any reply had a chance to land, permanently.
    // Fired after the same `invalidate_all()`, so `terminal.capabilities()`
    // inside the handler reads the value dispatch just repainted for.
    void set_capability_changed_handler(std::function<void()> handler);

    // --- Attention observers ----------------------------------------------
    //
    // For something that follows where the reader is looking rather than
    // any one view -- a tooltip controller, say -- and so cannot learn it
    // from on_focus or on_hover_changed, which each view hears only for
    // itself.
    //
    // What just changed: the keyboard focus (reported once focused() answers
    // the new view, nullptr included), the view under the pointer (once
    // hovered_view() answers the new one), or input about to be routed -- a
    // key press or repeat, a pointer press, a wheel turn or text input,
    // reported after the pointer's hover is brought up to date and before any
    // view or command sees the event. Pointer motion alone is reported only
    // as the hover change it causes; a key release, and a modifier or other
    // standalone key on its own (D-074), not at all.
    enum class AttentionChange { Focus, Hover, Input };
    // Identifies one add_attention_observer() call. Assigned from 1 upward
    // and never reused, so 0 is free for callers to mean "none".
    using AttentionObserverId = std::uint64_t;
    // Calls `observer` for every change listed above, observers in the order
    // they were added. An observer may add or remove observers, itself
    // included, and may move the focus (which reports again, nested). One
    // removed during a notification is not called for it afterwards, and one
    // added during it is first called for the next. An empty function is
    // ignored and returns 0.
    AttentionObserverId add_attention_observer(std::function<void(AttentionChange)> observer);
    // Stops calling that observer; a no-op for an unknown or removed id.
    void remove_attention_observer(AttentionObserverId id) noexcept;

    // --- Mouse input capture -------------------------------------------
    //
    // While set, EVERY MouseEvent routes directly to `view`, bypassing
    // topmost_view_at() entirely (and taking priority even over an
    // in-flight drag's mouse_capture_) — the mechanism a popup (a menu
    // dropdown, M5) uses for light-dismiss: it captures on open, and its
    // own on_mouse checks whether a click landed inside or outside its
    // bounds, dismissing itself on the latter. Cleared automatically
    // (like focused_/mouse_capture_) if `view` is removed or destroyed
    // while captured. Attached views receive Application-managed lifetime
    // validation during pointer dispatch; an unattached composition tree is
    // supported for low-level construction, with lifetime owned by its caller.
    void set_input_capture(View* view);
    void clear_input_capture() noexcept { input_capture_ = nullptr; }
    View* input_capture() const noexcept { return input_capture_; }

    // --- Focus (the architecture §5 "Focus and traversal") ---------------

    // The view holding keyboard focus, or nullptr. Cleared automatically when that view is
    // detached or destroyed; do not keep the pointer across callbacks (use save_focus()).
    View* focused() const noexcept { return focused_; }

    // A lifetime-checked place to return focus after temporary UI is removed.
    // Unlike retaining a raw View pointer, a bookmark cannot mistake a
    // destroyed control for a valid restoration target.
    class FocusBookmark {
    public:
        // An empty bookmark; restoring it clears focus.
        FocusBookmark() = default;

    private:
        // Only the Application reads or fills a bookmark.
        friend class Application;
        View* view_ = nullptr;
        std::weak_ptr<void> liveness_;
    };

    // Records the currently focused view (or no focus). restore_focus() refocuses that view if it
    // is still alive, still in this application's tree, and focusable; otherwise it clears focus.
    // Like set_focus(), a restore into a view outside the active modal scope is deferred until
    // the scope ends.
    FocusBookmark save_focus() const noexcept;
    void restore_focus(const FocusBookmark& bookmark);

    // Sets focus directly. `view` must be nullptr or focusable()
    // (CKV_ASSERT) — a caller wanting to focus a disabled/hidden view
    // must fix that view's state first, not silently no-op here.
    void set_focus(View* view);

    // Tab / Shift-Tab: moves to the next/previous focusable view in a
    // depth-first, declaration-order walk of the tree, wrapping around.
    // Returns false (no-op) when the tree has no focusable view at all.
    bool focus_next();
    bool focus_previous();

    // The same walk confined to `scope`'s subtree, wrapping within it: what
    // a Window offers its controls for the arrow keys they did not use
    // (the architecture §5, D-065). Returns false when the subtree has no
    // focusable view, or lies outside the active modal scope.
    bool focus_next_within(View& scope);
    bool focus_previous_within(View& scope);

    // The first view the Tab walk reaches in `scope`'s subtree, `scope`
    // itself included, passing over hidden and disabled subtrees; nullptr
    // when the subtree has no focus stop. A pure question about the tree: it
    // neither reads nor moves the focus, and it does not consult the modal
    // scope, which set_focus() applies to whatever the caller then does with
    // the answer. A Desktop asks it for the focus stop an activated window
    // opens at (D-107).
    static View* first_focus_stop(View& scope) noexcept;

    // --- Commands --------------------------------------------------------

    // Forwards to commands().set_handler()/execute() (M9/WP-10 moved
    // handler storage into CommandRegistry itself, so
    // CommandRegistry::declare()'s .handler field has somewhere to land
    // without giving the registry an Application dependency) — kept
    // here because callers already have an Application& in scope
    // everywhere dispatch happens. `id` need not be declared yet — a
    // handler may be attached before or after declare() — but
    // execute_command on an id with neither a handler nor a declaration
    // is a no-op.
    void set_command_handler(CommandId id, std::function<void()> handler);
    // CommandRegistry::is_available() and execute() with the command contexts named on the
    // current focus path (up to the active modal root) — the same answer menus and key dispatch
    // get. execute_command() returns whether the handler ran.
    bool command_available(CommandId id);
    bool execute_command(CommandId id);

    // --- Dispatch (the architecture §5 "Events") --------------------------

    // Routes one event per the three explicit routes; returns true if
    // something consumed it. Safe to call directly (e.g. from a test
    // driving a scripted event list) without going through step().
    //
    // Safe to remove or destroy the CURRENTLY focused or mouse-captured
    // view (or any of its ancestors/descendants) from ANY handler —
    // View's detach notification clears focused_/mouse_capture_
    // immediately, so the NEXT dispatch() call never touches a freed
    // view. Dispatch itself is non-reentrant: a handler that needs a
    // later event turn posts work instead of recursively entering it.
    bool dispatch(const term::TerminalEvent& event);

    // --- Cross-thread posting --------------------------------------------

    // Thread-safe; queues `fn` to run on the owning thread during the
    // next step(). An empty callback is a no-op. The only sanctioned way
    // another thread touches this Application (the architecture §5, §9).
    void post(std::function<void()> fn);

    // --- The embeddable loop (D-021) --------------------------------------

    // Polls the terminal up to `deadline_nanos` (clamped earlier if a
    // timer is due sooner), dispatches everything received, runs every
    // due timer, drains posted work, and — only if any of that
    // invalidated the tree — paints the retained base/layer scene and
    // presents it (the architecture §5's "one frame per batch"). Window
    // translation is composition-only; a Window repaint is confined to its
    // dirty local backing. Surface damage further limits compositor work and
    // Presenter emits only changed terminal bytes. Returns true if any event
    // was dispatched, any timer fired, or posted work ran.
    bool step(std::int64_t deadline_nanos);

    // D-021 external-loop integration. The view is borrowed from the terminal
    // backend and every attached private child session; it contains every
    // native source that can make step() useful. It is valid until the next
    // wait_handles() call or until this Application/session set changes. A
    // host waits on it alongside its own sources, then calls step(clock.now_nanos()).
    std::span<const term::WaitHandle> wait_handles() const {
        external_wait_handles_.clear();
        const std::span<const term::WaitHandle> terminal_handles = terminal_.wait_handles();
        external_wait_handles_.insert(external_wait_handles_.end(), terminal_handles.begin(), terminal_handles.end());
        for (const std::unique_ptr<term::TerminalSubsession>& session : terminal_subsessions_) {
            // A released slot is empty until the next step() compacts it.
            if (session == nullptr) continue;
            const std::span<const term::WaitHandle> session_handles = session->wait_handles();
            external_wait_handles_.insert(external_wait_handles_.end(), session_handles.begin(), session_handles.end());
        }
        return external_wait_handles_;
    }

    // The earliest application-owned timer deadline, if any. Combine this
    // with the host's own deadline when waiting on wait_handles(); application
    // state remains confined to the owning thread, so this query is likewise
    // owning-thread-only.
    std::optional<std::int64_t> next_timer_deadline_nanos() const noexcept;

    // Forces the next paint to redraw and re-present the whole frame
    // (e.g. after a runtime theme switch, which invalidates every
    // cell's style without any single View's bounds changing).
    void invalidate_all();

    // Makes a currently blocked step() return promptly through
    // Terminal::wake(). It is safe from another thread and does not
    // synthesize an input event; the owning thread resumes its normal
    // timer/post/paint batch after poll() returns.
    void wake() noexcept;

    // --- Standalone run loop (E8, D-021's convenience, M9/WP-14) ---------

    // The "just run this as a terminal program" convenience: steps
    // repeatedly, each call's deadline kDefaultFrameIntervalNanos out
    // (via the injected Clock), until request_quit() has been called.
    // Signal policy belongs to the terminal session host, never to an
    // Application: run() installs no process-global handlers, so two
    // Applications cannot overwrite one another's policy. An embedding
    // host uses step()/wake() directly; run() is only the simple
    // single-Application loop, defined in terms of run_until() below.
    void run();

    // Steps repeatedly, at run()'s own cadence, until either `done()`
    // returns true or quit_requested() does — whichever comes first —
    // and returns true only for normal completion. A request already
    // present, or observed during a step, returns false before another
    // `done()` invocation; a host shutdown therefore cannot be reported
    // as normal completion. This is the shared primitive
    // run() itself is built on (done = []{ return false; }, so only
    // quit_requested() ends it) and that every widgets::exec_modal_*
    // convenience (M9/WP-15, D-021) uses to block without a nested
    // native loop — neither duplicates run()'s own step-cadence logic,
    // and neither hangs forever if a quit is requested while blocked.
    // Nothing stops an application from calling this directly for its
    // own blocking-wait needs.
    bool run_until(const std::function<bool()>& done);

    // True only when a blocking convenience may begin a loop. Widget
    // helpers use this before attaching their modal Window, so an
    // attempted call from a handler fails deterministically without
    // leaving half-presented state in the tree.
    bool can_run_blocking() const noexcept { return !step_active_ && !dispatch_active_ && !run_active_; }

    // --- Modality (M9/WP-15, D-021) --------------------------------------
    //
    // A modal push scopes event routing to `modal_root`'s own subtree
    // until the matching pop_modal(): mouse hit-testing (topmost_view_at)
    // searches only within it — a click outside is simply not delivered,
    // never falls through to whatever is underneath; Tab/Shift-Tab
    // traversal only cycles views within it; and the keyboard-
    // accelerator command-keymap fallback permits only the scope-safe
    // standard commands focus_next, focus_previous, help, and tooltip
    // after the focus chain declines a key. Those commands operate through
    // the active scope/current focus only. Every other command (menu,
    // window, quit, and application-defined accelerators) is excluded;
    // the focus chain itself runs only up through modal_root — e.g.
    // Window::on_key's accept_request/cancel_request wiring — never
    // past it to whatever it is nested inside. A stack, not a single
    // slot, so a modal opening a second modal on top of it (the vision's
    // "nested modality") scopes correctly to the innermost one without
    // either side needing to know about the other.
    //
    // widgets::exec_modal (and every exec_modal_* built on it) is the
    // sanctioned way most applications reach this — push/pop is public
    // because D-021 makes the modal push itself, not just its blocking
    // wrapper, part of the decided design; an application with its own
    // non-blocking modal need (scoping input without wanting to block
    // the calling code) can call it directly.
    using ModalScopeId = std::uint64_t;

    // Returns the identity of this precise scope. Callers that can be
    // interrupted by quit or a detach retain it and pass it to the
    // overload below; that prevents an outer owner from accidentally
    // popping a newer, nested scope.
    ModalScopeId push_modal(View& modal_root);

    // Pops the most recently pushed modal. Takes no argument and never
    // dereferences the popped View. Application also removes an entry
    // when its root detaches. This strict LIFO convenience CKV_ASSERTs
    // if the stack is empty; owners that retain a particular scope use
    // the identity-taking overload below.
    void pop_modal();

    // Pops `scope` only when it is still the innermost scope. Returns
    // false when the scope already detached or a newer nested modal is
    // active, without changing the stack. This is the cleanup operation
    // for a specific non-blocking or blocking-modal owner.
    bool pop_modal(ModalScopeId scope);

    // Whether any modal scope is active.
    bool is_modal() const noexcept { return !modal_stack_.empty(); }

    // Whether `view` is the root of the innermost active modal scope.
    // Narrower than handing out modal_root() itself, which is a routing
    // internal: a caller asking this wants to treat one view differently
    // for being modal, not to reach into the scope stack.
    bool is_modal_root(const View& view) const noexcept {
        return !modal_stack_.empty() && modal_stack_.back().root == &view;
    }

    // Requests loop shutdown and wakes a currently blocked terminal wait.
    // Also callable directly regardless of whether run() is in use at all —
    // e.g. from a Quit command's own handler — it is a plain flag, not
    // coupled to run() having been called. Backed by std::atomic (relaxed
    // ordering — the flag needs no synchronization with any other state), so
    // an injected host policy may safely request shutdown from another thread
    // without separately remembering to call wake().
    void request_quit() noexcept;
    bool quit_requested() const noexcept { return quit_requested_.load(std::memory_order_relaxed); }

private:
    class ApplicationDiagnostics final : public DiagnosticsSink {
    public:
        void log(LogLevel level, std::string_view message) noexcept override;
        void set_observer(std::unique_ptr<DiagnosticsSink> sink);
        void flush_after_terminal_restore(term::Terminal& terminal) noexcept;

    private:
        BufferedDiagnostics buffered_;
        std::unique_ptr<DiagnosticsSink> observer_;
    };

    struct ViewHandle {
        View* view = nullptr;
        std::weak_ptr<void> liveness;
    };
    struct ModalScope {
        ModalScopeId id = 0;
        View* root = nullptr;
        std::optional<ViewHandle> restore_focus;
    };

    ViewHandle make_view_handle(View& view) const;
    View* resolve_attached_view(const ViewHandle& handle) const noexcept;
    static bool tree_contains(const View& root, const View* target) noexcept;
    static bool is_ancestor_of(const View& ancestor, const View& descendant) noexcept;
    bool in_modal_scope(const View& view) const noexcept;
    void restore_modal_focus_if_needed();
    // Repaints the whole tree when commands_.revision() has moved since the
    // last check: every surface that shows a command's chord or title
    // composes it while drawing, so a rebind reaches them by redrawing.
    void repaint_if_commands_changed();
    bool apply_deferred_focus_request();

    View* topmost_view_at(Point absolute_point) noexcept;
    static View* topmost_view_at_recursive(View& view, Point absolute_point) noexcept;
    // The view a pointer event hitting `hit` is delivered to: `hit` itself,
    // or — when `hit` lies in a disabled subtree — the enabled container just
    // outside it (D-076). Null only when nothing on the route is enabled.
    static View* nearest_enabled_view(View* hit) noexcept;
    // Re-resolves which view the pointer is over and notifies both sides of
    // the transition. `holder` is the view holding mouse capture, if any:
    // during a drag the pointer belongs to whatever it took hold of, so a
    // window being resized keeps its resize pointer even as the pointer
    // travels across everything else on the desktop.
    void update_hover(Point absolute_point, View* holder);
    // Calls every attention observer registered when the notification began.
    void notify_attention(AttentionChange change);
    // Tells every ancestor of the newly focused `target`, nearest first,
    // through View::on_descendant_focused. Stops as soon as a callback moves
    // the focus away from `target`.
    void notify_descendant_focused(View& target);
    static void collect_focusable(View& view, std::vector<View*>& out);
    const std::vector<View*>& focusable_views();
    const std::vector<View*>& focusable_views_within(View& scope);
    bool advance_focus(const std::vector<View*>& views, bool forward);
    const std::vector<std::string>& focused_command_contexts();
    // The innermost active modal, or nullptr if none — the single spot
    // dispatch()/topmost_view_at()/focusable_views() all consult to
    // scope routing (see push_modal's own doc comment above).
    View* modal_root() const noexcept {
        return modal_stack_.empty() ? nullptr : modal_stack_.back().root;
    }
    void layout_root_child(View& child);
    void layout_root_children();
    void paint_and_present();
    // Turns this terminal's replies into the outstanding-frame count,
    // and writes off what it never answered.
    void reconcile_frame_completion(std::int64_t now_nanos);
    // True only when a host known to answer still owes the completion of an
    // expensive frame. While true, dirty_ is retained as the latest scene
    // state rather than presented as another intermediate frame.
    bool frame_backpressure_active() const noexcept;
    // How long this host's silence is given before an unanswered frame is
    // written off — the same patience whether the wait happens between
    // frames or at the end of the session.
    std::int64_t frame_completion_patience_nanos() const noexcept;
    void paint_too_small_state(scene::Painter& painter, Size current_size) const;
    // The mouse half of dispatch(): counts the click, then routes the event.
    bool dispatch_mouse(const MouseEvent& reported);
    // `event`'s MouseEvent::click_count under the double-click rule, recording
    // a press that may begin a double click.
    int count_click(const MouseEvent& event);

    Application(term::Terminal& terminal, Clock& clock,
                std::unique_ptr<ClipboardWriter> owned_clipboard_writer,
                ClipboardWriter* borrowed_clipboard_writer);

    term::Terminal& terminal_;
    Clock& clock_;
    std::unique_ptr<ClipboardWriter> owned_clipboard_writer_;
    ClipboardWriter& clipboard_writer_;
    ApplicationDiagnostics diagnostics_;
    // Declared (and therefore constructed) before root_, and — the
    // reason this order matters, not just convention — destroyed
    // AFTER it: every widget in the tree holds a Context pointing at
    // roles_/theme_ (D-028) and can reach commands_ via context().app
    // (M9/WP-9…WP-13, e.g. MenuBar's destructor clearing its own
    // standard().menu handler). A widget's destructor firing
    // during root_'s own teardown must find these still alive — with
    // commands_/roles_/theme_ declared AFTER root_, they would already
    // be destroyed by the time root_'s children (transitively) run
    // their own destructors, a real dangling-reference bug found via
    // ASan while wiring WP-13's MenuBar-clears-its-own-handler cleanup.
    RoleRegistry roles_;
    Theme theme_{roles_};
    CommandRegistry commands_;
    std::uint64_t painted_command_revision_ = 0;
    // Owned child sessions. A slot may be empty: releasing one from inside a
    // change notification cannot erase from a vector a loop is walking, so it
    // empties the slot and the next step() compacts.
    std::vector<std::unique_ptr<term::TerminalSubsession>> terminal_subsessions_;
    bool terminal_subsessions_need_compaction_ = false;
    std::vector<term::WaitHandle> terminal_subsession_wait_handles_;
    mutable std::vector<term::WaitHandle> external_wait_handles_;
    int next_terminal_raster_identity_ = 1'000'000;
    GraphicsTrace graphics_trace_{};
    // Declared before root_ so it is destroyed AFTER it: root_'s teardown
    // fires the detach sink (set in the constructor), which walks
    // modal_stack_ to drop scopes for departing windows. Declared after
    // root_, the vector's storage was already freed by the time root_'s
    // children ran their destructors — a heap-use-after-free ASan caught
    // once the sanitizer lanes first built on Linux. Same rule, same
    // reason as roles_/theme_/commands_ above.
    std::vector<ModalScope> modal_stack_;
    // Same rule, same sink, same reason as modal_stack_ directly above. When
    // root_'s teardown drops a still-open innermost modal scope, the detach
    // sink hands that scope's saved focus to this member. Declared after root_
    // it had already been destroyed by then, and assigning to a destroyed
    // std::optional re-constructs the ViewHandle — and its weak_ptr — into
    // storage nothing will ever destroy again, pinning the focused View's
    // liveness control block forever. That is LeakSanitizer's "Direct leak of
    // 24 byte(s)" out of View's make_shared, one per test that leaves a modal
    // with a saved focus target standing when the Application goes.
    std::optional<ViewHandle> pending_modal_focus_restore_;
    // A focus request made WHILE a modal scope was up, kept rather than
    // dropped. set_focus refuses such a request — correctly, because focus may
    // not escape the active scope — but refusing is not the same as forgetting,
    // and its own comment says "defer". Declared here for the same lifetime
    // reason as the two members above it.
    std::optional<ViewHandle> deferred_focus_request_;
    View root_;
    // The retained base layer for root-owned chrome/background. Desktop and
    // other retained containers contribute child backing stores separately to
    // composition_layers_; this surface never contains a Window's content.
    scene::Surface surface_;
    std::vector<scene::Layer> composition_layers_;
    scene::Compositor compositor_;
    std::size_t mouse_events_dispatched_ = 0;
    std::optional<MouseEvent> last_mouse_event_;
    // The press a next one may complete a double click with: its button, its
    // cell and when it came, on clock_. Empty once a double click completes.
    struct Press {
        MouseButton button = MouseButton::None;
        Point cell;
        std::int64_t nanos = 0;
    };
    std::optional<Press> last_press_;
    term::Presenter presenter_;
    bool dirty_ = true;  // forces the very first frame to paint+present
    // Frame-completion bookkeeping. `presented_` mirrors the Presenter's own
    // marked count; `settled_` is how many of those the terminal has either
    // answered for or been forgiven.
    std::size_t frames_settled_ = 0;
    std::size_t terminal_acknowledgements_seen_ = 0;
    // Optional rather than a zero sentinel: an injected clock may
    // legitimately read zero, and a round trip measured from a sentinel
    // is measured from the wrong instant.
    std::optional<std::int64_t> oldest_unanswered_frame_nanos_;
    std::int64_t last_terminal_round_trip_nanos_ = -1;
    std::int64_t longest_terminal_round_trip_nanos_ = 0;
    bool terminal_ever_acknowledged_ = false;
    int frame_completion_timeouts_ = 0;
    std::atomic<bool> quit_requested_{false};
    HistoryRegistry history_;
    std::string clipboard_text_;
    View* focused_ = nullptr;
    View* mouse_capture_ = nullptr;
    View* hovered_ = nullptr;
    View* input_capture_ = nullptr;
    // Reused only on the owning thread. These preserve Application's
    // callback-safe snapshot semantics while keeping an already-warmed event
    // and focus path free of transient heap allocation.
    std::vector<ViewHandle> root_layout_scratch_;
    std::vector<ViewHandle> dispatch_route_scratch_;
    std::vector<ViewHandle> mouse_capture_route_scratch_;
    std::vector<View*> focus_scratch_;
    std::vector<std::string> command_context_scratch_;
    ModalScopeId next_modal_scope_id_ = 1;

    bool step_active_ = false;
    bool dispatch_active_ = false;
    bool run_active_ = false;

    std::function<void(const std::string&)> help_provider_;
    std::function<void()> capability_changed_handler_;

    // Application's destructor explicitly detaches and destroys every root
    // child while all members are still alive. After that teardown, destroy
    // callback captures before the retained scene and presenter unwind; a
    // timer callback may own client state whose destructor still refers to
    // those services. This ordering relies on the explicit child teardown --
    // moving it back to root_'s implicit member destruction would again make
    // timers_ too short-lived for Animation::~Animation's cancel_timer().
    struct Timer {
        TimerId id;
        std::int64_t next_fire_nanos;
        std::int64_t interval_nanos;
        bool repeating;
        std::function<void()> callback;
    };
    std::vector<Timer> timers_;
    std::vector<std::function<void()>> due_callback_scratch_;
    TimerId next_timer_id_ = 1;

    // Attention observers, in increasing id order. Each callback is shared so
    // that a notification can hold the one it is running while that observer
    // removes itself, without copying the closure on every event. Declared
    // here, with the timers, for the same teardown-ordering reason.
    struct AttentionObserver {
        AttentionObserverId id;
        std::shared_ptr<const std::function<void(AttentionChange)>> callback;
    };
    std::vector<AttentionObserver> attention_observers_;
    AttentionObserverId next_attention_observer_id_ = 1;

    std::mutex post_mutex_;
    std::vector<std::function<void()>> posted_;
    std::vector<std::function<void()>> posted_work_scratch_;
    bool wake_requested_ = false;
};

}  // namespace ckv::ui
