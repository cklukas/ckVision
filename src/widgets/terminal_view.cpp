// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/terminal_view.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/utf8.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {

namespace {

int xterm_modifier_parameter(Modifier modifiers) noexcept {
    int parameter = 1;
    if (has_modifier(modifiers, Modifier::Shift)) parameter += 1;
    if (has_modifier(modifiers, Modifier::Alt)) parameter += 2;
    if (has_modifier(modifiers, Modifier::Ctrl)) parameter += 4;
    return parameter;
}

std::string prefix_meta(std::string bytes, Modifier modifiers) {
    if (has_modifier(modifiers, Modifier::Alt)) bytes.insert(bytes.begin(), '\x1b');
    return bytes;
}

std::string encode_tilde_key(int code, Modifier modifiers) {
    const int parameter = xterm_modifier_parameter(modifiers);
    if (parameter == 1) return "\x1b[" + std::to_string(code) + "~";
    return "\x1b[" + std::to_string(code) + ";" + std::to_string(parameter) + "~";
}

std::string encode_cursor_key(char final_byte, Modifier modifiers, bool application_cursor_keys) {
    const int parameter = xterm_modifier_parameter(modifiers);
    if (parameter == 1)
        return application_cursor_keys ? std::string{"\x1bO"} + final_byte : std::string{"\x1b["} + final_byte;
    return "\x1b[1;" + std::to_string(parameter) + final_byte;
}

// --- The kitty keyboard protocol, on the side that sends keys -------------
//
// The legacy encoding above is correct and stays the default. What it cannot
// do is say some things at all: Ctrl+I and Tab are the same byte, a lone
// Escape is indistinguishable from the start of a sequence until a timer
// expires, and no key ever reports being released. A program that needs those
// asks for them, and then this encoding is what it gets.

int kitty_modifier_parameter(Modifier modifiers) noexcept {
    // The xterm parameter, extended with Super — same one-based counting, so
    // "no modifiers" is 1 in both.
    int parameter = xterm_modifier_parameter(modifiers);
    if (has_modifier(modifiers, Modifier::Super)) parameter += 8;
    return parameter;
}

// Press is 1 and is left out, as the protocol specifies; repeat and release
// are the two that have to be said.
int kitty_event_type(KeyAction action) noexcept {
    switch (action) {
        case KeyAction::Press: return 1;
        case KeyAction::Repeat: return 2;
        case KeyAction::Release: return 3;
    }
    return 1;
}

// The text a key produced, as the codepoints the protocol carries it in.
std::string kitty_text_codepoints(std::string_view text) {
    std::string out;
    std::size_t position = 0;
    while (position < text.size()) {
        const char32_t codepoint = utf8::decode(text, position);
        if (!out.empty()) out += ':';
        out += std::to_string(static_cast<std::uint32_t>(codepoint));
    }
    return out;
}

// `CSI <number> ; <modifiers>[:<event>] [; <text>] <final>`, with every field
// the protocol allows to be left out left out — a bare press of an unmodified
// key is `CSI 97 u`, not `CSI 97;1:1 u`.
std::string kitty_sequence(int number, char final_byte, int modifiers, int event,
                           std::string_view text) {
    std::string out = "\x1b[";
    out += std::to_string(number);
    if (modifiers != 1 || event != 1 || !text.empty()) {
        out += ';';
        out += std::to_string(modifiers);
        if (event != 1) {
            out += ':';
            out += std::to_string(event);
        }
    }
    if (!text.empty()) {
        out += ';';
        out += text;
    }
    out += final_byte;
    return out;
}

// The codepoint that names a key: the character it produces with no modifiers
// applied, which for the Latin letters means the lower-case one. ckVision's
// input model carries the text a key produced rather than the layout behind
// it, so this is as close to "the key itself" as the model can get — and it
// is exact for every key a program actually binds.
char32_t kitty_key_code(std::string_view text) noexcept {
    if (text.empty()) return 0;
    std::size_t position = 0;
    const char32_t codepoint = utf8::decode(text, position);
    if (codepoint >= U'A' && codepoint <= U'Z') return codepoint - U'A' + U'a';
    return codepoint;
}

// Which of the named keys keep a legacy escape form under the protocol, and
// what it is. The protocol deliberately does not renumber the keys that
// already had an encoding: Up stays `CSI A`, gaining only the fields it never
// had room for. `number` is the parameter the final byte expects — 1 for the
// letter finals, the key's own number for `~`.
struct LegacyForm {
    int number = 1;
    char final_byte = 'u';
    bool exists = false;
};

LegacyForm legacy_form(Key key) noexcept {
    switch (key) {
        case Key::Up: return {1, 'A', true};
        case Key::Down: return {1, 'B', true};
        case Key::Right: return {1, 'C', true};
        case Key::Left: return {1, 'D', true};
        case Key::Home: return {1, 'H', true};
        case Key::End: return {1, 'F', true};
        case Key::F1: return {1, 'P', true};
        case Key::F2: return {1, 'Q', true};
        case Key::F3: return {1, 'R', true};
        case Key::F4: return {1, 'S', true};
        case Key::Insert: return {2, '~', true};
        case Key::Delete: return {3, '~', true};
        case Key::PageUp: return {5, '~', true};
        case Key::PageDown: return {6, '~', true};
        case Key::F5: return {15, '~', true};
        case Key::F6: return {17, '~', true};
        case Key::F7: return {18, '~', true};
        case Key::F8: return {19, '~', true};
        case Key::F9: return {20, '~', true};
        case Key::F10: return {21, '~', true};
        case Key::F11: return {23, '~', true};
        case Key::F12: return {24, '~', true};
        // The four that are control characters rather than sequences. They
        // have no parameters to grow, so under the protocol they become
        // ordinary `CSI ... u` keys, numbered by the code they used to send.
        case Key::Escape: return {27, 'u', false};
        case Key::Enter: return {13, 'u', false};
        case Key::Tab: return {9, 'u', false};
        case Key::Backspace: return {127, 'u', false};
        default: return {};
    }
}

}  // namespace

