// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// A non-focusable text label. `text` may carry a '&'-marked mnemonic
// (see widgets/mnemonic.hpp); `buddy` is the control the mnemonic will
// jump focus to once accelerator-table routing lands with M5 — stored
// now so a caller wiring a form doesn't have to revisit every Label
// construction later, but not yet activated by any key path.
class Label : public View {
public:
    Label(std::string text, const ui::Theme& theme, ui::RoleId text_role, ui::RoleId mnemonic_role);

    const std::string& text() const noexcept { return raw_text_; }
    void set_text(std::string text);

    void set_buddy(View* buddy) noexcept { buddy_ = buddy; }
    View* buddy() const noexcept { return buddy_; }

    const std::string& mnemonic() const noexcept { return mnemonic_; }

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;

private:
    std::string raw_text_;
    std::string display_text_;
    std::string mnemonic_;
    std::size_t mnemonic_byte_offset_ = std::string::npos;
    const ui::Theme& theme_;
    ui::RoleId text_role_;
    ui::RoleId mnemonic_role_;
    View* buddy_ = nullptr;
};

}  // namespace ckv::widgets
