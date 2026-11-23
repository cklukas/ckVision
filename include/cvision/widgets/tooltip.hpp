// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// How serious a notification is. It chooses the mark drawn before the text --
// "i", "!" or "x" -- and that mark's colour, from the "ckv.message.info.text",
// "ckv.message.warning.text" or "ckv.message.error.text" role. It does not
// change how long the notification lives.
enum class NotificationSeverity { Info, Warning, Error };

// One line posted to a NotificationCenter.
struct CVISION_API Notification {
    // Its severity, and the message, drawn on one row and elided with an
    // ellipsis when wider than the view.
    NotificationSeverity severity = NotificationSeverity::Info;
    std::string text;
    // Whether this one waits for the reader. A persistent notification is
    // never taken away by time — it stays until it is dismissed, by a click,
    // by Escape, or by the host. Everything else expires on its own once the
    // host has said how long a toast lives (set_auto_dismiss).
    //
    // The distinction is about whether the reader has to have SEEN it. "Config
    // reloaded" missed is nothing lost; "this session was taken over by
    // ttys011" missed leaves a reader wondering where their work went, so that
    // one waits however long it has to.
    bool persistent = false;
};

// Non-modal feedback: a short stack of lines the application posts and the
// reader does not have to answer. It is not a message box — anything needing
// a decision, or needing to be acknowledged before the application goes on,
// is a modal dialog and belongs in one.
//
// Toasts arrive and leave by themselves once a host calls set_auto_dismiss;
// without it nothing expires, which is what every consumer written before
// that got and still gets. A notification the reader must not miss says so
// with `Notification::persistent` and outlives any timer.
// Explicit notification chrome; Lines remains the compact default.
enum class NotificationPresentation { Lines, Banners, Framed };

// A notification stack with shared expiry and dismissal behavior.
class CVISION_API NotificationCenter : public ui::View {
public:
    // Focusable by default, because a centre a reader Tabs to and dismisses
    // with Escape is what a form wants. Set in the CONSTRUCTOR rather than on
    // attach so a host can say otherwise and be obeyed: an application that
    // lays toasts over its work — where the notification is news rather than
    // something to answer — takes the focus stop away, and re-attaching must
    // not quietly put it back.
    NotificationCenter();
    // Alternate cards use three rows plus one transparent gap between cards.
    void set_presentation(NotificationPresentation presentation);
    NotificationPresentation presentation() const noexcept { return presentation_; }
    // Intrinsic stack height, including card gaps, independent of live bounds.
    ui::SizeHint vertical_size_hint() const override;
    Rect notification_bounds(std::size_t index) const noexcept;
    Rect dismiss_bounds(std::size_t index) const noexcept;
    std::optional<PointerShape> pointer_shape_at(Point local) const override;

    // Posts a notification below the others and returns its index in
    // notifications(). The index is only good until the next removal: every
    // dismissal or expiry of an earlier line shifts the later ones up. Its
    // expiry, if any, is fixed now from the interval in force, or on
    // attachment for one posted to a detached centre. Fires on_change.
    std::size_t add(Notification notification);
    // Removes the notification at `index`, persistent or not, and fires
    // on_change. An index past the end is ignored.
    void dismiss(std::size_t index);
    // The notifications on show, oldest first, clipped to the view. Escape
    // dismisses the newest; a click dismisses a line or a card dismissal control.
    const std::vector<Notification>& notifications() const noexcept { return notifications_; }

    // --- How long a toast lives ---------------------------------------
    //
    // The lifetime of a NON-persistent notification, measured on the injected
    // Clock from the moment it was added. Zero — the default — is no expiry
    // at all: nothing is taken away, which is the behaviour this widget had
    // before it could tell the time, and a host that never calls this sees no
    // change whatsoever. A negative interval is taken as zero.
    //
    // Time is read from the Application this view is attached to, so a
    // detached centre simply holds what it was given until it is attached;
    // there is nowhere to read a clock from and refusing to accept a
    // notification would be worse than showing it a moment late.
    //
    // Changing the interval re-times what is already on screen, deliberately:
    // a host that shortens its toasts means the ones in front of the reader
    // too, and leaving them on the old interval would make the setting take
    // effect at a moment nobody chose.
    void set_auto_dismiss(std::int64_t nanos);
    std::int64_t auto_dismiss_nanos() const noexcept { return auto_dismiss_nanos_; }

