// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/view.hpp"
#include "cvision/widgets/common_components.hpp"

namespace ckv::widgets {

// Block glyphs: text drawn large enough to read across a room. Every glyph is
// kBigGlyphHeight rows tall; a digit is five columns wide, and adjacent glyphs
// are separated by one blank column.
//
// The shapes are the seven-segment display's, drawn in cells, so the digits
// read as one family. The 1 is the exception: a centred stem with a flag and a
// foot, because the segment 1 hugs the right edge of its cell and leaves a gap
// in front of it that reads as a space.
inline constexpr int kBigGlyphHeight = 5;

// The characters that have a block glyph: the digits, ':', '-', '.', '/' and
// the space. A clock and a date need nothing else. Any other character is
// drawn as a blank the width of a digit, so what is missing shows as a gap
// rather than silently closing up.
bool has_big_glyph(char c) noexcept;

// `text` in block glyphs, as kBigGlyphHeight rows of equal width: '#' for a
// lit cell, ' ' for an unlit one. Pure, so a test can read a glyph back.
std::vector<std::string> big_glyph_rows(std::string_view text);

// The width `big_glyph_rows(text)` would have, without building it.
int big_glyph_width(std::string_view text) noexcept;

// What a BigClockView shows.
enum class BigClockContent { Time, Date, DateAndTime };

// A clock face that fills whatever it is given: the time, the date, or both,
// in block glyphs, centred. Where the glyphs do not fit — a window too narrow
// or too short for them — the same lines are drawn centred as plain text, so a
// small window still tells the time rather than showing nothing.
//
// Like ClockView it ticks once a second, re-reads its provider and repaints
// only when what it shows has changed. The reading comes from an injected
// provider for the reason ClockView gives: ckVision's Clock is monotonic and
// deliberately not a wall clock, and the provider is what makes a face
// testable.
//
// A face is put up to stay: a reader shows the time in one window and goes on
// working in another, glancing across at it. So losing the focus dismisses
// nothing. What does is the reader turning to the face itself — a key while it
// has the keyboard, or a completed click on a face that already had it — which
// asks for dismissal through `on_dismiss`. A key the application has bound to
// a command is not a dismissal either: the face leaves it unhandled, so the
// command runs and the face stays — which is how a reader switches away from a
// face without putting it away. The click that merely brings the
// face back into focus is not that: it is how the reader returns to it, and a
// face that vanished on the way back would never be looked at twice. It does
// not remove itself: whoever put it up knows what it was covering.
class BigClockView : public ui::View {
public:
    // A Tab stop showing the time, 24-hour with seconds. It shows nothing
    // and does not tick until a moment provider is set.
    BigClockView();

    // Date and time in one reading, so a face showing both cannot pair one
    // day's date with the next day's time.
    void set_moment_provider(std::function<DateTimeValue()> provider);
    void set_content(BigClockContent content);
    BigClockContent content() const noexcept { return content_; }
    void set_show_seconds(bool show);
    bool show_seconds() const noexcept { return show_seconds_; }
    void set_hour_format(HourFormat format);
    HourFormat hour_format() const noexcept { return hour_format_; }
    // The words for the two halves of a twelve-hour day, drawn as plain text
    // beneath the time: they are words, and the glyphs are for digits. Empty
    // labels suppress the caption.
    void set_meridiem_labels(std::string am, std::string pm);

    // What the face is showing, one entry per line in reading order: the date
    // as YYYY-MM-DD, the time as HH:MM[:SS] (twelve-hour: H:MM[:SS]).
    const std::vector<std::string>& lines() const noexcept { return lines_; }
    // The meridiem word under the time, or empty.
    const std::string& caption() const noexcept { return caption_; }
    // Whether the lines are drawn in block glyphs at the current size, rather
    // than as plain text.
    bool drawn_large() const noexcept;

    // Fires on any key the application has not bound to a command, and on a
    // completed click that began while the face already had the keyboard.
    std::function<void()> on_dismiss;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    void refresh();
    void ensure_ticking();
    int large_height() const noexcept;
    int large_width() const noexcept;

    std::function<DateTimeValue()> moment_provider_;
    BigClockContent content_ = BigClockContent::Time;
    bool show_seconds_ = true;
    HourFormat hour_format_ = HourFormat::TwentyFour;
    std::string am_label_ = "AM";
    std::string pm_label_ = "PM";
    std::vector<std::string> lines_;
    std::string caption_;
    bool ticking_ = false;
    bool pressed_ = false;
    // Whether the press under way began on a face that already had the
    // keyboard — the one kind of click that asks for dismissal.
    bool press_dismisses_ = false;
    ui::RoleId face_role_ = ui::kInvalidRole;
    ui::RoleId lit_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
