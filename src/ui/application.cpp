// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/application.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <string>
#include <type_traits>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

namespace {

bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

// The "terminal too small" state's own fixed style (M10/WP-21) — NOT
// resolved through theme()/roles(): this is a last-resort framework
// fallback that must render sanely even for an application that never
// configured a theme at all, so it cannot depend on that configuration
// having happened correctly.
constexpr Color kTooSmallFg = Color::rgb(255, 255, 255);
constexpr Color kTooSmallBg = Color::rgb(170, 0, 0);

// run()'s SIGINT handling (M9/WP-14): a signal handler cannot capture
// `this`, so the currently-run()-ning Application (if any) lives here
// instead — swapped in/out around the loop, restoring whatever was
// there before (nullptr for the common single-Application case, but
// this also does the right thing for a nested/sequential run() call).
// request_quit() is just a bool store, safe to call from a signal
// handler; nothing else here touches Application state.
std::atomic<Application*> g_run_instance{nullptr};

void handle_run_sigint(int) {
    if (Application* app = g_run_instance.load(std::memory_order_relaxed)) app->request_quit();
}

}  // namespace

Application::Application(term::Terminal& terminal, Clock& clock)
    : terminal_(terminal),
      clock_(clock),
      root_(Rect{0, 0, terminal.size().width, terminal.size().height}),
      surface_(terminal.size()),
      background_layer_(terminal.size()),
      compositor_(terminal.size()),
      presenter_(terminal) {
    // Never call back into `v` here: it may be mid-destructor (its
    // derived-class members already torn down) when this fires. Just
    // drop the raw pointers — no on_focus notification, nothing that
    // would touch the (possibly half-destroyed) object.
    root_.set_detach_sink([this](View& v) {
        if (focused_ == &v) focused_ = nullptr;
        if (mouse_capture_ == &v) mouse_capture_ = nullptr;
        if (input_capture_ == &v) input_capture_ = nullptr;
        // A public removal path may detach a modal before its owner has
        // reached the matching pop_modal().  Drop the scope here, while
        // the View identity is still valid, so no later routing path can
        // observe a stale modal root.  The sink also runs recursively for
        // a torn-down subtree, covering nested modal windows in order.
        modal_stack_.erase(std::remove(modal_stack_.begin(), modal_stack_.end(), &v), modal_stack_.end());
    });
    // Every invalidate() anywhere in the tree marks the whole
    // Application dirty (v1 scope: see step()'s doc comment — the
    // dirty RECT itself is intentionally not yet used to scope
    // painting, only to decide WHETHER to paint at all).
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
    // The standard command set, with its default chords (M9/WP-12,
    // D-013/D-029) — an application attaches handlers to whichever of
    // these it needs; it never re-registers the metadata itself.
    commands_.register_standard_commands();
    // std_commands::kHelp's own default behavior (D-027): resolve the
    // focused view's nearest help-context key and hand it to whatever
    // set_help_provider() installed. An application that wants
    // completely different F1 behavior replaces this with its own
    // set_handler(kHelp, ...) — same as any other standard command.
    commands_.set_handler(std_commands::kHelp, [this] {
        if (help_provider_ == nullptr || focused_ == nullptr) return;
        const std::string* key = focused_->resolve_help_context_key();
        if (key != nullptr) help_provider_(*key);
    });
    // Tab/Shift-Tab traversal (M9/WP-13, D-029) — always installed,
    // unconditionally: Application itself outlives everything that
    // could invoke these, so there is no lifetime concern to guard
    // against the way MenuBar's kMenu default handler has to.
    commands_.set_handler(std_commands::kFocusNext, [this] { focus_next(); });
    commands_.set_handler(std_commands::kFocusPrevious, [this] { focus_previous(); });
}

void Application::layout_root_child(View& child) {
    if (!child.fills_root()) return;
    const Rect bounds = root_.bounds();
    child.set_bounds(Rect{0, 0, bounds.width, bounds.height});
}

void Application::layout_root_children() {
    for (const auto& child : root_.children()) layout_root_child(*child);
}

