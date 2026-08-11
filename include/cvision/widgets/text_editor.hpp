// Copyright (c) 2026 C. Klukas. All rights reserved.
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

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
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
    std::string profile_id = "plain";
    DocumentEncoding encoding = DocumentEncoding::Utf8;
    DocumentNewline newline = DocumentNewline::Lf;
};

class TextEditor final : public ui::View {
public:
    using StatusObserverId = std::uint64_t;
    using StatusObserver = std::function<void(const EditorStatus&)>;

    explicit TextEditor(std::shared_ptr<EditorDocument> document, SyntaxProfileRegistry* profiles = nullptr);
    ~TextEditor() override;

    const std::shared_ptr<EditorDocument>& document() const noexcept { return document_; }
    DocumentPosition cursor() const noexcept { return cursor_; }
    std::optional<DocumentRange> selection() const noexcept;
    EditorStatus status() const;

    void set_show_line_numbers(bool enabled);
    bool show_line_numbers() const noexcept { return show_line_numbers_; }
    // Wraps a logical line into viewport-width display rows. The document's
    // line/column model remains logical and therefore stable when this is
    // toggled; a visible reflow marker identifies each continued row.
    void set_word_wrap(bool enabled);
    bool word_wrap() const noexcept { return word_wrap_; }
    void set_read_only(bool value) noexcept { read_only_ = value; }
    bool read_only() const noexcept { return read_only_; }
    void set_overwrite(bool value);
    bool overwrite() const noexcept { return overwrite_; }

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
    void ensure_cursor_visible();
    void notify_status_changed();
    void refresh_search();
    bool activate_search_match(std::size_t index);
    void move_cursor(DocumentPosition target, bool extend);
    std::optional<DocumentPosition> position_for_screen_cell(Point absolute_cell) const;
    bool replace_selection(std::string text);
    bool erase_backward();
    bool erase_forward();
    bool erase_backward_word();
    bool erase_forward_word();
    std::size_t gutter_width() const;
    Style style_for(std::size_t line, std::size_t line_byte, bool selected) const;
    void clamp_cursor();

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
    EditorSearchQuery search_query_;
    std::vector<EditorSearchMatch> search_matches_;
    std::optional<std::size_t> active_search_match_;
    EditorDocument::ObserverId observer_ = 0;
    int top_display_row_ = 0;
    bool show_line_numbers_ = false;
    bool word_wrap_ = false;
    bool read_only_ = false;
    bool overwrite_ = false;
    bool has_focus_ = false;
    bool dragging_ = false;

    ui::RoleId text_role_ = ui::kInvalidRole;
    ui::RoleId gutter_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId search_role_ = ui::kInvalidRole;
    std::vector<ui::RoleId> syntax_roles_;
    std::function<void(const EditorStatus&)> status_changed_;
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
