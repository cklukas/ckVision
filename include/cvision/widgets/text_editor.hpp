// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TextEditor is the document-editor widget. Memo remains a compact form field;
// this type is designed around a shared revisioned EditorDocument.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/key.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/text_layout.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "cvision/widgets/editor_search.hpp"
#include "cvision/widgets/syntax_cache.hpp"
#include "cvision/widgets/syntax_profile.hpp"

namespace ckv::widgets {

struct EditorStatus {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t selection_bytes = 0;
    bool modified = false;
    bool overwrite = false;
    // The caret is a provisional position past the text (see VirtualCaret):
    // line and column above name that position, not a place in the document.
    bool virtual_caret = false;
    std::string profile_id = "plain";
    DocumentEncoding encoding = DocumentEncoding::Utf8;
    DocumentNewline newline = DocumentNewline::Lf;
};

// What a chord asks an editor to do beyond moving the caret and changing the
// text at it. These are the verbs an application commonly also offers from a
// menu, which is why they are data: an application that owns its own command
// table and keyboard scheme gives the editor exactly the bindings it wants,
// or none, and reaches the same verbs through perform().
enum class EditorCommand {
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    SelectAll,
    FindSelection,
    FindNext,
    FindPrevious,
    ToggleOverwrite,
};

struct EditorKeyBinding {
    KeyChord chord;
    EditorCommand command = EditorCommand::Undo;

    friend bool operator==(const EditorKeyBinding&, const EditorKeyBinding&) = default;
};

// The bindings every TextEditor starts with: Ctrl+Z undo, Ctrl+Y redo,
// Ctrl+X / Shift+Delete cut, Ctrl+C / Ctrl+Insert copy, Ctrl+V / Shift+Insert
// paste, Ctrl+A select all, Ctrl+F find the selection, F3 / Shift+F3 find
// next / previous, and Insert toggling overwrite.
std::vector<EditorKeyBinding> default_editor_key_bindings();

// A caret the reader placed past the text: right of a line's end, or below the
// last line. Nothing is written when it is placed. The first insertion at it
// supplies the line breaks and spaces needed to reach it together with the
// inserted text, as one edit; any caret motion, and any deletion, abandons it.
struct VirtualCaret {
    // Logical line, which may lie below the document's last line.
    std::size_t line = 0;
    // Display cells from the start of that line.
    std::size_t column = 0;

    friend bool operator==(const VirtualCaret&, const VirtualCaret&) = default;
};

// The kind of text change the reader asked for.
enum class EditKind {
    Insert,              // text typed at the caret
    Paste,               // text arriving from the clipboard or a terminal paste
    LineBreak,           // Enter
    Indent,              // Tab
    DeleteBackward,      // Backspace
    DeleteForward,       // Delete
    DeleteWordBackward,  // Ctrl+Backspace
    DeleteWordForward,   // Ctrl+Delete
    Cut,                 // the selection, taken to the clipboard
};

// One text change the reader asked for, described twice: as the reader's
// intent (kind, text, virtual caret), and as the replacement the editor would
// commit on its own (range, replacement). A host that keeps its own editing
// rules — a session with named undo steps, a structured editor — acts on the
// intent; a host that only wants to observe or veto can commit the
// replacement itself.
struct EditRequest {
    EditKind kind = EditKind::Insert;
    // The typed or pasted text, "\n" for a line break, the indentation for
    // Indent, the selected text for Cut; empty for the deletions.
    std::string text;
    // The current text the editor would replace: the selection, the grapheme
    // or word beside the caret, the graphemes overwrite mode covers, or an
    // empty range at the insertion point.
    DocumentRange range;
    // What the editor would put in its place, including any line breaks and
    // spaces a virtual caret needs.
    std::string replacement;
    // The provisional caret the change starts from, when there is one.
    std::optional<VirtualCaret> virtual_caret;
};

// Host-computed colouring for one byte range of the document, resolved
// through the theme by role. A grapheme takes the style of the span that
// contains its first byte.
struct HighlightSpan {
    std::size_t begin_byte = 0;
    std::size_t end_byte = 0;
    ui::RoleId role = ui::kInvalidRole;

