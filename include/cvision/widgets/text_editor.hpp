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

// A snapshot of what a status readout shows for one TextEditor. TextEditor::status() builds it,
// and the status handler and status observers receive it.
struct EditorStatus {
    // The caret's line and column, both one-based. The column counts grapheme clusters, not
    // cells; at a virtual caret it counts the line's graphemes and then one per blank cell
    // past the line's end.
    std::size_t line = 1;
    std::size_t column = 1;
    // The selection's length in bytes; 0 without a selection.
    std::size_t selection_bytes = 0;
    // The document's modified flag.
    bool modified = false;
    // Whether the editor is in overwrite mode rather than insert mode.
    bool overwrite = false;
    // The caret is a provisional position past the text (see VirtualCaret):
    // line and column above name that position, not a place in the document.
    bool virtual_caret = false;
    // The active syntax profile's id, and the document's encoding and preferred newline.
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

// One chord bound to one editor verb. A key press runs the verb when its chord equals `chord`
// exactly, modifiers and text included; the first matching binding in the list wins.
struct EditorKeyBinding {
    // The chord, and the verb it runs.
    KeyChord chord;
    EditorCommand command = EditorCommand::Undo;

    // Memberwise equality.
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

    // Memberwise equality.
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
    Replace,             // the current search match, replaced
    ReplaceAll,          // every match of the search query, replaced in one transaction
};

// One text change the reader asked for, described twice: as the reader's
// intent (kind, text, virtual caret), and as the replacements the editor would
// commit on its own (edits). A host that keeps its own editing rules — a
// session with named undo steps, a structured editor — acts on the intent; a
// host that only wants to observe or veto can commit the edits itself.
struct EditRequest {
    // Which kind of change the reader asked for.
    EditKind kind = EditKind::Insert;
    // The typed or pasted text, "\n" for a line break, the indentation for
    // Indent, the selected text for Cut, the replacement text for Replace and
    // ReplaceAll; empty for the deletions.
    std::string text;
    // The replacements the editor would commit as one transaction: ranges of
    // the current revision, in document order, that do not overlap. ReplaceAll
    // has one per match of the search query; every other kind has exactly one,
    // whose range is the selection, the grapheme or word beside the caret, the
    // graphemes overwrite mode covers, the current search match, or an empty
    // range at the insertion point, and whose text includes any line breaks
    // and spaces a virtual caret needs.
    std::vector<DocumentTextEdit> edits;
    // The provisional caret the change starts from, when there is one.
    std::optional<VirtualCaret> virtual_caret;
};

// Host-computed colouring for one byte range of the document, resolved
// through the theme by role. A grapheme takes the style of the span that
// contains its first byte.
struct HighlightSpan {
    // A half-open byte range of the text at the revision the spans were set for, and the theme
    // role that colours it; kInvalidRole colours it as plain editor text.
    std::size_t begin_byte = 0;
    std::size_t end_byte = 0;
    ui::RoleId role = ui::kInvalidRole;

    // Memberwise equality.
    friend bool operator==(const HighlightSpan&, const HighlightSpan&) = default;
};

// A multi-line editor view over a shared EditorDocument: caret and selection, keyboard and mouse
// editing, undo, clipboard, search, syntax colouring from a profile registry or from spans a
// host supplies, an optional line-number gutter, wrapping and scroll bars. Several editors, and
// other code, may change one document; each editor carries its caret and selection along with
// the text through changes it did not make.
//
// Theme roles, resolved once attached, each registered with a built-in fallback style when the
// theme lacks it: "ckv.editor.text", "ckv.editor.gutter" (also the wrap marker),
// "ckv.editor.selection", "ckv.editor.search", and "ckv.editor.syntax." followed by plain,
// keyword, type, property, string, number, comment, command, operator, escape or error.
class TextEditor final : public ui::View {
public:
    // A status subscription's identity, starting at 1 and never reused by an editor, and its
    // callback; see subscribe_status.
    using StatusObserverId = std::uint64_t;
    using StatusObserver = std::function<void(const EditorStatus&)>;
    // Called for every text change the reader asks for, including the search
    // replacements, before the editor commits anything. Return true after
    // handling it — typically by committing a transaction of the host's own
    // and restoring a current selection with set_selection() — and the editor
    // does nothing further; return false, having changed nothing, and the
    // editor commits `request.edits` itself as one transaction, recording its
    // selection before the edit for undo, and leaves the caret after the last
    // of them with nothing selected. Never called for a read-only or disabled
    // editor.
    using EditHandler = std::function<bool(TextEditor&, const EditRequest&)>;
    // Called when the reader asks for a context menu: a right click, a
    // Ctrl+click (for terminals that keep the right button for themselves), or
    // the Menu key or Shift+F10, which ask at the caret. A click places the
    // caret first unless it lands inside the selection. `screen_cell` is where
    // the menu belongs, in screen cells.
    using ContextMenuHandler = std::function<void(TextEditor&, Point screen_cell)>;

