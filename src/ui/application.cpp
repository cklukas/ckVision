// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/application.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <type_traits>

#include "cvision/core/assert.hpp"
#include "cvision/term/terminal_clipboard.hpp"

namespace ckv::ui {

namespace {

bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

// A modal blocks commands that address application/background state, but its
// own focused controls still need the framework keymap used to navigate and
// ask for context help. These commands act exclusively through the current
// focus and Application's modal-aware traversal, so executing them cannot
// route into a background view. Window/menu/quit commands and every
// application-defined command remain outside this deliberately small set.
bool is_modal_scope_command(const StandardCommands& standard, CommandId id) noexcept {
    return id == standard.focus_next || id == standard.focus_previous || id == standard.help;
}

// The "terminal too small" state's own fixed style (M10/WP-21) — NOT
// resolved through theme()/roles(): this is a last-resort framework
// fallback that must render sanely even for an application that never
// configured a theme at all, so it cannot depend on that configuration
// having happened correctly.
constexpr Color kTooSmallFg = Color::rgb(255, 255, 255);
constexpr Color kTooSmallBg = Color::rgb(170, 0, 0);

class ActiveOperation final {
  public:
    explicit ActiveOperation(bool& active) : active_(active) {
        CKV_ASSERT(!active_);
        active_ = true;
    }
    ~ActiveOperation() { active_ = false; }

    ActiveOperation(const ActiveOperation&) = delete;
    ActiveOperation& operator=(const ActiveOperation&) = delete;

