// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_view.hpp"

#include "cvision/widgets/text_layout.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "widgets/frame_scrollbar_internal.hpp"

namespace ckv::widgets {

TextView::TextView() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    scrollbar_liveness_ = scrollbar_->lifetime_token();
    scrollbar_->bind_subject(this);
    scrollbar_->set_policy(ScrollbarPolicy::Auto);
    scrollbar_->on_change = [this](int p) { set_top_line(p); };
    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_liveness_ = h_scrollbar_->lifetime_token();
    h_scrollbar_->bind_subject(this);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_->on_change = [this](int p) { set_left_column(p); };
    set_focus_policy(ui::FocusPolicy::TabStop);
}

TextView::~TextView() {
    detail::sever_scrollbar(*this, vertical_bar());
    detail::sever_scrollbar(*this, horizontal_bar());
}

Scrollbar* TextView::vertical_bar() const noexcept {
    return scrollbar_ != nullptr && !scrollbar_liveness_.expired() ? scrollbar_ : nullptr;
}

Scrollbar* TextView::horizontal_bar() const noexcept {
    return h_scrollbar_ != nullptr && !h_scrollbar_liveness_.expired() ? h_scrollbar_ : nullptr;
}

bool TextView::vertical_bar_embedded() const noexcept {
    Scrollbar* const bar = vertical_bar();
    return bar != nullptr && bar->parent() == this;
}

bool TextView::horizontal_bar_embedded() const noexcept {
    Scrollbar* const bar = horizontal_bar();
    return bar != nullptr && bar->parent() == this;
}

std::unique_ptr<Scrollbar> TextView::take_vertical_scrollbar() {
    if (!vertical_bar_embedded()) return nullptr;
    v_policy_ = scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    on_resized();
    invalidate();
    return owned;
}

std::unique_ptr<Scrollbar> TextView::take_horizontal_scrollbar() {
    if (!horizontal_bar_embedded()) return nullptr;
    h_policy_ = h_scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(h_scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    on_resized();
    invalidate();
    return owned;
}

void TextView::on_detaching() {
    const bool v_detached = detail::detach_orphaned_scrollbar(*this, scrollbar_, scrollbar_liveness_);
    const bool h_detached = detail::detach_orphaned_scrollbar(*this, h_scrollbar_, h_scrollbar_liveness_);
    if (v_detached || h_detached) on_resized();
}

void TextView::on_effective_visibility_changed() {
    if (Scrollbar* v = vertical_bar())
        v->set_visible(show_vertical_ && detail::subject_visible_for_bar(*this, *v));
    if (Scrollbar* h = horizontal_bar())
        h->set_visible(show_horizontal_ && detail::subject_visible_for_bar(*this, *h));
}

void TextView::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    v_policy_ = policy;
    if (Scrollbar* v = vertical_bar()) v->set_policy(policy);
    on_resized();
    invalidate();
}

void TextView::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    h_policy_ = policy;
    if (Scrollbar* h = horizontal_bar()) h->set_policy(policy);
    on_resized();
    invalidate();
}

ScrollbarPolicy TextView::vertical_scrollbar_policy() const noexcept {
    Scrollbar* const v = vertical_bar();
    return v != nullptr ? v->policy() : v_policy_;
}

ScrollbarPolicy TextView::horizontal_scrollbar_policy() const noexcept {
    Scrollbar* const h = horizontal_bar();
    return h != nullptr ? h->policy() : h_policy_;
}

int TextView::left_column() const noexcept { return left_column_; }

void TextView::set_wrap_mode(WrapMode mode) {
    if (wrap_mode_ == mode) return;
    wrap_mode_ = mode;
    on_resized();
    invalidate();
}

void TextView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->find("ckv.textview.text");
}

void TextView::set_vertical_scrollbar_visible(bool visible) noexcept {
    if (vertical_scrollbar_visible_ == visible) return;
    vertical_scrollbar_visible_ = visible;
    on_resized();
    invalidate();
}

