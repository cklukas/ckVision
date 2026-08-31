// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Memo: multi-line editing, scrolling, selection (the widget catalog
// M6b baseline). Grapheme-indexed per line, mirroring InputLine's own
// discipline (cursor/selection/Backspace/Delete never split a
// cluster).
//
// Clipboard and undo are application-local and deterministic: cut/copy/paste
// route through Application's injected clipboard state, and undo stores bounded
// grapheme-indexed field snapshots. Wrapping affects display rows only and
// never the stored document text; the cursor and selection are held in
// document coordinates and mapped through the display rows for drawing and
// hit-testing, so a rewrap moves neither.
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
#include "cvision/widgets/text_layout.hpp"

namespace ckv::widgets {

struct MemoPosition {
    int line = 0;
    int column = 0;  // grapheme index within the line

    friend bool operator==(const MemoPosition&, const MemoPosition&) = default;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.memo.normal"/"ckv.memo.focused"/"ckv.memo.invalid";
// its embedded
// Scrollbar resolves its own roles independently.
class Memo : public ui::View {
public:
    Memo();

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role,
                           ui::RoleId invalid_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalid_role_ = invalid_role;
    }

    void set_text(std::string text);
    std::string text() const;

    // The dialog materializer uses this external validation state on accept.
    // The next content edit clears a stale rejection, as InputLine does.
    void set_valid(bool valid) noexcept;
    bool valid() const noexcept { return valid_; }

    // How a line too wide for the view is broken into display rows, and
    // when each scrollbar is on screen. WrapMode::Word is the default here:
    // a memo holds prose a reader is writing, and prose wants word wrap.
    void set_wrap_mode(WrapMode mode);
    WrapMode wrap_mode() const noexcept { return wrap_mode_; }
    void set_vertical_scrollbar_policy(ScrollbarPolicy policy);
    void set_horizontal_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy vertical_scrollbar_policy() const noexcept;
    ScrollbarPolicy horizontal_scrollbar_policy() const noexcept;

    // The widest display row, in cells, and the leftmost column on screen.
    int content_width() const noexcept { return content_width_; }
    int left_column() const noexcept;

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
    // Editable text, over its whole area including the blank part of a
    // short last line: the caret goes there too.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Text : PointerShape::NotAllowed;
    }
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
    void relayout_scrollbars();
    // Cells from the start of `row` to `column` — the x a cursor or a
    // selection edge lands on within its display row.
    int column_x(const VisualRow& row, int column) const;
    MemoPosition previous_word(MemoPosition from) const noexcept;
    MemoPosition next_word(MemoPosition from) const noexcept;

    std::vector<std::vector<std::string>> lines_;  // each line: graphemes
    MemoPosition cursor_;
    std::optional<MemoPosition> selection_anchor_;
    std::vector<EditState> undo_stack_;
    bool has_focus_ = false;
    WrapMode wrap_mode_ = WrapMode::Word;
    bool dragging_selection_ = false;
    bool valid_ = true;

    Scrollbar* scrollbar_ = nullptr;
    Scrollbar* h_scrollbar_ = nullptr;
    // The area left for text once whichever bars are showing have taken
    // their column and row. Wrapping, drawing and hit-testing all measure
    // against these, never against bounds(), so they cannot disagree.
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    int content_width_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