    friend bool operator==(const HighlightSpan&, const HighlightSpan&) = default;
};

class TextEditor final : public ui::View {
public:
    using StatusObserverId = std::uint64_t;
    using StatusObserver = std::function<void(const EditorStatus&)>;
    // Called for every text change the reader asks for, before the editor
    // commits anything. Return true after handling it — typically by
    // committing a transaction of the host's own and restoring a current
    // selection with set_selection() — and the editor does nothing further;
    // return false, having changed nothing, and the editor commits
    // `request.replacement` over `request.range` itself. Never called for a
    // read-only or disabled editor.
    using EditHandler = std::function<bool(TextEditor&, const EditRequest&)>;
    // Called when the reader asks for a context menu: a right click, a
    // Ctrl+click (for terminals that keep the right button for themselves), or
    // Shift+F10. A click places the caret first unless it lands inside the
    // selection. `screen_cell` is where the menu belongs, in screen cells.
    using ContextMenuHandler = std::function<void(TextEditor&, Point screen_cell)>;

    explicit TextEditor(std::shared_ptr<EditorDocument> document, SyntaxProfileRegistry* profiles = nullptr);
    ~TextEditor() override;

    const std::shared_ptr<EditorDocument>& document() const noexcept { return document_; }
    DocumentPosition cursor() const noexcept { return cursor_; }
    std::optional<DocumentRange> selection() const noexcept;
    // Selects a current, grapheme-aligned document range. Controllers that
    // apply a document transaction can restore the semantic selection around
    // the transformed content without synthesizing keyboard input. A virtual
    // caret is abandoned.
    bool set_selection(DocumentRange range);
    bool select_all();
    EditorStatus status() const;

    void set_show_line_numbers(bool enabled);
    bool show_line_numbers() const noexcept { return show_line_numbers_; }
    // How a logical line is broken into viewport-width display rows. The
    // document's line/column model stays logical and is therefore unchanged
    // by this; a visible reflow marker identifies each continued row.
    // WrapMode::None is the default — source and logs mean what they mean at
    // their own line breaks — and the horizontal bar reaches the rest.
    void set_wrap_mode(WrapMode mode);
    WrapMode wrap_mode() const noexcept { return wrap_mode_; }
    void set_vertical_scrollbar_policy(ScrollbarPolicy policy);
    void set_horizontal_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy vertical_scrollbar_policy() const noexcept;
    ScrollbarPolicy horizontal_scrollbar_policy() const noexcept;
    // The widest display row in cells, and the leftmost text column showing.
    // The gutter never scrolls: line numbers that slid away with the text
    // would stop being an index into it.
    int content_width() const noexcept { return content_width_; }
    int left_column() const noexcept;
    void set_read_only(bool value) noexcept { read_only_ = value; }
    bool read_only() const noexcept { return read_only_; }
    void set_overwrite(bool value);
    bool overwrite() const noexcept { return overwrite_; }
    // The number of spaces Tab inserts (at least one).
    void set_tab_width(int spaces);
    int tab_width() const noexcept { return tab_width_; }

    // The chords this editor answers itself. Replacing the list rebinds them;
    // an empty list leaves every verb to the application's own commands,
    // which reach it through perform().
    void set_key_bindings(std::vector<EditorKeyBinding> bindings);
    const std::vector<EditorKeyBinding>& key_bindings() const noexcept { return key_bindings_; }
    // Runs one editor verb. Returns whether it did anything: an undo with
    // nothing to undo, or a find with nothing selected, returns false.
    bool perform(EditorCommand command);

    void set_edit_handler(EditHandler handler) { edit_handler_ = std::move(handler); }

    // Where the editor's undo and redo keys go. Without a handler they walk
    // the document's own history. With one, they are handed to it — the
    // host that keeps the authoritative journal elsewhere (an application
    // session whose undo steps are named and shared with every other view
    // of the same document) answers them, and the document's own history
    // is left out of the reader's hands so the two can never disagree
    // about what "undo" reverses. `redo` says which of the two was asked.
    using HistoryHandler = std::function<bool(TextEditor&, bool redo)>;
    void set_history_handler(HistoryHandler handler) { history_handler_ = std::move(handler); }

