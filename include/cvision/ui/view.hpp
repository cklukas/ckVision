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
#include "cvision/ui/context.hpp"

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

    // The polymorphic entry point for "detach whatever this child is,
    // correctly" — for a caller that only knows it has a View* and a
    // parent View*, with no way to know whether the parent needs MORE
    // than plain child-list removal to stay internally consistent
    // (Desktop overrides this to also drop the child from windows_/
    // popups_ when applicable; the default here is exactly
    // remove_child()). widgets::schedule_self_detach is the motivating
    // caller: a Window doesn't know what kind of View parents it.
    virtual std::unique_ptr<View> detach_child(View* child) { return remove_child(child); }

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

    // Fired after on_resized(), with this view's NEW bounds (local —
    // same value bounds() now returns), whenever set_bounds() actually
    // changed them. The general-purpose escape hatch for an owner that
    // needs to react to a view's own geometry change without
    // subclassing it — the same shape as Button::on_press or
    // Window::on_closed, just for geometry. Application installs one
    // on root() to implement the fill-the-root resize policy (see
    // fills_root() below); nothing else in the library sets this by
    // default.
    std::function<void(Rect)> on_bounds_changed;

    // Whether this view is resized to fill Application::root()'s
    // bounds whenever root's OWN bounds change (a terminal resize) —
    // the policy every real root child (a Desktop, or an application's
    // own full-screen surface) wants, which is why it defaults to
    // true. Meaningless for any view that is not a direct child of
    // root(); a view that must not stretch with the terminal (a fixed
    // HUD overlay, say) opts out here.
    bool fills_root() const noexcept { return fills_root_; }
    void set_fills_root(bool fills) noexcept { fills_root_ = fills; }

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

    // Paints every child (in z-order: later children on top, matching
    // Application::topmost_view_at's convention), each through its own
    // translated+clipped Painter derived from `own_painter` (which is
    // in THIS view's own local space). The default just walks
    // children() in order via paint_one_child(). Overridden by Desktop
    // to interleave window-shadow compositing between siblings — a
    // shadow must dim exactly the window's footprint MINUS whatever is
    // drawn above it in z-order, which only the view driving the
    // z-ordered walk itself can get right; a separate post-pass over
    // the whole tree cannot, since it can't tell "drawn after me" from
    // "drawn before me" once painting has finished.
    virtual void paint_children(const scene::Painter& own_painter);

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

    // --- Context (theme/roles/application access, D-028) ----------------
    //
    // Propagates exactly like DirtyRectSink/DetachSink above: Application
    // installs one on root() once; add_child propagates whatever context
    // THIS view already has down to a newly attached subtree, so a
    // widget tree built in memory and attached in one shot resolves
    // correctly the moment it lands under a live context. Also settable
    // directly on any (possibly never-attached) view for headless unit
    // testing without a full Application — see context.hpp.
    const Context& context() const noexcept { return context_; }
    void set_context(const Context& context);

    // Fired exactly once, the first time this view's context() becomes
    // valid() — either via attachment under a context-bearing parent or
    // a direct set_context() call. This is where a widget interns its
    // own standard role names (RoleRegistry::intern is idempotent, so
    // calling it again after Application's own intern_standard_roles
    // pass is always safe) and caches the resolved RoleIds; the default
    // does nothing, for views (plain containers, most of the tree) that
    // carry no theming of their own.
    virtual void on_attached() {}

    // --- Events (ARCHITECTURE.md §5 "Events") ---------------------------
    // Each returns true if it consumed the event (stopping the route).

    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_text(const TextEvent&) { return false; }
    virtual bool on_mouse(const MouseEvent&) { return false; }
    virtual void on_focus(const FocusEvent&) {}

    // Capture-phase notification (M8 WP-3): fired on every ancestor of
    // `target` — NOT on target itself — when target is about to
    // receive a MouseAction::Down, before that delivery happens.
    // Default: does nothing. This exists because ordinary event
    // delivery only reaches the single deepest hit-tested view: a
    // click on a Button inside a Window's content is delivered
    // directly to the Button, and Window::on_mouse (which only knows
    // about clicks on its OWN frame chrome — title bar, close/zoom,
    // resize grip) never runs for it at all. Desktop overrides this to
    // activate+raise whichever of its owned windows is `target` or an
    // ancestor of it, regardless of how deep the actual click landed —
    // the mechanism behind "clicking anywhere in a background window
    // brings it to front," not just its title bar.
    virtual void on_descendant_mouse_down(View& target) { (void)target; }

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

protected:
    // Paints ONE child (visibility-gated) through a Painter translated
    // into its bounds, then recurses into ITS children via its own
    // paint_children() override — the shared per-child step every
    // paint_children() override (including the default) is built from.
    // Returns the child's own Painter (nullopt if the child was
    // invisible and therefore skipped) so an override (Desktop) can
    // reuse the exact same coordinate frame for follow-up compositing
    // (e.g. transform_style over a shadow footprint) without
    // re-deriving the translate/clip math.
    static std::optional<scene::Painter> paint_one_child(View& child, const scene::Painter& parent_painter);

private:
    void propagate_dirty_rect_sink(const DirtyRectSink& sink);
    void propagate_detach_sink(const DetachSink& sink);
    void propagate_context(const Context& context);
    void notify_detaching_recursive();  // fires detach_sink_ for `this` and every descendant, alive

    View* parent_ = nullptr;
    std::vector<std::unique_ptr<View>> children_;
    Rect bounds_;
    Size preferred_size_;
    bool visible_ = true;
    bool enabled_ = true;
    FocusPolicy focus_policy_ = FocusPolicy::None;
    bool fills_root_ = true;
    std::optional<std::string> help_context_key_;
    DirtyRectSink dirty_rect_sink_;
    DetachSink detach_sink_;
    Context context_;
};

}  // namespace ckv::ui
