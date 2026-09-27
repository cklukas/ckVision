// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// Baseline per the widget catalog: default/normal styling, mnemonic
// (visual only — see widgets/mnemonic.hpp), pressed feedback. Activation
// follows the mouse model on both input paths: a full Down-then-Up cycle
// commits, and what happens in between can take the press back. With the
// mouse that is a drag off the button; with the keyboard — on a session
// whose verified kitty enhancements report every key's release (D-055) —
// Enter/Space hold the button visibly down until the key comes back up,
// and Tab away, Escape, or the terminal losing focus cancels the press
// without firing. On a session without that promise the keystroke fires
// immediately with a brief depressed flash, and auto-repeat re-fires,
// exactly as a legacy terminal reports it.
//
// Repeat on hold (opt-in, set_hold_repeat): for a control that steps
// something -- a stepper, a scroll arrow -- where holding it down is how a
// reader asks for more. The pointer then acts on the press rather than on the
// release: the primary button going down fires on_press once, and while it
// stays down on the button the press fires again after the initial delay and
// then once per interval, timed on the Application's injected clock through
// its timers. Repeating stops when the button comes up (which fires nothing
// more), when the pointer leaves the button, and when the button is disabled;
// a pointer that comes back onto the button while it is still held starts
// the repeat again from the initial delay. The keyboard is unaffected: Enter
// and Space press a repeating button exactly as they press any other.
//
// Rendering follows ckVision's documented classic-desktop button contract:
// a solid face with the label centered, a drop shadow composited from
// half-block glyphs — "▄" at the face's right edge on its first row,
// "█" below it, and a "▀" run along the bottom row — and a depressed
// state that shifts the face one cell right while the shadow
// disappears, which is what makes a click visibly "push" the button
// into the surface. `shadow_role`'s background must match the surface
// the button sits on (a dialog's background, typically): the shadow
// glyphs' foreground paints the dark halves, and their background
// fills the rest of those cells.
//
// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.button.normal/focused/default/shadow/disabled" and
// "ckv.label.mnemonic". A disabled button (D-076) wears the disabled face,
// with no press, focus, hover, or mnemonic accent, and ignores its
// mnemonic. Already
// defaults to FocusPolicy::TabStop — a caller never needs to set that
// itself. set_role_override lets a caller redirect all four roles at
// once (e.g. a distinct "danger" button family); takes effect
// immediately whether called before or after attachment.
class Button : public View {
public:
    // The classic desktop metric reserves a ten-cell button footprint even
    // for a short caption such as "OK". Longer captions grow naturally;
    // callers can request a wider uniform family where a dialog needs it.
    static constexpr int kClassicMinimumWidth = 10;

    // A shadowed button captioned `text`, which may mark a mnemonic with '&'
    // (see widgets/mnemonic.hpp). It is a Tab stop from construction.
    explicit Button(std::string text);

    // The caption exactly as given, '&' markers included. The face shows it
    // with the markers stripped and the marked grapheme accented, centred and
    // clipped to the face. Setting it recomputes the preferred size (the
    // label's width plus four cells, at least minimum_width(), two rows),
    // repaints, and tells the parent the size hint changed.
    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    // Marks the button as its dialog's default, which only selects the
    // ckv.button.default face for a button that is neither focused, hovered
    // nor pressed. It is appearance only: no key is routed to the button
    // because of it. A change repaints; the button's size is not affected.
    void set_default(bool is_default) noexcept {
        if (is_default_ == is_default) return;
        is_default_ = is_default;
        invalidate();
    }
    bool is_default() const noexcept { return is_default_; }

    // A button drawn as a bare face: no cast shadow, no depressed shift, one
    // row high, and no wider than its label needs. For a control that lives
    // inside a dense row -- a stepper beside a field, a strip of small
    // actions -- where a dialog button's shadow and ten-cell footprint do not
    // fit. It is still a Button: it takes focus, it arms on press and takes
    // the press back if the pointer leaves, and it fires on release. Only the
    // shape differs, so pressing shows in the colours (ckv.button.pressed)
    // where a shadowed button shows it in the geometry.
    void set_flat(bool flat);
    bool flat() const noexcept { return flat_; }