void TextView::split_lines() {
    lines_.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = raw_text_.find('\n', start);
        if (nl == std::string::npos) {
            lines_.push_back(raw_text_.substr(start));
            break;
        }
        lines_.push_back(raw_text_.substr(start, nl - start));
        start = nl + 1;
    }
    line_runs_.clear();
    line_runs_.reserve(lines_.size());
    for (const auto& line : lines_) line_runs_.push_back({LineRun{line, static_cast<Attr>(0), std::nullopt}});
}

void TextView::set_text(std::string text) {
    raw_text_ = std::move(text);
    spans_.clear();
    link_targets_.clear();
    current_link_.reset();
    split_lines();
    on_resized();
    invalidate();
}

void TextView::set_spans(std::vector<TextSpan> spans) {
    spans_ = std::move(spans);
    rebuild_from_spans();
    on_resized();
    invalidate();
}

void TextView::rebuild_from_spans() {
    raw_text_.clear();
    lines_.clear();
    line_runs_.clear();
    link_targets_.clear();
    current_link_.reset();
    lines_.emplace_back();
    line_runs_.emplace_back();

    for (const auto& span : spans_) {
        std::optional<std::size_t> link_index;
        if (span.link_target) {
            link_index = link_targets_.size();
            link_targets_.push_back(*span.link_target);
            if (!current_link_) current_link_ = link_index;
        }

        std::size_t start = 0;
        while (true) {
            const std::size_t nl = span.text.find('\n', start);
            const std::string piece =
                nl == std::string::npos ? span.text.substr(start) : span.text.substr(start, nl - start);
            raw_text_ += piece;
            lines_.back() += piece;
            if (!piece.empty()) line_runs_.back().push_back(LineRun{piece, span.attrs, link_index});
            if (nl == std::string::npos) break;
            raw_text_ += '\n';
            lines_.emplace_back();
            line_runs_.emplace_back();
            start = nl + 1;
        }
    }
    if (lines_.empty()) {
        lines_.emplace_back();
        line_runs_.emplace_back();
    }
}

int TextView::top_line() const noexcept { return top_line_; }

void TextView::set_current_link(std::optional<std::size_t> index) {
    if (index && *index >= link_targets_.size()) index.reset();
    if (current_link_ == index) return;
    current_link_ = index;
    invalidate();
}

bool TextView::activate_current_link() {
    if (!current_link_ || *current_link_ >= link_targets_.size()) return false;
    if (on_link_activate) on_link_activate(link_targets_[*current_link_]);
    return true;
}

void TextView::rebuild_display() {
    display_runs_.clear();
    content_width_ = 0;

    for (const std::vector<LineRun>& runs : line_runs_) {
        // Flattened to clusters so a break can fall anywhere without first
        // having to split a styled run by hand — and so a fragment never
        // loses which link it belonged to.
        std::vector<std::string> graphemes;
        std::vector<const LineRun*> owners;
        for (const LineRun& run : runs) {
            for (const std::string_view grapheme : text::split_graphemes(run.text)) {
                graphemes.emplace_back(grapheme);
                owners.push_back(&run);
            }
        }

        const std::vector<WrapSegment> segments =
            wrap_graphemes(graphemes, WrapOptions{viewport_width_, wrap_mode_, 0});
        for (const WrapSegment& segment : segments) {
            std::vector<LineRun> row;
            int row_width = 0;
            for (std::size_t i = segment.begin; i < segment.end && i < graphemes.size(); ++i) {
                row_width += std::max(1, text::grapheme_width(graphemes[i]));
                if (!row.empty() && row.back().attrs == owners[i]->attrs &&
                    row.back().link_index == owners[i]->link_index) {
                    row.back().text += graphemes[i];
                } else {
                    row.push_back(LineRun{graphemes[i], owners[i]->attrs, owners[i]->link_index});
                }
            }
            content_width_ = std::max(content_width_, row_width);
            display_runs_.push_back(std::move(row));
        }
    }
    if (display_runs_.empty()) display_runs_.emplace_back();
}

