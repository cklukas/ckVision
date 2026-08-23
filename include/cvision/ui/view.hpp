// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// View: the one base class of the ui tree (the architecture §5).
// Parents own children (unique_ptr, explicit transfer API); bounds are
// parent-local; invalidation propagates as dirty rects up to whatever
// root observer the Application installs (M4's loop, built next, is
// the one that turns dirty rects into a repaint).
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/core/cursor.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/pointer_shape.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/context.hpp"

namespace ckv::core {
class TerminalSubsession;
}

namespace ckv::ui {

class Application;

// Requested size along one axis: a view proposes [min, preferred, max];
// a layout container reconciles competing hints (the architecture §5
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
// focus at all (the architecture §5 "Focus and traversal"). Disabled and
// hidden views are transparent to traversal regardless of policy.
enum class FocusPolicy {
    None,      // never focusable (e.g. a decorative label)
    TabStop,   // focusable and included in Tab order
};

// Why a View's pixels are invalid. Retained containers use this to keep an
// unchanged local backing through a translation while still repainting a
// view whose own state asked for new pixels. Geometry is emitted only by
// View::set_bounds(); public invalidate() calls always mean content damage.
enum class InvalidationKind {
    Content,
    Geometry,
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

    // A non-owning identity token for code that must retain a View observer
    // across a user callback. An expired token proves that this particular
    // View instance was destroyed; it therefore also distinguishes a later
    // object allocated at the same address. It grants no ownership or access
    // to the View and is intentionally independent of tree attachment.
    std::weak_ptr<void> lifetime_token() const noexcept { return liveness_; }

    // Returns a non-owning observer pointer to the child just added, or
    // nullptr when an on_attached() / root attachment callback synchronously
    // detached or destroyed that child (or this parent). The type-erased
    // primitive every other insertion form below is built on. The child is
    // linked into the ownership tree before its context callback runs, so an
    // attachment callback can use ordinary remove_child() rather than
    // observing a half-attached tree.
    virtual View* add_child(std::unique_ptr<View> child);

    // Typed insertion (M9/WP-9): adds `child` and returns it back as
    // T*, not View* — no static_cast at the call site. `add_child`
    // itself stays the type-erased primitive library internals reach
    // for when they only have a View&/View*; add<T>/make<T> are what
    // application code and library composition should reach for
    // instead whenever the concrete type is known up front.
    template <class T>
    T* add(std::unique_ptr<T> child) {
        static_assert(std::is_base_of_v<View, T>, "T must derive from ui::View");
        return static_cast<T*>(add_child(std::move(child)));
    }

    // Emplace form of add<T>: constructs T in place and attaches it.
    template <class T, class... Args>
    T* make(Args&&... args) {
        return add(std::make_unique<T>(std::forward<Args>(args)...));
    }

    // Detaches and returns ownership of `child` (nullptr if `child` is
    // not one of this view's children). The caller decides its fate —
    // View never silently destroys a view a caller is mid-inspecting.
    virtual std::unique_ptr<View> remove_child(View* child);

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

    // Moves `child` to the beginning of this view's child list — the
    // backmost z-position. Like raise_to_front(), this is a pure in-place
    // reorder and never detaches or re-attaches the child.
    void lower_to_back(View* child);

    // --- Geometry -------------------------------------------------------

    Rect bounds() const noexcept { return bounds_; }  // parent-local
    void set_bounds(Rect bounds);

    // Hook for containers (Row/Column/Grid/Dock) that must relayout
    // their children whenever their own bounds change size. Called
    // after bounds_ is updated and this view's own invalidation has
    // already been reported; the default does nothing. It may detach
    // or destroy this View; in that case no later bounds observer runs.
    virtual void on_resized() {}

    // Delivers a contained terminal-session state change through the view
    // tree.  Retained containers redraw only after a child invalidates, so a
    // TerminalView uses this notification to invalidate itself when its
    // privately owned session has new output.
    void notify_terminal_subsession_changed(const core::TerminalSubsession& session);

    // A focused editable view may provide one absolute-frame cursor state.
    // The Application publishes only the focused view's cursor after paint;
    // ordinary views return no cursor and cannot affect terminal state.
    virtual std::optional<CursorState> cursor_state() const { return std::nullopt; }

    // Fired after on_resized(), with this view's NEW bounds (local —
    // same value bounds() now returns), whenever set_bounds() actually
    // changed them. The general-purpose escape hatch for an owner that
    // needs to react to a view's own geometry change without
    // subclassing it — the same shape as Button::on_press or
    // Window::on_closed, just for geometry. It is an observer only:
    // framework layout runs through a separate internal path after this
    // observer returns, so replacing or mutating through this callback
    // can never disable layout.
    std::function<void(Rect)> on_bounds_changed;