    // Whether a double-click past the text places a virtual caret there
    // (see VirtualCaret). Off by default: a double-click past the text then
    // places an ordinary caret at the nearest position.
    void set_virtual_space(bool enabled);
    bool virtual_space() const noexcept { return virtual_space_; }
    const std::optional<VirtualCaret>& virtual_caret() const noexcept { return virtual_caret_; }
    // Places a virtual caret, as a host restoring one after its own
    // transaction does. Refused (false) when virtual space is off or the
    // position is not past the text.
    bool set_virtual_caret(VirtualCaret caret);

    // Colours the document from spans a host computed over the text at
    // `revision`, in place of the syntax profile's colouring. Refused
    // (false) unless `revision` is the document's current one and the spans
    // are ordered, non-overlapping and inside the document. An edit keeps
    // the spans it does not touch — shifted with the text — and drops the
    // ones it does, until the host supplies new ones.
    bool set_highlights(DocumentRevision revision, std::vector<HighlightSpan> spans);
    // Returns to the syntax profile's colouring.
    void clear_highlights();
    bool has_highlights() const noexcept { return highlights_active_; }

    void set_context_menu_handler(ContextMenuHandler handler) { context_menu_handler_ = std::move(handler); }

    void set_file_name(std::string name);
    void set_profile(std::optional<std::string> profile_id);
    const std::string& profile_id() const noexcept { return profile_id_; }
    void refresh_syntax();

    // Called after a cursor, selection, document, mode, profile, or document-
    // format change. The callback is application-owned; the editor never
    // assumes a window chrome arrangement.
    // It is suitable for a frame-overlay status readout.
    void set_status_changed_handler(std::function<void(const EditorStatus&)> handler);
    // A status observer is independent of window chrome. This supports a
    // reusable EditorStatusModel or any client-owned status presentation.
    StatusObserverId subscribe_status(StatusObserver observer);
    void unsubscribe_status(StatusObserverId observer) noexcept;

    // Search state belongs to this editor view, while matching and mutation
    // remain in the pure EditorSearch/EditorDocument layers. Matches are
    // revision-bound and paint below a primary selection.
    void set_search_query(EditorSearchQuery query);
    const EditorSearchQuery& search_query() const noexcept { return search_query_; }
    void clear_search();
    std::size_t search_match_count() const noexcept { return search_matches_.size(); }
    bool use_selection_as_search_query();
    bool find_next(bool forward = true);
    bool replace_current_search_match(std::string replacement);
    DocumentEditResult replace_all_search_matches(const std::string& replacement);

    bool copy_selection_to_clipboard();
    bool cut_selection_to_clipboard();
    bool paste_from_clipboard();

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Editable text.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Text;
    }
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    void on_resized() override;
    std::optional<CursorState> cursor_state() const override;

private:
    struct Line {
        std::size_t start_byte = 0;
        std::string text;
        std::vector<SyntaxSpan> spans;
        std::string incoming_state;
        std::string outgoing_state;
    };

    struct DisplayRow {
        std::size_t line = 0;
        std::size_t begin_byte = 0;
        std::size_t end_byte = 0;
        bool continues = false;
    };

    void rebuild_lines(std::size_t first_dirty_line = 0);
    // Layout is retained between paints. Building it may inspect every logical
    // line (for wrap-aware scrolling), but a steady-state viewport frame only
    // reads this cache and paints its visible rows.
    const std::vector<DisplayRow>& display_rows(int content_width) const;
    std::size_t cursor_display_row(const std::vector<DisplayRow>& rows) const;
    // The display row and cell column the caret is drawn at, counting rows
    // below the text and cells past a line's end for a virtual caret.
    std::pair<int, int> caret_display_cell(const std::vector<DisplayRow>& rows) const;
    void ensure_cursor_visible();
    void relayout_scrollbars();
    // Cells from the start of a display row to `byte` within it.
    int column_x(const DisplayRow& row, std::size_t byte) const;
    void notify_status_changed();
    void refresh_search();
    bool activate_search_match(std::size_t index);
    // Moves the caret. Every motion abandons a virtual caret; only vertical
    // motion keeps the column the caret is trying to return to.
    void move_cursor(DocumentPosition target, bool extend, bool keep_desired_column = false);
    void move_vertically(std::ptrdiff_t lines, bool extend);
    std::optional<DocumentPosition> position_for_screen_cell(Point absolute_cell) const;
    // The virtual caret a double-click at `absolute_cell` places, when the
    // cell lies past the text.
    std::optional<VirtualCaret> virtual_caret_for_screen_cell(Point absolute_cell) const;
    // The document position a virtual caret's padding is inserted at, and
    // the padding itself.
    DocumentPosition virtual_caret_anchor(const VirtualCaret& caret) const;
    std::string virtual_caret_padding(const VirtualCaret& caret) const;
    // Asks for one text change: the edit handler first, then the editor's
    // own commit. The one path every typed, pasted, and erased change takes.
    bool submit_edit(EditRequest request);
    bool submit_text(EditKind kind, std::string text);
    bool erase(EditKind kind);
    std::size_t gutter_width() const;
    Style style_for(std::size_t line, std::size_t line_byte, bool selected, ui::RoleId highlight) const;
    void clamp_cursor();
    void carry_highlights(const DocumentChange& change);

