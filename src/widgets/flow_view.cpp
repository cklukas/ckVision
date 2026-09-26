// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/flow_view.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

FlowView::FlowView() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void FlowView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->find("ckv.flow.text");
}

void FlowView::set_document(FlowDocument document) {
    document_ = std::move(document);
    current_link_.reset();
    invalidate_layout();
}

void FlowView::append_block(FlowBlock block) {
    document_.blocks.push_back(std::move(block));
    invalidate_layout();
}

bool FlowView::replace_block(std::size_t index, FlowBlock block) {
    if (index >= document_.blocks.size()) return false;
    document_.blocks[index] = std::move(block);
    current_link_.reset();
    const bool is_final_block = index + 1 == document_.blocks.size();
    const bool has_current_layout = layout_width_ == content_width() &&
                                    block_layout_offsets_.size() == document_.blocks.size();
    if (is_final_block && has_current_layout) {
        const BlockLayoutOffset offset = block_layout_offsets_.back();
        rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(offset.row_begin), rows_.end());
        images_.erase(images_.begin() + static_cast<std::ptrdiff_t>(offset.image_begin), images_.end());
        link_targets_.erase(link_targets_.begin() + static_cast<std::ptrdiff_t>(offset.link_begin), link_targets_.end());
        block_layout_offsets_.pop_back();
        append_block_layout(index, layout_width_);
        if (rows_.empty()) rows_.push_back(LayoutRow{});
        if (!link_targets_.empty()) current_link_ = 0;
        update_scrollbar_range();
        invalidate();
        return true;
    }
    invalidate_layout();
    return true;
}

void FlowView::invalidate_layout() {
    layout_width_ = -1;
    if (scrollbar_ != nullptr) on_resized();
    invalidate();
}

int FlowView::content_width() const noexcept { return std::max(0, bounds().width - 1); }

void FlowView::ensure_layout() const {
    const int width = content_width();
    if (layout_width_ == width) return;
    rebuild_layout(width);
}

void FlowView::rebuild_layout(int width) const {
    layout_width_ = width;
    rows_.clear();
    images_.clear();
    link_targets_.clear();
    block_layout_offsets_.clear();

    for (std::size_t block_index = 0; block_index < document_.blocks.size(); ++block_index)
        append_block_layout(block_index, width);
    if (rows_.empty()) rows_.push_back(LayoutRow{});
    if (current_link_ && *current_link_ >= link_targets_.size()) current_link_.reset();
    if (!current_link_ && !link_targets_.empty()) current_link_ = 0;
    update_scrollbar_range();
}

