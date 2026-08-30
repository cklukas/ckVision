// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Window: frame chrome, title, close/zoom controls, move/resize (mouse
// drag or keyboard mode), min/max size limits, shadow declaration, and
// a single content view (the architecture §5 "Windows, popups,
// modality"). Desktop-agnostic and independently testable — z-order,
// activation, and tile/cascade are Desktop's job once it exists;
// Window exposes what Desktop needs (casts_shadow(), the close
// protocol, active()/set_active()) without depending on it.
//
// Scope note: backing store ("drag never repaints content") is listed
// as beyond-baseline in the widget catalog — baseline Window draws
// through the ordinary View/Painter tree like everything else, so
// moving/resizing a window repaints it like any other bounds change.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include <unordered_map>
#include <vector>

#include "cvision/ui/anchor_pane.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Which border a frame overlay lives on (M10/WP-20). Top and Bottom
// carry the short text runs (a path indicator, a "line:col" readout)
// that a one-row border has room for. Left and Right are one cell wide
// — no room for words, exactly room for the one overlay a side border
// has always carried on the classic desktop: a scrollbar standing in
// the frame between the corner cells, costing the content nothing. A
// side slot spans rows, so its `alignment` works down the border the
// way a top slot's works across it, and `Fill` — the shape a scrollbar
// wants — takes the whole run between the corners. Still not Dock's
// Center: a border has no middle ground to give.
enum class Edge { Top, Bottom, Left, Right };

// `alignment` is `ui::Alignment` (Start/Center/End/Fill) — the SAME
// enum Row/Column/Grid already use, positioning the overlay within its
// edge's own row or column exactly the way align_cross_axis positions
// a child within its container. `offset` nudges the result along the
// edge — right or down for positive — from where `alignment` alone
// would place it, still clamped to the edge's own available span.
//
// Edge::Top only accepts Start/End: the title already owns Center, and
// Window::add_frame_overlay CKV_ASSERTs against Top+Center or Top+Fill
// rather than leaving an implicit "whoever draws last wins" collision
// with the title — the "deterministic precedence" is "the title always
// wins Center on Top," not a stacking order. Edge::Bottom accepts all
// four; there is no title there to collide with.
struct FrameSlot {
    Edge edge = Edge::Bottom;
    ui::Alignment alignment = ui::Alignment::End;  // the old hard-wired bottom-right default
    int offset = 0;
};

// How a FLOATING window's bounds respond to its owning Desktop growing (M8
// WP-4) — orthogonal to reposition_within()'s shrink-side clamping, which
// every floating window gets regardless of policy. A window that is part of
// an arrangement its Desktop remembers — a tiling command, a cascade, a
// restored snapshot, or a tiling a reader built by hand — is sized by that
// arrangement on every resize instead, in both directions and whatever this
// enum says. The two never overlap: a Desktop refuses to remember any
// arrangement containing a window whose policy is not None, or that is
// zoomed, precisely so that no window is ever sized twice.
enum class DesktopGrowPolicy {
    None,         // a floating window is never grown by the desktop, only clamped on shrink — the default
    KeepFilling,  // stays sized to exactly fill Desktop::content_area(), like a permanently zoomed window
    AnchorEdges,  // keeps its current distance to the right/bottom edges as the desktop grows or shrinks
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.window.frame.active/inactive" and
// "ckv.window.title.active/inactive" — the document-window family.
// set_role_override redirects all four at once; the dialog/message-
// box/file-dialog family of internal windows calls it with
// StandardRoles::dialog_frame/dialog_background (frame and title
// share one role in that family) immediately after construction.
class Window : public ui::View {
public:
    explicit Window(std::string title);

    void set_role_override(ui::RoleId frame_active_role, ui::RoleId frame_inactive_role,
                            ui::RoleId title_active_role, ui::RoleId title_inactive_role) noexcept {
        frame_active_role_ = frame_active_role;
        frame_inactive_role_ = frame_inactive_role;
        title_active_role_ = title_active_role;
        title_inactive_role_ = title_inactive_role;
    }