TerminalView::TerminalView(core::TerminalSubsession& session) : session_(&session) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

namespace {

// Three lines to a wheel notch, the convention every terminal that offers
// wheel scrolling uses; a wheel that moved one line would need three times
// the turning for the same page.
constexpr int kWheelLinesPerNotch = 3;

int history_row_count(std::span<const Cell> history, int width) {
    return static_cast<int>(history.size() / static_cast<std::size_t>(std::max(1, width)));
}

}  // namespace

TerminalView::ScrollState TerminalView::scroll_state_from(const core::TerminalStatus& status) const {
    ScrollState state;
    state.primary_screen = !status.alternate_buffer;
    const int history_rows =
        state.primary_screen ? history_row_count(session_->scrollback(), status.cells.width) : 0;
    state.total_rows = history_rows + std::max(0, status.cells.height);
    state.viewport_rows = std::max(0, std::min(bounds().height, status.cells.height));
    state.offset = state.primary_screen
                       ? std::clamp(scrollback_offset_, 0,
                                    std::max(0, state.total_rows - state.viewport_rows))
                       : 0;
    return state;
}

TerminalView::ScrollState TerminalView::scroll_state() const {
    if (session_ == nullptr) return {};
    return scroll_state_from(session_->status());
}

void TerminalView::set_scrollback_offset(int rows) {
    if (session_ == nullptr) return;
    apply_scrollback_offset(rows, session_->status());
}

void TerminalView::apply_scrollback_offset(int rows, const core::TerminalStatus& status) {
    const ScrollState state = scroll_state_from(status);
    const int maximum =
        state.primary_screen ? std::max(0, state.total_rows - state.viewport_rows) : 0;
    const int clamped = std::clamp(rows, 0, maximum);
    if (clamped != scrollback_offset_) {
        scrollback_offset_ = clamped;
        invalidate();
    }
    publish_scroll_state(status);
}

void TerminalView::return_to_live(const core::TerminalStatus& status) {
    if (scrollback_offset_ == 0) return;
    scrollback_offset_ = 0;
    invalidate();
    publish_scroll_state(status);
}

void TerminalView::publish_scroll_state(const core::TerminalStatus& status) {
    const ScrollState state = scroll_state_from(status);
    // The stored offset follows the published clamp, so a later relative step
    // ("three more rows up") starts from where the reader actually is.
    scrollback_offset_ = state.offset;
    if (state == published_scroll_state_) return;
    published_scroll_state_ = state;
    if (on_scroll_state_changed) on_scroll_state_changed();
}

void TerminalView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->intern("ckv.terminal.text", Style{});
    if (!has_explicit_cell_metrics_ && context().app != nullptr)
        cell_pixels_ = context().app->terminal_cell_pixels();
    on_resized();
}

