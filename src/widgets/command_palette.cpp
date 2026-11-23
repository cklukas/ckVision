// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/command_palette.hpp"
#include "cvision/core/ascii.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace ckv::widgets {
using detail::is_press;
using detail::enclosing_desktop;
using detail::without_last_grapheme;

CommandPalette::CommandPalette() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{40, 8});
}
void CommandPalette::set_query(std::string query) { query_ = std::move(query); highlighted_ = 0; invalidate(); }
void CommandPalette::set_invocation_contexts(std::vector<std::string> contexts) {
    invocation_contexts_ = std::move(contexts);
    highlighted_ = 0;
    invalidate();
}
void CommandPalette::set_framed(bool framed) {
    if (framed_ == framed) return;
    framed_ = framed;
    invalidate();
}
std::vector<std::string> CommandPalette::answering_contexts() const {
    if (invocation_contexts_) return *invocation_contexts_;
    return ui::command_context_path(this);
}
bool CommandPalette::enabled_command(ui::CommandId id) const {
    return context().app != nullptr && context().app->commands().is_enabled(id);
}
namespace {
int score_token(std::string_view token, std::string_view title, std::string_view category) {
    if (token.empty()) return 0;
    const auto find_ci = [](std::string_view hay, std::string_view needle) -> std::size_t {
        if (needle.size() > hay.size()) return std::string_view::npos;
        for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
            bool match = true;
            for (std::size_t j = 0; j < needle.size(); ++j) {
                if (ascii_lower(static_cast<unsigned char>(hay[i + j])) !=
                    ascii_lower(static_cast<unsigned char>(needle[j]))) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return std::string_view::npos;
    };

    if (const std::size_t pos = find_ci(title, token); pos != std::string_view::npos) {
        if (pos == 0) return 120;
        const char prev = title[pos - 1];
        if (prev == ' ' || prev == '-' || prev == '_' || prev == '/') return 90;
        return 70;
    }
    if (find_ci(category, token) != std::string_view::npos) return 50;

    std::string combined;
    combined.reserve(category.size() + 1 + title.size());
    combined.append(category);
    if (!category.empty()) combined.push_back(' ');
    combined.append(title);

    std::size_t ti = 0;
    int gaps = 0;
    std::size_t last_match = 0;
    bool have_match = false;
    for (std::size_t ci = 0; ci < combined.size() && ti < token.size(); ++ci) {
        if (ascii_lower(static_cast<unsigned char>(combined[ci])) ==
            ascii_lower(static_cast<unsigned char>(token[ti]))) {
            if (have_match && ci > last_match + 1) gaps += static_cast<int>(ci - last_match - 1);
            last_match = ci;
            have_match = true;
            ++ti;
        }
    }
    if (ti == token.size()) return std::max(5, 30 - gaps);
    return -1;
}
}  // namespace

std::vector<ui::CommandInfo> CommandPalette::filtered_commands() const {
    if (context().app == nullptr) return {};
    const ui::CommandRegistry& registry = context().app->commands();
    const std::vector<std::string> contexts = answering_contexts();

    std::vector<std::string_view> tokens;
    std::size_t pos = 0;
    while (pos < query_.size()) {
        while (pos < query_.size() && (query_[pos] == ' ' || query_[pos] == '\t')) ++pos;
        if (pos >= query_.size()) break;
        const std::size_t start = pos;
        while (pos < query_.size() && query_[pos] != ' ' && query_[pos] != '\t') ++pos;
        tokens.emplace_back(query_.data() + start, pos - start);
    }

    struct ScoredCommand {
        ui::CommandInfo info;
        int score = 0;
    };
    std::vector<ScoredCommand> scored;
    for (const ui::CommandInfo& info : registry.all()) {
        if (info.visibility != ui::CommandVisibility::Palette || !registry.in_scope(info.id, contexts))
            continue;
        const std::string title = parse_mnemonic(info.title).display;
        int total_score = 0;
        bool matched = true;
        for (const std::string_view token : tokens) {
            const int token_score = score_token(token, title, info.category);
            if (token_score < 0) {
                matched = false;
                break;
            }
            total_score += token_score;
        }
        if (!matched) continue;
        for (std::size_t i = 0; i < mru_order_.size(); ++i) {
            if (mru_order_[i] == info.id) {
                total_score += static_cast<int>(mru_order_.size() - i) * 15;
                break;
            }
        }
        scored.push_back(ScoredCommand{info, total_score});
    }
    std::stable_sort(scored.begin(), scored.end(), [&](const ScoredCommand& a, const ScoredCommand& b) {
        if (a.score != b.score) return a.score > b.score;
        const bool a_app = a.info.key.rfind("ckv.", 0) != 0;
        const bool b_app = b.info.key.rfind("ckv.", 0) != 0;
        if (a_app != b_app) return a_app;
        const bool a_enabled = enabled_command(a.info.id);
        const bool b_enabled = enabled_command(b.info.id);
        if (a_enabled != b_enabled) return a_enabled;
        return a.info.id < b.info.id;
    });
    std::vector<ui::CommandInfo> out;
    out.reserve(scored.size());
    for (auto& entry : scored) out.push_back(std::move(entry.info));
    return out;
}
std::optional<std::size_t> CommandPalette::resolved_highlight(const std::vector<ui::CommandInfo>& commands) const {
    if (commands.empty()) return std::nullopt;
    const std::size_t chosen = std::min(highlighted_, commands.size() - 1);
    for (std::size_t index = chosen; index < commands.size(); ++index)
        if (enabled_command(commands[index].id)) return index;
    for (std::size_t index = chosen; index-- > 0;)
        if (enabled_command(commands[index].id)) return index;
    return std::nullopt;
}
std::optional<ui::CommandId> CommandPalette::highlighted_command() const {
    const auto commands = filtered_commands();
    const std::optional<std::size_t> row = resolved_highlight(commands);
    if (!row) return std::nullopt;
    return commands[*row].id;
}
void CommandPalette::move_highlight(int delta) {
    const auto commands = filtered_commands();
    const std::optional<std::size_t> from = resolved_highlight(commands);
    if (!from) return;
    // Past the disabled rows, as a menu's highlight goes, and no further than
    // the last enabled row in either direction.
    std::size_t index = *from;
    while (true) {
        if (delta > 0 ? index + 1 >= commands.size() : index == 0) return;
        index = delta > 0 ? index + 1 : index - 1;
        if (!enabled_command(commands[index].id)) continue;
        highlighted_ = index;
        invalidate();
        return;
    }
}
namespace {
// The palette's layout: a two-cell inset on every side — the outer ring for a
// popup's frame, the inner one blank — and a blank row between the search
// field and its results (see CommandPalette::draw).
constexpr int kPaletteInset = 2;
constexpr int kPaletteSearchRow = 2;
constexpr int kPaletteFirstResultRow = 4;
}  // namespace

std::size_t CommandPalette::first_visible(std::size_t highlighted) const {
    // The result rows stop one row short of the bottom when framed, the
    // frame's own row.
    const int visible_rows = std::max(0, bounds().height - kPaletteFirstResultRow - (framed_ ? 1 : 0));
    if (visible_rows == 0 || highlighted < static_cast<std::size_t>(visible_rows)) return 0;
    return highlighted - static_cast<std::size_t>(visible_rows) + 1;
}

std::optional<CursorState> CommandPalette::cursor_state() const {
    if (!has_focus()) return std::nullopt;
    const int field_width = std::max(0, bounds().width - 2 * kPaletteInset);
    if (field_width < 2) return std::nullopt;
    const int caret = std::min(text::text_width(query_), field_width - 2);
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + kPaletteInset + 1 + caret, absolute.y + kPaletteSearchRow},
                       CursorShape::Bar, false};
}