  private:
    bool& active_;
};

} // namespace

Application::Application(term::Terminal& terminal, Clock& clock)
    : Application(terminal, clock, std::make_unique<term::TerminalClipboardWriter>(terminal), nullptr) {}

Application::Application(term::Terminal& terminal, Clock& clock, ClipboardWriter& clipboard_writer)
    : Application(terminal, clock, nullptr, &clipboard_writer) {}

Application::Application(term::Terminal& terminal, Clock& clock,
                         std::unique_ptr<ClipboardWriter> owned_clipboard_writer,
                         ClipboardWriter* borrowed_clipboard_writer)
    : terminal_(terminal), clock_(clock), owned_clipboard_writer_(std::move(owned_clipboard_writer)),
      clipboard_writer_(borrowed_clipboard_writer != nullptr ? *borrowed_clipboard_writer
                                                              : *owned_clipboard_writer_),
      root_(Rect{0, 0, terminal.size().width, terminal.size().height}), surface_(terminal.size()),
      compositor_(terminal.size()), presenter_(terminal) {
    // Never call back into `v` here: it may be mid-destructor (its
    // derived-class members already torn down) when this fires. Just
    // drop the raw pointers — no on_focus notification, nothing that
    // would touch the (possibly half-destroyed) object.
    root_.set_detach_sink([this](View& v) {
        // notify_detaching_recursive() visits a parent before its children.
        // A Window's detachment callback may therefore run while focus still
        // names one of its descendants. Clear any application-held pointer
        // inside the whole departing subtree before that callback can present
        // a new modal or otherwise observe it. The subtree is still alive at
        // this point, so containment is safe; do not call into the views.
        if (focused_ != nullptr && tree_contains(v, focused_))
            focused_ = nullptr;
        if (mouse_capture_ != nullptr && tree_contains(v, mouse_capture_))
            mouse_capture_ = nullptr;
        // A view that leaves while the pointer is over it is no longer
        // under it. Dropped rather than re-resolved here: the tree is
        // mid-teardown and hit-testing it would be asking a question of a
        // structure that is still changing. The next reported motion
        // resolves hover again from whatever is actually there.
        if (hovered_ != nullptr && tree_contains(v, hovered_))
            hovered_ = nullptr;
        if (input_capture_ != nullptr && tree_contains(v, input_capture_))
            input_capture_ = nullptr;
        // A public removal path may detach a modal before its owner has
        // reached the matching pop_modal().  Drop the scope here, while
        // the View identity is still valid, so no later routing path can
        // observe a stale modal root.  The sink also runs recursively for
        // a torn-down subtree, covering nested modal windows in order.
        for (auto it = modal_stack_.begin(); it != modal_stack_.end();) {
            if (it->root != &v) {
                ++it;
                continue;
            }
            const bool was_innermost = std::next(it) == modal_stack_.end();
            const std::optional<ViewHandle> restore = it->restore_focus;
            it = modal_stack_.erase(it);

            // If an outer modal disappears while an inner one remains,
            // the inner scope's saved focus can point into the detached
            // outer subtree. Carry the outer scope's restoration forward
            // instead of losing it now; when the inner scope eventually
            // ends it still restores to the right surviving owner.
            if (it != modal_stack_.end() && restore) {
                const View* const next_restore =
                    it->restore_focus ? resolve_attached_view(*it->restore_focus) : nullptr;
                if (next_restore == nullptr)
                    it->restore_focus = restore;
            }
            // Only removing the active scope may restore focus now. A
            // detached outer scope must never pull focus out of a still
            // active inner modal.
            if (was_innermost && restore)
                pending_modal_focus_restore_ = restore;
        }
    });
    // Every invalidate() schedules a frame. Surface row damage and retained
    // backing stores then constrain the actual composition work; the
    // Application deliberately keeps this scheduling boundary simple and
    // instance-local.
    root_.set_dirty_rect_sink([this](Rect) { dirty_ = true; });
    // Root layout is Application-owned and deliberately separate from
    // View::on_bounds_changed: a public observer can observe geometry but
    // cannot replace the framework's direct-root-child fill invariant.
    root_.set_bounds_changed_sink([this](Rect) { layout_root_children(); });
    root_.set_child_attached_sink([this](View& child) { layout_root_child(child); });
    // The context every widget resolves its own theme roles through
    // (M9 WP-7, D-028) — installed once, propagates to every current
    // and future descendant exactly like the two sinks above.
    root_.set_context(Context{&theme_, &roles_, this});
    // standard().help's own default behavior (D-027): resolve the
    // focused view's nearest help-context key and hand it to whatever
    // set_help_provider() installed. An application that wants
    // completely different F1 behavior replaces this with its own
    // set_handler(standard().help, ...) — same as any other standard
    // command. (The standard set itself, with its default chords, is
    // declared by CommandRegistry's own constructor — M9/WP-12,
    // D-013/D-029.)
    commands_.set_handler(commands_.standard().help, [this] {
        if (help_provider_ == nullptr || focused_ == nullptr)
            return;
        const std::string* key = focused_->resolve_help_context_key();
        if (key != nullptr)
            help_provider_(*key);
    });
    // Tab/Shift-Tab traversal (M9/WP-13, D-029) — always installed,
    // unconditionally: Application itself outlives everything that
    // could invoke these, so there is no lifetime concern to guard
    // against the way MenuBar's menu-command default handler has to.
    commands_.set_handler(commands_.standard().focus_next, [this] { focus_next(); });
    commands_.set_handler(commands_.standard().focus_previous, [this] { focus_previous(); });
}

Application::~Application() {
    // Before the terminal is handed back, and not after: a reply collected
    // once the session is restored has already been given to somebody else.
    try {
        settle_frame_completion();
    } catch (...) {
        // Nothing here can be reported and nothing can be retried — the
        // session is being handed back either way, and a destructor that
        // let this out would end the process over a byte.
    }
    terminal_.restore();
    diagnostics_.flush_after_terminal_restore(terminal_);

    // Tear down direct children while root_'s storage is still intact. If the
    // children_ vector destroys them itself, unique_ptr clears an element
    // before entering that child's destructor. A still-open context menu may
    // restore its focus bookmark from there, and tree_contains() would then
    // walk the null slot belonging to the Desktop currently being destroyed.
    // Detaching first removes the slot and clears Application-held pointers;
    // the returned owner then destroys the subtree while every service a View
    // destructor may reach is still alive.
    while (!root_.children().empty()) {
        View* const child = root_.children().back().get();
        std::unique_ptr<View> owned = root_.remove_child(child);
        CKV_ASSERT(owned != nullptr);
    }
}

DiagnosticsSink& Application::diagnostics() noexcept { return diagnostics_; }

void Application::set_diagnostics_sink(std::unique_ptr<DiagnosticsSink> sink) {
    CKV_ASSERT(sink != nullptr);
    diagnostics_.set_observer(std::move(sink));
}

void Application::ApplicationDiagnostics::log(LogLevel level, std::string_view message) noexcept {
    buffered_.log(level, message);
    if (observer_ != nullptr) observer_->log(level, message);
}

void Application::ApplicationDiagnostics::set_observer(std::unique_ptr<DiagnosticsSink> sink) {
    CKV_ASSERT(sink != nullptr);
    observer_ = std::move(sink);
}

void Application::ApplicationDiagnostics::flush_after_terminal_restore(term::Terminal& terminal) noexcept {
    for (const DiagnosticsEntry& entry : buffered_.entries()) {
        switch (entry.level) {
            case LogLevel::Trace: terminal.write_diagnostic_after_restore("trace: "); break;
            case LogLevel::Debug: terminal.write_diagnostic_after_restore("debug: "); break;
            case LogLevel::Info: terminal.write_diagnostic_after_restore("info: "); break;
            case LogLevel::Warning: terminal.write_diagnostic_after_restore("warning: "); break;
            case LogLevel::Error: terminal.write_diagnostic_after_restore("error: "); break;
        }
        terminal.write_diagnostic_after_restore(entry.text);
        terminal.write_diagnostic_after_restore("\n");
    }
    buffered_.clear();
}

void Application::layout_root_child(View& child) {
    if (!child.fills_root())
        return;
    const Rect bounds = root_.bounds();
    child.set_bounds(Rect{0, 0, bounds.width, bounds.height});
}

void Application::layout_root_children() {
    // A root child's on_resized() is application code and may detach or
    // destroy itself or another root child. Snapshot exact identities before
    // entering any such callback; each remaining direct child still receives
    // this resize, while a departed or reparented one is simply skipped.
    std::vector<ViewHandle>& children = root_layout_scratch_;
    children.clear();
    children.reserve(root_.children().size());
    for (const auto& child : root_.children())
        children.push_back(make_view_handle(*child));
    for (const ViewHandle& handle : children) {
        View* const child = resolve_attached_view(handle);
        if (child == nullptr || child->parent() != &root_)
            continue;
        layout_root_child(*child);
    }
}

void Application::set_focus(View* view) {
    try {
        CKV_ASSERT(view == nullptr || view->focusable());
        // A closing modal may attempt its ordinary focus restoration before
        // its deferred detach has dropped the modal scope. Defer that request
        // rather than letting focus escape the active scope; the detach path
        // restores the scope's saved focus once routing is safe again.
        if (view != nullptr && modal_root() != nullptr &&
            (!tree_contains(root_, view) || !in_modal_scope(*view))) {
            // Refused, and REMEMBERED. Dropping it here is what made a ckmux
            // terminal open, render, prompt — and never receive a keystroke:
            // the client focuses its new terminal once, inline, and if any
            // transient modal happened to be up at that instant the request
            // went on the floor with no retry. The window looked active and
            // every key went to the command table instead of the child.
            // Deferring is what the comment above has always promised.
            if (view != nullptr && tree_contains(root_, view))
                deferred_focus_request_ = make_view_handle(*view);
            return;
        }
        if (view == focused_)
            return;
        // Application-owned trees get the lifetime-safe path below. A few
        // low-level construction tests intentionally focus an unattached tree;
        // retain that narrow standalone-view facility, whose lifetime remains
        // wholly the caller's responsibility until it is attached.
        const bool requested_is_attached = view != nullptr && tree_contains(root_, view);
        const std::optional<ViewHandle> requested =
            requested_is_attached ? std::optional{make_view_handle(*view)} : std::nullopt;
        if (focused_ != nullptr)
            focused_->on_focus(FocusEvent{false});
        focused_ = nullptr;
        // Focus loss is user code: it may remove, destroy, hide, disable, or
        // reparent the requested view. Resolve the capability only after that
        // callback, rather than trusting its original raw pointer.
        if (requested) {
            View* const next = resolve_attached_view(*requested);
            if (next != nullptr && next->focusable()) {
                focused_ = next;
                focused_->on_focus(FocusEvent{true});
            }
        } else if (view != nullptr) {
            focused_ = view;
            focused_->on_focus(FocusEvent{true});
        }
    } catch (...) {
        terminal_.terminate_after_callback_failure();
        std::abort();  // the [[noreturn]] contract, locally enforced —
                       // see the first call site in this file
        // Unreachable while every Terminal override honours the base's
        // [[noreturn]] — which nothing but convention makes an override
        // do. Enforced here so a host that breaks the contract aborts
        // deterministically instead of letting control fall off a
        // non-void function, which is undefined behaviour and exactly
        // what GCC's -Wreturn-type flagged at five call sites.
        std::abort();
    }
}

Application::FocusBookmark Application::save_focus() const noexcept {
    FocusBookmark bookmark;
    bookmark.view_ = focused_;
    if (focused_ != nullptr) bookmark.liveness_ = focused_->liveness_;
    return bookmark;
}

void Application::restore_focus(const FocusBookmark& bookmark) {
    if (bookmark.view_ == nullptr || bookmark.liveness_.expired() ||
        !tree_contains(root_, bookmark.view_) || !bookmark.view_->focusable()) {
        set_focus(nullptr);
        return;
    }
    set_focus(bookmark.view_);
}

void Application::set_input_capture(View* view) {
    input_capture_ = view;
}

Application::ViewHandle Application::make_view_handle(View& view) const {
    return ViewHandle{&view, view.liveness_};
}

View* Application::resolve_attached_view(const ViewHandle& handle) const noexcept {
    if (handle.view == nullptr || handle.liveness.expired())
        return nullptr;
    return tree_contains(root_, handle.view) ? handle.view : nullptr;
}

bool Application::in_modal_scope(const View& view) const noexcept {
    View* const scope = modal_root();
    return scope == nullptr || is_ancestor_of(*scope, view);
}

void Application::restore_modal_focus_if_needed() {
    // Not an early return any more. A deferred focus REQUEST and a modal's
    // saved RESTORE are independent: a scope can end with a request waiting and
    // nothing to restore, and returning early there dropped the request for a
    // second time — which is how the first version of this fix applied only
    // three of seven deferrals and left the bug looking half-fixed.
    if (pending_modal_focus_restore_) {
        const ViewHandle restore = *pending_modal_focus_restore_;
        pending_modal_focus_restore_.reset();
        View* const candidate = resolve_attached_view(restore);
        if (candidate != nullptr && candidate->focusable() && in_modal_scope(*candidate))
            set_focus(candidate);
    }
    apply_deferred_focus_request();
}

// A focus request that arrived during a modal scope, applied once routing is
// safe again. It runs after the scope's own restoration and therefore wins:
// the caller asked for this view later than the modal saved its predecessor,
// and the later explicit intent is the one to honour. Silently dropped if the
// view has gone away or is no longer focusable, which is the same contract
// set_focus itself keeps.
void Application::apply_deferred_focus_request() {
    if (!deferred_focus_request_)
        return;
    const ViewHandle wanted = *deferred_focus_request_;
    deferred_focus_request_.reset();
    View* const candidate = resolve_attached_view(wanted);
    if (candidate != nullptr && candidate->focusable() && in_modal_scope(*candidate))
        set_focus(candidate);
}

bool Application::tree_contains(const View& root, const View* target) noexcept {
    if (&root == target)
        return true;
    for (const auto& child : root.children())
        if (tree_contains(*child, target))
            return true;
    return false;
}

bool Application::is_ancestor_of(const View& ancestor, const View& descendant) noexcept {
    for (const View* current = &descendant; current != nullptr; current = current->parent())
        if (current == &ancestor)
            return true;
    return false;
}

void Application::collect_focusable(View& view, std::vector<View*>& out) {
    if (view.focusable())
        out.push_back(&view);
    if (!view.visible())
        return; // hidden subtrees are transparent to traversal
    for (const auto& child : view.children())
        collect_focusable(*child, out);
}

const std::vector<View*>& Application::focusable_views() {
    std::vector<View*>& out = focus_scratch_;
    out.clear();
    View& start = modal_stack_.empty() ? const_cast<View&>(root_) : *modal_stack_.back().root;
    collect_focusable(start, out);
    return out;
}

const std::vector<std::string>& Application::focused_command_contexts() {
    std::vector<std::string>& out = command_context_scratch_;
    out.clear();
    View* const scope_root = modal_root();
    View* start = focused_;
    if (scope_root != nullptr && (start == nullptr || !is_ancestor_of(*scope_root, *start)))
        start = scope_root;
    for (View* view = start; view != nullptr; view = view->parent()) {
        if (view->command_context()) out.push_back(*view->command_context());
        if (view == scope_root) break;
    }
    return out;
}

bool Application::focus_next() {
    const std::vector<View*>& views = focusable_views();
    if (views.empty())
        return false;
    if (focused_ == nullptr) {
        set_focus(views.front());
        return true;
    }
    auto it = std::find(views.begin(), views.end(), focused_);
    // Entering a modal leaves its saved background focus intact until the
    // presentation operation selects an initial child. If a Tab arrives in
    // that interval (or a focused view was detached), traversal must enter
    // at the first focusable view, not skip it as though an invisible item
    // preceded the scope.
    if (it == views.end()) {
        set_focus(views.front());
        return true;
    }
    const std::size_t index = static_cast<std::size_t>(it - views.begin());
    set_focus(views[(index + 1) % views.size()]);
    return true;
}

bool Application::focus_previous() {
    const std::vector<View*>& views = focusable_views();
    if (views.empty())
        return false;
    if (focused_ == nullptr) {
        set_focus(views.back());
        return true;
    }
    auto it = std::find(views.begin(), views.end(), focused_);
    const std::size_t index =
        (it == views.end()) ? 0 : static_cast<std::size_t>(it - views.begin());
    set_focus(views[(index + views.size() - 1) % views.size()]);
    return true;
}

void Application::set_clipboard_text(std::string text) {
    clipboard_text_ = std::move(text);
    clipboard_writer_.write_text(clipboard_text_);
}

Application::TimerId Application::start_timer(std::int64_t interval_nanos, bool repeating,
                                              std::function<void()> callback) {
    CKV_ASSERT(interval_nanos > 0);
    const TimerId id = next_timer_id_++;
    timers_.push_back(Timer{id, clock_.now_nanos() + interval_nanos, interval_nanos, repeating,
                            std::move(callback)});
    return id;
}

void Application::cancel_timer(TimerId id) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->id == id) {
            timers_.erase(it);
            return;
        }
    }
}

