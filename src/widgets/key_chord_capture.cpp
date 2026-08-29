// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/key_chord_capture.hpp"

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

bool contains(const Rect& rect, Point point) noexcept {
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height;
}

bool begins_capture(const KeyChord& chord) noexcept {
    return chord.key == Key::Enter || (chord.key == Key::Char && chord.text == " ");
}

bool clears_binding(const KeyChord& chord) noexcept {
    return chord.key == Key::Backspace || chord.key == Key::Delete;
}

}  // namespace

KeyChordCapture::KeyChordCapture() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{18, 1});
}

void KeyChordCapture::set_chord(std::optional<KeyChord> chord) {
    if (chord_ == chord) return;
    chord_ = std::move(chord);
    invalidate();
}

void KeyChordCapture::begin_capture() {
    if (!enabled() || capturing_) return;
    capturing_ = true;
    invalidate();
}

void KeyChordCapture::cancel_capture() {
    if (!capturing_) return;
    capturing_ = false;
    invalidate();
}

void KeyChordCapture::clear() {
    cancel_capture();
    if (!chord_) return;
    chord_.reset();
    invalidate();
    publish_change();
}

void KeyChordCapture::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
}

void KeyChordCapture::on_focus(const FocusEvent& event) {
    focused_ = event.gained;
    if (!focused_) cancel_capture();
    invalidate();
}

void KeyChordCapture::draw(scene::Painter& painter) {
    const int width = bounds().width;
    if (width <= 0 || bounds().height <= 0) return;
    const Style style = context().theme->resolve(focused_ ? focused_role_ : normal_role_);
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", style));
    const std::string shown = capturing_ ? "Press a shortcut..." : chord_ ? format(*chord_) : "Unbound";
    painter.draw_text(Point{0, 0}, text::clip_to_width(shown, width), style);
}

ui::SizeHint KeyChordCapture::horizontal_size_hint() const {
    return ui::SizeHint{8, 18, ui::kUnboundedExtent};
}

ui::SizeHint KeyChordCapture::vertical_size_hint() const {
    return ui::SizeHint{1, 1, 1};
}

bool KeyChordCapture::on_key(const KeyEvent& event) {
    if (!enabled() || event.action != KeyAction::Press) return false;
    if (capturing_) {
        if (event.chord.key == Key::Escape) {
            cancel_capture();
            return true;
        }
        if (event.chord.key == Key::None) return true;
        chord_ = event.chord;
        capturing_ = false;
        invalidate();
        publish_change();
        return true;
    }
    if (begins_capture(event.chord)) {
        begin_capture();
        return true;
    }
    if (clears_binding(event.chord)) {
        clear();
        return true;
    }
    return false;
}

bool KeyChordCapture::on_mouse(const MouseEvent& event) {
    if (!enabled() || event.action != MouseAction::Down || !contains(absolute_bounds(), event.cell)) return false;
    begin_capture();
    return true;
}

void KeyChordCapture::publish_change() {
    if (on_chord_changed) on_chord_changed(chord_);
}

}  // namespace ckv::widgets