void FlowView::append_block_layout(std::size_t block_index, int width) const {
    block_layout_offsets_.push_back(BlockLayoutOffset{rows_.size(), images_.size(), link_targets_.size()});
    if (block_index != 0) rows_.push_back(LayoutRow{});

    LayoutRow current;
    int current_width = 0;
    bool current_open = false;
    const auto flush = [&]() {
        rows_.push_back(std::move(current));
        current = LayoutRow{};
        current_width = 0;
        current_open = false;
    };
    // One grapheme placed on the current row, merged into the last run when
    // it shares that run's attributes and link.
    struct Piece {
        std::string_view grapheme;
        Attr attrs;
        std::optional<std::size_t> link;
        int width;
    };
    const auto place = [&](const Piece& piece) {
        if (!current_open) current_open = true;
        if (!current.runs.empty() && current.runs.back().attrs == piece.attrs && current.runs.back().link == piece.link) {
            current.runs.back().text.append(piece.grapheme);
        } else {
            current.runs.push_back(LayoutRun{std::string(piece.grapheme), piece.attrs, piece.link});
        }
        current_width += piece.width;
    };
    // Rows break between words. A word is gathered whole -- across runs of
    // different attributes or links, since emphasis inside a word does not
    // make it two -- and moves to the next row when it does not fit, taking
    // nothing of the spaces before it: a separator that wraps belongs to
    // neither row. Only a word longer than a whole row is broken, grapheme
    // by grapheme, because there is no row it would fit on.
    std::vector<Piece> word;
    int word_width = 0;
    std::vector<Piece> spaces;
    int spaces_width = 0;
    const auto commit_word = [&]() {
        if (word.empty() || width <= 0) {
            word.clear();
            word_width = 0;
            return;
        }
        if (current_open && current_width + spaces_width + word_width > width) flush();
        if (current_open)
            for (const Piece& space : spaces) place(space);
        for (const Piece& piece : word) {
            if (current_open && current_width + piece.width > width) flush();
            place(piece);
        }
        word.clear();
        word_width = 0;
        spaces.clear();
        spaces_width = 0;
    };
    const auto end_row = [&]() {
        commit_word();
        spaces.clear();
        spaces_width = 0;
        flush();
    };
    const auto append_grapheme = [&](std::string_view grapheme, Attr attrs, std::optional<std::size_t> link) {
        const int grapheme_width = text::text_width(grapheme);
        if (grapheme_width <= 0 || width <= 0) return;
        if (grapheme == " ") {
            commit_word();
            // Leading blanks never start a visual row.
            if (current_open) {
                spaces.push_back(Piece{grapheme, attrs, link, grapheme_width});
                spaces_width += grapheme_width;
            }
            return;
        }
        word.push_back(Piece{grapheme, attrs, link, grapheme_width});
        word_width += grapheme_width;
    };

    for (const FlowInline& inline_item : document_.blocks[block_index].content) {
        std::visit(
            [&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, FlowText>) {
                    std::optional<std::size_t> link;
                    if (item.link_target) {
                        link = link_targets_.size();
                        link_targets_.push_back(*item.link_target);
                    }
                    for (std::string_view grapheme : text::split_graphemes(item.text)) {
                        if (grapheme == "\n") {
                            end_row();
                        } else {
                            append_grapheme(grapheme, item.attrs, link);
                        }
                    }
                } else if constexpr (std::is_same_v<T, FlowLineBreak>) {
                    end_row();
                } else {
                    commit_word();
                    if (current_open) flush();
                    const Size extent{std::max(1, item.cell_extent.width), std::max(1, item.cell_extent.height)};
                    images_.push_back(LayoutImage{static_cast<int>(rows_.size()), extent, item.image, item.fallback});
                    for (int row = 0; row < extent.height; ++row) rows_.push_back(LayoutRow{});
                }
            },
            inline_item);
    }
    commit_word();
    if (current_open) flush();
}

void FlowView::update_scrollbar_range() const {
    if (scrollbar_ != nullptr) {
        scrollbar_->set_range(static_cast<int>(rows_.size()), std::max(1, bounds().height));
    }
}

int FlowView::line_count() const {
    ensure_layout();
    return static_cast<int>(rows_.size());
}

int FlowView::top_line() const noexcept { return scrollbar_ != nullptr ? scrollbar_->position() : 0; }

std::size_t FlowView::link_count() const {
    ensure_layout();
    return link_targets_.size();
}

void FlowView::set_current_link(std::optional<std::size_t> index) {
    ensure_layout();
    if (index && *index >= link_targets_.size()) index.reset();
    if (current_link_ == index) return;
    current_link_ = index;
    invalidate();
}

bool FlowView::activate_current_link() {
    ensure_layout();
    if (!current_link_ || *current_link_ >= link_targets_.size()) return false;
    if (on_link_activate) on_link_activate(link_targets_[*current_link_]);
    return true;
}

void FlowView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    layout_width_ = -1;
    ensure_layout();
}

void FlowView::scroll_to(int position) {
    ensure_layout();
    if (scrollbar_ != nullptr) scrollbar_->set_position(position);
    invalidate();
}