    // The narrowest a shadowed button may be, in cells, shadow columns
    // included; kClassicMinimumWidth by default. Values below 3 are raised to
    // 3. A wider caption still widens the button, and a flat button ignores
    // this width entirely. Setting it repaints and reports a size-hint change.
    void set_minimum_width(int width);
    int minimum_width() const noexcept { return minimum_width_; }

    // Replace the theme roles the button would otherwise resolve on
    // attachment: the normal, focused and default faces and the shadow (whose
    // background must match the surface beneath the button). The hovered face
    // is not overridable. An override set before attachment survives it;
    // passing ui::kInvalidRole before attachment leaves that role to the
    // standard lookup. Each of these setters repaints when it changes a
    // role.
    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId default_role,
                            ui::RoleId shadow_role) noexcept {
        if (normal_role_ == normal_role && focused_role_ == focused_role && default_role_ == default_role &&
            shadow_role_ == shadow_role)
            return;
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        default_role_ = default_role;
        shadow_role_ = shadow_role;
        invalidate();
    }
    // The same kind of override for the flat button's pressed face
    // (ckv.button.pressed), the accented mnemonic grapheme
    // (ckv.label.mnemonic, of which only the foreground and attributes are
    // used) and the disabled face (ckv.button.disabled).
    void set_pressed_role_override(ui::RoleId role) noexcept {
        if (pressed_role_ == role) return;
        pressed_role_ = role;
        invalidate();
    }
    void set_mnemonic_role_override(ui::RoleId role) noexcept {
        if (mnemonic_role_ == role) return;
        mnemonic_role_ = role;
        invalidate();
    }
    void set_disabled_role_override(ui::RoleId role) noexcept {
        if (disabled_role_ == role) return;
        disabled_role_ = role;
        invalidate();
    }

    // The timing of a repeating press, on the Application's clock: the first
    // repeat comes `initial_delay_nanos` after the press, and each later one
    // `interval_nanos` after the one before. Both must be positive.
    struct HoldRepeat {
        // From the press to the first repeat: 400 ms by default.
        std::int64_t initial_delay_nanos = 400'000'000;
        // Between one repeat and the next: 100 ms by default.
        std::int64_t interval_nanos = 100'000'000;
    };
    // Makes a held pointer press repeat with `repeat`'s timing (see the class
    // comment), or, with std::nullopt -- the default -- makes the button act
    // on release again. Turning it off stops a repeat in flight.
    void set_hold_repeat(std::optional<HoldRepeat> repeat);
    const std::optional<HoldRepeat>& hold_repeat() const noexcept { return hold_repeat_; }

    // Invoked by a containing dialog/window for a matching Alt+mnemonic.
    // The button remains independently activatable by Enter/Space when it
    // owns focus; this route supplies the conventional direct accelerator.
    // Fires on_press at once, without a depressed state, and returns true when
    // the button is enabled in its tree and `mnemonic` equals its marked
    // grapheme (ASCII letters compared case-insensitively); otherwise returns
    // false and does nothing. activate_control_mnemonic (widgets/label.hpp) is
    // the caller that routes Alt+letter here.
    bool activate_mnemonic(std::string_view mnemonic);

    // The action. Fires once per committed press: a pointer press released
    // over the button, a key-held press released without being taken back, a
    // keystroke on a session that reports no releases, or activate_mnemonic.
    // Never fires for a press that was taken back. A repeating button fires
    // on the pointer press instead, and again on each repeat while it is held
    // (set_hold_repeat). An empty function is allowed and makes the button
    // inert.
    std::function<void()> on_press;

    void draw(scene::Painter& painter) override;
    // Shadowed: exactly max(minimum_width(), label width + 4) cells wide, one
    // to two rows high (two preferred). Flat: at least the label's width
    // (never less than one cell) and free to grow, exactly one row high.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // The face sits on row 0 and the cast shadow on row 1 -- unless there is
    // no shadow to stand off from.
    bool trailing_row_is_shadow() const noexcept override { return !flat_; }
    // Consumes Enter and Space (modifiers are not consulted) as a press, and
    // Escape only while it takes back a key-held press. Every other key is
    // left unhandled. A repeat of a held key on a release-reporting session
    // is consumed without firing again.
    bool on_key(const KeyEvent& event) override;
    // The release that commits (or finds already taken back) a key-held
    // press. Routed separately from on_key by design: a release must never
    // reach a handler that would read it as a second activation.
    bool on_key_release(const KeyEvent& event) override;
    // A primary-button Down arms and depresses the button; a Down of another
    // button is left unhandled. A Move while armed lifts or re-depresses it as
    // the pointer leaves or returns. An Up ends the press, firing on_press
    // only when it lands inside the button and is the primary button's own
    // release (or names no button); another button's release takes the press
    // back. A repeating button fires on the Down and repeats while held
    // instead, and its Up fires nothing. Primary Down, Move and Up are
    // consumed; wheel events are not. The second press of a double click is a
    // press like any other.
    bool on_mouse(const MouseEvent& event) override;
    // A button is the plain case of something that acts when clicked;
    // a disabled one is still there, still hit-tested, and still refusing.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }
    // Losing the focus takes back a key-held press without firing.
    void on_focus(const FocusEvent& event) override;
    // Leaving the tree stops a repeat in flight.
    void on_detaching() override;
    // A button is one of the few views that really does look different
    // under the pointer, so it is one of the few that pays for a repaint.
    void on_hover_changed(bool) override { invalidate(); }

    // Whether the button is currently drawn depressed — exposed so a press
    // lifecycle can be asserted without scraping rendered cells.
    bool pressed() const noexcept { return pressed_; }

    void on_attached() override;

