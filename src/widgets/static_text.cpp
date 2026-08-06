// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/static_text.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

StaticText::StaticText(std::string text, const ui::Theme& theme, ui::RoleId role)
    : theme_(theme), role_(role) {
    set_text(std::move(text));
}

void StaticText::set_text(std::string text) {
    raw_text_ = std::move(text);
    invalidate();
}

std::vector<std::string> StaticText::wrap(int width) const {
    std::vector<std::string> lines;
    const int w = std::max(width, 1);

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
    const Style style = theme_.resolve(role_);
    for (std::size_t row = 0; row < lines.size() && static_cast<int>(row) < bounds().height; ++row)
        painter.draw_text(Point{0, static_cast<int>(row)}, lines[row], style);
}

}  // namespace ckv::widgets