bool FlowView::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    ensure_layout();
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
            scroll_to(top_line() - 1);
            return true;
        case Key::Down:
            scroll_to(top_line() + 1);
            return true;
        case Key::PageUp:
            scroll_to(top_line() - std::max(1, bounds().height));
            return true;
        case Key::PageDown:
            scroll_to(top_line() + std::max(1, bounds().height));
            return true;
        case Key::Home:
            scroll_to(0);
            return true;
        case Key::End:
            scroll_to(scrollbar_ != nullptr ? scrollbar_->max_position() : 0);
            return true;
        default:
            return false;
    }
}

std::optional<std::size_t> FlowView::link_at(int line, int column) const {
    if (line < 0 || static_cast<std::size_t>(line) >= rows_.size() || column < 0) return std::nullopt;
    int x = 0;
    for (const LayoutRun& run : rows_[static_cast<std::size_t>(line)].runs) {
        const int width = text::text_width(run.text);
        if (column >= x && column < x + width) return run.link;
        x += width;
    }
    return std::nullopt;
}

bool FlowView::on_mouse(const MouseEvent& event) {
    ensure_layout();
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        const Rect absolute = absolute_bounds();
        const int row = event.cell.y - absolute.y;
        if (row < 0 || row >= bounds().height) return false;
        const auto link = link_at(top_line() + row, event.cell.x - absolute.x);
        if (!link) return false;
        set_current_link(link);
        return activate_current_link();
    }
    const int rows = ui::wheel_scroll_rows(event);
    if (rows == 0) return false;
    scroll_to(top_line() + rows);
    return true;
}

void FlowView::on_focus(const FocusEvent&) { invalidate(); }

void FlowView::draw(scene::Painter& painter) {
    ensure_layout();
    const Style base = context().theme->resolve(text_role_);
    const int width = content_width();
    const int top = top_line();
    for (int visible_row = 0; visible_row < bounds().height; ++visible_row) {
        painter.fill(Rect{0, visible_row, width, 1}, Cell::from_grapheme(" ", base));
        const int document_row = top + visible_row;
        if (document_row < 0 || static_cast<std::size_t>(document_row) >= rows_.size()) continue;
        int x = 0;
        for (const LayoutRun& run : rows_[static_cast<std::size_t>(document_row)].runs) {
            Style style = base;
            style.attrs |= run.attrs;
            if (run.link) {
                style.attrs |= Attr::Underline;
                // Where Enter goes, which only means something while the
                // view holds the keyboard.
                if (current_link_ == run.link && has_focus()) style.attrs |= Attr::Reverse;
            }
            const std::string shown = text::clip_to_width(run.text, std::max(0, width - x));
            painter.draw_text(Point{x, visible_row}, shown, style);
            x += text::text_width(shown);
            if (x >= width) break;
        }
    }

    // A scrolled picture is drawn again at its new anchor, which may begin
    // above the first row shown, and the clip keeps what is visible (D-081):
    // the rows in view, and the columns left of the scrollbar. The picture is
    // never cut into a new image, so each of its rows keeps the pixels it had
    // before the scroll.
    scene::Painter content = painter.clipped(Rect{0, 0, width, bounds().height});
    for (const LayoutImage& image : images_) {
        const Rect anchor{0, image.top - top, image.cell_extent.width, image.cell_extent.height};
        if (anchor.y + anchor.height <= 0 || anchor.y >= bounds().height) continue;
        if (image.image == nullptr || image.image->empty()) {
            content.fill(anchor, Cell::from_grapheme(" ", base));
            if (!image.fallback.empty()) content.draw_text(Point{0, anchor.y}, image.fallback, base);
            continue;
        }
        content.draw_image(anchor, 0, image.image, [base, anchor, &image](scene::Painter& fallback) {
            fallback.fill(anchor, Cell::from_grapheme(" ", base));
            if (!image.fallback.empty()) fallback.draw_text(Point{0, anchor.y}, image.fallback, base);
        });
    }
}

}  // namespace ckv::widgets
