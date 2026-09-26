// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "echo_app.hpp"

#include "../example_about.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "cvision/core/cell.hpp"
#include "cvision/core/key.hpp"
#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/scene/painter.hpp"

namespace ckv::echo {
namespace {

constexpr std::string_view kTitle = "ckVision input echo";
constexpr std::string_view kDescription =
    "Every key, mouse, paste, focus and resize event this application receives, decoded, newest last.";
// The header rows above the log: title, description, copyright, rule.
constexpr int kHeaderRows = 4;
// A long paste is shown by its start; its length is always given in full.
constexpr std::size_t kQuotedCodepoints = 48;

// Pads `text` with spaces to `width` bytes, so the columns of ASCII labels
// line up.
std::string column(std::string_view text, std::size_t width) {
    std::string padded(text);
    if (padded.size() < width) padded.append(width - padded.size(), ' ');
    return padded;
}

// "Ctrl+Alt+" and so on, in the order a formatted chord spells them.
std::string modifier_prefix(Modifier modifiers) {
    std::string prefix;
    if (has_modifier(modifiers, Modifier::Ctrl)) prefix += "Ctrl+";
    if (has_modifier(modifiers, Modifier::Alt)) prefix += "Alt+";
    if (has_modifier(modifiers, Modifier::Shift)) prefix += "Shift+";
    if (has_modifier(modifiers, Modifier::Super)) prefix += "Super+";
    return prefix;
}

std::string hex(std::uint32_t value, int digits) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text(static_cast<std::size_t>(digits), '0');
    for (int index = digits - 1; index >= 0; --index) {
        text[static_cast<std::size_t>(index)] = kDigits[value & 0xFU];
        value >>= 4U;
    }
    return text;
}

// `text` in double quotes, with quote, backslash and every C0/C1 control
// written as an escape, and at most `limit` codepoints of it shown.
std::string quoted(std::string_view text, std::size_t limit) {
    std::string out = "\"";
    std::size_t pos = 0;
    std::size_t shown = 0;
    while (pos < text.size()) {
        if (shown == limit) {
            out += "\xE2\x80\xA6";
            break;
        }
        const char32_t cp = utf8::decode(text, pos);
        ++shown;
        switch (cp) {
            case U'"': out += "\\\""; continue;
            case U'\\': out += "\\\\"; continue;
            case U'\n': out += "\\n"; continue;
            case U'\r': out += "\\r"; continue;
            case U'\t': out += "\\t"; continue;
            default: break;
        }
        if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) {
            out += "\\x" + hex(static_cast<std::uint32_t>(cp), 2);
            continue;
        }
        utf8::encode(cp, out);
    }
    out += '"';
    return out;
}

// "U+0061 U+0301": the codepoints of a key's text.
std::string codepoints(std::string_view text) {
    std::string out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const char32_t cp = utf8::decode(text, pos);
        if (!out.empty()) out += ' ';
        out += "U+" + hex(static_cast<std::uint32_t>(cp), cp > 0xFFFF ? 6 : 4);
    }
    return out;
}

std::string_view action_name(KeyAction action) {
    switch (action) {
        case KeyAction::Press: return "press";
        case KeyAction::Repeat: return "repeat";
        case KeyAction::Release: return "release";
    }
    return "press";
}

std::string_view action_name(MouseAction action) {
    switch (action) {
        case MouseAction::Down: return "down";
        case MouseAction::Up: return "up";
        case MouseAction::Move: return "move";
        case MouseAction::Wheel: return "wheel";
    }
    return "move";
}

std::string_view button_name(MouseButton button) {
    switch (button) {
        case MouseButton::None: return "none";
        case MouseButton::Left: return "left";
        case MouseButton::Middle: return "middle";
        case MouseButton::Right: return "right";
        case MouseButton::WheelUp: return "wheel-up";
        case MouseButton::WheelDown: return "wheel-down";
        case MouseButton::WheelLeft: return "wheel-left";
        case MouseButton::WheelRight: return "wheel-right";
    }
    return "none";
}

std::string_view keyboard_name(term::KeyboardProtocol protocol) {
    switch (protocol) {
        case term::KeyboardProtocol::Legacy: return "legacy";
        case term::KeyboardProtocol::ModifyOtherKeys: return "modifyOtherKeys";
        case term::KeyboardProtocol::Kitty: return "kitty";
    }
    return "legacy";
}

std::string_view mouse_name(term::MouseProtocol protocol) {
    switch (protocol) {
        case term::MouseProtocol::None: return "none";
        case term::MouseProtocol::X10: return "X10";
        case term::MouseProtocol::SGR: return "SGR";
    }
    return "none";
}

std::string_view on_off(bool value) { return value ? "on" : "off"; }

}  // namespace

std::string describe(const KeyEvent& event) {
    std::string line = column("key", 8) + column(action_name(event.action), 8);
    const KeyChord& chord = event.chord;
    line += modifier_prefix(chord.modifiers);
    if (chord.key == Key::Char) {
        line += quoted(chord.text, kQuotedCodepoints) + "  " + codepoints(chord.text);
    } else if (chord.key == Key::None) {
        line += "(no key)";
    } else {
        line += key_name(chord.key);
    }
    if (event.reports_release && event.action != KeyAction::Release) line += "  (release follows)";
    return line;
}