    // `document` is shared with other views and controllers; a null one is replaced by a new
    // empty document. `profiles` is the registry syntax profiles are detected from and must
    // outlive the editor; null gives the editor its own registry of the standard profiles. The
    // editor starts with the plain-text profile (set_file_name or set_profile detects another),
    // the caret at the start of the text, Tab-stop focus, and both scroll bars on Auto. It
    // observes the document for its whole lifetime.
    explicit TextEditor(std::shared_ptr<EditorDocument> document, SyntaxProfileRegistry* profiles = nullptr);
    ~TextEditor() override;

    // The shared document; never null.
    const std::shared_ptr<EditorDocument>& document() const noexcept { return document_; }
    // The caret, which is the moving end of any selection and a position of the current
    // revision.
    DocumentPosition cursor() const noexcept { return cursor_; }
    // The selected range, ordered begin before end; nullopt when nothing is selected, including
    // when anchor and caret coincide.
    std::optional<DocumentRange> selection() const noexcept;
    // Selects a current, grapheme-aligned document range. Controllers that
    // apply a document transaction can restore the semantic selection around
    // the transformed content without synthesizing keyboard input. A virtual
    // caret is abandoned. The caret goes to the range's end. Any other range
    // is refused (false) and changes nothing.
    bool set_selection(DocumentRange range);
    // Selects the whole text with the caret at its end; false, changing nothing, when the
    // document is empty.
    bool select_all();
    // The current status snapshot.
    EditorStatus status() const;

    // A gutter at the left edge with one-based line numbers, right-aligned to the width of the
    // largest and followed by one blank column. A wrapped line is numbered on its first row
    // only. Off by default.
    void set_show_line_numbers(bool enabled);
    bool show_line_numbers() const noexcept { return show_line_numbers_; }
    // How a logical line is broken into viewport-width display rows. The
    // document's line/column model stays logical and is therefore unchanged
    // by this; a visible reflow marker identifies each continued row.
    // WrapMode::None is the default — source and logs mean what they mean at
    // their own line breaks — and the horizontal bar reaches the rest.
    void set_wrap_mode(WrapMode mode);
    WrapMode wrap_mode() const noexcept { return wrap_mode_; }
    // When each scroll bar is on screen; both are ScrollbarPolicy::Auto by default. The
    // horizontal bar spans the text area only, never the gutter.
    void set_vertical_scrollbar_policy(ScrollbarPolicy policy);
    void set_horizontal_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy vertical_scrollbar_policy() const noexcept;
    ScrollbarPolicy horizontal_scrollbar_policy() const noexcept;
    // The widest display row in cells, and the leftmost text column showing.
    // The gutter never scrolls: line numbers that slid away with the text
    // would stop being an index into it.
    int content_width() const noexcept { return content_width_; }
    int left_column() const noexcept;
    // While read-only, typing, pasting, cutting, deleting, the search replacements, undo and
    // redo are refused, and neither the edit handler nor the history handler is called; the
    // caret, the selection, copy and find still work. Off by default.
    void set_read_only(bool value) noexcept { read_only_ = value; }
    bool read_only() const noexcept { return read_only_; }
    // Overwrite mode: typed text without a selection replaces as many following graphemes as
    // it has, never reaching past the end of the line; pasted text and line breaks still
    // insert. The caret is drawn as a block instead of a bar, and a change is reported to the
    // status handler and observers. Off by default.
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
    // nothing to undo, or a find with nothing selected, returns false. The
    // verbs that change the text — undo, redo, cut and paste — are refused
    // (false) by a read-only or disabled editor. Without a history handler,
    // undo and redo walk the document's history and then take the selection
    // the step restores (DocumentChange::selection): after an undo, the
    // selection or caret the undone edit was made at; after a redo, a caret
    // after the replayed text. Other views of the document only carry their
    // own selections through the step.
    bool perform(EditorCommand command);

