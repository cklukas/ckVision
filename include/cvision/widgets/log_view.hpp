// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/widgets/log_model.hpp"
#include "cvision/widgets/log_literal.hpp"
#include "cvision/widgets/text_layout.hpp"

namespace ckv::widgets {

// Stable record plus UTF-8 byte position, normalized to a display grapheme.
struct LogPosition {
    // Record identity, byte offset and value comparison.
    LogRecordId id;
    std::size_t byte = 0;
    friend bool operator==(const LogPosition&, const LogPosition&) = default;
};
// Anchor/cursor endpoints; direction does not alter the selected content.
struct LogSelection {
    // Normalized endpoints and value comparison for coherent selection changes.
    LogPosition anchor, cursor;
    friend bool operator==(const LogSelection&, const LogSelection&) = default;
};
// Live/inspect position, unread admissions, unavailable anchors and workspace limits.
struct LogViewState {
    // Follow flag, unread count, eviction/selection notices, exceeded budgets
    // and cumulative provider evictions; value comparison suppresses repeats.
    bool following = true;
    std::uint64_t unread = 0;
    bool anchor_unavailable = false;
    bool selection_unavailable = false;
    bool viewport_exceeded = false;
    bool record_exceeded = false;
    std::uint64_t evicted = 0;
    friend bool operator==(const LogViewState&, const LogViewState&) = default;
};
// Bounded copy outcome and owned safe display text; no raw control forwarding.
enum class LogCopyState { Complete, Truncated, Unavailable };
struct LogCopy {
    // Owned safe selected bytes and explicit completion/truncation/unavailability.
    std::string text;
    LogCopyState state = LogCopyState::Unavailable;
};

// UI-thread view borrowing its provider for its lifetime. Call refresh after
// publication, once per application drain/frame deadline, never per input byte.
// draw also reconciles a missed refresh. No full-history layout; the vertical
// track navigates records, while keys/wheel navigate display rows. Stable tracks
// reserve one column/row where space permits. Horizontal extent grows from the
// encountered records, never a scan of history. Single-record wrap scratch and
// viewport rows are preallocated; copy is an explicit bounded allocation.
class LogView : public ui::View {
public:
    // Borrow provider and reserve bounded record/viewport/layout/highlight storage.
    explicit LogView(const LogRecordProvider& provider, std::size_t max_viewport_rows = 4096);
    // Reconcile a coherent provider publication without laying out full history.
    void refresh();
    // Pause presentation only; resume returns to the tail and clears unread.
    void pause();
    void resume();
    // Clear eviction/selection notices after the application has shown them.
    void acknowledge_notices();
    // Change/read wrapping policy; row and horizontal navigation remain bounded.
    void set_wrap_mode(WrapMode mode);
    WrapMode wrap_mode() const noexcept { return wrap_; }
    void scroll_rows(int delta);
    void set_top_record(std::size_t index);
    // Borrow the stable top position, horizontal offset and current view state.
    std::optional<LogPosition> top() const noexcept { return top_; }
    void set_left_column(int column);
    int left_column() const noexcept { return left_; }
    const LogViewState& state() const noexcept { return state_; }
    // Normalize retained endpoints; invalid/evicted identities cannot alias.
    bool set_selection(LogSelection selection);
    void clear_selection();
    const std::optional<LogSelection>& selection() const noexcept { return selection_; }
    // Allocate at most max_bytes safe selected text; report truncation explicitly.
    LogCopy selected_text(std::size_t max_bytes = 1024 * 1024) const;
    // Copy through the injected clipboard, reporting result via on_copy_result.
    bool copy_selection();
    // Select/reveal a literal result, expanding byte offsets to whole touched
    // graphemes. One bounded record is inspected; no history is traversed.
    bool reveal(LogRecordId id, LogTextRange range);
    // Borrow a bounded record-window highlighter for the enclosing view life.
    void set_highlighter(const LogHighlighter* highlighter) noexcept {
        highlighter_ = highlighter; invalidate();
    }
    // Separate from clearing retention: hide current admissions in this view.
    // Later admissions remain eligible. reset_view restores retained entries.
    void clear_view();
    void reset_view();
    // Object plus reserved wrap/viewport/highlight capacities, excluding provider.
    std::size_t allocated_bytes() const noexcept;
    void on_resized() override;
    void on_attached() override;
    void on_focus(const FocusEvent&) override { invalidate(); }
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    std::optional<PointerShape> pointer_shape_at(Point) const override { return PointerShape::Text; }
    // Called after coherent state changes. May observe state, but must not
    // destroy the view or mutate its provider while refresh/draw is executing.
    std::function<void(const LogViewState&)> on_state_changed;
    std::function<void(LogCopyState)> on_copy_result;

private:
    struct Row { LogPosition start; std::size_t end = 0; };
    bool eligible(const LogRecord& record) const noexcept;
    std::optional<std::size_t> first_index() const noexcept;
    bool normalize(LogPosition& position) const noexcept;
    bool layout(const LogRecord& record);
    std::optional<Row> row_at(LogPosition position);
    std::optional<LogPosition> next_row(LogPosition position);
    std::optional<LogPosition> previous_row(LogPosition position);
    std::optional<LogPosition> last_row();
    void rebuild();
    void publish_state();
    std::optional<LogPosition> hit(Point local) const;
    std::optional<LogPosition> row_column(const Row& row, int column) const;
    bool extend_selection(Key key);
    void reconcile_selection();
    bool selected(LogRecordId id, std::size_t byte) const noexcept;

    const LogRecordProvider& provider_;
    std::size_t text_limit_, row_limit_;
    std::vector<WrapSegment> layout_;
    std::vector<Row> rows_;
    std::vector<unsigned char> highlight_bytes_;
    const LogHighlighter* highlighter_ = nullptr;
    std::optional<LogRecordId> layout_id_;
    std::uint64_t layout_revision_ = 0;
    int layout_width_ = 0;
    WrapMode layout_mode_ = WrapMode::None;
    std::optional<LogPosition> top_;
    std::optional<LogSelection> selection_;
    std::uint64_t seen_revision_ = 0, seen_sequence_ = 0, hidden_through_ = 0;
    bool dirty_ = true, syncing_bars_ = false, dragging_ = false;
    int width_ = 0, height_ = 0, left_ = 0, encountered_width_ = 0;
    WrapMode wrap_ = WrapMode::None;
    LogViewState state_, published_state_;
    Scrollbar *vertical_ = nullptr, *horizontal_ = nullptr;
    ui::RoleId text_role_ = ui::kInvalidRole, selection_role_ = ui::kInvalidRole;
    ui::RoleId inactive_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