void TerminalView::on_focus(const FocusEvent& event) {
    focused_ = event.gained;
    if (session_ != nullptr && session_->status().focus_reporting_enabled)
        session_->send_input(event.gained ? "\x1b[I" : "\x1b[O");
    invalidate();
}

void TerminalView::on_terminal_subsession_changed(const core::TerminalSubsession& session) {
    if (&session != session_) return;
    const core::TerminalStatus status = session_->status();
    // The serial first, and the text only if it has moved. That is what the
    // serial is for: the clipboard text is the one payload a snapshot carries
    // that a host wants rarely, and this is called on every change.
    const std::uint64_t serial = status.clipboard_serial;
    if (serial != forwarded_clipboard_serial_) {
        forwarded_clipboard_serial_ = serial;
        if (on_clipboard_write) on_clipboard_write(session_->snapshot().clipboard_text);
    }
    // A reader scrolled back is reading; a child that keeps printing must not
    // slide the text out from under them. Growth is added to the offset so
    // the same rows stay on screen, and the live edge keeps following.
    const int history_rows = history_row_count(session_->scrollback(), status.cells.width);
    if (scrollback_offset_ > 0 && history_rows > last_history_rows_)
        scrollback_offset_ += history_rows - last_history_rows_;
    last_history_rows_ = history_rows;
    publish_scroll_state(status);
    invalidate();
}

std::optional<CursorState> TerminalView::cursor_state() const {
    if (!focused_ || scrollback_offset_ != 0 || session_ == nullptr || bounds().width <= 0 || bounds().height <= 0)
        return std::nullopt;
    const core::TerminalStatus snapshot = session_->status();
    if (!snapshot.cursor.visible || snapshot.cursor.position.x < 0 || snapshot.cursor.position.y < 0)
        return std::nullopt;
    // A cursor past the edge is held at the edge rather than withdrawn.
    // Resizing tells the child its new size and then waits for it to redraw,
    // so for those frames the child's cursor still belongs to the old, larger
    // grid. Both answers are stale; this one is stale by a cell or two and
    // settles, where withdrawing it says "there is no cursor" -- which is
    // untrue, and blinks the cursor out of every drag.
    const Point clamped{std::min(snapshot.cursor.position.x, bounds().width - 1),
                        std::min(snapshot.cursor.position.y, bounds().height - 1)};
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + clamped.x, absolute.y + clamped.y},
                       snapshot.cursor.shape, true};
}

void TerminalView::set_cell_metrics(Size cell_pixels) {
    cell_pixels_ = {std::max(0, cell_pixels.width), std::max(0, cell_pixels.height)};
    has_explicit_cell_metrics_ = true;
    on_resized();
}

void TerminalView::on_resized() {
    if (!has_explicit_cell_metrics_ && context().app != nullptr)
        cell_pixels_ = context().app->terminal_cell_pixels();
    if (session_ != nullptr) {
        session_->resize(Size{bounds().width, bounds().height}, cell_pixels_);
        // The viewport is part of the scroll state, and it just changed.
        publish_scroll_state(session_->status());
    }
}