    // Keeps the theme's foregrounds and attributes while replacing the
    // background shared by the frame, title, footer, controls, and uncovered
    // interior. This is useful for content whose surrounding colour is
    // runtime state rather than an application theme role (for example a
    // video adapter's overscan colour).
    void set_chrome_background_override(std::optional<Color> color) noexcept {
        if (chrome_background_override_ == color) return;
        chrome_background_override_ = color;
        invalidate();
    }
    [[nodiscard]] std::optional<Color> chrome_background_override() const noexcept {
        return chrome_background_override_;
    }

    void set_title(std::string title);
    const std::string& title() const noexcept { return title_; }

    // A line on the BOTTOM border, left-aligned: what this window is
    // currently costing, counting, or waiting for.
    //
    // The top border says what a window IS and stays put; a footer says
    // what it is doing right now and changes as it does. Putting the
    // second on the frame rather than inside the content is what keeps a
    // running total or a progress note from stealing a row from the
    // thing the reader came to look at — and a window narrow enough that
    // the note would collide with the resize grip simply elides it.
    void set_footer(std::string footer);
    const std::string& footer() const noexcept { return footer_; }

    // Desktop sets this on activation/deactivation (the topmost window
    // is active; exactly one window is active at a time, or none).
    void set_active(bool active);
    bool active() const noexcept { return active_; }

    // The sole content child, positioned to fill the interior (inside
    // the 1-cell frame border). Replaces and returns ownership of any
    // previous content (nullptr if there was none).
    std::unique_ptr<ui::View> set_content(std::unique_ptr<ui::View> content);
    ui::View* content() const noexcept { return content_; }

    // The framed content's intrinsic size plus the two-cell frame. Desktop
    // presentation uses these hints to give an otherwise unpositioned modal
    // a visible, centered initial rectangle; callers that set bounds retain
    // complete control over modeless-window geometry.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    int height_for_width(int width) const override;

    // The interior rect set_content() will resize whatever's passed to
    // it into (inside the 1-cell frame border). Public so a caller can
    // size a content view correctly BEFORE constructing it, when the
    // view's own constructor needs a real Rect up front — e.g.
    // ui::Grid's or widgets::Splitter's, both of which compute their
    // initial layout (a Splitter's own starting 50/50 split) from
    // whatever bounds they're given at construction, not a later
    // resize.
    Rect content_rect() const noexcept;

    // Blank cells held between the frame and the content on each axis.
    // Text pressed against a border is harder to read than text with a
    // margin, and a dialog that keeps one reads as a composed panel
    // rather than as content stuffed into a box. Zero (the default)
    // leaves the content flush with the frame, which is what a view
    // meant to fill its window — an editor, a list — wants.
    void set_content_margin(int horizontal, int vertical);
    int horizontal_content_margin() const noexcept { return horizontal_content_margin_; }
    int vertical_content_margin() const noexcept { return vertical_content_margin_; }

    // Per-edge margins, for a layout whose edges do not want the same
    // padding. A button row is the usual reason: its cast shadow already
    // separates it from the frame, so a bottom margin under it reads as a
    // blank row nobody asked for, while the top of the window still wants
    // its breathing space. Symmetric padding cannot express that.
    void set_content_margins(int left, int top, int right, int bottom);
    void set_move_bounds(Rect bounds) noexcept { move_bounds_ = bounds; }
    // The margins actually applied. The bottom one is dropped when the
    // content's last row is a cast shadow: measuring, sizing and painting
    // all ask here, so a window cannot be sized for a gap it does not draw.
    int effective_top_margin() const noexcept;
    int effective_bottom_margin() const noexcept;
    int top_content_margin() const noexcept { return top_content_margin_; }
    int bottom_content_margin() const noexcept { return bottom_content_margin_; }

