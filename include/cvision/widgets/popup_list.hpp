// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The list a control drops when its choices are data rather than commands.
//
// A DropdownMenu is the right popup for things to DO -- its items carry
// commands, mnemonics, check marks and submenus, and it is as long as it is.
// A list of months, fonts, or files is things to BE: no commands, no
// mnemonics, and possibly more of them than fit on the screen. That is a
// ListView's job, so this is a ListView in a popup, and the two dropdowns
// share their frame, their colours and their dismissal rules rather than each
// inventing half of the other.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/view.hpp"
#include "cvision/widgets/list_view.hpp"

namespace ckv::ui {
class Application;
}

namespace ckv::widgets {

class Desktop;

// Floats above everything, framed and coloured as a dropdown menu. Escape or
// a press outside dismisses it; Enter or a press on a row chooses. It owns
// nothing about what opened it: the caller says where to hang it and what to
// do with the answer.
class PopupList : public ui::View {
public:
    PopupList(std::vector<std::string> items, std::optional<std::size_t> selected);

    ListView& list() noexcept { return *list_; }
    const ListView& list() const noexcept { return *list_; }

    // The row the reader settled on, and the fact that they did not settle on
    // one. Exactly one of the two runs, once.
    std::function<void(std::size_t)> on_choose;
    std::function<void()> on_dismiss;

    // Closes it as though the reader had pressed Escape -- for the control
    // that opened it, when something else has ended the interaction.
    void request_dismiss() { dismiss(); }

    // Wide enough for its longest item, tall enough for all of them, frame
    // included -- what it wants, before the desktop says what there is room
    // for.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every row is chosen by clicking it.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_resized() override;
    void on_attached() override;

private:
    void choose(std::size_t index);
    void dismiss();

    ListView* list_ = nullptr;
    std::vector<std::string> items_;
    std::optional<std::size_t> selected_;
    bool finished_ = false;
    ui::RoleId frame_role_ = ui::kInvalidRole;
};

// Hangs a list under `anchor_absolute` (its left edges aligned with it, or
// above it where there is no room below), takes the mouse and the keys while
// it is up, and hands back the popup so the caller can close it. Ownership is
// the desktop's; dismissal destroys it.
PopupList* show_popup_list(Rect anchor_absolute, std::vector<std::string> items,
                           std::optional<std::size_t> selected, ui::Application& app, Desktop& desktop,
                           std::function<void(std::size_t)> on_choose, std::function<void()> on_dismiss = {});

}  // namespace ckv::widgets
