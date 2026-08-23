// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/static_text.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

StaticText::StaticText(std::string text) { set_text(std::move(text)); }

void StaticText::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.static.text");
}

void StaticText::set_text(std::string text) {
    raw_text_ = std::move(text);
    invalidate();
    size_hint_changed();
}

SizeHint StaticText::horizontal_size_hint() const {
    int widest = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = raw_text_.find('\n', start);
        widest = std::max(widest, text::text_width(
                                      std::string_view(raw_text_).substr(start, end == std::string::npos
                                                                                       ? std::string::npos
                                                                                       : end - start)));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    const int natural = std::max(1, widest);
    // What the text would be on one line per paragraph is the width
    // preformatted text needs, since it cannot be reflowed. Prose can be,
    // so it asks for the width it is read at instead: a paragraph that
    // requested its own unwrapped length made every box built around it as
    // wide as its longest sentence, which on a wide terminal is a ribbon of
    // text spanning the screen.
    const int preferred = preformatted_ ? natural : std::min(natural, kProseMeasureCells);
    return SizeHint{1, preferred, ui::kUnboundedExtent};
}

SizeHint StaticText::vertical_size_hint() const {
    const int lines = 1 + static_cast<int>(std::count(raw_text_.begin(), raw_text_.end(), '\n'));
    return SizeHint{1, std::max(1, lines), ui::kUnboundedExtent};
}

std::vector<std::string> StaticText::wrap(int width) const {
    std::vector<std::string> lines;
    const int w = std::max(width, 1);

    if (preformatted_) {
        // Verbatim: the caller has already decided where lines end and
        // what the spacing means. Painter clipping handles a line wider
        // than the view; re-flowing it here would silently rewrite it.
        std::size_t start = 0;
        while (true) {
            const std::size_t nl = raw_text_.find('\n', start);
            lines.push_back(raw_text_.substr(start, nl == std::string::npos ? std::string::npos
                                                                            : nl - start));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        return lines;
    }

    std::size_t para_start = 0;
    while (true) {
        const std::size_t nl = raw_text_.find('\n', para_start);
        const std::string_view para = (nl == std::string::npos)
                                           ? std::string_view(raw_text_).substr(para_start)
                                           : std::string_view(raw_text_).substr(para_start, nl - para_start);

        std::string current_line;
        int current_width = 0;
        std::size_t i = 0;
        while (true) {
            const std::size_t sp = para.find(' ', i);
            const std::string_view word = (sp == std::string_view::npos) ? para.substr(i) : para.substr(i, sp - i);
            if (!word.empty()) {
                const int word_width = text::text_width(word);
                if (word_width > w) {
                    if (!current_line.empty()) {
                        lines.push_back(current_line);
                        current_line.clear();
                        current_width = 0;
                    }
                    std::size_t pos = 0;
                    while (pos < word.size()) {
                        const std::size_t end = text::grapheme_end(word, pos);
                        const std::string_view g = word.substr(pos, end - pos);
                        const int gw = text::grapheme_width(g);
                        if (current_width + gw > w && !current_line.empty()) {
                            lines.push_back(current_line);
                            current_line.clear();
                            current_width = 0;
                        }
                        current_line.append(g);
                        current_width += gw;
                        pos = end;
                    }
                } else {
                    const int needed = current_line.empty() ? word_width : current_width + 1 + word_width;
                    if (needed > w && !current_line.empty()) {
                        lines.push_back(current_line);
                        current_line = std::string(word);
                        current_width = word_width;
                    } else {
                        if (!current_line.empty()) {
                            current_line.push_back(' ');
                            ++current_width;
                        }
                        current_line.append(word);
                        current_width += word_width;
                    }
                }
            }
            if (sp == std::string_view::npos) break;
            i = sp + 1;
        }
        lines.push_back(current_line);  // preserves a blank line for an empty paragraph

        if (nl == std::string::npos) break;
        para_start = nl + 1;
    }
    return lines;
}

int StaticText::height_for_width(int width) const { return static_cast<int>(wrap(width).size()); }

void StaticText::draw(scene::Painter& painter) {
    const std::vector<std::string> lines = wrap(bounds().width);
    const Style style = context().theme->resolve(role_);
    for (std::size_t row = 0; row < lines.size() && static_cast<int>(row) < bounds().height; ++row) {
        const int line_width = text::text_width(lines[row]);
        int x = 0;
        switch (alignment_) {
            case ui::Alignment::Start:
            case ui::Alignment::Fill:
                x = 0;
                break;
            case ui::Alignment::Center:
                x = std::max(0, (bounds().width - line_width) / 2);
                break;
            case ui::Alignment::End:
                x = std::max(0, bounds().width - line_width);
                break;
        }
        Style line_style = style;
        if (static_cast<int>(row) < emphasized_leading_lines_) line_style.attrs |= Attr::Bold;
        painter.draw_text(Point{x, static_cast<int>(row)}, lines[row], line_style);
    }
}

}  // namespace ckv::widgets