void TerminalView::draw(scene::Painter& painter) {
    // The host's cell metric is not known when a view is attached: it arrives
    // with the capability probe, a frame or two into the session, and until
    // then the application reports nothing. A child resized in that window was
    // told its cells were one pixel across — so it believed its whole screen
    // was a hundred pixels wide and quietly stopped sending pictures, which is
    // what "no graphics in ckmux, graphics in the example" turned out to be.
    // The metric is therefore followed when it lands, not sampled once.
    if (!has_explicit_cell_metrics_ && context().app != nullptr && session_ != nullptr) {
        const Size metric = context().app->terminal_cell_pixels();
        if (metric.width > 0 && metric.height > 0 && !(metric == cell_pixels_)) {
            cell_pixels_ = metric;
            session_->resize(Size{bounds().width, bounds().height}, cell_pixels_);
        }
    }
    const Style fallback = session_ != nullptr ? session_->profile().default_style
                                               : context().theme->resolve(text_role_);
    const Rect area{0, 0, bounds().width, bounds().height};
    painter.fill(area, Cell::from_grapheme(" ", fallback));
    if (session_ == nullptr) return;
    // Borrowed, not copied. This runs on every repaint of every terminal, and a
    // snapshot carries the whole history: at ten thousand lines that is tens of
    // megabytes per frame per window, and for a ckmux mirror it would be that
    // much again for every keystroke. Reading what changed and where it lives is
    // exactly what U0-b is for.
    const core::TerminalStatus snapshot = session_->status();
    const std::span<const Cell> grid = session_->cells();
    const std::span<const Cell> history = session_->scrollback();
    const int rows = std::min(bounds().height, snapshot.cells.height);
    const int columns = std::min(bounds().width, snapshot.cells.width);
    const int history_rows =
        static_cast<int>(history.size() / static_cast<std::size_t>(std::max(1, snapshot.cells.width)));
    const int total_rows = history_rows + snapshot.cells.height;
    // The alternate buffer has no history to stand in: a full-screen program
    // owns the whole window, and a stale offset from before it started would
    // paint the prompt's history above its screen.
    if (snapshot.alternate_buffer) scrollback_offset_ = 0;
    scrollback_offset_ = std::clamp(scrollback_offset_, 0, std::max(0, total_rows - rows));
    const int first_row = std::max(0, total_rows - rows - scrollback_offset_);
    Point selection_first{};
    Point selection_last{};
    const bool has_selection = selection_start_.has_value() && selection_end_.has_value();
    if (has_selection) {
        selection_first = *selection_start_;
        selection_last = *selection_end_;
        if (selection_last.y < selection_first.y ||
            (selection_last.y == selection_first.y && selection_last.x < selection_first.x))
            std::swap(selection_first, selection_last);
    }
    for (int y = 0; y < rows; ++y) {
        const int source_row = first_row + y;
        const std::span<const Cell>& source = source_row < history_rows ? history : grid;
        const int source_y = source_row < history_rows ? source_row : source_row - history_rows;
        for (int x = 0; x < columns; ++x) {
            const Cell& cell = source[static_cast<std::size_t>(source_y * snapshot.cells.width + x)];
            if (cell.is_continuation()) continue;
            Style style = cell.style();
            if (style.fg.is_default()) style.fg = fallback.fg;
            if (style.bg.is_default()) style.bg = fallback.bg;
            const bool selected = has_selection &&
                (y > selection_first.y || (y == selection_first.y && x >= selection_first.x)) &&
                (y < selection_last.y || (y == selection_last.y && x <= selection_last.x));
            if (selected) style.attrs |= Attr::Reverse;
            painter.draw_text(Point{x, y}, cell.grapheme(), style);
        }
    }
    for (const core::TerminalRaster& raster : session_->rasters()) {
        if (raster.id == 0 || raster.image == nullptr) continue;
        const Rect anchor{raster.anchor.x, raster.anchor.y, raster.cell_extent.width, raster.cell_extent.height};
        painter.draw_image(anchor, raster.id, raster.image, [&raster, fallback, anchor](scene::Painter& fallback_painter) {
            fallback_painter.fill(anchor, Cell::from_grapheme(" ", fallback));
            if (!raster.fallback.empty()) fallback_painter.draw_text(Point{anchor.x, anchor.y}, raster.fallback, fallback);
        });
    }
    if (snapshot.state == core::TerminalSubsessionState::Exited && bounds().height > 0)
        painter.draw_text(Point{0, bounds().height - 1}, "[terminal exited]", fallback);
    else if (snapshot.state == core::TerminalSubsessionState::Failed && bounds().height > 0)
        painter.draw_text(Point{0, bounds().height - 1}, "[terminal failed]", fallback);
    else if (bounds().height > 0) {
        const std::span<const core::TerminalDiagnostic> diagnostics = session_->diagnostics();
        const auto diagnostic = std::find_if(diagnostics.rbegin(), diagnostics.rend(),
                                             [](const core::TerminalDiagnostic& entry) {
                                                 return entry.kind != core::TerminalDiagnostic::Kind::UnsupportedSequence;
                                             });
        if (diagnostic != diagnostics.rend())
            painter.draw_text(Point{0, bounds().height - 1}, "[terminal: " + diagnostic->message + "]", fallback);
    }
}

