// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Window: frame chrome, title, close/zoom controls, move/resize (mouse
// drag or keyboard mode), min/max size limits, shadow declaration, and
// a single content view (ARCHITECTURE.md §5 "Windows, popups,
// modality"). Desktop-agnostic and independently testable — z-order,
// activation, and tile/cascade are Desktop's job once it exists;
// Window exposes what Desktop needs (casts_shadow(), the close
// protocol, active()/set_active()) without depending on it.
//
// Scope note: backing store ("drag never repaints content") is listed
// as beyond-baseline in plans/widgets.md — baseline Window draws
// through the ordinary View/Painter tree like everything else, so
// moving/resizing a window repaints it like any other bounds change.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <unordered_map>
#include <vector>

#include "cvision/ui/anchor_pane.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Which border row a frame overlay lives on (M10/WP-20). Deliberately
// just these two, not Dock's full Top/Bottom/Left/Right/Center set: a
// window's side borders are one cell wide, with no room to host
// meaningful content — every real frame-overlay use (a path indicator,
// a "line:col" readout) is a short text run along the top or bottom.
enum class Edge { Top, Bottom };

// `alignment` is `ui::Alignment` (Start/Center/End/Fill) — the SAME
// enum Row/Column/Grid already use, positioning the overlay within its
// edge's own row exactly the way align_cross_axis positions a child
// within its container. `offset` nudges the result right (positive) or
// left (negative) from where `alignment` alone would place it, still
// clamped to the row's own available span.
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

// How a window's bounds respond to its owning Desktop growing (M8
// WP-4) — orthogonal to reposition_within()'s shrink-side clamping,
// which every window gets regardless of policy.
enum class DesktopGrowPolicy {
    None,         // never resized by desktop growth (only ever clamped on shrink) — the default
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

    void set_title(std::string title);
    const std::string& title() const noexcept { return title_; }

    // Desktop sets this on activation/deactivation (the topmost window
    // is active; exactly one window is active at a time, or none).
    void set_active(bool active);
    bool active() const noexcept { return active_; }

    // The sole content child, positioned to fill the interior (inside
    // the 1-cell frame border). Replaces and returns ownership of any
    // previous content (nullptr if there was none).
    std::unique_ptr<ui::View> set_content(std::unique_ptr<ui::View> content);
    ui::View* content() const noexcept { return content_; }

    // The interior rect set_content() will resize whatever's passed to
    // it into (inside the 1-cell frame border). Public so a caller can
    // size a content view correctly BEFORE constructing it, when the
    // view's own constructor needs a real Rect up front — e.g.
    // ui::Grid's or widgets::Splitter's, both of which compute their
    // initial layout (a Splitter's own starting 50/50 split) from
    // whatever bounds they're given at construction, not a later
    // resize.
    Rect content_rect() const noexcept;

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
    // behaves exactly as before this existed. Returns the observer
    // pointer (matches add_child's own contract).
    ui::View* add_frame_overlay(std::unique_ptr<ui::View> view, FrameSlot slot);
    // Detaches a specific overlay, returning ownership (nullptr if
    // `view` is not a currently-attached overlay of this window).
    std::unique_ptr<ui::View> remove_frame_overlay(ui::View* view);

    void set_min_size(Size min) noexcept;
    void set_max_size(Size max) noexcept;  // {0,0} means unbounded (the default)

    void set_movable(bool movable) noexcept { movable_ = movable; }
    bool movable() const noexcept { return movable_; }
    void set_resizable(bool resizable) noexcept { resizable_ = resizable; }
    bool resizable() const noexcept { return resizable_; }

    // Vetoable close protocol (ARCHITECTURE.md §5 "Closing is a
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

    // How this window's bounds respond to Desktop growth (default
    // None — see DesktopGrowPolicy). Desktop::on_resized() reads this
    // on every owned window; irrelevant for a standalone Window.
    void set_grow_policy(DesktopGrowPolicy policy) noexcept { grow_policy_ = policy; }
    DesktopGrowPolicy grow_policy() const noexcept { return grow_policy_; }

    // Clamps this window's bounds to fit and remain reachable within
    // `available` (ARCHITECTURE.md §5 "Sizing policy": "On shrink,
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

protected:
    void on_invalidated(Rect local_rect, ui::InvalidationKind kind) override;
    void on_descendant_invalidated(const ui::View& source, Rect source_local_rect,
                                   ui::InvalidationKind kind) override;

private:
    friend class Desktop;

    enum class KeyboardMode { None, Move, Resize };
    enum class DragKind { None, Move, Resize };
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
    bool point_in_resize_grip(Point local) const noexcept;
    Rect clamp_size(Rect bounds) const noexcept;
    Rect zoom_target() const noexcept;
    // Desktop owns this relationship.  It is deliberately not public:
    // allowing callers to install a closure that captures a Desktop
    // would let a detached Window retain a dangling reference.
    void bind_desktop_zoom_target(std::function<Rect()> target, std::weak_ptr<void> desktop_lifetime);
    void clear_desktop_zoom_target() noexcept;

    std::string title_;
    ui::RoleId frame_active_role_ = ui::kInvalidRole;
    ui::RoleId frame_inactive_role_ = ui::kInvalidRole;
    ui::RoleId title_active_role_ = ui::kInvalidRole;
    ui::RoleId title_inactive_role_ = ui::kInvalidRole;

    ui::View* content_ = nullptr;
    std::unordered_map<ui::View*, FrameSlot> frame_overlays_;

    bool active_ = false;
    bool movable_ = true;
    bool resizable_ = true;
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
