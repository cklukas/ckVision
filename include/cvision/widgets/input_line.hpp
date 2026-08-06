// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Single-line text editing (plans/widgets.md baseline core-editing
// slice). Grapheme-indexed throughout (never splits a cluster), with
// cursor movement, insert/overwrite, Backspace/Delete, Shift-extended
// selection, and horizontal scroll-to-keep-cursor-visible.
//
// v1 scope (documented, not an oversight): clipboard cut/copy/paste,
// validators, input masks, password echo, and the history dropdown are
// deferred — each depends on Application-level machinery (the internal
// clipboard, the history registry, the dialog-accept veto path) that
// does not exist yet; landing them here first would mean testing
// against a widget-local stand-in that the real machinery would then
// have to replace. Mouse interaction is click-to-place-cursor only;
// drag-to-select is deferred alongside clipboard (same missing
// machinery: without copy, a mouse selection has nothing to do).
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

class InputLine : public View {
public:
    InputLine(const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId focused_role,
              ui::RoleId invalid_role);

    void set_text(std::string text);  // resets cursor to end, clears selection
    std::string text() const;

    std::size_t cursor() const noexcept { return cursor_; }
    bool has_selection() const noexcept { return selection_anchor_.has_value(); }
    std::pair<std::size_t, std::size_t> selection_range() const noexcept;  // [begin, end) in graphemes; {cursor_,cursor_} if none

    bool overwrite_mode() const noexcept { return overwrite_mode_; }

    // Driven externally by whatever eventually implements the
    // dialog-accept validation veto (ARCHITECTURE.md §5); this widget
    // has no validator of its own.
    void set_valid(bool valid) noexcept;
    bool valid() const noexcept { return valid_; }

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void insert_graphemes(const std::vector<std::string>& graphemes);
    void erase_selection();
    void move_cursor(std::size_t new_cursor, bool extend_selection);
    int scroll_offset_for_display() const;

    std::vector<std::string> graphemes_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> selection_anchor_;
    bool overwrite_mode_ = false;
    bool has_focus_ = false;
    bool valid_ = true;

    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
    ui::RoleId invalid_role_;
};

}  // namespace ckv::widgets