std::optional<std::int64_t> Application::next_timer_deadline_nanos() const noexcept {
    std::optional<std::int64_t> deadline =
        presenter_.next_cursor_blink_deadline_nanos();
    for (const Timer& timer : timers_)
        deadline = deadline ? std::min(*deadline, timer.next_fire_nanos)
                            : timer.next_fire_nanos;
    return deadline;
}

void Application::set_help_provider(std::function<void(const std::string&)> provider) {
    help_provider_ = std::move(provider);
}

void Application::set_capability_changed_handler(std::function<void()> handler) {
    capability_changed_handler_ = std::move(handler);
}

void Application::set_command_handler(CommandId id, std::function<void()> handler) {
    commands_.set_handler(id, std::move(handler));
}

bool Application::command_available(CommandId id) {
    return commands_.is_available(id, focused_command_contexts());
}

bool Application::execute_command(CommandId id) {
    try {
        return commands_.execute(id, focused_command_contexts());
    } catch (...) {
        terminal_.terminate_after_callback_failure();
        std::abort();  // the [[noreturn]] contract, locally enforced —
                       // see the first call site in this file
    }
}

View* Application::topmost_view_at(Point absolute_point) noexcept {
    // Depth-first, last-added-on-top (children later in declaration
    // order paint over earlier ones, so they hit-test first): among
    // SIBLINGS that overlap the point, the first match (topmost)
    // stops the scan — a later, lower sibling that also happens to
    // contain the point must never overwrite an already-found result.
    // Modal scoping (M9/WP-15): while a modal is active, the search
    // starts AT it rather than at root_ — a click that lands outside
    // its own bounds (or on a sibling window entirely) simply never
    // matches anything here, rather than falling through to whatever
    // is underneath. No separate check needed anywhere downstream:
    // every mouse-routing decision in dispatch() keys off this
    // function's result.
    View* const scope_root = modal_root();
    return topmost_view_at_recursive(scope_root != nullptr ? *scope_root : root_, absolute_point);
}