    // The free-placement, resize-aware alternative to hand-building a
    // set_content() call yourself (M10/WP-18): lazily installs a fresh
    // ui::AnchorPane as this window's content the first time it's
    // called, so children added to it (ui::AnchorPane::add_item) keep
    // their own explicit bounds AND a sane position/size as the window
    // resizes, per each child's own ui::Anchors — a bottom-right status
    // label keeps its corner distance; an all-anchored child stretches
    // with the dialog. CKV_ASSERT if content() is already set to
    // something this method didn't itself install — content_pane() and
    // set_content() both claim the one content slot, not composable.
    ui::AnchorPane& content_pane();

    // Small children that live ON the frame border itself rather than
    // in the content interior (M10/WP-20) — the classic "current
    // line:col" indicator an editor shows in its own window's bottom
    // border, or a path indicator in the opposite corner, or any other
    // small at-a-glance readout that shouldn't cost a row of content
    // space. Each positioned at its own horizontal_size_hint().preferred
    // width (height always 1) per `slot` — repositioned automatically
    // on every window resize and on its own size-hint change (M9/WP-16),
    // same as content(). At most one overlay per (edge, alignment) pair
    // — CKV_ASSERTs if that pair is already occupied; remove_frame_
    // overlay first to replace one, or nest several views in a Row
    // yourself if you need more than one at the same position (the same
    // answer Row/Column/Grid/Dock give for anything beyond their own
    // scope). Unlike content(), unset overlays are the common case, so
    // there is no "must call this" contract — a plain Window with none
    // behaves exactly as before this existed. Typed insertion returns the
    // concrete observer pointer, same as View::add/Desktop::dock_*.
    template <class T>
    T* add_frame_overlay(std::unique_ptr<T> view, FrameSlot slot) {
        static_assert(std::is_base_of_v<ui::View, T>, "T must derive from ui::View");
        return static_cast<T*>(add_frame_overlay_impl(std::move(view), slot));
    }
    // Detaches a specific overlay, returning ownership (nullptr if
    // `view` is not a currently-attached overlay of this window).
    std::unique_ptr<ui::View> remove_frame_overlay(ui::View* view);

    void set_min_size(Size min) noexcept;
    void set_max_size(Size max) noexcept;  // {0,0} means unbounded (the default)

    void set_movable(bool movable) noexcept { movable_ = movable; }
    bool movable() const noexcept { return movable_; }
    void set_resizable(bool resizable) noexcept { resizable_ = resizable; }
    bool resizable() const noexcept { return resizable_; }

    // Vetoable close protocol (the architecture §5 "Closing is a
    // protocol, not an act"): close() invokes close_request if set; a
    // false return vetoes the close (close() itself returns false, and
    // nothing else happens). Otherwise on_closed fires so the owner
    // (Desktop) can remove/destroy this window. close() never destroys
    // the window itself — lifetime stays with whoever owns it in the
    // View tree. A recursive close() from either callback is accepted as
    // an idempotent no-op until the outer close request returns, so one
    // user request cannot re-enter its own callbacks. A close_request may
    // exceptionally detach and destroy its Window; a true return then
    // completes that close without running on_closed, because there is no
    // remaining instance or owner for a later protocol stage.
    std::function<bool()> close_request;
    std::function<void()> on_closed;
    bool close();

    // Optional hooks, invoked by on_key when Enter/Escape is not
    // already consumed by keyboard move/resize mode: accept_request
    // for Enter, cancel_request for Escape. Unset by default (a plain
    // Window doesn't treat Enter/Escape specially at all). This is how
    // widgets/dialog.hpp wires "Dialog accept" (default button, Enter,
    // accept-time validation veto) and "Esc cancels, bypassing
    // validation" onto a Window hosting a materialized dialog, without
    // Window itself knowing anything about dialogs, buttons, or
    // validation.
    std::function<void()> accept_request;
    std::function<void()> cancel_request;

    // Zoom/restore. `available` is the bounds to zoom into (typically
    // Desktop's content area) — passed explicitly so Window never has
    // to know about Desktop.
    void toggle_zoom(Rect available);
    // Whether this window currently presents as maximized. KeepFilling is a
    // permanent maximized presentation, unlike zoomed(), which records a
    // restorable pre-zoom geometry.
    bool maximized() const noexcept { return zoomed_ || grow_policy_ == DesktopGrowPolicy::KeepFilling; }
    bool zoomed() const noexcept { return zoomed_; }

