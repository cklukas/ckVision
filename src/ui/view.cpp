// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/view.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

View::~View() {
    if (detach_sink_) detach_sink_(*this);
    // Children destruct next (as children_ tears down after this body
    // returns), each firing its own detach_sink_ in turn — a subtree
    // torn down without ever going through remove_child() still
    // notifies every descendant, not just its root.
}

View* View::add_child(std::unique_ptr<View> child) {
    CKV_ASSERT(child != nullptr);
    CKV_ASSERT(child->parent_ == nullptr);  // no view may have two owners
    View* observer = child.get();
    const std::weak_ptr<void> parent_liveness = lifetime_token();
    const std::weak_ptr<void> child_liveness = observer->lifetime_token();
    // A root attachment observer is application code. Retain its callable
    // before any earlier attachment callback can destroy this parent.
    const ChildAttachedSink attached = child_attached_sink_;
    child->parent_ = this;
    child->propagate_dirty_rect_sink(dirty_rect_sink_);
    child->propagate_detach_sink(detach_sink_);
    children_.push_back(std::move(child));

    // on_attached() is user code. The child must already be owned so its
    // callback can detach itself normally; after that callback neither its
    // observer nor this parent can be assumed to survive. Context is passed
    // by value because this parent may disappear during the recursive walk.
    observer->propagate_context(context_);
    if (parent_liveness.expired() || child_liveness.expired() || observer->parent_ != this)
        return nullptr;

    if (attached) attached(*observer);
    if (parent_liveness.expired() || child_liveness.expired() || observer->parent_ != this)
        return nullptr;
    return observer;
}

void View::lower_to_back(View* child) {
    const auto found = std::find_if(children_.begin(), children_.end(),
                                    [child](const std::unique_ptr<View>& candidate) {
                                        return candidate.get() == child;
                                    });
    if (found == children_.end() || found == children_.begin()) return;
    std::unique_ptr<View> owned = std::move(*found);
    children_.erase(found);
    children_.insert(children_.begin(), std::move(owned));
    invalidate();
}

std::unique_ptr<View> View::remove_child(View* child) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->get() == child) {
            std::unique_ptr<View> owned = std::move(*it);
            children_.erase(it);
            owned->parent_ = nullptr;
            // Still fully alive here (only detached, not destroyed) —
            // notify while the sink is still installed, THEN clear it,
            // so a later destruction of this same subtree does not
            // fire a second, redundant notification.
            owned->notify_detaching_recursive();
            owned->propagate_dirty_rect_sink(nullptr);
            owned->propagate_detach_sink(nullptr);
            return owned;
        }
    }
    return nullptr;
}

void View::raise_to_front(View* child) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->get() == child) {
            if (it + 1 == children_.end()) return;  // already topmost
            std::unique_ptr<View> owned = std::move(*it);
            children_.erase(it);
            children_.push_back(std::move(owned));
            invalidate();
            return;
        }
    }
}

void View::set_bounds(Rect bounds) {
    if (bounds == bounds_) return;
    // Every hook below is application code. Preserve this View's identity
    // before the first one runs: on_resized() may detach and destroy this
    // object, in which case no later observer belongs to the former geometry
    // transition.
    const std::weak_ptr<void> self_liveness = lifetime_token();
    const Rect old_absolute = absolute_bounds();
    bounds_ = bounds;
    invalidate(Rect{0, 0, bounds_.width, bounds_.height}, InvalidationKind::Geometry);
    if (self_liveness.expired()) return;
    if (dirty_rect_sink_) dirty_rect_sink_(old_absolute);  // and the vacated old bounds
    if (self_liveness.expired()) return;
    on_resized();
    if (self_liveness.expired()) return;
    if (on_bounds_changed) on_bounds_changed(bounds_);
    if (self_liveness.expired()) return;
    if (bounds_changed_sink_) bounds_changed_sink_(bounds_);
}

Rect View::absolute_bounds() const noexcept {
    Rect result = bounds_;
    // This view's own paint offset, then every ancestor's position AND theirs:
    // where a view really is on the screen is where the chain of parents draws
    // it, which is not the same as where the chain of parents SAYS it is the
    // moment any of them is panned (see set_paint_offset).
    result.x += paint_offset_.x;
    result.y += paint_offset_.y;
    for (const View* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_) {
        result.x += ancestor->bounds_.x + ancestor->paint_offset_.x;
        result.y += ancestor->bounds_.y + ancestor->paint_offset_.y;
    }
    return result;
}

void View::set_paint_offset(Point offset) {
    if (offset.x == paint_offset_.x && offset.y == paint_offset_.y) return;
    paint_offset_ = offset;
    // The view has moved on screen without its bounds changing, so nothing
    // else will invalidate for it — and its OLD footprint needs repainting as
    // much as its new one, which is the parent's business rather than this
    // view's.
    if (parent_ != nullptr) parent_->invalidate();
    invalidate();
}

bool View::visible_in_tree() const noexcept {
    for (const View* current = this; current != nullptr; current = current->parent_)
        if (!current->visible_) return false;
    return true;
}

void View::set_visible(bool visible) {
    if (visible == visible_) return;
    visible_ = visible;
    invalidate();
}

void View::set_enabled(bool enabled) {
    if (enabled == enabled_) return;
    enabled_ = enabled;
    invalidate();
}

