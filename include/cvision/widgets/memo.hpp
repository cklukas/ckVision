// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Memo: multi-line editing, scrolling, selection (plans/widgets.md
// M6b baseline). Grapheme-indexed per line, mirroring InputLine's own
// discipline (cursor/selection/Backspace/Delete never split a
// cluster).
//
// Clipboard and undo are application-local and deterministic: cut/copy/paste
// route through Application's injected clipboard state, and undo stores bounded
// grapheme-indexed field snapshots. Word wrap is a widget policy bit that
// affects soft visual rows without changing the stored document text.
// Its editing keymap matches InputLine and TextEditor, including word/document
// navigation, Shift-extended selection, Ctrl+C/X/V, Ctrl+Insert/Shift+Insert,
// and Ctrl+Backspace/Delete.
#pragma once

#include <optional>
#include <string>
#include <utility>
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

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.memo.normal"/"ckv.memo.focused"; its embedded
// Scrollbar resolves its own roles independently.
class Memo : public ui::View {
public:
    Memo();

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
    }

    void set_text(std::string text);
    std::string text() const;

    void set_word_wrap(bool enabled);
    bool word_wrap() const noexcept { return word_wrap_; }

    MemoPosition cursor() const noexcept { return cursor_; }
    bool has_selection() const noexcept { return selection_anchor_.has_value(); }
    // [begin, end) in document order; {cursor, cursor} if no selection.
    std::pair<MemoPosition, MemoPosition> selection_range() const noexcept;

    bool copy_selection_to_clipboard();
    bool cut_selection_to_clipboard();
    bool paste_from_clipboard();
    bool undo();

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    struct EditState {
        std::vector<std::vector<std::string>> lines;
        MemoPosition cursor;
        std::optional<MemoPosition> selection_anchor;
    };

    struct VisualRow {
        int line = 0;
        int begin = 0;
        int end = 0;
    };

    static constexpr std::size_t kMaxUndoDepth = 64;

    void insert_text_at_cursor(std::string_view text);
    void erase_selection();
    void move_cursor(MemoPosition target, bool extend_selection);
    MemoPosition clamp_position(MemoPosition p) const noexcept;
    int line_length(int line) const noexcept;
    std::string selected_text() const;
    void record_undo_state();
    std::vector<VisualRow> visual_rows(int visible_width) const;
    int visual_row_for_position(MemoPosition position, int visible_width) const;
    MemoPosition position_at_visual_row_column(int visual_row, int local_x, int visible_width) const;
    void ensure_cursor_visible();
    MemoPosition previous_word(MemoPosition from) const noexcept;
    MemoPosition next_word(MemoPosition from) const noexcept;

    std::vector<std::vector<std::string>> lines_;  // each line: graphemes
    MemoPosition cursor_;
    std::optional<MemoPosition> selection_anchor_;
    std::vector<EditState> undo_stack_;
    bool has_focus_ = false;
    bool word_wrap_ = false;
    bool dragging_selection_ = false;

    Scrollbar* scrollbar_ = nullptr;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