    // Fires whenever the set of notifications changes for any reason — one
    // posted, one dismissed by the reader, or one that expired on its own.
    // A host that sizes or places this view from its vertical size hint
    // needs it, because expiry happens on a timer that the host never sees:
    // without it, a centre that emptied itself would leave the host holding
    // a rectangle for rows that are no longer there.
    std::function<void()> on_change;

    void draw(scene::Painter& painter) override;
    // The line Escape would take away is marked only while focused.
    void on_focus(const FocusEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    // Applies every expiry that is due, and arms one wake-up for the earliest
    // that is not. One timer for the whole stack rather than one per line:
    // what an expiry costs is a repaint of the view, so waking once and
    // sweeping is both cheaper and the only order in which two toasts that
    // come due together go together.
    void expire_due();
    void arm_expiry(std::int64_t deadline_nanos);
    void changed();

    NotificationPresentation presentation_ = NotificationPresentation::Lines;
    std::vector<Notification> notifications_;
    // When each notification is due to leave, parallel to `notifications_`
    // and maintained with it. kNever for a persistent one, and for every one
    // added while no expiry interval was set. Kept beside the notification
    // rather than inside it so that `Notification` stays what it is — a
    // record a host writes — rather than gaining a field only this view may
    // fill in.
    static constexpr std::int64_t kNever = 0;
    std::vector<std::int64_t> deadlines_;
    std::int64_t auto_dismiss_nanos_ = 0;
    ui::Application::TimerId expiry_timer_ = 0;
    std::int64_t expiry_wake_nanos_ = 0;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId info_role_ = ui::kInvalidRole;
    ui::RoleId warning_role_ = ui::kInvalidRole;
    ui::RoleId error_role_ = ui::kInvalidRole;
};

// A one-row label with one cell of padding either side, in the "ckv.tooltip"
// role (resolved on attach): the popup a TooltipController shows, and a
// plain floating note a host may position itself. It takes no focus. While
// it is not held it takes no input either; held, it answers keys and
// pointer presses the way an open menu does (see set_held).
class CVISION_API Tooltip : public ui::View {
public:
    // Hidden until shown; prefers the text's width plus two by one row.
    explicit Tooltip(std::string text = {});
    // Replaces the text and the preferred size. A tooltip already shown keeps
    // its bounds until it is shown again, eliding the text to fit them.
    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }
    // Shows the tooltip with its top-left cell at `position`, in its parent's
    // coordinates, sized to the text plus padding (at least two cells). It is
    // not moved to fit inside the parent.
    void show_at(Point position);
    // Shows the tooltip beside `anchor`, kept inside `area`; both rectangles
    // are in its parent's coordinates. The placement is a fixed rule, so the
    // same geometry always puts it in the same place: on the row below the
    // anchor when that row is inside the area, else on the row above it when
    // that one is, else over the anchor's bottom row, kept inside the area.
    // Its left edge starts under the anchor's and moves left as far as it
    // must to end inside the area, and never past the area's left edge. It
    // is as wide as its text and padding, and never wider than the area,
    // eliding the text to fit.
    void show_near(Rect anchor, Rect area);
    // Hides it; the text and bounds are kept.
    void hide();
    // Whether it is visible.
    bool shown() const noexcept { return visible(); }
    // Whether it answers input the way an open menu does. Held, every key
    // press, text input and pointer press -- inside it or outside -- fire
    // on_dismiss and are consumed, except a press outside it, which fires
    // on_dismiss and is left unhandled, as a menu leaves one. A host that
    // holds a tooltip first routes input to it, with a modal scope for the
    // keys and input capture for the pointer; TooltipController does this
    // for the tooltip key.
    void set_held(bool held) noexcept { held_ = held; }
    bool held() const noexcept { return held_; }
    // Fires on a dismissing key or press while held. The host hides or
    // removes the tooltip from here; the tooltip does nothing else itself.
    std::function<void()> on_dismiss;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    int shown_width() const noexcept;
    void dismiss();
    std::string text_;
    bool held_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
};