// The protocol's own encoding, for a child that asked for it. Returns an
// empty string when this key is one the enhancements in force do not cover,
// which leaves the legacy encoding to say it instead.
std::string TerminalView::encode_kitty_key(const KeyEvent& event,
                                           core::TerminalKeyboardFlags flags) {
    using core::TerminalKeyboardFlags;
    const bool report_all = has_flag(flags, TerminalKeyboardFlags::ReportAllKeysAsEscapeCodes);
    const bool disambiguate =
        report_all || has_flag(flags, TerminalKeyboardFlags::DisambiguateEscapeCodes);
    if (!disambiguate) return {};

    const Modifier modifiers = event.chord.modifiers;
    const int modifier_parameter = kitty_modifier_parameter(modifiers);
    const int event_type =
        has_flag(flags, TerminalKeyboardFlags::ReportEventTypes) ? kitty_event_type(event.action) : 1;
    // Without event reporting there is nothing a release can be encoded as,
    // and a release delivered as a press would be worse than silence.
    if (event_type == 1 && event.action != KeyAction::Press) return {};

    const bool wants_text = has_flag(flags, TerminalKeyboardFlags::ReportAssociatedText);
    // A release produced no text; neither did a key held with Ctrl.
    const std::string text =
        wants_text && event.action != KeyAction::Release && !event.chord.text.empty() &&
                !has_modifier(modifiers, Modifier::Ctrl)
            ? kitty_text_codepoints(event.chord.text)
            : std::string{};

    if (event.chord.key == Key::Char) {
        if (event.chord.text.empty()) return {};
        // A plain letter is still a letter. Only a program that asked for
        // every key as an escape code wants `a` to stop being `a`; the
        // disambiguation flag alone is about the keys that were ambiguous,
        // which the unmodified ones never were.
        const bool ambiguous = has_modifier(modifiers, Modifier::Ctrl) ||
                               has_modifier(modifiers, Modifier::Alt) ||
                               has_modifier(modifiers, Modifier::Super);
        if (!report_all && !ambiguous) return {};
        const char32_t code = kitty_key_code(event.chord.text);
        if (code == 0) return {};
        return kitty_sequence(static_cast<int>(code), 'u', modifier_parameter, event_type, text);
    }

    const LegacyForm form = legacy_form(event.chord.key);
    if (form.number == 1 && form.final_byte == 'u' && !form.exists) return {};  // a key we do not name
    if (form.exists)
        return kitty_sequence(form.number, form.final_byte, modifier_parameter, event_type, text);
    // Escape, Enter, Tab and Backspace. Their legacy spelling is a control
    // byte with nowhere to put a modifier, so an unmodified press of one is
    // left as that byte unless the child asked for every key as an escape
    // code — but Ctrl+Enter, Shift+Tab and a bare Escape under the protocol
    // are exactly what the program turned this on to be able to tell apart.
    const bool bare = modifier_parameter == 1 && event_type == 1;
    if (!report_all && bare && event.chord.key != Key::Escape) return {};
    return kitty_sequence(form.number, 'u', modifier_parameter, event_type, text);
}