    // Resizes to exactly fill `available`, honoring min/max size
    // limits (shrinking to fit a max_size smaller than `available`, or
    // growing to min_size's floor if `available` is smaller) — reused
    // by both refresh_zoom_area() and DesktopGrowPolicy::KeepFilling.
    void fill(Rect available) noexcept;

    // Re-applies the CURRENT zoom target — a no-op unless zoomed().
    // Desktop::on_resized() calls this on every zoomed window so a
    // maximized window keeps filling its content area as the desktop
    // grows or shrinks, rather than staying pinned at whatever size it
    // had when it was first zoomed. Does not touch restored_bounds_,
    // so un-zooming later still restores to the size before zooming
    // began, regardless of how many resizes happened while zoomed.
    void refresh_zoom_area(Rect available) noexcept;

    // --- Minimize (U4-i) -------------------------------------------
    //
    // Hiding a window is neither closing it nor merely making it
    // invisible. A minimized window stays in its Desktop's windows() —
    // that listing is how a reader gets it back, from a switcher bar —
    // while everything that ARRANGES or CYCLES windows steps over it:
    // no tiling gives it a band, activate_next/previous walk past it,
    // and the filled-tiling query does not count it.
    //
    // Minimizing disturbs nothing about the window's geometry: its
    // bounds, its zoom state and its place in the stack are what they
    // were, and that is why restoring needs no remembered copy of any of
    // them. A window minimized while maximized comes back maximized
    // because it never stopped being maximized. A recorded rectangle or
    // stack index would be a second source of truth that a desktop
    // resize, or a neighbour closing, could silently make wrong — and a
    // window that comes back somewhere it does not belong is exactly the
    // failure the reader would see.
    //
    // Its owner's sizing policy does still reach it while it is hidden:
    // Desktop::on_resized() re-clamps a hidden window and keeps a hidden
    // maximized one filling, because a frozen rectangle is precisely how
    // a window comes back off the edge of a desktop that shrank while it
    // was away.
    //
    // Invisibility is the MECHANISM, not the state: set_visible(false)
    // is already what the framework understands as "not painted, not
    // clickable, transparent to focus traversal", and a second private
    // notion of hiddenness inside Window would be one more thing for
    // hit-testing to get wrong. minimized() is the state a listing view
    // reads and the arrangement code skips on.
    void set_minimized(bool minimized);
    bool minimized() const noexcept { return minimized_; }

    // Whether the frame draws and answers the `_` control, immediately
    // left of the maximize/restore one — from 22 columns wide, which is
    // where three controls still leave the window its own name (see
    // draws_minimize_control). Default true, but — like the zoom
    // control — a fixed-size window keeps the close control alone:
    // an alert or an About box is not a thing a reader parks. Desktop's
    // modal presentation clears this as well, because hiding the one
    // window that is accepting input would leave an application that
    // answers nothing and no way to bring it back.
    void set_minimizable(bool minimizable) noexcept;
    bool minimizable() const noexcept { return minimizable_; }

    // How this window's bounds respond to Desktop growth (default
    // None — see DesktopGrowPolicy). Desktop::on_resized() reads this
    // on every owned window; irrelevant for a standalone Window.
    void set_grow_policy(DesktopGrowPolicy policy) noexcept {
        if (grow_policy_ == policy) return;
        grow_policy_ = policy;
        invalidate();  // KeepFilling changes the visible maximize/restore control.
    }
    DesktopGrowPolicy grow_policy() const noexcept { return grow_policy_; }

    // Clamps this window's bounds to fit and remain reachable within
    // `available` (the architecture §5 "Sizing policy": "On shrink,
    // windows are deterministically clamped and repositioned to remain
    // reachable"). Shrinks (honoring min/max_size) if the window is
    // currently wider/taller than `available`, then repositions so no
    // edge falls outside `available` — never just clips silently. A
    // no-op if the window already fits. Desktop calls this on every
    // owned window from its own on_resized().
    void reposition_within(Rect available) noexcept;