void Application::set_focus(View* view) {
    CKV_ASSERT(view == nullptr || view->focusable());
    if (view == focused_) return;
    // Application-owned trees get the lifetime-safe path below. A few
    // low-level construction tests intentionally focus an unattached tree;
    // retain that narrow standalone-view facility, whose lifetime remains
    // wholly the caller's responsibility until it is attached.
    const bool requested_is_attached = view != nullptr && tree_contains(root_, view);
    const std::optional<ViewHandle> requested = requested_is_attached
                                                    ? std::optional{make_view_handle(*view)}
                                                    : std::nullopt;
    if (focused_ != nullptr) focused_->on_focus(FocusEvent{false});
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
}

void Application::set_input_capture(View* view) {
    input_capture_ = view;
}

Application::ViewHandle Application::make_view_handle(View& view) const {
    return ViewHandle{&view, view.liveness_};
}

View* Application::resolve_attached_view(const ViewHandle& handle) const noexcept {
    if (handle.view == nullptr || handle.liveness.expired()) return nullptr;
    return tree_contains(root_, handle.view) ? handle.view : nullptr;
}

bool Application::tree_contains(const View& root, const View* target) noexcept {
    if (&root == target) return true;
    for (const auto& child : root.children())
        if (tree_contains(*child, target)) return true;
    return false;
}

bool Application::is_ancestor_of(const View& ancestor, const View& descendant) noexcept {
    for (const View* current = &descendant; current != nullptr; current = current->parent())
        if (current == &ancestor) return true;
    return false;
}

void Application::collect_focusable(View& view, std::vector<View*>& out) {
    if (view.focusable()) out.push_back(&view);
    if (!view.visible()) return;  // hidden subtrees are transparent to traversal
    for (const auto& child : view.children()) collect_focusable(*child, out);
}

std::vector<View*> Application::focusable_views() const {
    std::vector<View*> out;
    View& start = modal_stack_.empty() ? const_cast<View&>(root_) : *modal_stack_.back();
    collect_focusable(start, out);
    return out;
}

bool Application::focus_next() {
    const std::vector<View*> views = focusable_views();
    if (views.empty()) return false;
    if (focused_ == nullptr) {
        set_focus(views.front());
        return true;
    }
    auto it = std::find(views.begin(), views.end(), focused_);
    const std::size_t index = (it == views.end()) ? 0 : static_cast<std::size_t>(it - views.begin());
    set_focus(views[(index + 1) % views.size()]);
    return true;
}

bool Application::focus_previous() {
    const std::vector<View*> views = focusable_views();
    if (views.empty()) return false;
    if (focused_ == nullptr) {
        set_focus(views.back());
        return true;
    }
    auto it = std::find(views.begin(), views.end(), focused_);
    const std::size_t index = (it == views.end()) ? 0 : static_cast<std::size_t>(it - views.begin());
    set_focus(views[(index + views.size() - 1) % views.size()]);
    return true;
}