// Shows explanatory tooltips for an application's views. A host tells it
// the tip of each view it wants explained (set_tip); the controller then
// shows the tip of the view the reader turns to, as a Tooltip popup on the
// desktop, placed with Tooltip::show_near.
//
// It shows a tip in three ways. When the pointer comes to rest over a view,
// or the keyboard focus arrives on one, and that view or the nearest of its
// ancestors has a tip, the tip shows once the delay (set_delay) has passed
// on the Application's injected clock, unless the reader has moved on
// meanwhile: under the pointer's cell for the pointer, under the view for
// the focus. Such a tip is passive -- it takes no input, and goes when the
// pointer leaves the view it explains (for the tooltip itself, which the
// reader may move onto to read), when the focus leaves the view it explains,
// or at the next key press, pointer press, wheel turn or text input.
//
// And the tooltip command (CommandRegistry::standard().tooltip, Ctrl+F1 by
// default) shows the focused view's tip at once, under the view. That tip
// is held: it keeps a modal scope and the pointer the way an open menu does,
// so Escape -- or any other key -- or a press anywhere dismisses it, and
// nothing else reaches the application until it has gone. The command is
// available inside a modal dialog as well.
//
// The controller learns of the focus, the pointer and the input through
// Application::add_attention_observer, and keeps only lifetime-checked
// references to the views it explains. It installs the tooltip command's
// handler when nothing has claimed it yet, and withdraws it again on
// destruction if it was the one that installed it.
class CVISION_API TooltipController {
public:
    // Watches `app` and shows its tips over `desktop`. Both must outlive the
    // controller.
    TooltipController(ui::Application& app, Desktop& desktop);
    // Takes any tip down, cancels a pending one and stops watching.
    ~TooltipController();
    // Not copyable: the observers and the command handler it installs name
    // this controller.
    TooltipController(const TooltipController&) = delete;
    TooltipController& operator=(const TooltipController&) = delete;

    // The tip for `view`, and for everything inside it that has none of its
    // own; empty text removes the view's tip. The controller does not keep
    // the view alive and forgets tips whose views are gone.
    void set_tip(const ui::View& view, std::string text);
    // The tip that `view` shows: its own, or the nearest ancestor's; empty
    // when neither it nor any ancestor has one.
    std::string tip_for(const ui::View& view) const;
    // How long the pointer rests on a view, or the focus stays on one,
    // before its tip shows, in nanoseconds of the injected clock. 500 ms by
    // default; a value below one nanosecond is taken as one.
    void set_delay(std::int64_t nanos);
    std::int64_t delay_nanos() const noexcept { return delay_nanos_; }

    // Shows the focused view's tip at once, held, as the tooltip command
    // does. Returns false, showing nothing, when nothing is focused or the
    // focused view has no tip.
    bool show_for_focus();
    // Takes down the tip on show, if any, and forgets a pending one.
    void dismiss();
    // The tooltip on show, or nullptr when none is. Owned by the desktop;
    // valid until the tip goes.
    const Tooltip* tooltip() const noexcept;

private:
    // What asked for the tip on show or pending: the pointer resting, the
    // focus arriving, or the tooltip command.
    enum class Source { Pointer, Focus, Command };
    struct Tip {
        const ui::View* view = nullptr;
        std::weak_ptr<void> liveness;
        std::string text;
    };
    const Tip* tip_entry_for(const ui::View& view) const;
    void observe(ui::Application::AttentionChange change);
    void arm(Source source, const ui::View& target);
    void cancel_pending();
    void show(Source source, const Tip& tip, Rect anchor);
    void take_down();

    ui::Application& app_;
    Desktop& desktop_;
    std::weak_ptr<void> desktop_liveness_;
    std::vector<Tip> tips_;
    std::int64_t delay_nanos_ = 500'000'000;
    ui::Application::AttentionObserverId observer_ = 0;
    ui::Application::TimerId pending_timer_ = 0;
    Source pending_source_ = Source::Pointer;
    const ui::View* pending_target_ = nullptr;
    std::weak_ptr<void> pending_liveness_;
    Tooltip* tooltip_ = nullptr;
    std::weak_ptr<void> tooltip_liveness_;
    Source shown_source_ = Source::Pointer;
    const ui::View* shown_owner_ = nullptr;
    ui::Application::ModalScopeId held_scope_ = 0;
    bool installed_command_handler_ = false;
};

}  // namespace ckv::widgets