    // Installs the handler for text changes, replacing any earlier one; an empty function
    // removes it. See EditHandler.
    void set_edit_handler(EditHandler handler) { edit_handler_ = std::move(handler); }

    // Where the editor's undo and redo keys go. Without a handler they walk
    // the document's own history. With one, they are handed to it — the
    // host that keeps the authoritative journal elsewhere (an application
    // session whose undo steps are named and shared with every other view
    // of the same document) answers them, and the document's own history
    // is left out of the reader's hands so the two can never disagree
    // about what "undo" reverses. `redo` says which of the two was asked.
    // Never called for a read-only or disabled editor.
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
    // Whether host highlights, rather than the profile, colour the text.
    bool has_highlights() const noexcept { return highlights_active_; }

    // Installs the context-menu handler, replacing any earlier one; see ContextMenuHandler.
    // Without one, a right click, the Menu key and Shift+F10 pass through unconsumed and a
    // Ctrl+click is an ordinary click.
    void set_context_menu_handler(ContextMenuHandler handler) { context_menu_handler_ = std::move(handler); }

    // The file name syntax detection reads; the standard detectors test its suffix. Setting it
    // detects the profile afresh, as set_profile(std::nullopt) does.
    void set_file_name(std::string name);
    // Chooses the syntax profile: `profile_id` when it names a registered profile, otherwise
    // the registry's best detection from the file name, the first 512 bytes of the text and
    // the first line within them, and plain text when nothing claims it. The editor keeps its
    // own copy of the chosen profile, so registering further profiles never disturbs it; they
    // take part in detection from the next call. A change of profile is reported to the status
    // handler and observers.
    void set_profile(std::optional<std::string> profile_id);
    // The active profile's id; "plain" until another is detected or chosen.
    const std::string& profile_id() const noexcept { return profile_.id; }
    // Rebuilds the editor's lines and their syntax colouring from the document with the
    // current profile. It requests no repaint by itself.
    void refresh_syntax();

    // Called with the new status after a change of the caret, the selection,
    // the document's text, its clean state or its format metadata (mark_clean,
    // set_preferred_newline, set_utf8_bom), the overwrite mode or the syntax
    // profile, on a focus change, and once when installed (which also calls
    // the status observers). The callback is application-owned; the editor
    // never assumes a window chrome arrangement.
    // It is suitable for a frame-overlay status readout.
    void set_status_changed_handler(std::function<void(const EditorStatus&)> handler);
    // A status observer is independent of window chrome. This supports a
    // reusable EditorStatusModel or any client-owned status presentation.
    // Observers are called whenever the status handler is, after it, in
    // subscription order over a snapshot of the list. Unsubscribing an
    // unknown id does nothing.
    StatusObserverId subscribe_status(StatusObserver observer);
    void unsubscribe_status(StatusObserverId observer) noexcept;

    // Search state belongs to this editor view, while matching and mutation
    // remain in the pure EditorSearch/EditorDocument layers. Matches are
    // revision-bound and paint below a primary selection.
    //
    // Sets the query and finds its matches at once, painting them in the search role, without
    // moving the caret or the selection; search_query() returns it. The matches are found
    // again after every document change.
    void set_search_query(EditorSearchQuery query);
    const EditorSearchQuery& search_query() const noexcept { return search_query_; }
    // Drops the query, its matches and the current match.
    void clear_search();
    // How many matches the query has in the current text.
    std::size_t search_match_count() const noexcept { return search_matches_.size(); }
    // Makes the selected text the query, case-sensitive and not whole-word. False, leaving the
    // query as it was, when nothing is selected; otherwise whether the query has any match.
    bool use_selection_as_search_query();
    // Selects a match and makes it the current match, scrolling it into view. The current
    // match follows its text through later changes and ends when a change touches it or it no
    // longer matches the query. Forward, it is
    // the first match that begins at or after the selection's end; backward, the last that ends
    // at or before the selection's start; without a selection, the caret stands for both.
    // Either direction wraps around the document. False when the query has no match.
    bool find_next(bool forward = true);
    // Asks for a Replace edit of the current match (the one find_next last selected) with
    // `replacement`, through the edit handler like any other edit (see EditHandler), and
    // clears the current match. False, changing nothing, when there is no current match, for a
    // read-only or disabled editor, or when the document refuses the edit.
    bool replace_current_search_match(std::string replacement);
    // Asks for a ReplaceAll edit: every match of the query replaced with `replacement` in one
    // transaction, so one revision and one undo step, through the edit handler like any other
    // edit. False, changing nothing, when the query has no match, for a read-only or disabled
    // editor, or when the document refuses the transaction.
    bool replace_all_search_matches(const std::string& replacement);