void Application::set_clipboard_text(std::string text) {
    clipboard_text_ = std::move(text);
    terminal_.write_clipboard(clipboard_text_);
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

void Application::set_help_provider(std::function<void(const std::string&)> provider) {
    help_provider_ = std::move(provider);
}

void Application::set_command_handler(CommandId id, std::function<void()> handler) {
    commands_.set_handler(id, std::move(handler));
}

bool Application::execute_command(CommandId id) { return commands_.execute(id); }

View* Application::topmost_view_at(Point absolute_point) noexcept {
    // Depth-first, last-added-on-top (children later in declaration
    // order paint over earlier ones, so they hit-test first): among
    // SIBLINGS that overlap the point, the first match (topmost)
    // stops the scan — a later, lower sibling that also happens to
    // contain the point must never overwrite an already-found result.
    std::function<View*(View&)> visit = [&](View& v) -> View* {
        if (!v.visible()) return nullptr;
        if (!contains(v.absolute_bounds(), absolute_point)) return nullptr;
        for (auto it = v.children().rbegin(); it != v.children().rend(); ++it)
            if (View* hit = visit(**it)) return hit;
        return &v;
    };
    // Modal scoping (M9/WP-15): while a modal is active, the search
    // starts AT it rather than at root_ — a click that lands outside
    // its own bounds (or on a sibling window entirely) simply never
    // matches anything here, rather than falling through to whatever
    // is underneath. No separate check needed anywhere downstream:
    // every mouse-routing decision in dispatch() keys off this
    // function's result.
    View* const scope_root = modal_root();
    return visit(scope_root != nullptr ? *scope_root : root_);
}

bool Application::dispatch(const term::TerminalEvent& event) {
    return std::visit(
        [this](const auto& e) -> bool {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, KeyEvent>) {
                // Modal scoping (M9/WP-15): the ancestor walk never
                // continues past the active modal's own root (Window::
                // on_key itself — accept_request/cancel_request — still
                // fires; whatever it's nested inside, e.g. Desktop, does
                // not), and the global accelerator fallback below is
                // skipped entirely — a background window's own chords
                // (including std_commands like kMenu/kQuit) must not
                // fire while a modal owns the scope.
                View* const scope_root = modal_root();
                for (View* v = focused_; v != nullptr; v = v->parent()) {
                    if (v->on_key(e)) return true;
                    if (v == scope_root) break;
                }
                if (scope_root != nullptr) return false;
                // F1 context help (D-027) routes through std_commands::
                // kHelp like any other standard chord (M9/WP-12) — its
                // default handler (installed below, in the constructor)
                // is exactly the old F1-specific logic this branch used
                // to run inline.
                if (auto id = commands_.command_for_key(e.chord)) return execute_command(*id);
                return false;
            } else if constexpr (std::is_same_v<T, TextEvent>) {
                if (e.from_paste) clipboard_text_ = e.text;  // system import mirrors into the internal clipboard
                for (View* v = focused_; v != nullptr; v = v->parent())
                    if (v->on_text(e)) return true;
                return false;
            } else if constexpr (std::is_same_v<T, MouseEvent>) {
                // Captured BEFORE target resolution: a popup (menu
                // dropdown light-dismiss) already owning input capture
                // means clicks are that popup's own business end to
                // end — click-to-focus below must never fire for them,
                // regardless of what delivery does to input_capture_
                // as a side effect (e.g. opening ANOTHER dropdown).
                const bool had_input_capture = input_capture_ != nullptr;
                View* target = input_capture_;
                if (target == nullptr) target = mouse_capture_;
                if (target == nullptr) target = topmost_view_at(e.cell);
                if (target == nullptr) return false;
                const ViewHandle target_handle = make_view_handle(*target);
                if (resolve_attached_view(target_handle) == nullptr) return false;
                if (e.action == MouseAction::Down && input_capture_ == nullptr) mouse_capture_ = target;
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
                if (e.action == MouseAction::Down) {
                    // Snapshot no raw parent pointers across callbacks. Each
                    // route entry and the original target is revalidated just
                    // before use; detachment, destruction, or reparenting
                    // ends this old route deterministically.
                    std::vector<ViewHandle> capture_route;
                    for (View* ancestor = target->parent(); ancestor != nullptr; ancestor = ancestor->parent())
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
                View* const delivery_target = resolve_attached_view(target_handle);
                if (delivery_target == nullptr) {
                    if (e.action == MouseAction::Up) mouse_capture_ = nullptr;
                    return false;
                }
                const bool handled = delivery_target->on_mouse(e);
                if (e.action == MouseAction::Up) mouse_capture_ = nullptr;
                // Click-to-focus (VISION #7): a press outside any
                // popup capture moves focus to the nearest focusable
                // view at or above the click target. Deliberately
                // AFTER delivery, not before: a widget that manages
                // its own focus-on-click transition (MenuBar opening
                // its dropdown on first click, which records "what was
                // focused before" via its own bookkeeping) must see
                // the TRUE prior focus, not one this dispatch already
                // reassigned out from under it. set_focus is a no-op
                // when the widget already focused itself during
                // delivery, so this never fights a widget's own
                // choice — it only fills in when nothing did.
                if (e.action == MouseAction::Down && !had_input_capture) {
                    for (View* v = resolve_attached_view(target_handle); v != nullptr; v = v->parent()) {
                        if (v->focusable()) {
                            set_focus(v);
                            break;
                        }
                    }
                }
                return handled;
            } else if constexpr (std::is_same_v<T, FocusEvent>) {
                if (focused_ == nullptr) return false;
                focused_->on_focus(e);
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
                return true;
            } else {
                static_assert(!sizeof(T*), "unhandled TerminalEvent alternative");
            }
        },
        event);
}

void Application::post(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(post_mutex_);
    posted_.push_back(std::move(fn));
}

bool Application::step(std::int64_t deadline_nanos) {
    bool wake_now = false;
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        wake_now = wake_requested_;
        wake_requested_ = false;
    }
    std::int64_t effective_deadline = wake_now ? clock_.now_nanos() : deadline_nanos;
    for (const Timer& timer : timers_) effective_deadline = std::min(effective_deadline, timer.next_fire_nanos);

    bool did_work = false;
    for (const term::TerminalEvent& event : terminal_.poll(effective_deadline)) {
        did_work = true;
        dispatch(event);
    }

    const std::int64_t now = clock_.now_nanos();
    std::vector<std::function<void()>> due_callbacks;
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
        if (fn) fn();
    }

    std::vector<std::function<void()>> work;
    {
        std::lock_guard<std::mutex> lock(post_mutex_);
        work.swap(posted_);
    }
    for (auto& fn : work) {
        did_work = true;
        fn();
    }

    paint_and_present();
    return did_work;
}