    // Keyboard move/resize mode: while active, arrow keys nudge one
    // cell per press; Enter confirms (keeps the new bounds); Esc
    // reverts to the bounds captured when the mode was entered.
    void enter_move_mode();
    void enter_resize_mode();
    bool in_keyboard_mode() const noexcept { return keyboard_mode_ != KeyboardMode::None; }

    // Whether this window is currently painting without its pictures,
    // which it does for the duration of a move or resize. Observable
    // because "the picture went away while I dragged" is a question a
    // reader — and a test — is entitled to an answer to.
    bool rasters_suppressed() const noexcept { return rasters_suppressed_; }

    bool casts_shadow() const noexcept override { return true; }

    // Desktop's retained-scene path owns the layer relationship; a Window
    // owns only its local backing store. Moving changes the layer position
    // and therefore never enters this repaint operation.
    bool repaint_backing_if_needed();
    scene::Surface& backing_surface() noexcept;
    const scene::Surface& backing_surface() const noexcept;
    std::size_t content_repaint_count() const noexcept { return content_repaint_count_; }

    // Runs after this Window has left the Application tree and its
    // modal scope (if any) has been removed. Standard-dialog
    // presentation helpers use it to publish completion only once no
    // stale input scope remains. General windows may use it for their
    // own detach cleanup.
    std::function<void()> on_detached;

    void draw(scene::Painter& painter) override;
    void on_resized() override;
    void on_attached() override;
    void on_detaching() override;
    // Re-derives and reapplies ONE frame overlay's position/width
    // whenever ITS OWN preferred size changes (M9/WP-16) — e.g. a Label
    // overlay whose set_text() makes it longer or shorter — without the
    // caller having to trigger it by hand. content() needs no reaction
    // here: content_rect() depends only on this Window's own bounds(),
    // never on content()'s size hint.
    void on_child_size_hint_changed(ui::View& child) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // The frame's own affordances, and only those it really has. A ckVision
    // window resizes from its corners, so the corners say so and the plain
    // edges between them say nothing rather than promising a resize that
    // pressing there would not start.
    std::optional<PointerShape> pointer_shape_at(Point local) const override;

protected:
    void on_invalidated(Rect local_rect, ui::InvalidationKind kind) override;
    void on_descendant_invalidated(const ui::View& source, Rect source_local_rect,
                                   ui::InvalidationKind kind) override;

private:
    friend class Desktop;

    enum class KeyboardMode { None, Move, Resize };
    enum class DragKind { None, Move, Resize };
    // Which corner a resize is anchored opposite to. Every corner resizes;
    // only one of them says so while the window sits idle.
    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
    // close() callbacks may detach and destroy their Window. The close state
    // therefore has independent lifetime; close() retains it on its stack
    // until its final reset is complete.
    struct CloseState {
        bool in_progress = false;
    };

    Rect frame_overlay_rect(ui::View& view, FrameSlot slot) const noexcept;
    void relayout_frame_overlays() noexcept;
    bool point_in_close_control(Point local) const noexcept;
    bool point_in_zoom_control(Point local) const noexcept;
    bool point_in_minimize_control(Point local) const noexcept;
    // Whether the minimize control is on this frame at this width. One gate,
    // four readers — the drawing, the hit-testing, the top border's overlay
    // margins and the title's own budget — because a control that is drawn
    // where nothing reserves room for it collides with the title, and one
    // that is hit-tested where it is not drawn answers a press the reader
    // never aimed at anything.
    bool draws_minimize_control() const noexcept;
    // The corner `local` grabs, if any. All four answer, whatever is drawn:
    // the classic desktop marks one corner and resizes from any of them, and
    // a reader who tries a corner should find it works.
    std::optional<Corner> resize_corner_at(Point local) const noexcept;
    // Which diagonal a corner pulls along. Two corners share each diagonal,
    // which is what makes this worth naming rather than repeating.
    static PointerShape corner_pointer_shape(Corner corner) noexcept;
    // Where a grip is currently drawn. Idle, that is the bottom right alone,
    // which is the mark the convention uses. During a resize it is all four:
    // the moment the reader is resizing is the moment the other three are
    // worth showing, and showing them only then keeps the idle frame quiet.
    bool corner_shows_grip(Corner corner) const noexcept;
    // How many cells of the bottom border each corner grip occupies, and so
    // how large a target the pointer has to hit. One cell is a small thing
    // to aim at; drawing and hit-testing both read this, which is what keeps
    // the affordance and the region that answers it the same size.
    int resize_grip_width() const noexcept;
    Rect clamp_size(Rect bounds) const noexcept;
    Rect zoom_target() const noexcept;
    // Desktop owns this relationship.  It is deliberately not public:
    // allowing callers to install a closure that captures a Desktop
    // would let a detached Window retain a dangling reference.
    void bind_desktop_zoom_target(std::function<Rect()> target, std::weak_ptr<void> desktop_lifetime);
    void clear_desktop_zoom_target() noexcept;