    // The clipboard verbs, through the attached Application's clipboard. Copy puts the selected
    // text there; false without an application or a selection. Cut asks for a Cut edit of the
    // selection (see EditHandler): false without a selection, for a read-only or disabled
    // editor, or, when the editor commits it itself, without an application. Paste inserts the
    // clipboard's text as a Paste edit, replacing any selection; false without an application,
    // with an empty clipboard, or when the edit is refused.
    bool copy_selection_to_clipboard();
    bool cut_selection_to_clipboard();
    bool paste_from_clipboard();

    void draw(scene::Painter& painter) override;
    // While enabled, consumes: the Menu key and Shift+F10 when a context-menu handler is set;
    // a chord in key_bindings() when its verb does something (see perform); printable text
    // without Ctrl, Alt or Super, and Tab without modifiers, which inserts tab_width() spaces
    // rather than moving focus, when the edit is made; the arrows, Home and End (with Ctrl, by word or to
    // the document's ends), PageUp and PageDown, Shift extending the selection; Backspace and
    // Delete (with Ctrl, a word) when they erase; and Enter, which inserts a line break.
    // Editing keys a read-only editor refuses pass on unconsumed. Key releases are ignored.
    bool on_key(const KeyEvent& event) override;
    // Inserts the event's text at the caret, as a Paste edit when it came from a terminal paste
    // and as typed text otherwise; true when the edit is made.
    bool on_text(const TextEvent& event) override;
    // While enabled: the wheel scrolls ui::kWheelRows display rows per notch; a left press
    // places the caret (Shift extends the selection) and dragging extends it, scrolling when
    // the pointer leaves the top or bottom; a double click (MouseEvent::click_count) selects the
    // word or grapheme under it, or places a virtual caret past the text when virtual space is
    // on; a right click or Ctrl+click goes to the context-menu handler when one is set. Other
    // buttons are not consumed.
    bool on_mouse(const MouseEvent& event) override;
    // Editable text.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Text;
    }
    // A focus change republishes the status (see set_status_changed_handler).
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    void on_resized() override;
    // Present only while focused and while the caret lies in the visible text area: a bar in
    // insert mode, a block in overwrite mode.
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
    bool activate_search_match(const DocumentRange& range);
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
    // Brings the caret and the selection anchor, already carried through a change, to the
    // current revision; one the change left inside a grapheme cluster moves to its start.
    void clamp_cursor();
    // Selects the bytes an undo or redo step hands back, when they are current positions.
    void restore_selection(DocumentSelection selection);
    void carry_highlights(const DocumentChange& change);
    // Carries the current search match through a change it does not touch, and ends it when
    // the change touches it.
    void carry_search_match(const DocumentChange& change);

    std::shared_ptr<EditorDocument> document_;
    SyntaxProfileRegistry* profiles_ = nullptr;
    SyntaxProfileRegistry fallback_profiles_;
    SyntaxCache syntax_cache_;
    LanguageProfile profile_;
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
    std::optional<DocumentRange> active_search_match_;
    EditorDocument::ObserverId observer_ = 0;
    EditorDocument::ObserverId state_observer_ = 0;
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
    // A subscription's identity, starting at 1 and never reused by a model, and its callback.
    using ObserverId = std::uint64_t;
    using Observer = std::function<void(const EditorStatus&)>;

    // Takes the editor's current status and subscribes to its updates. The editor must outlive
    // the model; the destructor unsubscribes.
    explicit EditorStatusModel(TextEditor& editor);
    ~EditorStatusModel();

    // Not copyable, since the editor's subscription refers to this object.
    EditorStatusModel(const EditorStatusModel&) = delete;
    EditorStatusModel& operator=(const EditorStatusModel&) = delete;

    // The status taken at construction until the editor publishes one, then the latest it
    // published.
    const EditorStatus& value() const noexcept { return value_; }
    // subscribe registers an observer called with every status the editor publishes after
    // this point (not with the current value), in subscription order over a snapshot of the
    // list; unsubscribe removes one and ignores an unknown id.
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