void Application::update_hover(Point absolute_point, View* holder) {
    View* const next = holder != nullptr ? holder : topmost_view_at(absolute_point);
    if (next == hovered_) return;
    // Both sides are held as handles across the notifications: a hover
    // callback is ordinary widget code and may detach or destroy either
    // view — including, by way of a container reacting to the repaint, the
    // one that is about to be entered.
    const ViewHandle left = hovered_ != nullptr ? make_view_handle(*hovered_) : ViewHandle{};
    const ViewHandle entered = next != nullptr ? make_view_handle(*next) : ViewHandle{};
    // Published before either callback runs, so a widget that asks the
    // Application what the pointer is over during its own transition is
    // told the new answer rather than the one being replaced.
    hovered_ = next;
    if (View* const previous = resolve_attached_view(left)) previous->set_hovered(false);
    if (View* const current = resolve_attached_view(entered)) current->set_hovered(true);
}

PointerShape Application::pointer_shape() const noexcept {
    PointerShape requested = PointerShape::Default;
    if (hovered_ != nullptr && last_mouse_event_) {
        const Point pointer = last_mouse_event_->cell;
        // Up the ancestry until somebody has an opinion: the deepest view
        // under the pointer answers for itself, and one with nothing to say
        // lets the container around it answer — which is how a window's
        // border keeps its resize pointer under a content pane that fills
        // the window and cares about none of it.
        for (const View* view = hovered_; view != nullptr; view = view->parent()) {
            const Rect absolute = view->absolute_bounds();
            const std::optional<PointerShape> shape =
                view->pointer_shape_at(Point{pointer.x - absolute.x, pointer.y - absolute.y});
            if (shape) {
                requested = *shape;
                break;
            }
        }
    }
    return term::effective_pointer_shape(terminal_.capabilities(), requested);
}

View* Application::topmost_view_at_recursive(View& view, Point absolute_point) noexcept {
    if (!view.visible() || !contains(view.absolute_bounds(), absolute_point))
        return nullptr;
    for (auto it = view.children().rbegin(); it != view.children().rend(); ++it)
        if (View* hit = topmost_view_at_recursive(**it, absolute_point))
            return hit;
    return &view;
}