    // Whether this view is resized to fill Application::root()'s
    // bounds whenever root's OWN bounds change (a terminal resize), and
    // immediately when it becomes a direct root child. The policy every
    // real root child (a Desktop, or an application's own full-screen
    // surface) wants, which is why it defaults to true. Meaningless for
    // any view that is not a direct child of root(); a view that must not
    // stretch with the terminal (a fixed HUD overlay, say) opts out
    // before attachment. Changing this after attachment controls future
    // root resizes; it deliberately does not restore prior geometry.
    bool fills_root() const noexcept { return fills_root_; }
    void set_fills_root(bool fills) noexcept { fills_root_ = fills; }

    // Absolute (Desktop-root-relative) bounds, computed by walking
    // ancestors. O(depth); layout and hit-testing use this sparingly
    // and cache within a single pass rather than per-cell.
    Rect absolute_bounds() const noexcept;

    // --- Where a view is DRAWN, as against where it IS ----------------
    //
    // An offset added to this view's own position within its parent, without
    // touching `bounds()`. Zero for almost everything, and the whole of what
    // makes a panned surface possible: a `Desktop` whose world is larger than
    // the view of it moves its windows by setting this, and every window's
    // `bounds()` goes on meaning what it always meant.
    //
    // That distinction is the point rather than an implementation detail. A
    // window's rect is an ARRANGEMENT — in ckmux it is session state, shared
    // between readers and stored by a server — while a pan is one reader
    // looking somewhere. Folding the second into the first would mean a reader
    // moved everybody's windows by scrolling, and it would do it invisibly,
    // because the numbers would still look like an arrangement.
    //
    // Applied in exactly three places, which is what keeps drawing and
    // hit-testing from disagreeing: the two child-painting paths and
    // `absolute_bounds()`. A view that is drawn somewhere is hit-tested there
    // too, or a reader's click lands on what WOULD have been under the
    // pointer if nobody had scrolled.
    void set_paint_offset(Point offset);
    Point paint_offset() const noexcept { return paint_offset_; }

    // Preferred size (the architecture §5 layout): deliberately decoupled
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

    // True when this view's final row is a cast shadow rather than content.
    //
    // Such a row already stands the view off whatever sits below it, so a
    // container that then adds a margin under it produces two gaps where one
    // was meant -- the blank line under a dialog's buttons. Containers
    // forward the question to whichever child occupies their own last row,
    // so a caller states the padding it wants and the arrangement works out
    // where that padding is already present.
    virtual bool trailing_row_is_shadow() const noexcept { return false; }
    void set_preferred_size(Size size) { preferred_size_ = size; }

    // Fired on the immediate parent (M9/WP-16, E10) whenever one of its
    // direct children calls size_hint_changed() below — a container
    // that must relayout when a child's own preferred size changes
    // WITHOUT the container itself being resized (a Label growing
    // longer inside a Row, a status line's own height changing inside
    // Desktop's dock) overrides this instead of requiring every caller
    // to remember to trigger relayout by hand. Single-hop by design: a
    // container whose own aggregate hint depends on `child`'s (Row/
    // Column, whose horizontal/vertical_size_hint() already reads
    // children() live every call) needs no further action here beyond
    // relaying out; one that itself needs to bubble the change further
    // up calls its own size_hint_changed() from inside this override.
    // Default: does nothing, the same shape as on_descendant_mouse_down.
    virtual void on_child_size_hint_changed(View& child) { (void)child; }

    // The one sanctioned second pass (the architecture §5): a wrapped
    // view (static text, memo) reports the height it needs for a given
    // width. Default: height is independent of width.
    virtual int height_for_width(int /*width*/) const { return preferred_size_.height; }

    bool visible() const noexcept { return visible_; }
    void set_visible(bool visible);

    // This view AND every ancestor visible, which is what "on the frame at
    // all" means: a view is hidden as effectively by a minimized window three
    // levels up as by its own set_visible(false), and its own flag cannot say
    // so. Nothing is painted for such a view, so nothing that describes where
    // it is — its cursor, above all — describes anything the reader can see.
    bool visible_in_tree() const noexcept;

    bool enabled() const noexcept { return enabled_; }
    void set_enabled(bool enabled);

    // Overrides the propagated theme for this view and all descendants. This
    // is the per-window/theme-scope mechanism: a Window can switch to Dark or
    // Mono without reconstructing its controls or changing any constructor
    // signature. Clearing the override restores the parent/Application theme.
    void set_theme_override(Theme theme);
    void clear_theme_override();

    // --- Painting / invalidation ---------------------------------------