std::string TerminalView::encode_key(const KeyEvent& event, bool application_cursor_keys) {
    if (event.action == KeyAction::Release) return {};
    const Modifier modifiers = event.chord.modifiers;
    if (event.chord.key == Key::Char) {
        if (event.chord.text.empty()) return {};
        if (has_modifier(modifiers, Modifier::Ctrl) && event.chord.text.size() == 1) {
            const unsigned char character = static_cast<unsigned char>(event.chord.text.front());
            unsigned char control = 0;
            if (character >= '@' && character <= '_') control = character & 0x1fU;
            else if (character >= 'a' && character <= 'z') control = static_cast<unsigned char>(std::toupper(character)) & 0x1fU;
            else if (character == ' ') control = 0;
            if (control != 0 || character == ' ')
                return prefix_meta(std::string(1, static_cast<char>(control)), modifiers);
        }
        return prefix_meta(event.chord.text, modifiers);
    }
    switch (event.chord.key) {
        case Key::Enter: return prefix_meta("\r", modifiers);
        case Key::Tab:
            if (modifiers == Modifier::Shift) return "\x1b[Z";
            return prefix_meta("\t", modifiers);
        case Key::Backspace: return prefix_meta("\x7f", modifiers);
        case Key::Escape: return prefix_meta("\x1b", modifiers);
        case Key::Up: return encode_cursor_key('A', modifiers, application_cursor_keys);
        case Key::Down: return encode_cursor_key('B', modifiers, application_cursor_keys);
        case Key::Right: return encode_cursor_key('C', modifiers, application_cursor_keys);
        case Key::Left: return encode_cursor_key('D', modifiers, application_cursor_keys);
        case Key::Home: return encode_cursor_key('H', modifiers, false);
        case Key::End: return encode_cursor_key('F', modifiers, false);
        case Key::PageUp: return encode_tilde_key(5, modifiers);
        case Key::PageDown: return encode_tilde_key(6, modifiers);
        case Key::Delete: return encode_tilde_key(3, modifiers);
        case Key::Insert: return encode_tilde_key(2, modifiers);
        case Key::F1: return xterm_modifier_parameter(modifiers) == 1 ? "\x1bOP" : "\x1b[1;" + std::to_string(xterm_modifier_parameter(modifiers)) + "P";
        case Key::F2: return xterm_modifier_parameter(modifiers) == 1 ? "\x1bOQ" : "\x1b[1;" + std::to_string(xterm_modifier_parameter(modifiers)) + "Q";
        case Key::F3: return xterm_modifier_parameter(modifiers) == 1 ? "\x1bOR" : "\x1b[1;" + std::to_string(xterm_modifier_parameter(modifiers)) + "R";
        case Key::F4: return xterm_modifier_parameter(modifiers) == 1 ? "\x1bOS" : "\x1b[1;" + std::to_string(xterm_modifier_parameter(modifiers)) + "S";
        case Key::F5: return encode_tilde_key(15, modifiers);
        case Key::F6: return encode_tilde_key(17, modifiers);
        case Key::F7: return encode_tilde_key(18, modifiers);
        case Key::F8: return encode_tilde_key(19, modifiers);
        case Key::F9: return encode_tilde_key(20, modifiers);
        case Key::F10: return encode_tilde_key(21, modifiers);
        case Key::F11: return encode_tilde_key(23, modifiers);
        case Key::F12: return encode_tilde_key(24, modifiers);
        default: return {};
    }
}

bool TerminalView::on_key(const KeyEvent& event) {
    // Before anything else, and only once there is no child to write to: a
    // host may claim keys in a window whose program has ended, where every
    // key would otherwise be written into a subsession that cannot take it.
    // A live terminal never reaches this, so the child's dialect is untouched.
    if (on_key_after_exit && session_ != nullptr) {
        const core::TerminalSubsessionState state = session_->state();
        if ((state == core::TerminalSubsessionState::Exited ||
             state == core::TerminalSubsessionState::Failed) &&
            on_key_after_exit(event))
            return true;
    }
    if (event.chord == parent_escape_) {
        // The one chord the child never sees, and it is claimed on the press:
        // letting the matching release through would deliver half a chord to
        // a program that was told nothing about the other half.
        if (event.action == KeyAction::Press && on_parent_escape) on_parent_escape();
        return true;
    }
    const core::TerminalStatus status = session_->status();
    if ((event.chord.key == Key::PageUp || event.chord.key == Key::PageDown) &&
        event.chord.modifiers == Modifier::None && !status.alternate_buffer) {
        // The primary screen's plain paging keys are the terminal's own,
        // under every keyboard protocol alike — and the release is claimed
        // with the press, so a child that asked for event types never sees a
        // release whose press never arrived. A modified press stays the
        // child's: Shift+PageDown is a different key, and one the reader
        // pressed at something.
        if (event.action != KeyAction::Release) {
            const int page = std::max(1, bounds().height - 1);
            apply_scrollback_offset(
                scrollback_offset_ + (event.chord.key == Key::PageUp ? page : -page), status);
        }
        return true;
    }
    return deliver_key(event, status);
}

bool TerminalView::send_key(const KeyEvent& event) {
    if (session_ == nullptr) return false;
    return deliver_key(event, session_->status());
}

bool TerminalView::deliver_key(const KeyEvent& event, const core::TerminalStatus& status) {
    const core::TerminalKeyboardFlags keyboard_flags = status.keyboard_flags;
    if (keyboard_flags != core::TerminalKeyboardFlags::None) {
        const std::string kitty = encode_kitty_key(event, keyboard_flags);
        if (!kitty.empty()) {
            return_to_live(status);
            session_->send_input(kitty);
            return true;
        }
    }
    if (event.action == KeyAction::Release) return false;
    const std::string encoded = encode_key(event, status.application_cursor_keys);
    if (encoded.empty()) return false;
    return_to_live(status);
    session_->send_input(encoded);
    return true;
}

