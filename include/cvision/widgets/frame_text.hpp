// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "cvision/ui/layout.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Text set into a window's border — a line and column, a page count, the path
// being browsed — in the border's own style, which follows the window becoming
// active and inactive and whatever role override the window carries. Add it
// with Window::add_frame_overlay; hosted anywhere else it draws nothing.
//
// Only the text and one space either side are painted. The rest of the width
// the overlay reserves stays border line, so the text reads as a label on the
// frame rather than as a gap cut out of it.
class FrameText final : public ui::View {
public:
    explicit FrameText(std::string text = {});

    const std::string& text() const noexcept { return text_; }
    void set_text(std::string text);

    // The width the overlay keeps whatever its text says, so a readout whose
    // text changes width does not move along the border. Zero, the default,
    // is the text's own width and its two spaces.
    void set_reserved_width(int cells);
    int reserved_width() const noexcept { return reserved_width_; }

    // Where the text sits within a reserved width wider than it: Start,
    // Center or End (Fill is Start).
    void set_alignment(ui::Alignment alignment);
    ui::Alignment alignment() const noexcept { return alignment_; }

    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    std::string text_;
    int reserved_width_ = 0;
    ui::Alignment alignment_ = ui::Alignment::Start;
};

}  // namespace ckv::widgets
