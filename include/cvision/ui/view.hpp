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
    explicit View(Rect bounds = {}) : bounds_(bounds), preferred_size_(Size{bounds.width, bounds.height}) {}
    virtual ~View();

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

    // Moves `child` to the end of this view's child list — the topmost
    // z-position, since hit-testing (Application::topmost_view_at) and
    // painting both treat later children as on top of earlier ones.
    // Pure in-place reorder: does NOT detach or re-attach, so ownership,
    // the dirty-rect sink, and the detach sink are all left exactly as
    // they were (unlike remove_child()+add_child(), which would fire a
    // spurious detach notification for a window merely being raised to
    // front). A harmless no-op if `child` is not currently a child of
    // this view.
    void raise_to_front(View* child);

    // --- Geometry -------------------------------------------------------

    Rect bounds() const noexcept { return bounds_; }  // parent-local
    void set_bounds(Rect bounds);

    // Hook for containers (Row/Column/Grid/Dock) that must relayout
    // their children whenever their own bounds change size. Called
    // after bounds_ is updated and this view's own invalidation has
    // already been reported; the default does nothing.
    virtual void on_resized() {}

    // Absolute (Desktop-root-relative) bounds, computed by walking
    // ancestors. O(depth); layout and hit-testing use this sparingly
    // and cache within a single pass rather than per-cell.
    Rect absolute_bounds() const noexcept;

    // Preferred size (ARCHITECTURE.md §5 layout): deliberately decoupled
    // from the LIVE bounds_ (set once at construction, or explicitly via
    // set_preferred_size) rather than reflecting wherever a layout
    // container most recently placed this view — a hint that tracked
    // live bounds would feed back into itself across repeated relayouts
    // (a container resizes the view, which changes its own "preferred"
    // size, which the container then reads on the next pass). Widgets
    // with real intrinsic content (Label, Button) override these
    // directly instead of relying on a stored preferred size at all.
    virtual SizeHint horizontal_size_hint() const {
        return SizeHint{0, preferred_size_.width, kUnboundedExtent};
    }
    virtual SizeHint vertical_size_hint() const {
        return SizeHint{0, preferred_size_.height, kUnboundedExtent};
    }
    void set_preferred_size(Size size) { preferred_size_ = size; }

    // The one sanctioned second pass (ARCHITECTURE.md §5): a wrapped
    // view (static text, memo) reports the height it needs for a given
    // width. Default: height is independent of width.
    virtual int height_for_width(int /*width*/) const { return preferred_size_.height; }

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

    // Installed by the owning Application (root only; propagates to
    // every current and future descendant, mirroring DirtyRectSink).
    // Fired for a view — and, as destruction cascades, for every one of
    // its descendants in turn — at the earlier of: remove_child()
    // detaching it (still alive, now owned by the caller), or its own
    // destructor running. This is how Application clears raw View*
    // members (focused_, mouse_capture_) instead of ever holding one
    // past the view's removal or destruction — the observer must not
    // call back into `view` (it may be mid-destructor: its own derived
    // members are already gone by the time ~View() fires this).
    using DetachSink = std::function<void(View&)>;
    void set_detach_sink(DetachSink sink);

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
    void propagate_detach_sink(const DetachSink& sink);
    void notify_detaching_recursive();  // fires detach_sink_ for `this` and every descendant, alive

    View* parent_ = nullptr;
    std::vector<std::unique_ptr<View>> children_;
    Rect bounds_;
    Size preferred_size_;
    bool visible_ = true;
    bool enabled_ = true;
    FocusPolicy focus_policy_ = FocusPolicy::None;
    std::optional<std::string> help_context_key_;
    DirtyRectSink dirty_rect_sink_;
    DetachSink detach_sink_;
};

}  // namespace ckv::ui