bool TerminalView::on_text(const TextEvent& event) {
    if (event.text.empty()) return false;
    const core::TerminalStatus status = session_->status();
    return_to_live(status);
    if (event.from_paste && status.bracketed_paste_enabled)
        session_->send_input("\x1b[200~" + event.text + "\x1b[201~");
    else
        session_->send_input(event.text);
    return true;
}

// Which pointer events a child hears about, which is the whole difference
// between the three tracking modes. A program that asked for 1000 wants presses
// and releases; one that asked for 1002 wants those and the drags; only 1003
// asked to be told where the pointer is when nothing is held. Sending every
// program the 1003 stream is not generosity — a program reading only what it
// asked for gets a report it did not expect between the ones it did.
//
// A drag is told from a hover by the button the report carries: the input model
// keeps the held button on a motion event and leaves it None when nothing is
// down, which is the same distinction the wire makes.
bool TerminalView::tracking_reports(const MouseEvent& event, const core::TerminalStatus& status) {
    using core::TerminalMouseTracking;
    // `TerminalSubsession` is a seam other projects implement — a mirror over a
    // protocol, a recording — and one that fills in only the older, coarser
    // `mouse_reporting_enabled` is saying "the child is tracking" without
    // saying at what level. That is not the same statement as "not tracking",
    // and reading it as one would silence every pointer event such a host
    // delivers. It is read as the level this view behaved as before there was
    // one to name, so a host that has not adopted the field is unchanged.
    const TerminalMouseTracking tracking =
        status.mouse_tracking == TerminalMouseTracking::None && status.mouse_reporting_enabled
            ? TerminalMouseTracking::AnyMotion
            : status.mouse_tracking;
    if (tracking == TerminalMouseTracking::None) return false;
    if (event.action != MouseAction::Move) return true;
    if (tracking == TerminalMouseTracking::AnyMotion) return true;
    if (tracking == TerminalMouseTracking::ButtonMotion)
        return event.button != MouseButton::None;
    return false;
}

std::string TerminalView::encode_mouse(const MouseEvent& event, core::TerminalMouseEncoding encoding) const {
    const Rect absolute = absolute_bounds();
    const int x = event.cell.x - absolute.x + 1;
    const int y = event.cell.y - absolute.y + 1;
    if (x < 1 || y < 1 || x > bounds().width || y > bounds().height) return {};

    int button = 0;
    switch (event.button) {
        case MouseButton::Left: button = 0; break;
        case MouseButton::Middle: button = 1; break;
        case MouseButton::Right: button = 2; break;
        case MouseButton::WheelUp: button = 64; break;
        case MouseButton::WheelDown: button = 65; break;
        case MouseButton::WheelLeft: button = 66; break;
        case MouseButton::WheelRight: button = 67; break;
        case MouseButton::None: button = 3; break;
    }
    if (event.action == MouseAction::Move) button |= 32;
    if (has_modifier(event.modifiers, Modifier::Shift)) button |= 4;
    if (has_modifier(event.modifiers, Modifier::Alt)) button |= 8;
    if (has_modifier(event.modifiers, Modifier::Ctrl)) button |= 16;
    if (encoding == core::TerminalMouseEncoding::X10) {
        if (event.action == MouseAction::Move || x > 223 || y > 223) return {};
        std::string encoded{"\x1b[M"};
        encoded.push_back(static_cast<char>(32 + button));
        encoded.push_back(static_cast<char>(32 + x));
        encoded.push_back(static_cast<char>(32 + y));
        return encoded;
    }
    if (encoding != core::TerminalMouseEncoding::Sgr) return {};
    const char terminator = event.action == MouseAction::Up ? 'm' : 'M';
    return "\x1b[<" + std::to_string(button) + ";" + std::to_string(x) + ";" + std::to_string(y) + terminator;
}

std::optional<Point> TerminalView::local_cell(Point absolute) const {
    const Rect rectangle = absolute_bounds();
    const Point local{absolute.x - rectangle.x, absolute.y - rectangle.y};
    return Rect{0, 0, bounds().width, bounds().height}.contains(local) ? std::optional<Point>{local} : std::nullopt;
}

