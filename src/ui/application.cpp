// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/application.hpp"

#include <algorithm>
#include <type_traits>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

namespace {

bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
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
    });
    // Every invalidate() anywhere in the tree marks the whole
    // Application dirty (v1 scope: see step()'s doc comment — the
    // dirty RECT itself is intentionally not yet used to scope
    // painting, only to decide WHETHER to paint at all).
    root_.set_dirty_rect_sink([this](Rect) { dirty_ = true; });
}

void Application::set_focus(View* view) {
    CKV_ASSERT(view == nullptr || view->focusable());
    if (view == focused_) return;
    if (focused_ != nullptr) focused_->on_focus(FocusEvent{false});
    focused_ = view;
    if (focused_ != nullptr) focused_->on_focus(FocusEvent{true});
}

void Application::collect_focusable(View& view, std::vector<View*>& out) {
    if (view.focusable()) out.push_back(&view);
    if (!view.visible()) return;  // hidden subtrees are transparent to traversal
    for (const auto& child : view.children()) collect_focusable(*child, out);
}

std::vector<View*> Application::focusable_views() const {
    std::vector<View*> out;
    collect_focusable(const_cast<View&>(root_), out);
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
    command_handlers_[id] = std::move(handler);
}

bool Application::execute_command(CommandId id) {
    auto it = command_handlers_.find(id);
    if (it == command_handlers_.end() || !it->second) return false;
    if (!commands_.is_enabled(id)) return false;
    it->second();
    return true;
}

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
    return visit(root_);
}

bool Application::dispatch(const term::TerminalEvent& event) {
    return std::visit(
        [this](const auto& e) -> bool {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, KeyEvent>) {
                for (View* v = focused_; v != nullptr; v = v->parent())
                    if (v->on_key(e)) return true;
                if (auto id = commands_.command_for_key(e.chord)) return execute_command(*id);
                if (e.chord.key == Key::F1 && help_provider_ && focused_ != nullptr) {
                    const std::string* key = focused_->resolve_help_context_key();
                    if (key != nullptr) {
                        help_provider_(*key);
                        return true;
                    }
                }
                return false;
            } else if constexpr (std::is_same_v<T, TextEvent>) {
                if (e.from_paste) clipboard_text_ = e.text;  // system import mirrors into the internal clipboard
                for (View* v = focused_; v != nullptr; v = v->parent())
                    if (v->on_text(e)) return true;
                return false;
            } else if constexpr (std::is_same_v<T, MouseEvent>) {
                View* target = input_capture_;
                if (target == nullptr) target = mouse_capture_;
                if (target == nullptr) target = topmost_view_at(e.cell);
                if (target == nullptr) return false;
                if (e.action == MouseAction::Down && input_capture_ == nullptr) mouse_capture_ = target;
                const bool handled = target->on_mouse(e);
                if (e.action == MouseAction::Up) mouse_capture_ = nullptr;
                return handled;
            } else if constexpr (std::is_same_v<T, FocusEvent>) {
                if (focused_ == nullptr) return false;
                focused_->on_focus(e);
                return true;
            } else if constexpr (std::is_same_v<T, ResizeEvent>) {
                root_.set_bounds(Rect{0, 0, e.cells.width, e.cells.height});
                return true;
            } else if constexpr (std::is_same_v<T, term::CapabilityChangedEvent>) {
                return false;  // no capability-dependent ui-layer state yet (M4 scope)
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
    root_.paint_children(root_painter);
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

void Application::wake() noexcept {
    std::lock_guard<std::mutex> lock(post_mutex_);
    wake_requested_ = true;
}

}  // namespace ckv::ui
