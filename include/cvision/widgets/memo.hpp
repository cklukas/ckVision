// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Memo: multi-line editing, scrolling, selection (plans/widgets.md
// M6b baseline). Grapheme-indexed per line, mirroring InputLine's own
// discipline (cursor/selection/Backspace/Delete never split a
// cluster).
//
// Scope note (documented, not an oversight, matching InputLine's own
// documented gaps): clipboard integration and bounded undo are
// deferred — both depend on Application-level machinery (the internal
// clipboard, a real undo/redo command stack) that doesn't have a
// concrete second consumer yet beyond InputLine's own already-deferred
// clipboard wiring; landing a widget-local stand-in here would mean
// throwing it away when that machinery arrives. Word-wrap toggle and
// grapheme-correct navigation refinements are explicitly beyond-
// baseline in the catalog.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

struct MemoPosition {
    int line = 0;
    int column = 0;  // grapheme index within the line

    friend bool operator==(const MemoPosition&, const MemoPosition&) = default;
};

class Memo : public ui::View {
public:
    Memo(const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId track_role,
         ui::RoleId thumb_role);

    void set_text(std::string text);
    std::string text() const;

    MemoPosition cursor() const noexcept { return cursor_; }
    bool has_selection() const noexcept { return selection_anchor_.has_value(); }
    // [begin, end) in document order; {cursor, cursor} if no selection.
    std::pair<MemoPosition, MemoPosition> selection_range() const noexcept;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void insert_text_at_cursor(std::string_view text);
    void erase_selection();
    void move_cursor(MemoPosition target, bool extend_selection);
    MemoPosition clamp_position(MemoPosition p) const noexcept;
    int line_length(int line) const noexcept;
    void ensure_cursor_visible();

    std::vector<std::vector<std::string>> lines_;  // each line: graphemes
    MemoPosition cursor_;
    std::optional<MemoPosition> selection_anchor_;
    bool has_focus_ = false;

    Scrollbar* scrollbar_ = nullptr;
    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
};

}  // namespace ckv::widgets
