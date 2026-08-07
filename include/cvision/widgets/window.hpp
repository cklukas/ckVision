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
#include <string>

#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class Window : public ui::View {
public:
    Window(std::string title, const ui::Theme& theme, ui::RoleId frame_active_role,
           ui::RoleId frame_inactive_role, ui::RoleId title_active_role, ui::RoleId title_inactive_role);

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
    // View tree.
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

    // Keyboard move/resize mode: while active, arrow keys nudge one
    // cell per press; Enter confirms (keeps the new bounds); Esc
    // reverts to the bounds captured when the mode was entered.
    void enter_move_mode();
    void enter_resize_mode();
    bool in_keyboard_mode() const noexcept { return keyboard_mode_ != KeyboardMode::None; }

    bool casts_shadow() const noexcept { return true; }

    void draw(scene::Painter& painter) override;
    void on_resized() override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    enum class KeyboardMode { None, Move, Resize };
    enum class DragKind { None, Move, Resize };

    Rect content_rect() const noexcept;
    bool point_in_close_control(Point local) const noexcept;
    bool point_in_zoom_control(Point local) const noexcept;
    bool point_in_resize_grip(Point local) const noexcept;
    Rect clamp_size(Rect bounds) const noexcept;

    std::string title_;
    const ui::Theme& theme_;
    ui::RoleId frame_active_role_;
    ui::RoleId frame_inactive_role_;
    ui::RoleId title_active_role_;
    ui::RoleId title_inactive_role_;

    ui::View* content_ = nullptr;

    bool active_ = false;
    bool movable_ = true;
    bool resizable_ = true;
    Size min_size_{10, 4};
    Size max_size_{0, 0};

    bool zoomed_ = false;
    Rect restored_bounds_;

    KeyboardMode keyboard_mode_ = KeyboardMode::None;
    Rect keyboard_mode_start_bounds_;

    DragKind drag_kind_ = DragKind::None;
    Point drag_start_mouse_;
    Rect drag_start_bounds_;
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
// already been detached (parent() is null) by the time the posted work
// runs.
void schedule_self_detach(Window& window, ui::Application& app);

}  // namespace ckv::widgets
