// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/big_clock.hpp"

#include <algorithm>
#include <array>
#include <memory>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {
namespace {

using GlyphRows = std::array<std::string_view, kBigGlyphHeight>;

// Seven segments in a 5 x 5 cell box: a bar across the top, middle and bottom
// row, and an upright down each side of each half. Every digit but one lights
// a subset of those seven. The exception is the 1: its segments are the two
// right-hand uprights, which leave four empty columns in front of it, so "19"
// reads as "1 9" and a clock at 18:19 reads as "18: 19". It is drawn as a
// centred stem with a flag and a foot instead, which fills its cell the way
// the other digits fill theirs.
constexpr std::array<GlyphRows, 10> kDigits{{
    {"#####", "#   #", "#   #", "#   #", "#####"},  // 0
    {" ##  ", "  #  ", "  #  ", "  #  ", " ### "},  // 1
    {"#####", "    #", "#####", "#    ", "#####"},  // 2
    {"#####", "    #", "#####", "    #", "#####"},  // 3
    {"#   #", "#   #", "#####", "    #", "    #"},  // 4
    {"#####", "#    ", "#####", "    #", "#####"},  // 5
    {"#####", "#    ", "#####", "#   #", "#####"},  // 6
    {"#####", "    #", "    #", "    #", "    #"},  // 7
    {"#####", "#   #", "#####", "#   #", "#####"},  // 8
    {"#####", "#   #", "#####", "    #", "#####"},  // 9
}};
constexpr GlyphRows kColon{" ", "#", " ", "#", " "};
constexpr GlyphRows kDash{"   ", "   ", "###", "   ", "   "};
constexpr GlyphRows kDot{" ", " ", " ", " ", "#"};
constexpr GlyphRows kSlash{"    #", "   # ", "  #  ", " #   ", "#    "};
constexpr GlyphRows kSpace{"  ", "  ", "  ", "  ", "  "};
// What a character with no glyph is drawn as: a digit's width of nothing.
constexpr GlyphRows kMissing{"     ", "     ", "     ", "     ", "     "};

const GlyphRows& glyph_for(char c) noexcept {
    if (c >= '0' && c <= '9') return kDigits[static_cast<std::size_t>(c - '0')];
    switch (c) {
        case ':': return kColon;
        case '-': return kDash;
        case '.': return kDot;
        case '/': return kSlash;
        case ' ': return kSpace;
        default: return kMissing;
    }
}

std::string two_digit(int value) {
    value = std::clamp(value, 0, 99);
    return std::string{static_cast<char>('0' + value / 10), static_cast<char>('0' + value % 10)};
}

// A blank row between stacked blocks, and between the time and its caption.
constexpr int kBlockGap = 1;

}  // namespace

bool has_big_glyph(char c) noexcept { return &glyph_for(c) != &kMissing; }

int big_glyph_width(std::string_view text) noexcept {
    int width = 0;
    for (const char c : text) width += static_cast<int>(glyph_for(c)[0].size());
    // One blank column between glyphs, none after the last.
    if (!text.empty()) width += static_cast<int>(text.size()) - 1;
    return width;
}

std::vector<std::string> big_glyph_rows(std::string_view text) {
    std::vector<std::string> rows(kBigGlyphHeight);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const GlyphRows& glyph = glyph_for(text[i]);
        for (int row = 0; row < kBigGlyphHeight; ++row) {
            if (i > 0) rows[static_cast<std::size_t>(row)] += ' ';
            rows[static_cast<std::size_t>(row)] += glyph[static_cast<std::size_t>(row)];
        }
    }
    return rows;
}

BigClockView::BigClockView() { set_focus_policy(ui::FocusPolicy::TabStop); }

void BigClockView::set_moment_provider(std::function<DateTimeValue()> provider) {
    moment_provider_ = std::move(provider);
    refresh();
    ensure_ticking();
}

void BigClockView::set_content(BigClockContent content) {
    if (content_ == content) return;
    content_ = content;
    refresh();
}

void BigClockView::set_show_seconds(bool show) {
    if (show_seconds_ == show) return;
    show_seconds_ = show;
    refresh();
}

void BigClockView::set_hour_format(HourFormat format) {
    if (hour_format_ == format) return;
    hour_format_ = format;
    refresh();
}

void BigClockView::set_meridiem_labels(std::string am, std::string pm) {
    am_label_ = std::move(am);
    pm_label_ = std::move(pm);
    refresh();
}

void BigClockView::refresh() {
    std::vector<std::string> lines;
    std::string caption;
    if (moment_provider_) {
        const DateTimeValue now = moment_provider_();
        if (content_ != BigClockContent::Time) lines.push_back(format_iso_date(now.date));
        if (content_ != BigClockContent::Date) {
            int hour = std::clamp(now.time.hour, 0, 23);
            std::string time;
            if (hour_format_ == HourFormat::TwelveHour) {
                caption = hour < 12 ? am_label_ : pm_label_;
                hour %= 12;
                if (hour == 0) hour = 12;  // midnight and noon read as twelve, not zero
                time = std::to_string(hour);
            } else {
                time = two_digit(hour);
            }
            time += ":" + two_digit(std::clamp(now.time.minute, 0, 59));
            if (show_seconds_) time += ":" + two_digit(std::clamp(now.time.second, 0, 59));
            lines.push_back(std::move(time));
        }
    }
    // Re-read every tick, repainted only on a difference: a face without
    // seconds repaints once a minute, not once a second.
    if (lines == lines_ && caption == caption_) return;
    lines_ = std::move(lines);
    caption_ = std::move(caption);
    invalidate();
}