void CommandPalette::draw(scene::Painter& painter) {
    const Style input = context().theme->resolve(input_role_);
    const Style normal = context().theme->resolve(result_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style disabled = context().theme->resolve(disabled_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    // A popup has an edge. Drawn in the outer ring of the inset, so the
    // search field and the results sit exactly where they do unframed.
    if (framed_) painter.draw_box(Rect{0, 0, bounds().width, bounds().height}, scene::LineStyle::Single, normal);

    // The palette deliberately reserves a two-cell inset on every side: the
    // search field reads as an edit control, while its results read as a
    // separate, scrollable choice surface.  This is also what keeps a
    // constrained palette legible when it sits above document content.
    constexpr int kInset = kPaletteInset;
    constexpr int kSearchRow = kPaletteSearchRow;
    constexpr int kFirstResultRow = kPaletteFirstResultRow;
    const int result_width = std::max(0, bounds().width - 2 * kInset);
    painter.fill(Rect{kInset, kSearchRow, result_width, 1}, Cell::from_grapheme(" ", input));
    painter.draw_text(Point{kInset + 1, kSearchRow},
                      text::clip_to_width(query_, std::max(0, result_width - 1)), input);

    const auto commands = filtered_commands();
    const std::optional<std::size_t> highlighted = resolved_highlight(commands);
    const int visible_rows = std::max(0, bounds().height - kFirstResultRow - (framed_ ? 1 : 0));
    const std::size_t first = first_visible(highlighted.value_or(0));
    const bool needs_scrollbar = commands.size() > static_cast<std::size_t>(visible_rows);
    const int text_columns = std::max(0, result_width - (needs_scrollbar ? 1 : 0));
    const Style hotkey = context().theme->resolve(context().roles->find("ckv.hotkey"));
    const Style mnemonic = accent_style(normal, hotkey);
    const Style selected_mnemonic = accent_style(selected, hotkey);
    for (std::size_t index = first;
         index < commands.size() && index - first < static_cast<std::size_t>(visible_rows); ++index) {
        const bool enabled = enabled_command(commands[index].id);
        const bool is_highlighted = highlighted == index;
        const Style style = !enabled ? disabled : is_highlighted ? selected : normal;
        const int row = kFirstResultRow + static_cast<int>(index - first);
        painter.fill(Rect{kInset, row, result_width, 1}, Cell::from_grapheme(" ", style));
        const std::string chord = context().app->commands().chord_text(commands[index].id);
        const int chord_width = chord.empty() ? 0 : text::text_width(chord);
        const int chord_reserve = chord.empty() ? 0 : chord_width + 2;
        const int left_budget = std::max(0, text_columns - 1 - chord_reserve);
        int cursor_x = kInset + 1;
        if (!commands[index].category.empty() && left_budget > 0) {
            const std::string prefix = commands[index].category + ": ";
            const std::string clipped_prefix = text::clip_to_width(prefix, left_budget);
            Style dim_style = style;
            dim_style.attrs = dim_style.attrs | Attr::Dim;
            painter.draw_text(Point{cursor_x, row}, clipped_prefix, dim_style);
            cursor_x += text::text_width(clipped_prefix);
        }
        const int remaining_title = std::max(0, (kInset + 1 + left_budget) - cursor_x);
        const auto parsed = parse_mnemonic(commands[index].title);
        if (remaining_title > 0) {
            draw_mnemonic(painter, Point{cursor_x, row}, parsed, remaining_title, style,
                          !enabled ? style : is_highlighted ? selected_mnemonic : mnemonic);
        }
        if (!chord.empty() && chord_width < text_columns) {
            const int chord_x = kInset + text_columns - chord_width;
            painter.draw_text(Point{chord_x, row}, chord, style);
        }
    }
    if (needs_scrollbar && visible_rows > 0) {
        const int column = kInset + result_width - 1;
        const Style track = context().theme->resolve(context().roles->find("ckv.scrollbar.track"));
        const Style thumb = context().theme->resolve(context().roles->find("ckv.scrollbar.thumb"));
        // Same vocabulary as Scrollbar: arrows, a medium-shade page area
        // and a single-cell indicator, so a palette's gutter is not a
        // second, different-looking kind of scrollbar.
        painter.fill(Rect{column, kFirstResultRow, 1, visible_rows}, Cell::from_grapheme("▒", track));
        painter.draw_text(Point{column, kFirstResultRow}, "▲", track);
        painter.draw_text(Point{column, kFirstResultRow + visible_rows - 1}, "▼", track);
        const int travel = std::max(0, visible_rows - 3);
        const std::size_t maximum_first = commands.size() - static_cast<std::size_t>(visible_rows);
        const int thumb_row = kFirstResultRow + 1 +
                              (maximum_first == 0 ? 0 : static_cast<int>(first * static_cast<std::size_t>(travel) /
                                                                            maximum_first));
        painter.draw_text(Point{column, thumb_row}, "■", thumb);
    }
}
void CommandPalette::dismiss() {
    if (dismissed_ || !on_dismiss) return;
    dismissed_ = true;
    // Copied first: the call may destroy this palette, and the function with
    // it.
    const std::function<void()> callback = on_dismiss;
    callback();
}
bool CommandPalette::run(ui::CommandId id) {
    ui::Application* const app = context().app;
    if (app == nullptr) return false;
    std::erase(mru_order_, id);
    mru_order_.insert(mru_order_.begin(), id);
    if (mru_order_.size() > 32U) mru_order_.resize(32U);
    // Everything the command needs is taken off the palette before a popup
    // tears itself down, so the command runs after the focus has gone back
    // to where the reader was — a command that acts on "the active window" or
    // "the focused field" finds theirs, not the palette.
    const std::optional<std::vector<std::string>> contexts = invocation_contexts_;
    dismiss();
    if (contexts) return app->commands().execute(id, *contexts);
    return app->execute_command(id);
}
bool CommandPalette::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    if (event.chord.modifiers == Modifier::Ctrl && event.chord.key == Key::Char) {
        if (event.chord.text == "u" || event.chord.text == "U") {
            set_query("");
            return true;
        }
        if (event.chord.text == "w" || event.chord.text == "W") {
            std::string trimmed = query_;
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
                trimmed.pop_back();
            while (!trimmed.empty() && trimmed.back() != ' ' && trimmed.back() != '\t')
                trimmed.pop_back();
            set_query(std::move(trimmed));
            return true;
        }
    }
    // A typed character reaches the query the way a terminal reports it:
    // as a Key::Char event. Alt, Ctrl and Super chords stay the registry's.
    if (event.chord.key == Key::Char && !event.chord.text.empty() &&
        !has_modifier(event.chord.modifiers, Modifier::Alt) && !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super))
        return on_text(TextEvent{event.chord.text, false});
    const int visible_rows = std::max(1, bounds().height - kPaletteFirstResultRow - (framed_ ? 1 : 0));
    switch (event.chord.key) {
        case Key::Down: move_highlight(1); return true;
        case Key::Up: move_highlight(-1); return true;
        case Key::PageDown: move_highlight(visible_rows); return true;
        case Key::PageUp: move_highlight(-visible_rows); return true;
        case Key::Home: {
            highlighted_ = 0;
            invalidate();
            return true;
        }
        case Key::End: {
            const auto commands = filtered_commands();
            for (std::size_t i = commands.size(); i-- > 0;) {
                if (enabled_command(commands[i].id)) {
                    highlighted_ = i;
                    invalidate();
                    break;
                }
            }
            return true;
        }
        case Key::Backspace:
            if (query_.empty()) return false;
            set_query(without_last_grapheme(query_));
            return true;
        case Key::Enter: {
            // A popup owns Enter whatever happens: passing it on would reach
            // whatever is behind the palette the reader is looking at. Read
            // first, since running the command may destroy the popup.
            const bool popup = static_cast<bool>(on_dismiss);
            const std::optional<ui::CommandId> command = highlighted_command();
            const bool ran = command && run(*command);
            return ran || popup;
        }
        case Key::Escape:
            if (!on_dismiss) return false;
            dismiss();
            return true;
        default: return false;
    }
}
bool CommandPalette::on_text(const TextEvent& event) {
    if (!enabled_in_tree()) return false;
    std::string cleaned;
    cleaned.reserve(event.text.size());
    for (const char c : event.text) {
        if (c != '\r' && c != '\n') cleaned.push_back(c);
    }
    if (!cleaned.empty()) set_query(query_ + cleaned);
    return true;
}
bool CommandPalette::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) {
        scrollbar_dragging_ = false;
        return false;
    }
    const Rect abs = absolute_bounds();
    if (event.action == MouseAction::Up && scrollbar_dragging_) {
        scrollbar_dragging_ = false;
        return true;
    }
    if (!abs.contains(event.cell) && !scrollbar_dragging_) {
        // Only a popup holding the input capture sees presses outside it:
        // the light dismissal a menu has. Read first: dismissing may destroy
        // the palette.
        const bool popup = static_cast<bool>(on_dismiss);
        if (event.action == MouseAction::Down) dismiss();
        return popup;
    }
    if (event.action == MouseAction::Wheel) {
        if (event.button == MouseButton::WheelUp) move_highlight(-1);
        if (event.button == MouseButton::WheelDown) move_highlight(1);
        return true;
    }
    const auto commands = filtered_commands();
    const int row = event.cell.y - abs.y - kPaletteFirstResultRow;
    const int column = event.cell.x - abs.x;
    const int result_width = std::max(0, bounds().width - 2 * kPaletteInset);
    const int visible_rows = std::max(0, bounds().height - kPaletteFirstResultRow - (framed_ ? 1 : 0));
    const bool needs_scrollbar = commands.size() > static_cast<std::size_t>(visible_rows);
    const int scrollbar_col = kPaletteInset + result_width - 1;
    if (needs_scrollbar && visible_rows > 0 &&
        ((event.action == MouseAction::Down && event.button == MouseButton::Left &&
          column == scrollbar_col && row >= 0 && row < visible_rows) ||
         (scrollbar_dragging_ && event.action == MouseAction::Move))) {
        if (event.action == MouseAction::Down && row == 0) {
            move_highlight(-1);
            return true;
        }
        if (event.action == MouseAction::Down && row == visible_rows - 1) {
            move_highlight(1);
            return true;
        }
        scrollbar_dragging_ = true;
        const int clamped_row = std::clamp(row - 1, 0, std::max(0, visible_rows - 3));
        const int travel = std::max(1, visible_rows - 3);
        const std::size_t target =
            commands.empty() ? 0 : (static_cast<std::size_t>(clamped_row) * (commands.size() - 1)) /
                                       static_cast<std::size_t>(travel);
        highlighted_ = std::min(target, commands.size() - 1);
        invalidate();
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return true;
    if (row < 0 || row >= visible_rows || column < kPaletteInset || column >= bounds().width - kPaletteInset)
        return true;
    const std::size_t index = first_visible(resolved_highlight(commands).value_or(0)) + static_cast<std::size_t>(row);
    if (index >= commands.size() || !enabled_command(commands[index].id)) return true;
    highlighted_ = index;
    invalidate();
    run(commands[index].id);
    return true;
}
void CommandPalette::on_focus(const FocusEvent&) { invalidate(); }
void CommandPalette::on_attached() {
    if (input_role_ == ui::kInvalidRole) input_role_ = context().roles->find("ckv.input.normal");
    if (result_role_ == ui::kInvalidRole) result_role_ = context().roles->find("ckv.option.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.option.focused");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.option.disabled");
}

