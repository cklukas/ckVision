// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/view.hpp"

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
    child->parent_ = this;
    child->propagate_dirty_rect_sink(dirty_rect_sink_);
    child->propagate_detach_sink(detach_sink_);
    children_.push_back(std::move(child));
    return observer;
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

void View::set_bounds(Rect bounds) {
    if (bounds == bounds_) return;
    const Rect old_absolute = absolute_bounds();
    bounds_ = bounds;
    invalidate();  // damage the new bounds
    if (dirty_rect_sink_) dirty_rect_sink_(old_absolute);  // and the vacated old bounds
    on_resized();
}

Rect View::absolute_bounds() const noexcept {
    Rect result = bounds_;
    for (const View* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_) {
        result.x += ancestor->bounds_.x;
        result.y += ancestor->bounds_.y;
    }
    return result;
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

void View::invalidate() { invalidate(Rect{0, 0, bounds_.width, bounds_.height}); }

void View::invalidate(Rect local_rect) {
    if (!dirty_rect_sink_) return;
    const Rect absolute = absolute_bounds();
    dirty_rect_sink_(Rect{absolute.x + local_rect.x, absolute.y + local_rect.y, local_rect.width,
                           local_rect.height});
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

void View::notify_detaching_recursive() {
    if (detach_sink_) detach_sink_(*this);
    for (auto& child : children_) child->notify_detaching_recursive();
}

}  // namespace ckv::ui
