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
    : terminal_(terminal), clock_(clock), root_(Rect{0, 0, terminal.size().width, terminal.size().height}) {}

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
                return false;
            } else if constexpr (std::is_same_v<T, TextEvent>) {
                if (e.from_paste) clipboard_text_ = e.text;  // system import mirrors into the internal clipboard
                for (View* v = focused_; v != nullptr; v = v->parent())
                    if (v->on_text(e)) return true;
                return false;
            } else if constexpr (std::is_same_v<T, MouseEvent>) {
                View* target = mouse_capture_;
                if (target == nullptr) target = topmost_view_at(e.cell);
                if (target == nullptr) return false;
                if (e.action == MouseAction::Down) mouse_capture_ = target;
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
    const std::int64_t effective_deadline = wake_now ? clock_.now_nanos() : deadline_nanos;

    bool did_work = false;
    for (const term::TerminalEvent& event : terminal_.poll(effective_deadline)) {
        did_work = true;
        dispatch(event);
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
    return did_work;
}

void Application::wake() noexcept {
    std::lock_guard<std::mutex> lock(post_mutex_);
    wake_requested_ = true;
}

}  // namespace ckv::ui