std::string describe(const MouseEvent& event) {
    std::string line = column("mouse", 8) + column(action_name(event.action), 8) +
                       column(button_name(event.button), 12) + "cell " + std::to_string(event.cell.x) + "," +
                       std::to_string(event.cell.y);
    if (event.pixel) line += "  pixel " + std::to_string(event.pixel->x) + "," + std::to_string(event.pixel->y);
    // The Application counts clicks on its clock; the second press of a double
    // click says so.
    if (event.action == MouseAction::Down && event.click_count > 1)
        line += "  click " + std::to_string(event.click_count);
    if (event.modifiers != Modifier::None) {
        std::string held = modifier_prefix(event.modifiers);
        held.pop_back();  // the trailing '+'
        line += "  " + held;
    }
    return line;
}

std::string describe(const TextEvent& event) {
    std::string line = column(event.from_paste ? "paste" : "text", 8) + std::to_string(event.text.size()) +
                       " bytes  " + quoted(event.text, kQuotedCodepoints);
    if (event.paste_recovered) line += "  (recovered from incomplete framing)";
    return line;
}

std::string describe(const FocusEvent& event) {
    return column("focus", 8) + (event.gained ? "gained" : "lost");
}

std::string describe(const ResizeEvent& event) {
    return column("resize", 8) + std::to_string(event.cells.width) + "x" + std::to_string(event.cells.height);
}

std::string describe(const term::Capabilities& capabilities) {
    std::string line = column("host", 8) + "keyboard " + std::string(keyboard_name(capabilities.keyboard_protocol));
    if (capabilities.keyboard_protocol == term::KeyboardProtocol::Kitty)
        line += " (flags " + std::to_string(capabilities.kitty_keyboard_flags) + ")";
    line += ", mouse " + std::string(mouse_name(capabilities.mouse_protocol));
    if (capabilities.pixel_mouse) line += " with pixels";
    line += ", focus reports " + std::string(on_off(capabilities.focus_events)) + ", bracketed paste " +
            std::string(on_off(capabilities.bracketed_paste));
    return line;
}

EchoView::EchoView(ui::StandardRoles roles, std::function<void(std::string_view)> observer)
    : View(Rect{}, ui::FocusPolicy::TabStop), roles_(roles), observer_(std::move(observer)) {}

void EchoView::record(std::string_view line) {
    ++recorded_;
    std::string numbered = std::to_string(recorded_);
    if (numbered.size() < 6) numbered.insert(0, 6 - numbered.size(), ' ');
    numbered += "  ";
    numbered += line;
    if (observer_) observer_(numbered);
    lines_.push_back(std::move(numbered));
    if (lines_.size() > kKeptLines) lines_.pop_front();
    invalidate();
}

void EchoView::draw(scene::Painter& painter) {
    const int width = bounds().width;
    const int height = bounds().height;
    const ui::Theme& theme = *context().theme;
    const Style body = theme.resolve(roles_.text_view_text);
    const Style header = theme.resolve(roles_.window_title_active);
    painter.fill(Rect{0, 0, width, height}, Cell::from_grapheme(" ", body));
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", header));
    painter.draw_text(Point{1, 0}, kTitle, header);
    // The quit chord is the keymap's to state, so it is asked for rather
    // than written down: a rebinding changes the hint with it.
    if (const ui::Application* app = context().app; app != nullptr) {
        const std::string chord = app->commands().chord_text(app->commands().standard().quit);
        if (!chord.empty()) {
            const std::string hint = chord + " quits";
            const int hint_x = width - text::text_width(hint) - 1;
            if (hint_x > text::text_width(kTitle) + 2) painter.draw_text(Point{hint_x, 0}, hint, header);
        }
    }
    painter.draw_text(Point{1, 1}, kDescription, body);
    painter.draw_text(Point{1, 2}, examples::kCopyrightNotice, body);
    painter.hline(Point{0, kHeaderRows - 1}, width, scene::LineStyle::Single, body);

    const int rows = std::max(0, height - kHeaderRows);
    const std::size_t shown = std::min(lines_.size(), static_cast<std::size_t>(rows));
    const std::size_t first = lines_.size() - shown;
    for (std::size_t index = 0; index < shown; ++index)
        painter.draw_text(Point{1, kHeaderRows + static_cast<int>(index)}, lines_[first + index], body);
}

void EchoView::on_resized() { record(describe(ResizeEvent{Size{bounds().width, bounds().height}})); }

bool EchoView::on_key(const KeyEvent& event) {
    record(describe(event));
    return false;
}

bool EchoView::on_key_release(const KeyEvent& event) {
    record(describe(event));
    return true;
}

bool EchoView::on_text(const TextEvent& event) {
    record(describe(event));
    return true;
}

bool EchoView::on_mouse(const MouseEvent& event) {
    record(describe(event));
    return true;
}

void EchoView::on_focus(const FocusEvent& event) { record(describe(event)); }

EchoApp::EchoApp(ui::Application& app, std::function<void(std::string_view)> observer)
    : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);
    auto view = std::make_unique<EchoView>(roles_, std::move(observer));
    view_ = view.get();
    // The first line: what the host is assumed to send before any probe
    // has answered. Every refinement after it is recorded as it arrives.
    view_->record(describe(app_.terminal_capabilities()));
    app_.root().add_child(std::move(view));
    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_capability_changed_handler([this] { view_->record(describe(app_.terminal_capabilities())); });
    app_.set_focus(view_);
}

}  // namespace ckv::echo
