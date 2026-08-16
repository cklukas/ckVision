// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/terminal_view.hpp"

#include <algorithm>
#include <cctype>

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

}  // namespace

TerminalView::TerminalView(core::TerminalSubsession& session) : session_(&session) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void TerminalView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->intern("ckv.terminal.text", Style{});
    if (!has_explicit_cell_metrics_ && context().app != nullptr)
        cell_pixels_ = context().app->terminal_cell_pixels();
    on_resized();
}

void TerminalView::on_focus(const FocusEvent& event) {
    focused_ = event.gained;
    if (session_ != nullptr && session_->snapshot().focus_reporting_enabled)
        session_->send_input(event.gained ? "\x1b[I" : "\x1b[O");
    invalidate();
}

void TerminalView::on_terminal_subsession_changed(const core::TerminalSubsession& session) {
    if (&session == session_) invalidate();
}

std::optional<CursorState> TerminalView::cursor_state() const {
    if (!focused_ || scrollback_offset_ != 0 || session_ == nullptr || bounds().width <= 0 || bounds().height <= 0)
        return std::nullopt;
    const core::TerminalSnapshot snapshot = session_->snapshot();
    if (!snapshot.cursor.visible || snapshot.cursor.position.x < 0 || snapshot.cursor.position.y < 0 ||
        snapshot.cursor.position.x >= bounds().width || snapshot.cursor.position.y >= bounds().height)
        return std::nullopt;
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + snapshot.cursor.position.x, absolute.y + snapshot.cursor.position.y},
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
    if (session_ != nullptr)
        session_->resize(Size{bounds().width, bounds().height}, cell_pixels_);
}

void TerminalView::draw(scene::Painter& painter) {
    const Style fallback = session_ != nullptr ? session_->profile().default_style
                                               : context().theme->resolve(text_role_);
    const Rect area{0, 0, bounds().width, bounds().height};
    painter.fill(area, Cell::from_grapheme(" ", fallback));
    if (session_ == nullptr) return;
    const core::TerminalSnapshot snapshot = session_->snapshot();
    const int rows = std::min(bounds().height, snapshot.cells.height);
    const int columns = std::min(bounds().width, snapshot.cells.width);
    const int history_rows = static_cast<int>(snapshot.scrollback.size() / static_cast<std::size_t>(snapshot.cells.width));
    const int total_rows = history_rows + snapshot.cells.height;
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
        const std::vector<Cell>& source = source_row < history_rows ? snapshot.scrollback : snapshot.cell_buffer;
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
    for (const core::TerminalRaster& raster : snapshot.rasters) {
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
        const auto diagnostic = std::find_if(snapshot.diagnostics.rbegin(), snapshot.diagnostics.rend(),
                                             [](const core::TerminalDiagnostic& entry) {
                                                 return entry.kind != core::TerminalDiagnostic::Kind::UnsupportedSequence;
                                             });
        if (diagnostic != snapshot.diagnostics.rend())
            painter.draw_text(Point{0, bounds().height - 1}, "[terminal: " + diagnostic->message + "]", fallback);
    }
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
    if (event.action == KeyAction::Release) return false;
    if (event.chord == parent_escape_) {
        if (on_parent_escape) on_parent_escape();
        return true;
    }
    if ((event.chord.key == Key::PageUp || event.chord.key == Key::PageDown) && !session_->snapshot().alternate_buffer) {
        const core::TerminalSnapshot snapshot = session_->snapshot();
        const int history_rows = static_cast<int>(snapshot.scrollback.size() / static_cast<std::size_t>(snapshot.cells.width));
        const int maximum = std::max(0, history_rows);
        const int page = std::max(1, bounds().height - 1);
        scrollback_offset_ = std::clamp(scrollback_offset_ + (event.chord.key == Key::PageUp ? page : -page), 0, maximum);
        invalidate();
        return true;
    }
    const std::string encoded = encode_key(event, session_->snapshot().application_cursor_keys);
    if (encoded.empty()) return false;
    session_->send_input(encoded);
    return true;
}

bool TerminalView::on_text(const TextEvent& event) {
    if (event.text.empty()) return false;
    if (event.from_paste && session_->snapshot().bracketed_paste_enabled)
        session_->send_input("\x1b[200~" + event.text + "\x1b[201~");
    else
        session_->send_input(event.text);
    return true;
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
    const core::TerminalSnapshot snapshot = session_->snapshot();
    Point first = *selection_start_;
    Point last = *selection_end_;
    if (last.y < first.y || (last.y == first.y && last.x < first.x)) std::swap(first, last);
    const int history_rows = static_cast<int>(snapshot.scrollback.size() / static_cast<std::size_t>(snapshot.cells.width));
    const int total_rows = history_rows + snapshot.cells.height;
    const int visible_rows = std::min(bounds().height, snapshot.cells.height);
    const int first_visible_row = std::max(0, total_rows - visible_rows - scrollback_offset_);
    std::string text;
    for (int y = std::max(0, first.y); y <= std::min(last.y, visible_rows - 1); ++y) {
        const int source_row = first_visible_row + y;
        const std::vector<Cell>& source = source_row < history_rows ? snapshot.scrollback : snapshot.cell_buffer;
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
    const core::TerminalSnapshot snapshot = session_->snapshot();
    if (!snapshot.mouse_reporting_enabled) return false;
    const std::string encoded = encode_mouse(event, snapshot.mouse_encoding);
    if (encoded.empty()) return false;
    session_->send_input(encoded);
    return true;
}

}  // namespace ckv::widgets