bool Application::dispatch(const term::TerminalEvent& event) {
    try {
        ActiveOperation operation(dispatch_active_);
        const bool handled = std::visit(
            [this](const auto& e) -> bool {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, KeyEvent>) {
                    // Modal scoping (M9/WP-15): the ancestor walk never
                    // continues past the active modal's own root (Window::
                    // on_key itself — accept_request/cancel_request — still
                    // fires; whatever it's nested inside, e.g. Desktop, does
                    // not). The fallback below permits only the modal-safe
                    // default keymap (Tab/Shift-Tab/F1); background window,
                    // menu, quit, and application accelerators remain
                    // excluded while a modal owns the scope.
                    View* const scope_root = modal_root();
                    View* start = focused_;
                    if (scope_root != nullptr &&
                        (start == nullptr || !is_ancestor_of(*scope_root, *start)))
                        start = scope_root;
                    // Low-level unit construction may focus a detached tree.
                    // Its lifetime is explicitly caller-owned; preserve that
                    // narrow facility while all Application-owned routes use
                    // lifetime handles below.
                    if (scope_root == nullptr && start != nullptr && !tree_contains(root_, start)) {
                        for (View* v = start; v != nullptr; v = v->parent())
                            if (e.action == KeyAction::Release ? v->on_key_release(e) : v->on_key(e))
                                return true;
                        if (e.action == KeyAction::Release) return false;
                        if (auto id = commands_.command_for_key(e.chord))
                            return commands_.execute(*id, focused_command_contexts());
                        return false;
                    }
                    std::vector<ViewHandle>& route = dispatch_route_scratch_;
                    route.clear();
                    for (View* v = start; v != nullptr; v = v->parent()) {
                        route.push_back(make_view_handle(*v));
                        if (v == scope_root)
                            break;
                    }
                    for (const ViewHandle& handle : route) {
                        View* const v = resolve_attached_view(handle);
                        if (v == nullptr ||
                            (scope_root != nullptr && !is_ancestor_of(*scope_root, *v)))
                            break;
                        if (e.action == KeyAction::Release ? v->on_key_release(e) : v->on_key(e))
                            return true;
                    }
                    // Commands are activations, never releases. This is a
                    // security boundary as well as compatibility behavior:
                    // a kitty release must not execute an accelerator that
                    // older controls would interpret as a second press.
                    if (e.action == KeyAction::Release) return false;
                    if (scope_root != nullptr) {
                        if (auto id = commands_.command_for_key(e.chord)) {
                            const std::vector<std::string>& contexts = focused_command_contexts();
                            const CommandInfo* info = commands_.find(*id);
                            const bool modal_context_command =
                                info != nullptr && !info->context.empty() &&
                                commands_.is_available(*id, contexts);
                            if (is_modal_scope_command(commands_.standard(), *id) ||
                                modal_context_command)
                                return commands_.execute(*id, contexts);
                        }
                        return false;
                    }
                    // F1 context help (D-027) routes through the standard
                    // help command like any other standard chord (M9/
                    // WP-12) — its default handler (installed by the
                    // constructor) is exactly the old F1-specific logic
                    // this branch used to run inline.
                    if (auto id = commands_.command_for_key(e.chord))
                        return commands_.execute(*id, focused_command_contexts());
                    return false;
                } else if constexpr (std::is_same_v<T, TextEvent>) {
                    if (e.from_paste)
                        clipboard_text_ =
                            e.text; // system import mirrors into the internal clipboard
                    View* const scope_root = modal_root();
                    View* start = focused_;
                    if (scope_root != nullptr &&
                        (start == nullptr || !is_ancestor_of(*scope_root, *start)))
                        start = scope_root;
                    if (scope_root == nullptr && start != nullptr && !tree_contains(root_, start)) {
                        for (View* v = start; v != nullptr; v = v->parent())
                            if (v->on_text(e))
                                return true;
                        return false;
                    }
                    std::vector<ViewHandle>& route = dispatch_route_scratch_;
                    route.clear();
                    for (View* v = start; v != nullptr; v = v->parent()) {
                        route.push_back(make_view_handle(*v));
                        if (v == scope_root)
                            break;
                    }
                    for (const ViewHandle& handle : route) {
                        View* const v = resolve_attached_view(handle);
                        if (v == nullptr ||
                            (scope_root != nullptr && !is_ancestor_of(*scope_root, *v)))
                            break;
                        if (v->on_text(e))
                            return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, MouseEvent>) {
                    ++mouse_events_dispatched_;
                    last_mouse_event_ = e;
                    const bool press = e.action == MouseAction::Down || e.action == MouseAction::DoubleClick;
                    // Captured BEFORE target resolution: a popup (menu
                    // dropdown light-dismiss) already owning input capture
                    // means clicks are that popup's own business end to
                    // end — click-to-focus below must never fire for them,
                    // regardless of what delivery does to input_capture_
                    // as a side effect (e.g. opening ANOTHER dropdown).
                    const auto usable_capture = [this](View* capture) {
                        return capture != nullptr && tree_contains(root_, capture) &&
                               in_modal_scope(*capture);
                    };
                    if (!usable_capture(input_capture_))
                        input_capture_ = nullptr;
                    if (!usable_capture(mouse_capture_))
                        mouse_capture_ = nullptr;
                    // Before target resolution and before the early return
                    // for a report that lands on nothing: moving off the
                    // last control onto bare desktop is precisely the
                    // transition that has to clear a highlight and put the
                    // pointer back to its ordinary shape, and that report
                    // has no delivery target at all.
                    update_hover(e.cell, mouse_capture_);
                    const bool had_input_capture = input_capture_ != nullptr;
                    View* target = input_capture_;
                    if (target == nullptr)
                        target = mouse_capture_;
                    if (target == nullptr)
                        target = topmost_view_at(e.cell);
                    if (target == nullptr)
                        return false;
                    const ViewHandle target_handle = make_view_handle(*target);
                    if (resolve_attached_view(target_handle) == nullptr)
                        return false;
                    if (press && input_capture_ == nullptr)
                        mouse_capture_ = target;
                    // Click-to-activate/raise (M8 WP-3): notify every
                    // ancestor of target BEFORE delivery, so a container
                    // like Desktop can raise/activate whichever of its
                    // owned windows contains the click — including clicks
                    // deep inside content, which never reach the Window
                    // itself through ordinary on_mouse delivery. A no-op
                    // for any tree with no such container (View's default
                    // is empty), and naturally scoped away from popups:
                    // popups are Desktop's siblings, never descendants of
                    // a window, so this walk never finds one for them.
                    if (press) {
                        // Snapshot no raw parent pointers across callbacks. Each
                        // route entry and the original target is revalidated just
                        // before use; detachment, destruction, or reparenting
                        // ends this old route deterministically.
                        std::vector<ViewHandle>& capture_route = mouse_capture_route_scratch_;
                        capture_route.clear();
                        for (View* ancestor = target->parent(); ancestor != nullptr;
                             ancestor = ancestor->parent())
                            capture_route.push_back(make_view_handle(*ancestor));
                        for (const ViewHandle& ancestor_handle : capture_route) {
                            View* const current_target = resolve_attached_view(target_handle);
                            View* const ancestor = resolve_attached_view(ancestor_handle);
                            if (current_target == nullptr || ancestor == nullptr ||
                                !is_ancestor_of(*ancestor, *current_target))
                                break;
                            ancestor->on_descendant_mouse_down(*current_target);
                        }
                    }
                    if (e.action == MouseAction::Wheel) {
                        std::vector<ViewHandle>& wheel_route = mouse_capture_route_scratch_;
                        wheel_route.clear();
                        for (View* ancestor = target->parent(); ancestor != nullptr;
                             ancestor = ancestor->parent())
                            wheel_route.push_back(make_view_handle(*ancestor));
                    }
                    View* const delivery_target = resolve_attached_view(target_handle);
                    if (delivery_target == nullptr) {
                        if (e.action == MouseAction::Up)
                            mouse_capture_ = nullptr;
                        return false;
                    }
                    View* const focus_before_delivery = focused_;
                    // `target_handled`, not `handled`: the visit's own result
                    // at the top of this dispatch is already called that, and
                    // GCC's -Wshadow is right that two of them is one too many.
                    bool target_handled = delivery_target->on_mouse(e);
                    if (!target_handled && e.action == MouseAction::Wheel) {
                        std::vector<ViewHandle>& wheel_route = mouse_capture_route_scratch_;
                        for (const ViewHandle& ancestor_handle : wheel_route) {
                            View* const current_target = resolve_attached_view(target_handle);
                            View* const ancestor = resolve_attached_view(ancestor_handle);
                            if (current_target == nullptr || ancestor == nullptr ||
                                !is_ancestor_of(*ancestor, *current_target))
                                break;
                            if (ancestor->on_mouse(e)) {
                                target_handled = true;
                                break;
                            }
                            if (ancestor == modal_root())
                                break;
                        }
                    }
                    if (e.action == MouseAction::Up)
                        mouse_capture_ = nullptr;
                    // Click-to-focus (VISION #7): a press outside any
                    // popup capture moves focus to the nearest focusable
                    // view at or above the click target. Deliberately
                    // AFTER delivery, not before: a widget that manages
                    // its own focus-on-click transition (MenuBar opening
                    // its dropdown on first click, which records "what was
                    // focused before" via its own bookkeeping) must see
                    // the TRUE prior focus, not one this dispatch already
                    // reassigned out from under it. set_focus is a no-op
                    // A focus change during delivery is the widget's own
                    // choice; the equality guard leaves it alone and only
                    // fills in when delivery kept focus where it was.
                    if (press && !had_input_capture && focused_ == focus_before_delivery) {
                        for (View* v = resolve_attached_view(target_handle); v != nullptr;
                             v = v->parent()) {
                            if (v->focusable()) {
                                set_focus(v);
                                break;
                            }
                            if (v == modal_root())
                                break;
                        }
                    }
                    return target_handled;
                } else if constexpr (std::is_same_v<T, FocusEvent>) {
                    View* recipient = focused_;
                    View* const scope_root = modal_root();
                    if (scope_root != nullptr &&
                        (recipient == nullptr || !is_ancestor_of(*scope_root, *recipient)))
                        recipient = scope_root;
                    if (recipient == nullptr)
                        return false;
                    recipient->on_focus(e);
                    return true;
                } else if constexpr (std::is_same_v<T, ResizeEvent>) {
                    root_.set_bounds(Rect{0, 0, e.cells.width, e.cells.height});
                    return true;
                } else if constexpr (std::is_same_v<T, term::CapabilityChangedEvent>) {
                    // Presentation policy (graphics/fallback, color depth,
                    // synchronized output, width agreement) can change even
                    // when widget state does not. Force a full repaint and
                    // presenter reset so the old capability's terminal pixels
                    // cannot survive the transition.
                    invalidate_all();
                    // After the repaint, not before: a handler reading
                    // `terminal.capabilities()` here sees the value this
                    // dispatch just repainted for, not a stale one from
                    // before the redraw decided anything.
                    if (capability_changed_handler_) capability_changed_handler_();
                    return true;
                } else {
                    static_assert(!sizeof(T*), "unhandled TerminalEvent alternative");
                }
            },
            event);
        restore_modal_focus_if_needed();
        return handled;
    } catch (...) {
        terminal_.terminate_after_callback_failure();
        std::abort();  // the [[noreturn]] contract, locally enforced —
                       // see the first call site in this file
    }
}