private:
    // Which of the six faces this button is currently wearing. Disabled
    // overrules everything, because none of the others can happen to it.
    // The rest are ordered by how much each state tells the reader: a press
    // is happening now, focus says where the keyboard is, and hover only
    // says where the pointer happens to be resting -- so hover is the first
    // to be overruled.
    ui::RoleId face_role() const noexcept {
        if (!enabled_in_tree()) return disabled_role_;
        if (pressed_) return pressed_role_;
        if (has_focus()) return focused_role_;
        if (hovered()) return hovered_role_;
        return is_default_ ? default_role_ : normal_role_;
    }

    void fire_press();
    // Starts the repeat timer, replacing any pending one: a one-shot for the
    // initial delay, or the repeating one for the interval. A no-op without
    // an Application.
    void start_repeat_timer(std::int64_t nanos, bool repeating);
    // Cancels the repeat timer, if one is running.
    void stop_repeat();
    // The repeat timer fired: fires on_press while the press still holds --
    // following the initial delay with the interval timer -- and otherwise
    // stops, taking back a press whose button was disabled under it.
    void repeat_due(bool after_initial_delay);

    std::string raw_text_;
    std::string display_text_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId hovered_role_ = ui::kInvalidRole;
    ui::RoleId default_role_ = ui::kInvalidRole;
    ui::RoleId shadow_role_ = ui::kInvalidRole;
    ui::RoleId pressed_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    bool flat_ = false;
    bool is_default_ = false;
    int minimum_width_ = kClassicMinimumWidth;
    // Whether the button is drawn depressed right now. It follows the
    // pointer or the key, and is not by itself a promise that releasing
    // will act — `armed_` is.
    bool pressed_ = false;
    // A press is in flight and still eligible to fire: the pointer went
    // down on this button and has not been released elsewhere, or a key
    // went down while this button held focus and focus has not moved. A
    // press that leaves the button, or a focus change, disarms it — the
    // action then never runs, which is how a press is taken back.
    bool armed_ = false;
    // A keyboard press is in flight (as opposed to a pointer press), so
    // the release that ends it is a key release rather than a mouse-up.
    bool key_armed_ = false;
    std::optional<HoldRepeat> hold_repeat_;
    // The pointer press in flight fired on the way down and repeats while
    // held, so its release must not fire again -- decided when it began, so
    // turning repeating off mid-press cannot make it act twice.
    bool repeating_press_ = false;
    // The pending repeat's timer on repeat_app_, or 0 when none is pending.
    // The Application is remembered with the id because a timer can only be
    // cancelled where it was started.
    ui::Application::TimerId repeat_timer_ = 0;
    ui::Application* repeat_app_ = nullptr;
};

}  // namespace ckv::widgets