void TextView::relayout_scrollbars() {
    Scrollbar* const v = vertical_bar();
    Scrollbar* const h = horizontal_bar();
    const bool v_embedded = vertical_bar_embedded();
    const bool h_embedded = horizontal_bar_embedded();
    v_policy_ = vertical_scrollbar_policy();
    h_policy_ = horizontal_scrollbar_policy();

    // Both bars settled together by the shared resolver: each embedded bar's
    // presence changes the room left for the other, and with wrap the width
    // also decides how many display rows there are.
    const ScrollGeometry geometry = resolve_scroll_geometry(
        Size{bounds().width, bounds().height},
        vertical_scrollbar_visible_ ? v_policy_ : ScrollbarPolicy::Hidden,
        h_policy_,
        [this](int viewport_width) {
            viewport_width_ = viewport_width;
            rebuild_display();
            return Size{content_width_, display_line_count()};
        },
        v_embedded, h_embedded);

    viewport_width_ = geometry.viewport_width;
    viewport_height_ = geometry.viewport_height;
    show_vertical_ = geometry.show_vertical;
    show_horizontal_ = geometry.show_horizontal;
    rebuild_display();  // final width, so what is drawn is what was measured

    const int max_top = std::max(0, display_line_count() - std::max(1, viewport_height_));
    const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
    top_line_ = std::clamp(top_line_, 0, max_top);
    left_column_ = std::clamp(left_column_, 0, max_left);

    const int v_width = (v_embedded && geometry.show_vertical) ? std::min(1, bounds().width) : 0;
    const int h_height = (h_embedded && geometry.show_horizontal) ? std::min(1, bounds().height) : 0;
    if (v != nullptr) {
        v->set_range(display_line_count(), std::max(1, geometry.viewport_height));
        v->set_position(top_line_);
        if (v_embedded)
            v->set_bounds(Rect{std::max(0, bounds().width - v_width), 0, v_width,
                               std::max(0, bounds().height - h_height)});
        v->set_visible(geometry.show_vertical && detail::subject_visible_for_bar(*this, *v));
    }
    if (h != nullptr) {
        h->set_range(content_width_, std::max(1, geometry.viewport_width));
        h->set_position(left_column_);
        if (h_embedded)
            h->set_bounds(Rect{0, std::max(0, bounds().height - h_height),
                               std::max(0, bounds().width - v_width), h_height});
        h->set_visible(geometry.show_horizontal && detail::subject_visible_for_bar(*this, *h));
    }
}

void TextView::on_resized() { relayout_scrollbars(); }

bool TextView::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Tab && !link_targets_.empty()) {
        // Tab walks the links and then lets go. Past the last link (or before
        // the first, going back) the key is left for focus traversal, so the
        // links are stops on the way through the view rather than a ring the
        // keyboard cannot leave: a reader who Tabs into a page of links must
        // still be able to Tab on to the buttons below it.
        const int delta = has_modifier(event.chord.modifiers, Modifier::Shift) ? -1 : 1;
        const int count = static_cast<int>(link_targets_.size());
        const int current = current_link_ ? static_cast<int>(*current_link_) : (delta > 0 ? -1 : count);
        const int next = current + delta;
        if (next < 0 || next >= count) return false;
        set_current_link(static_cast<std::size_t>(next));
        return true;
    }
    if (event.chord.key == Key::Enter && activate_current_link()) return true;
    const int max_top = std::max(0, display_line_count() - std::max(1, viewport_height_));
    const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
    switch (event.chord.key) {
        case Key::Up:
            set_top_line(top_line_ - 1);
            return true;
        case Key::Down:
            set_top_line(top_line_ + 1);
            return true;
        case Key::PageUp:
            set_top_line(top_line_ - std::max(1, viewport_height_));
            return true;
        case Key::PageDown:
            set_top_line(top_line_ + std::max(1, viewport_height_));
            return true;
        case Key::Home:
            set_top_line(0);
            return true;
        case Key::End:
            set_top_line(max_top);
            return true;
        case Key::Left:
            if (max_left <= 0) return false;
            set_left_column(left_column_ - 1);
            return true;
        case Key::Right:
            if (max_left <= 0) return false;
            set_left_column(left_column_ + 1);
            return true;
        default:
            return false;
    }
}