void BigClockView::ensure_ticking() {
    if (ticking_ || context().app == nullptr || !moment_provider_) return;
    ticking_ = true;
    ui::Application* const app = context().app;
    const std::weak_ptr<void> liveness = lifetime_token();
    auto id = std::make_shared<ui::Application::TimerId>(0);
    *id = app->start_timer(1'000'000'000, /*repeating=*/true, [this, liveness, id, app] {
        // Nothing cancels a repeating timer for a view that has gone except
        // the timer itself.
        if (liveness.expired()) {
            app->cancel_timer(*id);
            return;
        }
        refresh();
    });
}

int BigClockView::large_height() const noexcept {
    if (lines_.empty()) return 0;
    const int count = static_cast<int>(lines_.size());
    int height = count * kBigGlyphHeight + (count - 1) * kBlockGap;
    if (!caption_.empty()) height += kBlockGap + 1;
    return height;
}

int BigClockView::large_width() const noexcept {
    int width = text::text_width(caption_);
    for (const std::string& line : lines_) width = std::max(width, big_glyph_width(line));
    return width;
}

bool BigClockView::drawn_large() const noexcept {
    return !lines_.empty() && large_width() <= bounds().width && large_height() <= bounds().height;
}

void BigClockView::on_attached() {
    if (face_role_ == ui::kInvalidRole) face_role_ = context().roles->find("ckv.textview.text");
    // The progress bar's fill: the theme's one "lit" colour for a surface
    // made of filled cells.
    if (lit_role_ == ui::kInvalidRole) lit_role_ = context().roles->find("ckv.menu.bar.active");
    refresh();
    ensure_ticking();
}

void BigClockView::draw(scene::Painter& painter) {
    const int width = bounds().width;
    const int height = bounds().height;
    if (width <= 0 || height <= 0) return;
    const Style face = context().theme->resolve(face_role_);
    painter.fill(Rect{0, 0, width, height}, Cell::from_grapheme(" ", face));
    if (lines_.empty()) return;

    const auto centred = [width](int extent) { return std::max(0, (width - extent) / 2); };

    if (!drawn_large()) {
        // The same lines, as text: a small window still tells the time. The
        // caption joins the time it belongs to rather than taking a row.
        std::vector<std::string> plain = lines_;
        if (!caption_.empty()) plain.back() += " " + caption_;
        const int count = static_cast<int>(plain.size());
        int y = std::max(0, (height - count) / 2);
        for (const std::string& line : plain) {
            if (y >= height) break;
            const std::string shown = text::clip_to_width(line, width);
            painter.draw_text(Point{centred(text::text_width(shown)), y}, shown, face);
            ++y;
        }
        return;
    }

    const Style lit = context().theme->resolve(lit_role_);
    const Cell lit_cell = Cell::from_grapheme(" ", Style{lit.bg, lit.bg, Attr{}});
    int y = (height - large_height()) / 2;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i > 0) y += kBlockGap;
        const std::vector<std::string> rows = big_glyph_rows(lines_[i]);
        const int x0 = centred(big_glyph_width(lines_[i]));
        for (const std::string& row : rows) {
            for (std::size_t x = 0; x < row.size(); ++x)
                if (row[x] == '#') painter.fill(Rect{x0 + static_cast<int>(x), y, 1, 1}, lit_cell);
            ++y;
        }
    }
    if (!caption_.empty()) {
        y += kBlockGap;
        painter.draw_text(Point{centred(text::text_width(caption_)), y}, caption_, face);
    }
}

bool BigClockView::on_key(const KeyEvent& event) {
    // A key the application has bound to a command is that command, not a
    // dismissal: it is left unhandled so the application's keymap runs it,
    // and the face stays up. That is how a reader leaves a face without
    // putting it away — a window-switching chord, a multiplexer's prefix —
    // and how the menu accelerators and help keep working over one.
    if (context().app != nullptr && context().app->commands().command_for_key(event.chord))
        return false;
    if (on_dismiss) on_dismiss();
    return true;
}

bool BigClockView::on_mouse(const MouseEvent& event) {
    // Every pointer event is the face's while it is up: a click that fell
    // through to whatever it covers would act on something the reader cannot
    // see.
    if (event.action == MouseAction::Down) {
        pressed_ = true;
        // Asked at the press: the Application moves the focus to a clicked
        // view after delivering the press, so a click that brings the face
        // back reads as "not focused" here and only focuses it.
        press_dismisses_ = has_focus();
    } else if (event.action == MouseAction::Up) {
        const bool was_pressed = pressed_ && press_dismisses_;
        pressed_ = false;
        press_dismisses_ = false;
        const Rect abs = absolute_bounds();
        const bool inside = event.cell.x >= abs.x && event.cell.x < abs.right() &&
                            event.cell.y >= abs.y && event.cell.y < abs.bottom();
        if (was_pressed && inside && on_dismiss) on_dismiss();
    }
    return true;
}

}  // namespace ckv::widgets
