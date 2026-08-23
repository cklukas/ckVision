// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Single-line text editing (the widget catalog baseline). Grapheme-
// indexed throughout (never splits a cluster), with cursor movement,
// insert/overwrite, Backspace/Delete, Shift-extended selection,
// horizontal scroll-to-keep-cursor-visible, an automatic validator,
// input masks, password echo, and history-registry cycling.
//
// Clipboard cut/copy/paste routes through Application's deterministic
// internal clipboard; mouse drag selection and bounded undo are part of
// the control itself. History integration is Up/Down cycling through
// previously committed entries; ComboBox supplies the dropdown-style
// history surface. commit_to_history() is called explicitly by the
// owner (e.g. on dialog accept), never automatically on Enter, since
// Enter's meaning for a given field is the caller's to decide (submit
// the dialog vs. accept the field vs. both).
// The standard editing keymap is shared with Memo and TextEditor: Ctrl+Left/
// Right moves by word, Ctrl+Home/End reaches field boundaries, Shift extends
// movement, Ctrl+C/X/V and Ctrl+Insert/Shift+Insert access the clipboard, and
// Ctrl+Backspace/Delete erase by word.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.input.normal/focused/invalid". Already defaults
// to FocusPolicy::TabStop.
class InputLine : public View {
public:
    InputLine();

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId invalid_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalid_role_ = invalid_role;
    }

    void set_text(std::string text);  // resets cursor to end, clears selection
    std::string text() const;

    std::size_t cursor() const noexcept { return cursor_; }
    bool has_selection() const noexcept { return selection_anchor_.has_value(); }
    std::pair<std::size_t, std::size_t> selection_range() const noexcept;  // [begin, end) in graphemes; {cursor_,cursor_} if none

    bool overwrite_mode() const noexcept { return overwrite_mode_; }

    // set_valid() is still the EXTERNAL override an owner (e.g. the
    // dialog-accept veto, the architecture §5) uses to mark a field
    // invalid from outside. set_validator() additionally makes the
    // field self-validating: it re-runs after every internal edit
    // (typed/pasted text, Backspace/Delete, mask edits, set_text) and
    // updates valid() automatically — an edit therefore always
    // supersedes a stale external verdict, which is correct: the
    // owner's next accept attempt re-checks anyway.
    void set_valid(bool valid) noexcept;
    bool valid() const noexcept { return valid_; }
    // Enter was pressed here — the reader is done with this field.
    //
    // A one-line field is very often the whole of a small interaction: a
    // search box, a filter, a chat prompt, a rename. Without this, each
    // of those has to subclass the widget to catch one key, which is how
    // a toolkit ends up with five nearly identical subclasses. A dialog
    // that wants Enter to mean "accept the form" leaves this unset and
    // lets the window's own accept_request have it, which is why this
    // fires only when something is listening.
    std::function<void()> on_accept;

    void set_validator(std::function<bool(const std::string&)> validator);

    // Optional per-grapheme admission rule for interactive input. It applies
    // uniformly to typed characters and pasted TextEvents; rejected
    // graphemes are consumed without changing the field. Programmatic
    // set_text() deliberately bypasses the rule so an owner can display an
    // existing value even while editing is constrained.
    void set_grapheme_filter(std::function<bool(std::string_view)> filter);

    // Input mask: '9' = digit, 'A' = letter, '*' = any single
    // grapheme, any other character is a literal the user cannot edit
    // (auto-skipped by cursor movement and typing). Setting a mask
    // resets the field to an all-placeholder buffer of the mask's
    // fixed length; passing an empty mask disables masking and returns
    // to free-form editing (the CURRENT text is preserved as-is when
    // disabling — only enabling/changing a mask resets the buffer,
    // since there is no principled way to reinterpret existing
    // free-form text against a newly-imposed mask). `placeholder` is
    // the glyph shown at not-yet-filled editable positions.
    void set_mask(std::string mask, char placeholder = '_');
    bool has_mask() const noexcept { return !mask_.empty(); }

    // When true, draw() shows `echo_char` at every position instead of
    // the real content — editing/cursor/selection all still operate on
    // the real text, this is display-only.
    void set_password_echo(bool enabled, char echo_char = '*');
    bool password_echo() const noexcept { return password_echo_; }
    char password_echo_char() const noexcept { return echo_char_; }

    // `registry`/`key` for Up/Down history cycling; `registry` may be
    // nullptr to disable (the default). commit_to_history() records
    // the field's current text into the registry under `key` — call
    // explicitly when the owner considers the value "accepted".
    void set_history(ui::HistoryRegistry* registry, std::string key);
    void commit_to_history();

    bool copy_selection_to_clipboard();
    bool cut_selection_to_clipboard();
    bool paste_from_clipboard();
    bool undo();

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // A field the pointer can place a caret in and drag a selection
    // across is text, by exactly the definition the shape exists for.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Text : PointerShape::NotAllowed;
    }
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    struct EditState {
        std::vector<std::string> graphemes;
        std::size_t cursor = 0;
        std::optional<std::size_t> selection_anchor;
        bool overwrite_mode = false;
    };

    static constexpr std::size_t kMaxUndoDepth = 64;

    void insert_graphemes(const std::vector<std::string>& graphemes);
    void erase_selection();
    void move_cursor(std::size_t new_cursor, bool extend_selection);
    std::size_t cursor_index_at(Point absolute_cell) const;
    std::string selected_text() const;
    void record_undo_state();
    int scroll_offset_for_display() const;
    void revalidate();

    // Masked-editing path (separate from the free-form path above —
    // masked fields are fixed-length and never insert/shift).
    bool mask_position_editable(std::size_t index) const noexcept;
    bool mask_char_accepts(char mask_char, std::string_view grapheme) const noexcept;
    std::size_t mask_next_editable(std::size_t from) const noexcept;      // from, inclusive; graphemes_.size() if none
    std::size_t mask_previous_editable(std::size_t from) const noexcept;  // from, exclusive going backward
    bool on_key_masked(const KeyEvent& event);
    bool on_text_masked(const TextEvent& event);

    void history_show(int index);  // -1 = the live (pre-browsing) text

    std::size_t previous_word(std::size_t from) const noexcept;
    std::size_t next_word(std::size_t from) const noexcept;
    void erase_range(std::size_t begin, std::size_t end);

    std::vector<std::string> graphemes_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> selection_anchor_;
    bool overwrite_mode_ = false;
    bool has_focus_ = false;
    bool valid_ = true;
    std::function<bool(const std::string&)> validator_;
    std::function<bool(std::string_view)> grapheme_filter_;

    std::string mask_;
    char mask_placeholder_ = '_';

    bool password_echo_ = false;
    char echo_char_ = '*';

    ui::HistoryRegistry* history_registry_ = nullptr;
    std::string history_key_;
    int history_index_ = -1;  // -1 = showing live text, not browsing
    std::string history_saved_text_;
    std::vector<EditState> undo_stack_;
    bool dragging_selection_ = false;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
