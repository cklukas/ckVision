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
// never overwrites an explicit override.
class Label : public View {
public:
    explicit Label(std::string text);

    const std::string& text() const noexcept { return raw_text_; }
    void set_text(std::string text);

    void set_buddy(View* buddy) noexcept;
    View* buddy() const noexcept;

    const std::string& mnemonic() const noexcept { return mnemonic_; }

    void set_role_override(ui::RoleId text_role, ui::RoleId mnemonic_role) noexcept {
        text_role_ = text_role;
        mnemonic_role_ = mnemonic_role;
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
    View* buddy_ = nullptr;
    std::weak_ptr<void> buddy_liveness_;
};

// Widgets-layer mnemonic routing for containers that own Label subtrees.
// Handles Alt+Char key presses, finds the first visible/enabled matching
// Label inside `scope`, and focuses its still-live, focusable buddy.
bool activate_label_mnemonic(View& scope, const KeyEvent& event, ui::Application& app);

// Extends label-to-buddy mnemonics with direct Button accelerators. Windows
// use this one route so a dialog's Alt+mnemonic contract is consistent for
// fields and buttons alike.
bool activate_control_mnemonic(View& scope, const KeyEvent& event, ui::Application& app);

}  // namespace ckv::widgets