    std::shared_ptr<EditorDocument> document_;
    SyntaxProfileRegistry* profiles_ = nullptr;
    SyntaxProfileRegistry fallback_profiles_;
    SyntaxCache syntax_cache_;
    const LanguageProfile* profile_ = nullptr;
    std::string profile_id_ = "plain";
    std::string file_name_;
    std::vector<Line> lines_;
    mutable std::vector<DisplayRow> display_rows_cache_;
    mutable int display_rows_width_ = -1;
    mutable bool display_rows_dirty_ = true;
    DocumentPosition cursor_;
    std::optional<DocumentPosition> selection_anchor_;
    // The grapheme column vertical motion returns to; set by the first
    // vertical move after any other motion or edit.
    std::optional<std::size_t> desired_column_;
    std::optional<VirtualCaret> virtual_caret_;
    EditorSearchQuery search_query_;
    std::vector<EditorSearchMatch> search_matches_;
    std::optional<std::size_t> active_search_match_;
    EditorDocument::ObserverId observer_ = 0;
    int top_display_row_ = 0;
    Scrollbar* v_scrollbar_ = nullptr;
    Scrollbar* h_scrollbar_ = nullptr;
    // The text area, once the gutter and whichever bars are showing have
    // taken their columns and row.
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    int content_width_ = 0;
    bool show_line_numbers_ = false;
    WrapMode wrap_mode_ = WrapMode::None;
    bool read_only_ = false;
    bool overwrite_ = false;
    bool virtual_space_ = false;
    int tab_width_ = 4;
    std::vector<EditorKeyBinding> key_bindings_ = default_editor_key_bindings();
    EditHandler edit_handler_;
    ContextMenuHandler context_menu_handler_;
    bool dragging_ = false;
    bool highlights_active_ = false;
    std::vector<HighlightSpan> highlights_;

    ui::RoleId text_role_ = ui::kInvalidRole;
    ui::RoleId gutter_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId search_role_ = ui::kInvalidRole;
    std::vector<ui::RoleId> syntax_roles_;
    std::function<void(const EditorStatus&)> status_changed_;
    HistoryHandler history_handler_;
    StatusObserverId next_status_observer_id_ = 1;
    std::vector<std::pair<StatusObserverId, StatusObserver>> status_observers_;
};

// A small independently composable status model. It mirrors one editor's
// snapshot and offers its own scoped subscriptions; a Window, StatusLine, or
// client-owned view can render it without taking ownership of TextEditor.
class EditorStatusModel {
public:
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(const EditorStatus&)>;

    explicit EditorStatusModel(TextEditor& editor);
    ~EditorStatusModel();

    EditorStatusModel(const EditorStatusModel&) = delete;
    EditorStatusModel& operator=(const EditorStatusModel&) = delete;

    const EditorStatus& value() const noexcept { return value_; }
    ObserverId subscribe(Observer observer);
    void unsubscribe(ObserverId observer) noexcept;

private:
    void update(const EditorStatus& value);

    TextEditor* editor_ = nullptr;
    TextEditor::StatusObserverId editor_observer_ = 0;
    EditorStatus value_;
    ObserverId next_observer_id_ = 1;
    std::vector<std::pair<ObserverId, Observer>> observers_;
};

}  // namespace ckv::widgets