void Application::set_frame_completion_tracking(bool enabled) {
    if (presenter_.frame_completion_tracking() == enabled) return;
    presenter_.set_frame_completion_tracking(enabled);
    // Turning it off leaves nothing outstanding to reason about; turning it
    // on starts from what this terminal has already answered, so a backend
    // that was answering somebody else's questions does not read as a
    // backlog of ours.
    frames_settled_ = presenter_.frames_marked();
    terminal_acknowledgements_seen_ = terminal_.frame_acknowledgements();
    oldest_unanswered_frame_nanos_.reset();
    frame_completion_timeouts_ = 0;
    terminal_ever_acknowledged_ = false;
    longest_terminal_round_trip_nanos_ = 0;
}

std::int64_t Application::frame_completion_patience_nanos() const noexcept {
    // A host that has answered before is waited for as long as its own
    // slowest answer suggests it needs. Giving up on it early is worse than
    // not asking at all: the wait IS the back-pressure, and abandoning it
    // sends the next frame into a picture the terminal is still drawing.
    if (!terminal_ever_acknowledged_) return kFrameCompletionTimeoutNanos;
    return std::max(kFrameCompletionTimeoutNanos,
                    longest_terminal_round_trip_nanos_ * kFrameCompletionPatienceFactor);
}

void Application::reconcile_frame_completion(std::int64_t now_nanos) {
    if (!presenter_.frame_completion_tracking()) return;

    const std::size_t marked = presenter_.frames_marked();
    const std::size_t acknowledged = terminal_.frame_acknowledgements();
    if (acknowledged > terminal_acknowledgements_seen_) {
        // A terminal reads in order, so its replies answer the oldest
        // questions first. What is worth reporting is the newest round
        // trip: it describes how far behind the terminal is now.
        const std::size_t fresh = acknowledged - terminal_acknowledgements_seen_;
        terminal_acknowledgements_seen_ = acknowledged;
        frames_settled_ = std::min(marked, frames_settled_ + fresh);
        if (oldest_unanswered_frame_nanos_) {
            last_terminal_round_trip_nanos_ = now_nanos - *oldest_unanswered_frame_nanos_;
            longest_terminal_round_trip_nanos_ =
                std::max(longest_terminal_round_trip_nanos_, last_terminal_round_trip_nanos_);
        }
        terminal_ever_acknowledged_ = true;
        frame_completion_timeouts_ = 0;
        // Whatever is still outstanding has been waiting since at least now.
        if (frames_settled_ < marked) oldest_unanswered_frame_nanos_ = now_nanos;
        else oldest_unanswered_frame_nanos_.reset();
        return;
    }

    if (frames_settled_ >= marked || !oldest_unanswered_frame_nanos_) return;
    if (now_nanos - *oldest_unanswered_frame_nanos_ < frame_completion_patience_nanos()) return;

    // Written off. A terminal that does not answer must cost an application
    // the wait once, never its animation.
    frames_settled_ = marked;
    oldest_unanswered_frame_nanos_.reset();
    // Only a host that has never answered can be concluded not to answer.
    // One that has is merely late, however often: its replies are evidence
    // that the facility works here, and no amount of slowness turns that
    // into evidence that it does not.
    if (terminal_ever_acknowledged_) return;
    if (++frame_completion_timeouts_ >= kFrameCompletionGiveUpCount) {
        diagnostics().log(LogLevel::Info,
                          "frame completion: this terminal does not answer DSR, so pacing against it "
                          "is off for this session");
        set_frame_completion_tracking(false);
    }
}

void Application::settle_frame_completion() {
    if (!presenter_.frame_completion_tracking()) return;
    // An answer already read but not yet accounted for settles the debt
    // without a wait: the last thing a loop does is poll, so the reply is
    // often in hand before anyone asks for it.
    reconcile_frame_completion(clock_.now_nanos());
    if (frames_awaiting_terminal() == 0) return;
    // A host that has never answered is owed no wait. Its silence is the
    // same silence that turns tracking off in the running loop, and there
    // is no reply in flight from a terminal that does not send one.
    if (!terminal_ever_acknowledged_) return;

    const std::int64_t deadline = clock_.now_nanos() + frame_completion_patience_nanos();
    while (frames_awaiting_terminal() > 0) {
        const std::int64_t before = clock_.now_nanos();
        if (before >= deadline) return;
        const std::size_t acknowledged_before = terminal_.frame_acknowledgements();
        // Discarded deliberately: whatever else the terminal has to say
        // arrives after the last frame this session will ever paint.
        terminal_.poll(deadline);
        const std::int64_t after = clock_.now_nanos();
        reconcile_frame_completion(after);
        // A wait that consumed neither time nor an answer came from a
        // backend that does not wait. Repeating it would spin rather than
        // settle, and a headless or scripted backend has no reply to give.
        if (after == before && terminal_.frame_acknowledgements() == acknowledged_before) return;
    }
}

