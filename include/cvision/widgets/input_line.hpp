// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Single-line text editing (plans/widgets.md baseline). Grapheme-
// indexed throughout (never splits a cluster), with cursor movement,
// insert/overwrite, Backspace/Delete, Shift-extended selection,
// horizontal scroll-to-keep-cursor-visible, an automatic validator,
// input masks, password echo, and history-registry cycling.
//
// v1 scope (documented, not an oversight): clipboard cut/copy/paste is
// still deferred — it depends on Application's internal clipboard,
// landed after this widget's core editing but never wired back in
// (a real, separate increment: cut/copy need a selection-to-clipboard
// path and paste needs to route through Application::dispatch's
// existing paste-mirrors-into-clipboard logic, neither trivial).
// Mouse interaction is click-to-place-cursor only; drag-to-select is
// deferred alongside clipboard for the same reason — without copy, a
// mouse selection has nothing to do. History integration is Up/Down
// CYCLING through previously committed entries, not a dropdown popup
// (that needs the same popup+input-capture machinery menus use, not
// yet threaded through a plain field); commit_to_history() is called
// explicitly by the owner (e.g. on dialog accept), never automatically
// on Enter, since Enter's meaning for a given field is the caller's to
// decide (submit the dialog vs. accept the field vs. both).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/history.hpp"
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

    // set_valid() is still the EXTERNAL override an owner (e.g. the
    // dialog-accept veto, ARCHITECTURE.md §5) uses to mark a field
    // invalid from outside. set_validator() additionally makes the
    // field self-validating: it re-runs after every internal edit
    // (typed/pasted text, Backspace/Delete, mask edits, set_text) and
    // updates valid() automatically — an edit therefore always
    // supersedes a stale external verdict, which is correct: the
    // owner's next accept attempt re-checks anyway.
    void set_valid(bool valid) noexcept;
    bool valid() const noexcept { return valid_; }
    void set_validator(std::function<bool(const std::string&)> validator);

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

    // `registry`/`key` for Up/Down history cycling; `registry` may be
    // nullptr to disable (the default). commit_to_history() records
    // the field's current text into the registry under `key` — call
    // explicitly when the owner considers the value "accepted".
    void set_history(ui::HistoryRegistry* registry, std::string key);
    void commit_to_history();

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

    std::vector<std::string> graphemes_;
    std::size_t cursor_ = 0;
    std::optional<std::size_t> selection_anchor_;
    bool overwrite_mode_ = false;
    bool has_focus_ = false;
    bool valid_ = true;
    std::function<bool(const std::string&)> validator_;

    std::string mask_;
    char mask_placeholder_ = '_';

    bool password_echo_ = false;
    char echo_char_ = '*';

    ui::HistoryRegistry* history_registry_ = nullptr;
    std::string history_key_;
    int history_index_ = -1;  // -1 = showing live text, not browsing
    std::string history_saved_text_;

    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
    ui::RoleId invalid_role_;
};

}  // namespace ckv::widgets
