// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// A non-focusable text label. `text` may carry a '&'-marked mnemonic
// (see widgets/mnemonic.hpp); `buddy` is the control Alt+mnemonic will
// jump focus to when a widgets-layer container, such as Window, routes
// activate_label_mnemonic() over its subtree.
//
// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.label.text" / "ckv.label.mnemonic". A caller
// wanting different roles (e.g. a Label styled as static text, not a
// form-field label) calls set_role_override BEFORE or after
// attachment — either way takes effect immediately, and on_attached()
// never overwrites an explicit override. A disabled label (D-076) draws its
// text in "ckv.label.disabled"'s foreground on its own text role's
// background, so an overridden label keeps the surface it was styled for,
// and shows no mnemonic accent.
class Label : public View {
public:
    // A one-row label showing `text`, with no buddy.
    explicit Label(std::string text);

    // The text exactly as given, '&' markers included. It is drawn on one row
    // with the markers stripped, clipped to the label's width. Setting it
    // re-parses the mnemonic, repaints and reports a size-hint change.
    const std::string& text() const noexcept { return raw_text_; }
    void set_text(std::string text);

    // The control the mnemonic focuses. Not owned: the label watches its
    // lifetime, so buddy() returns nullptr once the buddy has been destroyed,
    // and nullptr clears it. The mnemonic only reaches a buddy that is inside
    // the scope being searched and focusable at the time.
    void set_buddy(View* buddy) noexcept;
    View* buddy() const noexcept;

    // The grapheme the text marks with '&', or empty when it marks none.
    const std::string& mnemonic() const noexcept { return mnemonic_; }

    // The width of the column this label stands in, beside others: the
    // label is at least that wide, its text at the start and the rest blank,
    // so the controls beside a column of labels line up. Zero, the default,
    // is exactly the text's own width.
    void set_column_width(int cells);
    int column_width() const noexcept { return column_width_; }

    // The text role and the mnemonic role, replacing "ckv.label.text" and
    // "ckv.label.mnemonic" (see the class comment). The mnemonic role is
    // applied whole to the marked grapheme, background included. The
    // disabled role is not overridable.
    void set_role_override(ui::RoleId text_role, ui::RoleId mnemonic_role) noexcept {
        if (text_role_ == text_role && mnemonic_role_ == mnemonic_role) return;
        text_role_ = text_role;
        mnemonic_role_ = mnemonic_role;
        invalidate();
    }

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    void on_attached() override;

private:
    std::string raw_text_;
    std::string display_text_;
    std::string mnemonic_;
    std::size_t mnemonic_byte_offset_ = std::string::npos;
    ui::RoleId text_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    View* buddy_ = nullptr;
    std::weak_ptr<void> buddy_liveness_;
    int column_width_ = 0;
};

// Widgets-layer mnemonic routing for containers that own Label subtrees.
// Handles Alt+Char key presses, finds the first visible/enabled matching
// Label inside `scope`, and focuses its still-live, focusable buddy. An
// option group's caption is its own label: a CheckGroup or RadioGroup whose
// caption marks the letter is focused the same way (D-068).
bool activate_label_mnemonic(View& scope, const KeyEvent& event, ui::Application& app);

// Extends label-to-buddy mnemonics with direct Button accelerators. Windows
// use this one route so a dialog's Alt+mnemonic contract is consistent for
// fields and buttons alike.
bool activate_control_mnemonic(View& scope, const KeyEvent& event, ui::Application& app);

}  // namespace ckv::widgets
