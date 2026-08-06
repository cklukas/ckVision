// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// View: the one base class of the ui tree (ARCHITECTURE.md §5).
// Parents own children (unique_ptr, explicit transfer API); bounds are
// parent-local; invalidation propagates as dirty rects up to whatever
// root observer the Application installs (M4's loop, built next, is
// the one that turns dirty rects into a repaint).
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/scene/painter.hpp"

namespace ckv::ui {

// Requested size along one axis: a view proposes [min, preferred, max];
// a layout container reconciles competing hints (ARCHITECTURE.md §5
// "Layout and dialog construction"). `max` of `Size::unbounded()`'s
// component (see below) means "grow to fill".
struct SizeHint {
    int min = 0;
    int preferred = 0;
    int max = 0;  // kUnboundedExtent means "no upper bound"

    friend bool operator==(const SizeHint&, const SizeHint&) = default;
};

inline constexpr int kUnboundedExtent = -1;

// Whether a view participates in Tab/Shift-Tab traversal and can hold
// focus at all (ARCHITECTURE.md §5 "Focus and traversal"). Disabled and
// hidden views are transparent to traversal regardless of policy.
enum class FocusPolicy {
    None,      // never focusable (e.g. a decorative label)
    TabStop,   // focusable and included in Tab order
};

class View {
public:
    explicit View(Rect bounds = {}) : bounds_(bounds) {}
    virtual ~View() = default;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    // --- Tree ---------------------------------------------------------

    View* parent() const noexcept { return parent_; }
    const std::vector<std::unique_ptr<View>>& children() const noexcept { return children_; }

    // Returns a non-owning observer pointer to the child just added.
    View* add_child(std::unique_ptr<View> child);

    // Detaches and returns ownership of `child` (nullptr if `child` is
    // not one of this view's children). The caller decides its fate —
    // View never silently destroys a view a caller is mid-inspecting.
    std::unique_ptr<View> remove_child(View* child);

    // --- Geometry -------------------------------------------------------

    Rect bounds() const noexcept { return bounds_; }  // parent-local
    void set_bounds(Rect bounds);

    // Absolute (Desktop-root-relative) bounds, computed by walking
    // ancestors. O(depth); layout and hit-testing use this sparingly
    // and cache within a single pass rather than per-cell.
    Rect absolute_bounds() const noexcept;

    virtual SizeHint horizontal_size_hint() const { return SizeHint{0, bounds_.width, kUnboundedExtent}; }
    virtual SizeHint vertical_size_hint() const { return SizeHint{0, bounds_.height, kUnboundedExtent}; }

    // The one sanctioned second pass (ARCHITECTURE.md §5): a wrapped
    // view (static text, memo) reports the height it needs for a given
    // width. Default: height is independent of width.
    virtual int height_for_width(int /*width*/) const { return bounds_.height; }

    bool visible() const noexcept { return visible_; }
    void set_visible(bool visible);

    bool enabled() const noexcept { return enabled_; }
    void set_enabled(bool enabled);

    // --- Painting / invalidation ---------------------------------------

    // Paints this view's own content only (children are painted
    // separately by whatever drives the tree, each through its own
    // translated+clipped Painter) into LOCAL (view-relative) space.
    virtual void draw(scene::Painter& painter) { (void)painter; }

    void invalidate();              // whole view
    void invalidate(Rect local_rect);

    // Installed by the owning Application/root; called with the
    // ABSOLUTE rect that became dirty. A view with no ancestor chain
    // yet installed (not attached under an Application) simply drops
    // the notification — invalidation before attachment is a no-op,
    // not an error, since nothing is driving a paint yet.
    using DirtyRectSink = std::function<void(Rect)>;
    void set_dirty_rect_sink(DirtyRectSink sink);

    // --- Events (ARCHITECTURE.md §5 "Events") ---------------------------
    // Each returns true if it consumed the event (stopping the route).

    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_text(const TextEvent&) { return false; }
    virtual bool on_mouse(const MouseEvent&) { return false; }
    virtual void on_focus(const FocusEvent&) {}

    // --- Focus ------------------------------------------------------

    FocusPolicy focus_policy() const noexcept { return focus_policy_; }
    void set_focus_policy(FocusPolicy policy) { focus_policy_ = policy; }
    bool focusable() const noexcept { return focus_policy_ == FocusPolicy::TabStop && visible_ && enabled_; }

    // --- Help context (ARCHITECTURE.md §5 "Commands and help", D-027) ---

    const std::optional<std::string>& help_context_key() const noexcept { return help_context_key_; }
    void set_help_context_key(std::string key) { help_context_key_ = std::move(key); }

    // Nearest help-context key walking from this view up through
    // ancestors (this view's own key wins if set).
    const std::string* resolve_help_context_key() const noexcept;

private:
    void propagate_dirty_rect_sink(const DirtyRectSink& sink);

    View* parent_ = nullptr;
    std::vector<std::unique_ptr<View>> children_;
    Rect bounds_;
    bool visible_ = true;
    bool enabled_ = true;
    FocusPolicy focus_policy_ = FocusPolicy::None;
    std::optional<std::string> help_context_key_;
    DirtyRectSink dirty_rect_sink_;
};

}  // namespace ckv::ui