    // Paints this view's own content only (children are painted
    // separately by whatever drives the tree, each through its own
    // isolated, translated, and clipped Painter) into LOCAL
    // (view-relative) space. Isolation prevents lower-z siblings' line
    // glyphs from becoming junctions in this view's own frame.
    virtual void draw(scene::Painter& painter) { (void)painter; }

    // Whether this view's retained layer casts the standard scene shadow.
    // Ordinary views are planar; Window and drop-down menu surfaces opt in.
    // A container that drives z-ordered painting applies the shadow after
    // this child, so higher siblings still replace its coverage normally.
    virtual bool casts_shadow() const noexcept { return false; }

    // Paints every child (in z-order: later children on top, matching
    // Application::topmost_view_at's convention), each through its own
    // isolated+translated+clipped Painter derived from `own_painter`
    // (which is in THIS view's own local space). The default just walks
    // children() in order via paint_one_child(). Overridden by Desktop
    // to interleave window-shadow compositing between siblings — a
    // shadow must dim exactly the window's footprint MINUS whatever is
    // drawn above it in z-order, which only the view driving the
    // z-ordered walk itself can get right; a separate post-pass over
    // the whole tree cannot, since it can't tell "drawn after me" from
    // "drawn before me" once painting has finished.
    virtual void paint_children(const scene::Painter& own_painter);

    // Retained-scene paint entry point. The default preserves ordinary
    // tree painting; containers that own composited backing stores override
    // it to paint their static background while contributing retained child
    // layers. Application calls this instead of paint_children().
    // draw_retained() is separate from draw() so a retained container can
    // skip an unchanged base surface without changing the direct Painter
    // contract used by focused scene/unit tests.
    virtual void draw_retained(scene::Painter& painter) { draw(painter); }
    virtual void paint_retained(const scene::Painter& own_painter,
                                std::vector<scene::Layer>& layers);

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

    // Fired after the owning Application has removed its raw observers
    // (focus, capture, modal scope) for this still-live view, but before
    // descendants receive the same notification. A widget may complete
    // its own presentation/lifecycle work here; it must not reattach
    // itself during detachment.
    virtual void on_detaching() {}

    // --- Events (the architecture §5 "Events") ---------------------------
    // Each returns true if it consumed the event (stopping the route).

    // Press and repeat use the ordinary key path. Release has a dedicated
    // hook so existing controls cannot accidentally perform their press
    // behavior a second time when a kitty-enabled terminal reports it.
    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_key_release(const KeyEvent&) { return false; }
    virtual bool on_text(const TextEvent&) { return false; }
    virtual bool on_mouse(const MouseEvent&) { return false; }
    virtual void on_focus(const FocusEvent&) {}

    // --- Pointer shape and hover ---------------------------------------

    // The shape the host's mouse pointer should take while the pointer is
    // at `local` (this view's own coordinates). Returning nullopt means
    // "no opinion", and the question passes to the parent — which is why
    // it is an optional rather than a Default: a Button inside a Window
    // has an opinion about its whole area, while the Window's content pane
    // has none and must not overrule the border its parent draws.
    //
    // Position-dependent because a single view legitimately wants several:
    // a Window is one view whose borders resize, whose corners resize
    // diagonally, and whose title moves the whole thing.
    //
    // Called during pointer routing, potentially on every reported motion.
    // It must be cheap, and it must not mutate: it is a question about
    // appearance, asked of a tree that is not being modified.
    virtual std::optional<PointerShape> pointer_shape_at(Point local) const {
        (void)local;
        return std::nullopt;
    }

    // Whether the pointer is currently over this view — true for the
    // single deepest view under it, not for its ancestors. A container
    // that wants to know the pointer is somewhere inside it asks its
    // children; making every ancestor "hovered" would light up a whole
    // window because the pointer is over one of its buttons.
    //
    // Always false on a terminal that does not report motion: hover is a
    // fact the host supplies, and a session without it simply never has
    // one. Nothing may depend on hover for correctness, only for
    // appearance (the architecture §5 — every pointer affordance has a
    // keyboard equivalent).
    bool hovered() const noexcept { return hovered_; }

    // Fired when hovered() changes. The default does nothing, and a widget
    // that draws itself differently under the pointer overrides it to
    // invalidate.
    //
    // That way round because most views have no hover appearance at all,
    // and the pointer crosses them constantly. An invalidate-by-default
    // would put a dirty rect under the pointer on every cell it travels
    // over — including across a window being dragged, whose whole point is
    // that its retained content is recomposed and not repainted.
    virtual void on_hover_changed(bool now_hovered) { (void)now_hovered; }

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

    // --- Help context (the architecture §5 "Commands and help", D-027) ---