void Application::invalidate_all() {
    dirty_ = true;
    presenter_.invalidate();
}

void Application::paint_and_present() {
    if (!dirty_) return;

    const Size current_size{root_.bounds().width, root_.bounds().height};
    if (current_size.width != surface_.size().width || current_size.height != surface_.size().height) {
        surface_.resize(current_size);
        background_layer_.resize(current_size);
        compositor_.resize(current_size);
        presenter_.invalidate();
    }

    // v1 full-tree repaint (see step()'s doc comment) redraws every
    // ImageView on every dirty frame, calling Painter::draw_image again
    // with the SAME region id each time — Surface::add_raster_region
    // requires ids to be unique among a Surface's CURRENT regions, so
    // last frame's regions must be cleared first or a live widget would
    // collide with its own still-registered region from one frame ago.
    for (const scene::RasterRegion& region : std::vector<scene::RasterRegion>(surface_.raster_regions()))
        surface_.remove_raster_region(region.id);

    scene::Painter root_painter(surface_, Rect{0, 0, current_size.width, current_size.height});
    if (terminal_too_small()) {
        paint_too_small_state(root_painter, current_size);
    } else {
        root_.paint_children(root_painter);
    }
    dirty_ = false;

    // surface_ is the sole layer, covering the whole frame — Compositor
    // occlusion math over one layer is trivial, but this is still the
    // real, tested pipeline that turns raster regions (Sixel/kitty
    // image content) into the RasterSlices Presenter needs, rather than
    // a hand-rolled duplicate of that logic here.
    const std::vector<scene::Layer> layers{scene::Layer{0, &surface_, Point{0, 0}, false}};
    compositor_.compose(layers, background_layer_);
    presenter_.present(compositor_.frame().view(), compositor_.cursor(), compositor_.visible_rasters());
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
    std::lock_guard<std::mutex> lock(post_mutex_);
    wake_requested_ = true;
}

void Application::run() {
    Application* previous_instance = g_run_instance.exchange(this, std::memory_order_relaxed);
    void (*previous_handler)(int) = std::signal(SIGINT, handle_run_sigint);
    run_until([] { return false; });
    std::signal(SIGINT, previous_handler);
    g_run_instance.store(previous_instance, std::memory_order_relaxed);
}

bool Application::run_until(const std::function<bool()>& done) {
    // Calls done() exactly once per check (never a redundant extra call
    // after the loop exits) — done() may carry real side effects (a
    // step counter, a captured optional) that a spurious re-evaluation
    // would visibly duplicate.
    bool finished = done();
    while (!finished && !quit_requested()) {
        step(clock_.now_nanos() + kDefaultFrameIntervalNanos);
        finished = done();
    }
    return finished;
}

void Application::push_modal(View& modal_root) {
    CKV_ASSERT(tree_contains(root_, &modal_root));
    modal_stack_.push_back(&modal_root);
}

void Application::pop_modal() {
    CKV_ASSERT(!modal_stack_.empty());
    modal_stack_.pop_back();
}

}  // namespace ckv::ui