std::string TerminalView::selected_text() const {
    if (!selection_start_ || !selection_end_ || session_ == nullptr) return {};
    const core::TerminalStatus snapshot = session_->status();
    const std::span<const Cell> grid = session_->cells();
    const std::span<const Cell> history = session_->scrollback();
    Point first = *selection_start_;
    Point last = *selection_end_;
    if (last.y < first.y || (last.y == first.y && last.x < first.x)) std::swap(first, last);
    const int history_rows =
        static_cast<int>(history.size() / static_cast<std::size_t>(std::max(1, snapshot.cells.width)));
    const int total_rows = history_rows + snapshot.cells.height;
    const int visible_rows = std::min(bounds().height, snapshot.cells.height);
    const int first_visible_row = std::max(0, total_rows - visible_rows - scrollback_offset_);
    std::string text;
    for (int y = std::max(0, first.y); y <= std::min(last.y, visible_rows - 1); ++y) {
        const int source_row = first_visible_row + y;
        const std::span<const Cell>& source = source_row < history_rows ? history : grid;
        const int source_y = source_row < history_rows ? source_row : source_row - history_rows;
        const int begin = y == first.y ? std::max(0, first.x) : 0;
        const int end = y == last.y ? std::min(last.x, snapshot.cells.width - 1) : snapshot.cells.width - 1;
        for (int x = begin; x <= end; ++x) {
            const Cell& cell = source[static_cast<std::size_t>(source_y * snapshot.cells.width + x)];
            if (!cell.is_continuation()) text += cell.grapheme();
        }
        if (y != std::min(last.y, visible_rows - 1)) text.push_back('\n');
    }
    return text;
}

bool TerminalView::on_mouse(const MouseEvent& event) {
    const std::optional<Point> local = local_cell(event.cell);
    if (has_modifier(event.modifiers, Modifier::Shift) && local &&
        (event.button == MouseButton::Left || (event.action == MouseAction::Move && selection_start_))) {
        if (event.action == MouseAction::Down) selection_start_ = selection_end_ = local;
        else if (event.action == MouseAction::Move && selection_start_) selection_end_ = local;
        else if (event.action == MouseAction::Up && selection_start_) {
            selection_end_ = local;
            if (on_selection_copy) on_selection_copy(selected_text());
        } else return false;
        invalidate();
        return true;
    }
    const core::TerminalStatus snapshot = session_->status();
    if (scroll_history(event, snapshot)) return true;
    if (!snapshot.mouse_reporting_enabled) return forward_alternate_scroll(event, snapshot);
    // Not claimed: a motion the child did not ask for is one this view has no
    // business consuming either, and leaving it unhandled lets whatever else
    // wants it — a hover shape, a container — have it.
    if (!tracking_reports(event, snapshot)) return false;
    const std::string encoded = encode_mouse(event, snapshot.mouse_encoding);
    if (encoded.empty()) return false;
    session_->send_input(encoded);
    return true;
}

bool TerminalView::scroll_history(const MouseEvent& event, const core::TerminalStatus& snapshot) {
    // The wheel over the primary screen walks this terminal's own history —
    // whenever the child is not watching the mouse, and under Shift even when
    // it is: a Shift-marked gesture is the host's, here as everywhere.
    if (snapshot.alternate_buffer) return false;
    if (event.button != MouseButton::WheelUp && event.button != MouseButton::WheelDown) return false;
    if (snapshot.mouse_reporting_enabled && !has_modifier(event.modifiers, Modifier::Shift))
        return false;
    if (!local_cell(event.cell)) return false;
    const int direction = event.button == MouseButton::WheelUp ? 1 : -1;
    apply_scrollback_offset(scrollback_offset_ + direction * kWheelLinesPerNotch, snapshot);
    return true;
}

bool TerminalView::forward_alternate_scroll(const MouseEvent& event,
                                            const core::TerminalStatus& snapshot) {
    // Only on the alternate screen. A full-screen program owns the whole
    // window and has somewhere to scroll to; on the primary buffer the wheel
    // belongs to the terminal's own history, and sending arrow keys there
    // would type into whatever is at the prompt.
    if (!snapshot.alternate_buffer || !snapshot.alternate_scroll_enabled) return false;
    if (event.button != MouseButton::WheelUp && event.button != MouseButton::WheelDown) return false;
    if (!local_cell(event.cell)) return false;
    const std::string key =
        encode_cursor_key(event.button == MouseButton::WheelUp ? 'A' : 'B', Modifier::None,
                          snapshot.application_cursor_keys);
    std::string bytes;
    bytes.reserve(key.size() * kWheelLinesPerNotch);
    for (int line = 0; line < kWheelLinesPerNotch; ++line) bytes += key;
    session_->send_input(bytes);
    return true;
}

}  // namespace ckv::widgets