    const std::optional<std::string>& help_context_key() const noexcept { return help_context_key_; }
    void set_help_context_key(std::string key) { help_context_key_ = std::move(key); }

    // Nearest help-context key walking from this view up through
    // ancestors (this view's own key wins if set).
    const std::string* resolve_help_context_key() const noexcept;

    // --- Command context (M9/WP-13) ------------------------------------
    //
    // A focused view can declare the named command context that should be
    // active for commands routed through its focus ancestry. Applications can
    // also push explicit contexts on CommandRegistry for non-focus-driven
    // scopes. The two sources compose: a command whose metadata names a
    // context is available when either source currently names it.
    const std::optional<std::string>& command_context() const noexcept { return command_context_; }
    void set_command_context(std::string context) { command_context_ = std::move(context); }
    void clear_command_context() noexcept { command_context_.reset(); }

protected:
    virtual void on_terminal_subsession_changed(const core::TerminalSubsession& session) { (void)session; }
    // Retained containers observe invalidation without changing the public
    // dirty-rect callback contract. The initiating view receives the first
    // hook, then each ancestor receives the descendant notification.
    virtual void on_invalidated(Rect local_rect, InvalidationKind kind) {
        (void)local_rect;
        (void)kind;
    }
    virtual void on_descendant_invalidated(const View& source, Rect source_local_rect,
                                           InvalidationKind kind) {
        (void)source;
        (void)source_local_rect;
        (void)kind;
    }

    // Called by a subclass whenever its OWN horizontal_size_hint()/
    // vertical_size_hint()/height_for_width() would now return
    // something different than before (M9/WP-16, E10) — e.g. Label/
    // Button's set_text() changing their intrinsic width, StaticText's
    // wrapping changing its intrinsic height. Notifies the immediate
    // parent only (on_child_size_hint_changed(*this)); a harmless no-op
    // before attachment (parent_ is null), the same as invalidate()
    // before a dirty-rect sink is installed.
    void size_hint_changed() {
        if (parent_ != nullptr) parent_->on_child_size_hint_changed(*this);
    }

    // Paints ONE child (visibility-gated) through a Painter translated
    // into its bounds, then recurses into ITS children via its own
    // paint_children() override — the shared per-child step every
    // paint_children() override (including the default) is built from.
    // Returns the child's own Painter (nullopt if the child was
    // invisible and therefore skipped) so an override (Desktop) can
    // reuse the exact same coordinate frame for follow-up compositing
    // (e.g. apply_shadow over a shadow footprint) without
    // re-deriving the translate/clip math.
    static std::optional<scene::Painter> paint_one_child(View& child, const scene::Painter& parent_painter);

private:
    friend class Application;

    // Application installs these on its root only. Keeping layout
    // ownership private makes public geometry observation non-replaceable
    // framework behavior instead of a callback convention.
    using BoundsChangedSink = std::function<void(Rect)>;
    using ChildAttachedSink = std::function<void(View&)>;
    // Application owns hover: it is the only participant that knows where
    // the pointer is, and a view that could declare itself hovered could
    // contradict the one place that knows. A no-op when unchanged, so the
    // notification fires on transitions only.
    void set_hovered(bool hovered);

    void set_bounds_changed_sink(BoundsChangedSink sink) { bounds_changed_sink_ = std::move(sink); }
    void set_child_attached_sink(ChildAttachedSink sink) { child_attached_sink_ = std::move(sink); }

    void propagate_dirty_rect_sink(const DirtyRectSink& sink);
    void propagate_detach_sink(const DetachSink& sink);
    void propagate_context(Context context);
    void notify_detaching_recursive();  // fires detach_sink_ for `this` and every descendant, alive
    void invalidate(Rect local_rect, InvalidationKind kind);

    View* parent_ = nullptr;
    std::vector<std::unique_ptr<View>> children_;
    Rect bounds_;
    Size preferred_size_;
    bool visible_ = true;
    bool enabled_ = true;
    bool hovered_ = false;
    FocusPolicy focus_policy_ = FocusPolicy::None;
    bool fills_root_ = true;
    std::unique_ptr<Theme> theme_override_;
    std::optional<std::string> help_context_key_;
    Point paint_offset_{0, 0};
    std::optional<std::string> command_context_;
    DirtyRectSink dirty_rect_sink_;
    DetachSink detach_sink_;
    BoundsChangedSink bounds_changed_sink_;
    ChildAttachedSink child_attached_sink_;
    // An Application keeps weak references to this token while delivering
    // pointer events. It can therefore prove that a raw View* survived a
    // user callback before using it again, without a global registry.
    std::shared_ptr<void> liveness_ = std::make_shared<int>(0);
    Context context_;
};

}  // namespace ckv::ui