CommandPalette* show_command_palette(ui::Application& app, Desktop& desktop) {
    // Read before the palette takes the focus: it lists what the reader's
    // own place allows, and gives the focus back there when it goes.
    ui::View* const invoker = app.focused();
    const detail::DialogFocusRestore focus_restore{invoker};
    auto palette = std::make_unique<CommandPalette>();
    palette->set_framed(true);
    palette->set_invocation_contexts(ui::command_context_path(invoker));
    CommandPalette* const raw = desktop.add_popup(std::move(palette));
    // Centred across the content, near its top: where the eye already is
    // after reaching for a chord, and clear of the text being worked on in
    // the middle of the screen.
    const Rect area = desktop.content_area();
    const int width = std::min(56, area.width);
    const int height = std::min(14, area.height);
    raw->set_bounds(Rect{area.x + (area.width - width) / 2, area.y + std::min(1, area.height - height), width,
                         height});
    const ui::Application::ModalScopeId scope = app.push_modal(*raw);
    raw->on_dismiss = [&app, &desktop, raw, scope, focus_restore] {
        if (app.input_capture() == raw) app.clear_input_capture();
        app.pop_modal(scope);
        focus_restore.restore(app);
        desktop.remove_popup(raw);  // discards ownership -> destroys the palette
    };
    app.set_input_capture(raw);
    app.set_focus(raw);
    return raw;
}


}  // namespace ckv::widgets
