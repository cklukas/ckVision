// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

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
// WP-7, D-028): "ckv.button.normal/focused/default/shadow" and
// "ckv.label.mnemonic". Already
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

    explicit Button(std::string text);

    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    void set_default(bool is_default) noexcept { is_default_ = is_default; }
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

    void set_minimum_width(int width);
    int minimum_width() const noexcept { return minimum_width_; }

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId default_role,
                            ui::RoleId shadow_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        default_role_ = default_role;
        shadow_role_ = shadow_role;
    }
    void set_pressed_role_override(ui::RoleId role) noexcept { pressed_role_ = role; }
    void set_mnemonic_role_override(ui::RoleId role) noexcept { mnemonic_role_ = role; }

    // Invoked by a containing dialog/window for a matching Alt+mnemonic.
    // The button remains independently activatable by Enter/Space when it
    // owns focus; this route supplies the conventional direct accelerator.
    bool activate_mnemonic(std::string_view mnemonic);

    std::function<void()> on_press;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // The face sits on row 0 and the cast shadow on row 1 -- unless there is
    // no shadow to stand off from.
    bool trailing_row_is_shadow() const noexcept override { return !flat_; }
    bool on_key(const KeyEvent& event) override;
    // The release that commits (or finds already taken back) a key-held
    // press. Routed separately from on_key by design: a release must never
    // reach a handler that would read it as a second activation.
    bool on_key_release(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // A button is the plain case of something that acts when clicked;
    // a disabled one is still there, still hit-tested, and still refusing.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }
    void on_focus(const FocusEvent& event) override;
    // A button is one of the few views that really does look different
    // under the pointer, so it is one of the few that pays for a repaint.
    void on_hover_changed(bool) override { invalidate(); }

    // Whether the button is currently drawn depressed — exposed so a press
    // lifecycle can be asserted without scraping rendered cells.
    bool pressed() const noexcept { return pressed_; }

    void on_attached() override;

private:
    // Which of the five faces this button is currently wearing. Ordered by
    // how much each state tells the reader: a press is happening now, focus
    // says where the keyboard is, and hover only says where the pointer
    // happens to be resting -- so hover is the first to be overruled.
    ui::RoleId face_role() const noexcept {
        if (pressed_) return pressed_role_;
        if (has_focus_) return focused_role_;
        if (hovered()) return hovered_role_;
        return is_default_ ? default_role_ : normal_role_;
    }

    void fire_press();

    std::string raw_text_;
    std::string display_text_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId hovered_role_ = ui::kInvalidRole;
    ui::RoleId default_role_ = ui::kInvalidRole;
    ui::RoleId shadow_role_ = ui::kInvalidRole;
    ui::RoleId pressed_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    bool flat_ = false;
    bool is_default_ = false;
    int minimum_width_ = kClassicMinimumWidth;
    bool has_focus_ = false;
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
};

}  // namespace ckv::widgets