void View::set_hovered(bool hovered) {
    if (hovered == hovered_) return;
    hovered_ = hovered;
    on_hover_changed(hovered_);
}

void View::set_theme_override(Theme theme) {
    theme_override_ = std::make_unique<Theme>(std::move(theme));
    if (context_.valid()) {
        Context override_context = context_;
        override_context.theme = theme_override_.get();
        propagate_context(override_context);
    }
    invalidate();
}

void View::clear_theme_override() {
    if (theme_override_ == nullptr) return;
    theme_override_.reset();
    if (parent_ != nullptr)
        propagate_context(parent_->context_);
    invalidate();
}

void View::invalidate() { invalidate(Rect{0, 0, bounds_.width, bounds_.height}); }

void View::invalidate(Rect local_rect) {
    invalidate(local_rect, InvalidationKind::Content);
}

void View::invalidate(Rect local_rect, InvalidationKind kind) {
    on_invalidated(local_rect, kind);
    for (View* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_)
        ancestor->on_descendant_invalidated(*this, local_rect, kind);
    if (!dirty_rect_sink_) return;
    const Rect absolute = absolute_bounds();
    dirty_rect_sink_(Rect{absolute.x + local_rect.x, absolute.y + local_rect.y, local_rect.width,
                           local_rect.height});
}

std::optional<scene::Painter> View::paint_one_child(
    View& child, const scene::Painter& parent_painter) {
    if (!child.visible()) return std::nullopt;
    const Rect local_bounds{0, 0, child.bounds().width, child.bounds().height};
    scene::Painter child_painter = parent_painter.isolated().translated(
        Point{child.bounds().x + child.paint_offset_.x, child.bounds().y + child.paint_offset_.y},
        local_bounds);
    child.draw(child_painter);
    child.paint_children(child_painter);
    return child_painter;
}

void View::paint_children(const scene::Painter& own_painter) {
    for (auto& child : children_) paint_one_child(*child, own_painter);
}

void View::notify_terminal_subsession_changed(const core::TerminalSubsession& session) {
    on_terminal_subsession_changed(session);
    const std::weak_ptr<void> self_liveness = lifetime_token();
    struct ChildHandle {
        View* view = nullptr;
        std::weak_ptr<void> liveness;
    };
    std::vector<ChildHandle> children;
    children.reserve(children_.size());
    for (const auto& child : children_)
        children.push_back(ChildHandle{child.get(), child->lifetime_token()});
    for (const ChildHandle& child : children) {
        if (self_liveness.expired()) return;
        if (child.liveness.expired() || child.view->parent_ != this) continue;
        child.view->notify_terminal_subsession_changed(session);
    }
}

void View::paint_retained(const scene::Painter& own_painter,
                          std::vector<scene::Layer>& layers) {
    for (auto& child_ptr : children_) {
        View& child = *child_ptr;
        if (!child.visible()) continue;
        const Rect local_bounds{0, 0, child.bounds().width, child.bounds().height};
        scene::Painter child_painter = own_painter.isolated().translated(
            Point{child.bounds().x + child.paint_offset_.x,
                  child.bounds().y + child.paint_offset_.y},
            local_bounds);
        child.draw_retained(child_painter);
        child.paint_retained(child_painter, layers);
    }
}

const std::string* View::resolve_help_context_key() const noexcept {
    for (const View* v = this; v != nullptr; v = v->parent_)
        if (v->help_context_key_) return &*v->help_context_key_;
    return nullptr;
}

void View::propagate_dirty_rect_sink(const DirtyRectSink& sink) {
    dirty_rect_sink_ = sink;
    for (auto& child : children_) child->propagate_dirty_rect_sink(sink);
}

void View::set_dirty_rect_sink(DirtyRectSink sink) { propagate_dirty_rect_sink(sink); }

void View::propagate_detach_sink(const DetachSink& sink) {
    detach_sink_ = sink;
    for (auto& child : children_) child->propagate_detach_sink(sink);
}

void View::set_detach_sink(DetachSink sink) { propagate_detach_sink(sink); }

void View::propagate_context(Context context) {
    const std::weak_ptr<void> self_liveness = lifetime_token();
    const bool newly_valid = !context_.valid() && context.valid();
    if (theme_override_ != nullptr)
        context.theme = theme_override_.get();
    context_ = context;
    if (newly_valid) on_attached();
    if (self_liveness.expired()) return;

    // A descendant's on_attached() can detach or destroy siblings, or this
    // whole subtree. Snapshot per-instance identities before invoking any
    // further callback and revalidate before each recursive propagation.
    struct ChildHandle {
        View* view = nullptr;
        std::weak_ptr<void> liveness;
    };
    std::vector<ChildHandle> children;
    children.reserve(children_.size());
    for (const auto& child : children_)
        children.push_back(ChildHandle{child.get(), child->lifetime_token()});
    for (const ChildHandle& child : children) {
        if (self_liveness.expired()) return;
        if (child.liveness.expired() || child.view->parent_ != this) continue;
        child.view->propagate_context(context);
    }
}

void View::set_context(const Context& context) { propagate_context(context); }

void View::notify_detaching_recursive() {
    if (detach_sink_) detach_sink_(*this);
    on_detaching();
    for (auto& child : children_) child->notify_detaching_recursive();
}

}  // namespace ckv::ui