    std::string title_;
    std::string footer_;
    ui::RoleId frame_active_role_ = ui::kInvalidRole;
    ui::RoleId frame_inactive_role_ = ui::kInvalidRole;
    ui::RoleId title_active_role_ = ui::kInvalidRole;
    ui::RoleId title_inactive_role_ = ui::kInvalidRole;
    ui::RoleId control_role_ = ui::kInvalidRole;
    std::optional<Color> chrome_background_override_;

    ui::View* content_ = nullptr;
    std::unordered_map<ui::View*, FrameSlot> frame_overlays_;
    ui::View* add_frame_overlay_impl(std::unique_ptr<ui::View> view, FrameSlot slot);

    bool active_ = false;
    bool movable_ = true;
    bool resizable_ = true;
    bool minimizable_ = true;
    bool minimized_ = false;
    // horizontal_/vertical_ remain the symmetric setter's own record so its
    // getters keep reporting what it was given; the four below are what
    // content_rect() actually measures with.
    int horizontal_content_margin_ = 0;
    int vertical_content_margin_ = 0;
    int left_content_margin_ = 0;
    int top_content_margin_ = 0;
    int right_content_margin_ = 0;
    int bottom_content_margin_ = 0;
    std::shared_ptr<CloseState> close_state_ = std::make_shared<CloseState>();
    Size min_size_{10, 4};
    Size max_size_{0, 0};

    bool zoomed_ = false;
    Rect restored_bounds_;
    DesktopGrowPolicy grow_policy_ = DesktopGrowPolicy::None;
    std::function<Rect()> desktop_zoom_target_;
    std::weak_ptr<void> desktop_lifetime_;

    KeyboardMode keyboard_mode_ = KeyboardMode::None;
    Rect keyboard_mode_start_bounds_;

    DragKind drag_kind_ = DragKind::None;
    bool rasters_suppressed_ = false;
    bool gesture_active_ = false;
    std::function<void(bool)> gesture_observer_;
    std::weak_ptr<void> gesture_observer_lifetime_;
    Corner resize_corner_ = Corner::BottomRight;
    // A frame control the pointer is holding down. Command-like controls act
    // on release inside themselves, never on press: a press is a question
    // ("this one?") and the release is the answer. Closing a window the
    // moment the button goes down gives the reader no way to change their
    // mind, and closing is not an action they can take back.
    enum class Control { None, Close, Minimize, Zoom };
    Control held_control_ = Control::None;
    // Whether the pointer is still on the control it went down on. A press
    // dragged away un-highlights but stays claimed, so returning to the
    // control re-arms it -- the highlight is the window saying what would
    // happen if the button came up now.
    bool held_inside_ = true;
    ui::RoleId control_pressed_role_ = ui::kInvalidRole;

