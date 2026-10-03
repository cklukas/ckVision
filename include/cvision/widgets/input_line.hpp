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
#include "cvision/widgets/input_presentation.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.input.normal/focused/invalid/disabled". Already
// defaults to FocusPolicy::TabStop. A disabled field (D-076) shows its text
// in the disabled role with no caret, selection, or invalid mark.
class InputLine : public View {
public:
    // An empty, free-form, valid field: a Tab stop, one row high, preferring
    // ten cells.
    InputLine();

    // Flat (default), Padded or Underlined color-led chrome. Changing it keeps
    // text/selection/undo, cancels selection dragging and relays size hints.
    void set_presentation(InputPresentation presentation);
    InputPresentation presentation() const noexcept { return presentation_; }
    // Local text/caret/hit rectangle, excluding padding and the optional rule.
    Rect content_bounds() const noexcept { return input_content_rect(presentation_, Size{bounds().width, bounds().height}); }
    // Query without materializing a copy of text(), for composite layout.
    bool empty() const noexcept { return graphemes_.empty(); }

    // Replace the theme roles the field would otherwise resolve on
    // attachment: the unfocused, focused and invalid faces, and separately
    // the disabled face. The invalid face wins over the focused one. An
    // override set before attachment survives it; ui::kInvalidRole before
    // attachment leaves that role to the standard lookup. Either setter
    // repaints when it changes a role.
    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId invalid_role) noexcept {
        if (normal_role_ == normal_role && focused_role_ == focused_role && invalid_role_ == invalid_role)
            return;
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalid_role_ = invalid_role;
        invalidate();
    }
    void set_disabled_role_override(ui::RoleId role) noexcept {
        if (disabled_role_ == role) return;
        disabled_role_ = role;
        invalidate();
    }

    // Replaces the whole text (split into graphemes), puts the caret at the
    // end, clears the selection and the undo history, and re-runs the
    // validator. It is the owner's change, so on_edited does not fire and
    // the grapheme filter is not applied. On a masked field the text becomes
    // the buffer as given; it is not fitted to the mask. text() returns the
    // graphemes concatenated, including a masked field's literals and
    // unfilled placeholders.
    void set_text(std::string text);  // resets cursor to end, clears selection
    std::string text() const;

    // The caret, as a grapheme index from 0 (before the first grapheme) to
    // the grapheme count (after the last).
    std::size_t cursor() const noexcept { return cursor_; }
    // Places the caret at `grapheme` (clamped to the text; on a masked field,
    // at the next editable position) with nothing selected — what an owner
    // does after seeding the field with a keystroke, or to point at the
    // place a validator objected to. Editing continues from there; the
    // select-on-focus offer (D-066) is for a field the reader arrives at,
    // and an owner that has just placed the caret has decided otherwise.
    void set_cursor(std::size_t grapheme);
    // Whether a selection is anchored, and its span. A selection anchored at
    // the caret itself counts as present although it covers nothing.
    bool has_selection() const noexcept { return selection_anchor_.has_value(); }
    std::pair<std::size_t, std::size_t> selection_range() const noexcept;  // [begin, end) in graphemes; {cursor_,cursor_} if none

    // Whether typing replaces the grapheme under the caret (appending at the
    // end) instead of inserting. The reader toggles it with Insert; it has
    // no setter. A masked field always overwrites and ignores the mode.
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

    // The reader changed the text — typed, erased, pasted, cut, undid, or
    // stepped through history — reported once the change is complete. A
    // search box filters as it is typed into; a form reacts to one field
    // while the reader fills it. A programmatic set_text() is the owner's own
    // change and does not report, and neither does an event that left the
    // text as it was (a caret move, a refused grapheme).
    std::function<void()> on_edited;

    // The self-validation rule described above set_valid: called with the
    // whole text, true meaning valid. It runs once immediately against the
    // current text. An empty function stops self-validation and leaves
    // valid() at its last value.
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

    // Names the history list this field shares: the list under `key` in the
    // Application::history() of the Application the field is attached to.
    // Every input line, combo box, search box and dialog field naming the same
    // key reads and records the one list, so there is no per-widget history to
    // keep in step. Up and Down then cycle through it, newest first, and back
    // to the text that was there before recall began. Empty (the default)
    // turns history off; a detached field recalls and records nothing. A
    // masked field never cycles history.
    void set_history_key(std::string key);
    const std::string& history_key() const noexcept { return history_key_; }
    // Records the field's current text as the newest entry of its history
    // list — called by the owner when it considers the value accepted, since
    // what Enter means for a field is the owner's to decide. Does nothing
    // without a history key or an Application.
    void commit_to_history();

    // The editing commands behind the clipboard and undo keys, callable by an
    // owner (a menu's Edit commands, say). Each returns true when it acted.
    // Copy needs a non-empty selection and an attached Application and leaves
    // the text alone. Cut additionally erases the selection (on a masked
    // field, resetting its editable positions to the placeholder). Paste
    // inserts the Application's clipboard text as if typed, so the grapheme
    // filter and mask apply; it returns false when the clipboard is empty and
    // true whenever the enabled field took the text, even if every grapheme
    // was refused. Undo restores the text, caret, selection and overwrite
    // mode from before the most recent edit (at most 64 levels; set_text
    // discards them) and returns false when there is nothing to undo. Cut,
    // paste and undo report on_edited when they change the text.
    bool copy_selection_to_clipboard();
    bool cut_selection_to_clipboard();
    bool paste_from_clipboard();
    bool undo();

    void draw(scene::Painter& painter) override;
    // A masked field is exactly the mask's length. A free-form one shrinks to
    // four cells, prefers its text's width plus one (at least ten) and grows
    // without bound.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    void on_resized() override;
    // Enter is consumed only when on_accept is set, and an attached unmasked
    // field with a history key consumes Up and Down even when the history is
    // empty.
    // Ctrl, Alt and Super characters other than the clipboard and undo
    // chords are left unhandled so commands can have them.
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // A field the pointer can place a caret in and drag a selection
    // across is text, by exactly the definition the shape exists for.
    std::optional<PointerShape> pointer_shape_at(Point local) const override {
        if (!content_bounds().contains(local)) return std::nullopt;
        return enabled_in_tree() ? PointerShape::Text : PointerShape::NotAllowed;
    }
    // Gaining the focus selects the whole text with the caret at the end
    // (the select-on-focus offer, D-066), except on a masked or empty field.
    // A pointer press then places the caret where it lands instead.
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

    // Runs one reader action and reports on_edited when it — and not an
    // action it runs in turn — changed the text.
    bool reporting_edits(const std::function<bool()>& action);
    bool handle_key(const KeyEvent& event);
    bool handle_text(const TextEvent& event);
    bool handle_undo();
    bool handle_cut();

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
    // The application's history registry while a key is set and the field is
    // attached; nullptr otherwise.
    ui::HistoryRegistry* history_registry() const noexcept;

    std::size_t previous_word(std::size_t from) const noexcept;
    std::size_t next_word(std::size_t from) const noexcept;
    void erase_range(std::size_t begin, std::size_t end);

    int edit_depth_ = 0;
    InputPresentation presentation_ = InputPresentation::Flat;
    std::vector<std::string> graphemes_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> selection_anchor_;
    bool overwrite_mode_ = false;
    bool valid_ = true;
    std::function<bool(const std::string&)> validator_;
    std::function<bool(std::string_view)> grapheme_filter_;

    std::string mask_;
    char mask_placeholder_ = '_';

    bool password_echo_ = false;
    char echo_char_ = '*';

    std::string history_key_;
    int history_index_ = -1;  // -1 = showing live text, not browsing
    std::string history_saved_text_;
    std::vector<EditState> undo_stack_;
    bool dragging_selection_ = false;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