void Application::post(std::function<void()> fn) {
    // Match timers' callback contract: an empty std::function represents no
    // work, not an asynchronous bad_function_call that would terminate the
    // owning application on its next step.
    if (!fn) return;
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        posted_.push_back(std::move(fn));
    }
    terminal_.wake();
}

bool Application::step(std::int64_t deadline_nanos) {
    try {
        ActiveOperation operation(step_active_);
        bool wake_now = false;
        {
            std::lock_guard<std::mutex> lock(post_mutex_);
            wake_now = wake_requested_;
            wake_requested_ = false;
        }
        std::int64_t effective_deadline = wake_now ? clock_.now_nanos() : deadline_nanos;
        if (const auto timer_deadline = next_timer_deadline_nanos())
            effective_deadline = std::min(effective_deadline, *timer_deadline);

        // Sessions released since the last step leave empty slots behind —
        // erasing one inside the loop that was notifying about it would pull
        // the vector out from under that loop. This is the one place where no
        // loop is running, so it is the one place that erases.
        if (terminal_subsessions_need_compaction_) {
            std::erase(terminal_subsessions_, nullptr);
            terminal_subsessions_need_compaction_ = false;
        }

        bool did_work = false;
        const auto drain_terminal_subsessions = [this, &did_work] {
            // By index, because a change notification is application code: it
            // may adopt another session, which can reallocate this vector, and
            // an iterator into it would not survive that.
            for (std::size_t index = 0; index < terminal_subsessions_.size(); ++index) {
                term::TerminalSubsession* const session = terminal_subsessions_[index].get();
                if (session == nullptr) continue;  // released; compacted next step
                if (!session->drain(32 * 1024)) continue;
                did_work = true;
                root_.notify_terminal_subsession_changed(*session);
            }
        };
        drain_terminal_subsessions();
        terminal_subsession_wait_handles_.clear();
        for (const std::unique_ptr<term::TerminalSubsession>& session : terminal_subsessions_) {
            if (session == nullptr) continue;
            const std::span<const term::WaitHandle> handles = session->wait_handles();
            terminal_subsession_wait_handles_.insert(terminal_subsession_wait_handles_.end(), handles.begin(), handles.end());
        }
        for (const term::TerminalEvent& event : terminal_.poll(effective_deadline, terminal_subsession_wait_handles_)) {
            did_work = true;
            dispatch(event);
        }
        // A private child can wake the combined wait without supplying an
        // outer-terminal event. Drain it before the frame is composed.
        drain_terminal_subsessions();

        const std::int64_t now = clock_.now_nanos();
        // Before any timer runs: a tick that paces itself against the
        // terminal must see the replies that arrived in this very poll,
        // not the ones that had arrived by the previous frame.
        reconcile_frame_completion(now);
        std::vector<std::function<void()>>& due_callbacks = due_callback_scratch_;
        due_callbacks.clear();
        for (auto it = timers_.begin(); it != timers_.end();) {
            if (it->next_fire_nanos > now) {
                ++it;
                continue;
            }
            due_callbacks.push_back(it->callback);
            if (it->repeating) {
                it->next_fire_nanos += it->interval_nanos;
                ++it;
            } else {
                it = timers_.erase(it);
            }
        }
        for (auto& fn : due_callbacks) {
            did_work = true;
            if (fn)
                fn();
        }

        std::vector<std::function<void()>>& work = posted_work_scratch_;
        work.clear();
        {
            std::lock_guard<std::mutex> lock(post_mutex_);
            work.swap(posted_);
        }
        for (auto& fn : work) {
            did_work = true;
            fn();
        }

        restore_modal_focus_if_needed();
        // Coalesce rather than pile on. A picture costs the host far more
        // than the cells around it, and the events that produce new frames
        // — a window being dragged, a resize in progress — arrive at the
        // pointer's rate, not at one the terminal agreed to. While it has
        // not finished the last frame that carried a picture, the newest
        // state simply waits here: nothing is queued, nothing is lost, and
        // the frame that does go out is the current one rather than one of
        // the positions passed through on the way. dirty_ stays set, and a
        // host that never answers is released by the write-off deadline.
        // ...but only where the host has actually answered. A terminal that
        // never replies would otherwise hold every frame back until the
        // write-off deadline, which is a stall invented out of silence.
        // Where there is no measurement there is no back-pressure to
        // respect, and an application that needs one guesses a rate instead.
        // The pointer's shape is an outcome of the input just processed, not
        // of the frame that may follow, so it is written here: once per
        // batch, whatever the batch contained, and whether or not anything
        // needs repainting. It also stays live while frames are being held
        // back for a busy host below -- a throttled picture is no reason for
        // the cursor to freeze on the last thing it crossed.
        presenter_.present_pointer_shape(pointer_shape());
        const bool waiting_on_a_host_that_answers = presenter_.frame_completion_tracking() &&
                                                    last_terminal_round_trip_nanos_ >= 0 &&
                                                    frames_awaiting_terminal() > 0 &&
                                                    presenter_.last_frame_carried_rasters();
        if (!waiting_on_a_host_that_answers) paint_and_present();
        if (presenter_.advance_cursor_blink(now)) did_work = true;
        return did_work;
    } catch (...) {
        terminal_.terminate_after_callback_failure();
        std::abort();  // the [[noreturn]] contract, locally enforced —
                       // see the first call site in this file
    }
}

term::TerminalSubsession& Application::launch_terminal_subsession(
    term::TerminalLaunchSpec spec, term::TerminalSubsessionOptions options) {
    std::unique_ptr<term::TerminalSubsession> session = term::launch_terminal_subsession(std::move(spec), std::move(options));
    CKV_ASSERT(session != nullptr);
    return adopt_terminal_subsession(std::move(session));
}

term::TerminalSubsession& Application::adopt_terminal_subsession(
    std::unique_ptr<term::TerminalSubsession> session) {
    CKV_ASSERT(session != nullptr);
    // The same raster identity a launched session gets, from the same
    // counter. A session left at the default id has its pictures dropped by
    // the view that would have drawn them, without a word — so an adopted
    // session that decodes graphics must be given one, and the host cannot be
    // the one to remember.
    session->set_raster_identity(next_terminal_raster_identity_++);
    terminal_subsessions_.push_back(std::move(session));
    return *terminal_subsessions_.back();
}