    // The area a drag must leave this window's title bar reachable within,
    // in parent coordinates. Set by whatever owns the window; empty means
    // unconstrained. Window stays Desktop-agnostic, so it is told rather
    // than asking.
    Rect move_bounds_;
    // Ends whatever drag is in progress and repaints if that changed what is
    // drawn. A drag has no guaranteed ending: release the button outside the
    // terminal and the release is delivered to somebody else, leaving this
    // window believing a gesture is still under way.
    void end_drag();
    // A move or resize paints without this window's pictures and brings
    // them back when it ends — see scene::Surface::set_rasters_suppressed
    // for why a gesture is the one time a picture is worth leaving out.
    void suspend_rasters();
    void resume_rasters();
    // The gesture itself, as a bracket: begin on the first drag motion or
    // keyboard mode entry, end when it finishes by any path. Distinct from
    // suspend/resume because a gesture is news the OWNER needs — a window
    // moving across a desktop churns every picture it passes over, not
    // only its own, so Desktop rests them all for the duration.
    void begin_gesture();
    void end_gesture();
    // Desktop owns this relationship, exactly like the zoom target: a
    // closure capturing a Desktop must not outlive it.
    void bind_gesture_observer(std::function<void(bool)> observer, std::weak_ptr<void> lifetime);
    void clear_gesture_observer() noexcept;
    // A rename, reported to the owner. A title is not only this window's own
    // chrome: it is what a switcher bar, a window list or a navigator draws
    // for it, and set_title()'s invalidate repaints this frame while saying
    // nothing at all to a view that merely mentions the window. Desktop owns
    // this relationship on the same weak-lifetime terms as the two above.
    void bind_title_observer(std::function<void()> observer, std::weak_ptr<void> lifetime);
    void clear_title_observer() noexcept;
    std::function<void()> title_observer_;
    std::weak_ptr<void> title_observer_lifetime_;
    // Minimizing or restoring, reported to the owner, on the same
    // weak-lifetime terms as the three above. It is news a Desktop must act
    // on, not only pass along: the window that just went away may have been
    // the active one, and an activation left pointing at something hidden is
    // a desktop with no window the reader can type into.
    void bind_minimize_observer(std::function<void()> observer, std::weak_ptr<void> lifetime);
    void clear_minimize_observer() noexcept;
    std::function<void()> minimize_observer_;
    std::weak_ptr<void> minimize_observer_lifetime_;
    // Clamps `moved` so enough of the title bar stays inside move_bounds_ to
    // grab again. A window dragged entirely under the menu bar or past the
    // footer cannot be brought back with the mouse at all.
    Rect clamp_move(Rect moved) const noexcept;
    Point drag_start_mouse_;
    Rect drag_start_bounds_;

    std::optional<scene::Surface> backing_surface_;
    bool backing_dirty_ = true;
    std::size_t content_repaint_count_ = 0;
    int compositor_layer_id_ = 0;
};

// A built-but-not-yet-attached Window paired with the control that
// should receive focus once it IS attached (M8 WP-5) — the shared
// return type for every standard-dialog factory (make_message_box,
// make_file_dialog, make_directory_picker, make_window_list_dialog,
// make_help_viewer). `initial_focus` is deliberately not focused by
// the factory itself: doing so before the caller attaches `window`
// under the Application's tree would steal focus to a still-detached
// view, silently swallowing input meant for whatever UI is currently
// on screen (see Desktop::present_modeless and present_modal, the
// sanctioned presentation operations).
struct WindowHandle {
    std::unique_ptr<Window> window;
    ui::View* initial_focus = nullptr;
};

// Schedules (via Application::post — never inline, since close() is
// typically still executing several stack frames deep inside a
// Button::on_press or similar that lives INSIDE `window`; destroying
// it before that call frame unwinds would be a use-after-free)
// `window`'s removal from whatever View currently parents it —
// Desktop, a plain View, or Application's own root. The sanctioned way
// for a self-contained dialog (message box, file dialog, window list,
// directory picker, help viewer) to actually detach and destroy itself
// once its own on_closed fires, rather than lingering forever in its
// parent's child list after Close/Cancel. A no-op if `window` has
// already been detached (parent() is null), or was detached and destroyed,
// by the time the posted work runs.
void schedule_self_detach(Window& window, ui::Application& app);

}  // namespace ckv::widgets
