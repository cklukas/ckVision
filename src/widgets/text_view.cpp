// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/text_view.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

namespace {
std::string without_controls(std::string_view text) {
    std::string out;
    for (unsigned char ch : text) {
        if (ch == '\n' || (ch >= 0x20 && ch != 0x7f)) out.push_back(static_cast<char>(ch));
    }
    return out;
}
}  // namespace

TextView::TextView() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void TextView::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    if (scrollbar_ != nullptr) scrollbar_->set_policy(policy);
    on_resized();
    invalidate();
}

void TextView::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_policy(policy);
    on_resized();
    invalidate();
}

ScrollbarPolicy TextView::vertical_scrollbar_policy() const noexcept {
    return scrollbar_ != nullptr ? scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

ScrollbarPolicy TextView::horizontal_scrollbar_policy() const noexcept {
    return h_scrollbar_ != nullptr ? h_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

int TextView::left_column() const noexcept { return h_scrollbar_ != nullptr ? h_scrollbar_->position() : 0; }

void TextView::set_word_wrap(bool enabled) {
    if (word_wrap_ == enabled) return;
    word_wrap_ = enabled;
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

int TextView::top_line() const noexcept { return scrollbar_ != nullptr ? scrollbar_->position() : 0; }

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

std::string TextView::osc8_text() const {
    if (spans_.empty()) return without_controls(raw_text_);
    std::string out;
    for (const auto& span : spans_) {
        if (span.link_target) {
            out += "\x1b]8;;";
            out += without_controls(*span.link_target);
            out += "\x1b\\";
            out += without_controls(span.text);
            out += "\x1b]8;;\x1b\\";
        } else {
            out += without_controls(span.text);
        }
    }
    return out;
}

void TextView::rebuild_display() {
    // One cell of laid-out text, kept flat while wrapping so a break can fall
    // anywhere without first having to split a styled run by hand — and so a
    // fragment never loses which link it belonged to.
    struct FlatCellLocal {
        std::string grapheme;
        int width = 1;
        Attr attrs = static_cast<Attr>(0);
        std::optional<std::size_t> link_index;
    };

    display_runs_.clear();
    content_width_ = 0;
    const int wrap_width = word_wrap_ ? std::max(1, viewport_width_) : 0;

    for (const std::vector<LineRun>& runs : line_runs_) {
        // Flatten to cells so a wrap point can be chosen by width and the
        // styling re-formed afterwards, rather than trying to cut runs in
        // place and losing which link a fragment belonged to.
        std::vector<FlatCellLocal> cells;
        for (const LineRun& run : runs) {
            for (const std::string_view grapheme : text::split_graphemes(run.text))
                cells.push_back(FlatCellLocal{std::string(grapheme),
                                              std::max(1, text::grapheme_width(grapheme)), run.attrs,
                                              run.link_index});
        }

        std::vector<std::pair<std::size_t, std::size_t>> segments;  // [begin, end)
        if (!word_wrap_) {
            segments.emplace_back(0, cells.size());
        } else {
            std::size_t begin = 0;
            while (begin < cells.size()) {
                std::size_t end = begin;
                int width = 0;
                std::size_t last_break = 0;  // one past the last space that fits
                while (end < cells.size() && width + cells[end].width <= wrap_width) {
                    width += cells[end].width;
                    ++end;
                    if (end < cells.size() && cells[end - 1].grapheme == " ") last_break = end;
                }
                if (end >= cells.size()) {
                    segments.emplace_back(begin, cells.size());
                    break;
                }
                if (last_break > begin) {
                    end = last_break;  // break at the space, keeping the word whole
                } else if (end == begin) {
                    // A single grapheme wider than the whole view: take it
                    // anyway, or nothing would ever be emitted.
                    end = begin + 1;
                } else {
                    // No space to break at: the word is wider than the view.
                    // It stays whole and overflows, which is what makes the
                    // horizontal bar appear rather than hiding the fact.
                    while (end < cells.size() && cells[end].grapheme != " ") ++end;
                }
                segments.emplace_back(begin, end);
                begin = end;
            }
            if (cells.empty()) segments.emplace_back(0, 0);
        }

        for (const auto& [begin, end] : segments) {
            std::vector<LineRun> row;
            int row_width = 0;
            for (std::size_t i = begin; i < end; ++i) {
                row_width += cells[i].width;
                if (!row.empty() && row.back().attrs == cells[i].attrs &&
                    row.back().link_index == cells[i].link_index) {
                    row.back().text += cells[i].grapheme;
                } else {
                    row.push_back(LineRun{cells[i].grapheme, cells[i].attrs, cells[i].link_index});
                }
            }
            content_width_ = std::max(content_width_, row_width);
            display_runs_.push_back(std::move(row));
        }
        if (segments.empty()) display_runs_.emplace_back();
    }
    if (display_runs_.empty()) display_runs_.emplace_back();
}

void TextView::relayout_scrollbars() {
    if (scrollbar_ == nullptr || h_scrollbar_ == nullptr) return;
    const int width = bounds().width;
    const int height = bounds().height;

    // Each bar's presence changes the room left for the other, and with word
    // wrap the room also changes how many display lines there are. Settle it
    // by repetition rather than by guessing an order: three passes is past
    // the point where anything still moves.
    bool show_v = false;
    bool show_h = false;
    for (int pass = 0; pass < 3; ++pass) {
        viewport_width_ = std::max(0, width - (show_v ? 1 : 0));
        rebuild_display();
        const int viewport_height = std::max(1, height - (show_h ? 1 : 0));
        scrollbar_->set_range(display_line_count(), viewport_height);
        h_scrollbar_->set_range(content_width_, std::max(1, viewport_width_));
        const bool next_v = vertical_scrollbar_visible_ && scrollbar_->should_show();
        const bool next_h = h_scrollbar_->should_show();
        if (next_v == show_v && next_h == show_h) break;
        show_v = next_v;
        show_h = next_h;
    }

    const int v_width = show_v ? std::min(1, width) : 0;
    const int h_height = show_h ? std::min(1, height) : 0;
    scrollbar_->set_bounds(Rect{std::max(0, width - v_width), 0, v_width, std::max(0, height - h_height)});
    h_scrollbar_->set_bounds(Rect{0, std::max(0, height - h_height), std::max(0, width - v_width), h_height});
    scrollbar_->set_visible(show_v);
    h_scrollbar_->set_visible(show_h);
    viewport_width_ = std::max(0, width - v_width);
    viewport_height_ = std::max(0, height - h_height);
}

void TextView::on_resized() { relayout_scrollbars(); }

bool TextView::on_key(const KeyEvent& event) {
    if (scrollbar_ == nullptr) return false;
    if (event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Tab && !link_targets_.empty()) {
        const int delta = has_modifier(event.chord.modifiers, Modifier::Shift) ? -1 : 1;
        const int count = static_cast<int>(link_targets_.size());
        const int current = current_link_ ? static_cast<int>(*current_link_) : (delta > 0 ? -1 : 0);
        set_current_link(static_cast<std::size_t>((current + delta + count) % count));
        return true;
    }
    if (event.chord.key == Key::Enter && activate_current_link()) return true;
    switch (event.chord.key) {
        case Key::Up:
            scrollbar_->set_position(scrollbar_->position() - 1);
            return true;
        case Key::Down:
            scrollbar_->set_position(scrollbar_->position() + 1);
            return true;
        case Key::PageUp:
            scrollbar_->set_position(scrollbar_->position() - std::max(1, viewport_height_));
            return true;
        case Key::PageDown:
            scrollbar_->set_position(scrollbar_->position() + std::max(1, viewport_height_));
            return true;
        case Key::Home:
            scrollbar_->set_position(0);
            return true;
        case Key::End:
            scrollbar_->set_position(scrollbar_->max_position());
            return true;
        default:
            return false;
    }
}

bool TextView::on_mouse(const MouseEvent& event) {
    if (scrollbar_ == nullptr) return false;
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        const Rect abs = absolute_bounds();
        const int row = event.cell.y - abs.y;
        if (row < 0 || row >= bounds().height) return false;
        const std::optional<std::size_t> link = link_at(top_line() + row, event.cell.x - abs.x);
        if (!link) return false;
        set_current_link(link);
        return activate_current_link();
    }
    if (event.action != MouseAction::Wheel) return false;
    if (event.button == MouseButton::WheelUp) {
        scrollbar_->set_position(scrollbar_->position() - 1);
        return true;
    }
    if (event.button == MouseButton::WheelDown) {
        scrollbar_->set_position(scrollbar_->position() + 1);
        return true;
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

void TextView::draw(scene::Painter& painter) {
    const Style base = context().theme->resolve(text_role_);
    const int visible_width = viewport_width_;
    const int visible_height = viewport_height_;
    const int top = top_line();
    const int left = left_column();
    for (int row = 0; row < visible_height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", base));
        if (index >= display_runs_.size()) continue;
        // `skipped` counts the cells scrolled off to the left, so a run that
        // straddles the left edge is entered part-way rather than dropped.
        int skipped = 0;
        int x = 0;
        for (const auto& run : display_runs_[index]) {
            Style style = base;
            style.attrs |= run.attrs;
            if (run.link_index) {
                style.attrs |= Attr::Underline;
                if (current_link_ == run.link_index) style.attrs |= Attr::Reverse;
            }
            std::string text_to_draw = run.text;
            const int run_width = text::text_width(text_to_draw);
            if (skipped + run_width <= left) {
                skipped += run_width;
                continue;  // entirely left of the viewport
            }
            if (skipped < left) {
                // Drop exactly the graphemes that lie left of the edge.
                std::string remainder;
                int dropped = skipped;
                for (const std::string_view grapheme : text::split_graphemes(text_to_draw)) {
                    const int w = std::max(1, text::grapheme_width(grapheme));
                    if (dropped + w <= left) {
                        dropped += w;
                        continue;
                    }
                    remainder.append(grapheme);
                }
                text_to_draw = std::move(remainder);
                skipped = left;
            }
            const std::string shown = text::clip_to_width(text_to_draw, std::max(0, visible_width - x));
            painter.draw_text(Point{x, row}, shown, style);
            x += text::text_width(shown);
            if (x >= visible_width) break;
        }
    }
}

}  // namespace ckv::widgets
