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
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void TextView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->find("ckv.textview.text");
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

void TextView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    scrollbar_->set_range(static_cast<int>(lines_.size()), std::max(1, bounds().height));
}

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
            scrollbar_->set_position(scrollbar_->position() - std::max(1, bounds().height));
            return true;
        case Key::PageDown:
            scrollbar_->set_position(scrollbar_->position() + std::max(1, bounds().height));
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
    if (line < 0 || static_cast<std::size_t>(line) >= line_runs_.size() || column < 0) return std::nullopt;
    int x = 0;
    for (const auto& run : line_runs_[static_cast<std::size_t>(line)]) {
        const int width = text::text_width(run.text);
        if (column >= x && column < x + width) return run.link_index;
        x += width;
    }
    return std::nullopt;
}

void TextView::draw(scene::Painter& painter) {
    const Style base = context().theme->resolve(text_role_);
    const int visible_width = std::max(0, bounds().width - 1);
    const int top = top_line();
    for (int row = 0; row < bounds().height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", base));
        if (index >= line_runs_.size()) continue;
        int x = 0;
        for (const auto& run : line_runs_[index]) {
            Style style = base;
            style.attrs |= run.attrs;
            if (run.link_index) {
                style.attrs |= Attr::Underline;
                if (current_link_ == run.link_index) style.attrs |= Attr::Reverse;
            }
            const std::string shown = text::clip_to_width(run.text, std::max(0, visible_width - x));
            painter.draw_text(Point{x, row}, shown, style);
            x += text::text_width(shown);
            if (x >= visible_width) break;
        }
    }
}

}  // namespace ckv::widgets