std::unique_ptr<term::TerminalSubsession> Application::release_terminal_subsession(
    const core::TerminalSubsession& session) {
    for (std::unique_ptr<term::TerminalSubsession>& owned : terminal_subsessions_) {
        // Compared as the base, so that the session a notification named and
        // the session this application owns are the same question.
        if (static_cast<const core::TerminalSubsession*>(owned.get()) != &session) continue;
        std::unique_ptr<term::TerminalSubsession> released = std::move(owned);
        // The slot stays, empty, until the next step(): this may be running
        // inside a drain loop that is walking the very vector an erase would
        // reallocate.
        terminal_subsessions_need_compaction_ = true;
        return released;
    }
    return nullptr;
}

void Application::invalidate_all() {
    dirty_ = true;
    presenter_.invalidate();
}

void Application::paint_and_present() {
    if (!dirty_)
        return;

    const Size current_size{root_.bounds().width, root_.bounds().height};
    if (current_size.width != surface_.size().width ||
        current_size.height != surface_.size().height) {
        surface_.resize(current_size);
        compositor_.resize(current_size);
        presenter_.invalidate();
    }

    // A redraw replaces the base surface's raster set. Retained Window and
    // popup backings manage their own corresponding lifecycle.
    surface_.clear_raster_regions();

    scene::Painter root_painter(surface_, Rect{0, 0, current_size.width, current_size.height});
    composition_layers_.clear();
    if (terminal_too_small()) {
        paint_too_small_state(root_painter, current_size);
    } else {
        root_.paint_retained(root_painter, composition_layers_);
    }
    dirty_ = false;

    compositor_.compose(composition_layers_, surface_, shadow_spec_);
    CursorState cursor;
    // A hidden view keeps the focus — a minimized window whose content nothing
    // else could take the keyboard from is the ordinary way — but it does not
    // keep the cursor. Not a cell of it was painted, so a cursor still placed
    // at its coordinates is a lit block sitting on whatever the reader IS
    // looking at: the desktop background, once a window is put away.
    //
    // Same place and the same reason as the off-screen rule just below, which
    // is the other half of one question: is this view on the frame at all?
    if (focused_ != nullptr && focused_->visible_in_tree()) {
        if (const std::optional<CursorState> focused_cursor = focused_->cursor_state()) cursor = *focused_cursor;
    }
    // A focused view can be carried off the screen entirely -- a window on a
    // desktop panned past it (Desktop::set_pan) is the ordinary way -- and its
    // cursor goes with it. Then there is no cell to put the cursor in, and
    // saying so to the terminal means addressing a row and column that do not
    // exist: `ESC[-3;-27H`, which a strict host refuses outright and a lenient
    // one interprets as it pleases. Not showing it is also what a reader means
    // by scrolling something out of view, so the honest answer and the safe
    // one are the same answer.
    //
    // Deliberately here rather than in the widget: every cursor in the tree
    // arrives through this one line, and a cursor being off-screen is a fact
    // about the frame, not a special case of any widget that owns one.
    if (cursor.visible &&
        !Rect{0, 0, current_size.width, current_size.height}.contains(cursor.position))
        cursor = CursorState{};
    compositor_.set_cursor(cursor);
    presenter_.present(compositor_.frame().view(), compositor_.cursor(),
                       clock_.now_nanos(), compositor_.visible_rasters());
    // The clock starts when the question goes out, not when the next step
    // notices it did: an application that paces against the terminal is
    // asking how long the terminal took, not how long we took to look.
    if (presenter_.frame_completion_tracking() && presenter_.frames_marked() > frames_settled_ &&
        !oldest_unanswered_frame_nanos_)
        oldest_unanswered_frame_nanos_ = clock_.now_nanos();
}

bool Application::terminal_too_small() const noexcept {
    return root_.bounds().width < kHardFloorSize.width ||
           root_.bounds().height < kHardFloorSize.height;
}

void Application::paint_too_small_state(scene::Painter& painter, Size current_size) const {
    const Style style{kTooSmallFg, kTooSmallBg, Attr{}};
    const Rect whole{0, 0, current_size.width, current_size.height};
    painter.fill(whole, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{0, 0}, "Terminal too small", style);
    const std::string need = "Resize to at least " + std::to_string(kMinFullChromeSize.width) +
                             "x" + std::to_string(kMinFullChromeSize.height);
    painter.draw_text(Point{0, 1}, need, style);
}

void Application::wake() noexcept {
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        wake_requested_ = true;
    }
    terminal_.wake();
}

void Application::request_quit() noexcept {
    // One wake is sufficient: once a loop observes this sticky request it
    // does not enter another terminal wait. Avoid filling a POSIX self-pipe
    // when several shutdown paths converge on the same Application.
    if (!quit_requested_.exchange(true, std::memory_order_relaxed))
        terminal_.wake();
}

void Application::run() {
    run_until([] { return false; });
}

bool Application::run_until(const std::function<bool()>& done) {
    try {
        CKV_ASSERT(can_run_blocking());
        ActiveOperation operation(run_active_);
        // Calls done() exactly once per check (never a redundant extra call
        // after the loop exits) — done() may carry real side effects (a
        // step counter, a captured optional) that a spurious re-evaluation
        // would visibly duplicate.
        if (quit_requested()) return false;
        bool finished = done();
        while (!finished && !quit_requested()) {
            step(clock_.now_nanos() + kDefaultFrameIntervalNanos);
            if (quit_requested()) break;
            finished = done();
        }
        return finished;
    } catch (...) {
        terminal_.terminate_after_callback_failure();
        std::abort();  // the [[noreturn]] contract, locally enforced —
                       // see the first call site in this file
    }
}

Application::ModalScopeId Application::push_modal(View& modal_root) {
    CKV_ASSERT(tree_contains(root_, &modal_root));
    std::optional<ViewHandle> restore;
    if (focused_ != nullptr && tree_contains(root_, focused_))
        restore = make_view_handle(*focused_);
    const ModalScopeId id = next_modal_scope_id_++;
    CKV_ASSERT(id != 0); // wraparound would make a retained identity ambiguous
    modal_stack_.push_back(ModalScope{id, &modal_root, std::move(restore)});
    return id;
}

void Application::pop_modal() {
    CKV_ASSERT(!modal_stack_.empty());
    const bool popped = pop_modal(modal_stack_.back().id);
    CKV_ASSERT(popped);
}

bool Application::pop_modal(ModalScopeId scope) {
    auto it = std::find_if(modal_stack_.begin(), modal_stack_.end(),
                           [scope](const ModalScope& current) { return current.id == scope; });
    if (it == modal_stack_.end() || std::next(it) != modal_stack_.end())
        return false;
    if (it->restore_focus)
        pending_modal_focus_restore_ = it->restore_focus;
    modal_stack_.erase(it);
    restore_modal_focus_if_needed();
    return true;
}

} // namespace ckv::ui