bool TextView::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) return false;
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        const Rect abs = absolute_bounds();
        const int row = event.cell.y - abs.y;
        if (row < 0 || row >= bounds().height) return false;
        const int column = event.cell.x - abs.x;
        if (column < 0 || column >= viewport_width_) return false;
        // The line is drawn from left_column() on, so that is where a column
        // of the view lands in it.
        const std::optional<std::size_t> link = link_at(top_line() + row, left_column() + column);
        if (!link) return false;
        set_current_link(link);
        return activate_current_link();
    }
    const int rows = ui::wheel_scroll_rows(event);
    if (rows != 0) {
        set_top_line(top_line_ + rows);
        return true;
    }
    const int cols = ui::wheel_scroll_cols(event);
    if (cols != 0) {
        const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
        if (max_left > 0) {
            set_left_column(left_column_ + cols);
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> TextView::link_at(int line, int column) const {
    if (line < 0 || static_cast<std::size_t>(line) >= display_runs_.size() || column < 0) return std::nullopt;
    int x = 0;
    for (const auto& run : display_runs_[static_cast<std::size_t>(line)]) {
        const int width = text::text_width(run.text);
        if (column >= x && column < x + width) return run.link_index;
        x += width;
    }
    return std::nullopt;
}

void TextView::on_focus(const FocusEvent& event) {
    // Leaving puts the walk back at its start, so coming back by Tab begins at
    // the first link again instead of on the last one -- from where the very
    // next Tab would leave.
    if (!event.gained && !link_targets_.empty()) current_link_ = 0;
    invalidate();
}

void TextView::set_top_line(int line) {
    const int max_top = std::max(0, display_line_count() - std::max(1, viewport_height_));
    const int clamped = std::clamp(line, 0, max_top);
    const bool changed = clamped != top_line_;
    top_line_ = clamped;
    if (Scrollbar* v = vertical_bar()) v->set_position(top_line_);
    if (changed) invalidate();
}

void TextView::set_left_column(int column) {
    const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
    const int clamped = std::clamp(column, 0, max_left);
    const bool changed = clamped != left_column_;
    left_column_ = clamped;
    if (Scrollbar* h = horizontal_bar()) h->set_position(left_column_);
    if (changed) invalidate();
}

void TextView::draw(scene::Painter& painter) {
    const Style base = context().theme->resolve(text_role_);
    const int visible_width = viewport_width_;
    const int visible_height = viewport_height_;
    const int top = top_line();
    const int left = left_column();
    // The text is drawn in the line's own columns less the scroll offset,
    // through a painter clipped to the viewport: a run that straddles the
    // left edge is entered part-way, a wide glyph the edge cuts in half shows
    // as a blank in its style, and every glyph after it keeps its column.
    scene::Painter viewport = painter.clipped(Rect{0, 0, visible_width, visible_height});
    for (int row = 0; row < visible_height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        viewport.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", base));
        if (index >= display_runs_.size()) continue;
        int x = -left;
        for (const auto& run : display_runs_[index]) {
            Style style = base;
            style.attrs |= run.attrs;
            if (run.link_index) {
                style.attrs |= Attr::Underline;
                // The current link is where Enter goes, which only means
                // something while the view holds the keyboard.
                if (current_link_ == run.link_index && has_focus()) style.attrs |= Attr::Reverse;
            }
            const int run_width = text::text_width(run.text);
            if (x + run_width > 0) {
                // The target goes with the cells, so a terminal that renders
                // hyperlinks makes the span clickable itself; one that is not a
                // terminal hyperlink is dropped there and stays the view's own.
                const std::string_view target =
                    run.link_index ? std::string_view(link_targets_[*run.link_index]) : std::string_view{};
                viewport.draw_text(Point{x, row}, run.text, style, target);
            }
            x += run_width;
            if (x >= visible_width) break;
        }
    }
}

}  // namespace ckv::widgets
