// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/theme_editor.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/palette.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme_format.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/table.hpp"

using ckv::Attr;
using ckv::Color;
using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::Style;
using ckv::UnderlineShape;
using ckv::ui::Application;
using ckv::ui::RoleId;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::present_modal_theme_editor;
using ckv::widgets::ThemeEditorPresentation;
using ckv::widgets::ThemeEditorResult;

namespace {

// An 80x24 application with the classic scheme and a desktop, as an example
// application has one.
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Fixture() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
    }

    ThemeEditorPresentation open() {
        ThemeEditorPresentation presentation = present_modal_theme_editor(app.theme(), app, *desktop, roles);
        app.step(0);
        return presentation;
    }

    void press(Key which, Modifier modifiers = Modifier::None) {
        app.dispatch(ckv::KeyEvent{KeyChord{which, modifiers, ""}});
        app.step(0);
    }

    void type(std::string_view text) {
        for (const char c : text) app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, std::string(1, c)}});
        app.step(0);
    }

    void click(Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt});
        app.step(0);
    }

    // Where `text` first appears on the composed frame, row by row, or
    // nullopt when it is not on screen.
    std::optional<Point> find(std::string_view text, int from_row = 0) const {
        const ckv::FrameView frame = app.current_frame();
        for (int y = from_row; y < frame.size().height; ++y) {
            std::string row;
            std::vector<int> columns;
            for (int x = 0; x < frame.size().width; ++x) {
                const std::string_view grapheme = frame.at(Point{x, y}).grapheme();
                for (std::size_t byte = 0; byte < grapheme.size(); ++byte) columns.push_back(x);
                row += grapheme;
            }
            const std::size_t at = row.find(text);
            if (at != std::string::npos) return Point{columns[at], y};
        }
        return std::nullopt;
    }

    Style style_at(Point cell) const { return app.current_frame().at(cell).style(); }

    // The first cell of the preview's sample of `name`: the role's name drawn
    // in its own style, below the editors. The role table above them lists
    // the same name, so the search starts at the editors' Attributes label,
    // which follows the table's Attributes heading.
    std::optional<Point> sample(const std::string& name) const {
        const std::optional<Point> heading = find("Attributes");
        const std::optional<Point> label = heading ? find("Attributes", heading->y + 1) : std::nullopt;
        const std::optional<Point> found = label ? find(" " + name + " ", label->y) : std::nullopt;
        return found ? std::optional<Point>{Point{found->x + 1, found->y}} : std::nullopt;
    }

    // Moves the role table's cursor, from its first row, to `name`.
    void select_role(const std::string& name) {
        std::vector<std::string> names;
        for (std::size_t role = 0; role < app.roles().size(); ++role)
            names.push_back(app.roles().name(static_cast<RoleId>(role)));
        std::sort(names.begin(), names.end());
        const auto index = static_cast<std::size_t>(std::find(names.begin(), names.end(), name) - names.begin());
        for (std::size_t step = 0; step < index; ++step) app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
        app.step(0);
    }
};

}  // namespace

CK_TEST(the_theme_editor_lists_every_role_of_the_registry_sorted_by_name_with_its_style) {
    Fixture f;
    ThemeEditorPresentation presentation = f.open();
    auto* table = dynamic_cast<ckv::widgets::Table*>(f.app.focused());
    CK_CHECK(table != nullptr);
    if (table == nullptr) return;
    CK_CHECK(table->row_count() == f.app.roles().size());
    ckv::widgets::TableModel* model = table->model();
    CK_CHECK(model != nullptr);
    if (model == nullptr) return;
    std::string previous;
    for (std::size_t row = 0; row < model->row_count(); ++row) {
        const ckv::widgets::TableRowId id = model->row_id_at(row);
        const std::string name = model->cell({id, 0}).display;
        CK_CHECK(previous < name);
        previous = name;
        const Style style = f.app.theme().resolve(f.app.roles().find(name));
        CK_CHECK(model->cell({id, 1}).style == std::optional<Style>{style});
        CK_CHECK(model->cell({id, 2}).display == ckv::ui::format_color(style.fg));
        CK_CHECK(model->cell({id, 3}).display == ckv::ui::format_color(style.bg));
    }
    // The dialog fits an 80x24 application under a menu bar and a status line.
    ckv::widgets::Window* dialog = f.desktop->active_window();
    CK_CHECK(dialog != nullptr);
    if (dialog != nullptr) CK_CHECK(dialog->bounds().width == 78 && dialog->bounds().height == 22);
    CK_CHECK(!presentation.completed());
}

CK_TEST(changing_a_roles_foreground_by_keyboard_shows_it_live_and_ok_returns_the_edited_theme) {
    Fixture f;
    const Theme before = f.app.theme();
    ThemeEditorPresentation presentation = f.open();
    f.select_role("ckv.tooltip");
    CK_CHECK(f.sample("ckv.tooltip").has_value());

    f.press(Key::Tab);   // the foreground's kind: RGB in the classic scheme
    f.press(Key::Left);  // Palette: the nearest entry to the RGB colour
    const std::optional<Point> converted = f.sample("ckv.tooltip");
    CK_CHECK(converted.has_value());
    if (converted) CK_CHECK(f.style_at(*converted).fg.is_indexed());
    f.press(Key::Tab);   // the value field, its text selected on arrival
    f.type("196");

    // Live: the preview's sample of the role already wears the new colour,
    // and the application's own theme is untouched.
    const std::optional<Point> sample = f.sample("ckv.tooltip");
    CK_CHECK(sample.has_value());
    if (sample) CK_CHECK(f.style_at(*sample).fg == Color::indexed(196));
    CK_CHECK(f.app.theme().resolve(f.roles.tooltip) == before.resolve(f.roles.tooltip));

    f.press(Key::Enter);
    CK_CHECK(presentation.completed());
    const std::optional<ThemeEditorResult> result = presentation.result();
    CK_CHECK(result.has_value() && result->theme.has_value());
    if (!result || !result->theme) return;
    Style expected = before.resolve(f.roles.tooltip);
    expected.fg = Color::indexed(196);
    CK_CHECK(result->theme->resolve(f.roles.tooltip) == expected);
    for (std::size_t role = 0; role < f.app.roles().size(); ++role) {
        if (static_cast<RoleId>(role) == f.roles.tooltip) continue;
        CK_CHECK(result->theme->resolve(static_cast<RoleId>(role)) == before.resolve(static_cast<RoleId>(role)));
    }
    CK_CHECK(f.desktop->windows().empty());
}

CK_TEST(escape_cancels_the_theme_editor_with_no_theme_and_restores_focus) {
    Fixture f;
    auto* holder = f.desktop->add_window(std::make_unique<ckv::widgets::Window>("Document"));
    auto content = std::make_unique<ckv::ui::View>(Rect{}, ckv::ui::FocusPolicy::TabStop);
    ckv::ui::View* field = content.get();
    holder->set_content(std::move(content));
    f.app.set_focus(field);
    ThemeEditorPresentation presentation = f.open();
    CK_CHECK(f.app.focused() != field);
    f.select_role("ckv.tooltip");
    f.press(Key::Tab);
    f.press(Key::Left);
    f.press(Key::Escape);
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result().has_value() && !presentation.result()->theme.has_value());
    CK_CHECK(f.app.focused() == field);
}

CK_TEST(a_value_the_colour_kind_cannot_hold_changes_nothing_until_it_is_valid) {
    Fixture f;
    const Theme before = f.app.theme();
    ThemeEditorPresentation presentation = f.open();
    f.select_role("ckv.tooltip");
    f.press(Key::Tab);
    f.press(Key::Tab);  // the foreground value, an RGB colour in the classic scheme
    f.type("#12xz");  // a letter that is not a hexadecimal digit is refused
    const std::optional<Point> sample = f.sample("ckv.tooltip");
    CK_CHECK(sample.has_value());
    if (sample) CK_CHECK(f.style_at(*sample).fg == before.resolve(f.roles.tooltip).fg);
    f.type("34Ab");
    if (sample) CK_CHECK(f.style_at(*sample).fg == Color::rgb(0x12, 0x34, 0xAB));
    f.press(Key::Enter);
    const std::optional<ThemeEditorResult> result = presentation.result();
    CK_CHECK(result.has_value() && result->theme.has_value());
    if (result && result->theme) CK_CHECK(result->theme->resolve(f.roles.tooltip).fg == Color::rgb(0x12, 0x34, 0xAB));
}

CK_TEST(choosing_a_colour_kind_converts_the_colour_the_role_had) {
    Fixture f;
    // A theme whose tooltip has a palette foreground and a default
    // background, so every conversion is reachable from it.
    Theme theme = f.app.theme();
    theme.set(f.roles.tooltip, Style{Color::indexed(9), Color::default_color(), Attr{}});
    ThemeEditorPresentation presentation = present_modal_theme_editor(theme, f.app, *f.desktop, f.roles);
    f.app.step(0);
    f.select_role("ckv.tooltip");
    f.press(Key::Tab);
    f.press(Key::Right);  // foreground: Palette -> RGB, the colour entry 9 names
    f.press(Key::Tab);
    f.press(Key::Tab);
    f.press(Key::Right);  // background: Default -> Palette, entry 0 for a background
    f.press(Key::Enter);
    const std::optional<ThemeEditorResult> result = presentation.result();
    CK_CHECK(result.has_value() && result->theme.has_value());
    if (!result || !result->theme) return;
    const Style style = result->theme->resolve(f.roles.tooltip);
    CK_CHECK(style.fg == ckv::palette_color(9));
    CK_CHECK(style.bg == Color::indexed(0));
}

CK_TEST(clearing_underline_drops_its_shape_and_colour_and_revert_restores_the_role) {
    Fixture f;
    Theme theme = f.app.theme();
    const Style curly{Color::indexed(1), Color::indexed(15), Attr::Underline | Attr::Bold, UnderlineShape::Curly,
                      Color::indexed(9)};
    theme.set(f.roles.tooltip, curly);
    {
        ThemeEditorPresentation presentation = present_modal_theme_editor(theme, f.app, *f.desktop, f.roles);
        f.app.step(0);
        f.select_role("ckv.tooltip");
        for (int tab = 0; tab < 5; ++tab) f.press(Key::Tab);  // the attribute check boxes, at Bold
        f.press(Key::Down);                                   // Underline, below Bold in three columns
        f.type(" ");
        f.press(Key::Enter);
        const std::optional<ThemeEditorResult> result = presentation.result();
        CK_CHECK(result.has_value() && result->theme.has_value());
        if (result && result->theme)
            CK_CHECK(result->theme->resolve(f.roles.tooltip) ==
                     (Style{Color::indexed(1), Color::indexed(15), Attr::Bold}));
    }
    {
        ThemeEditorPresentation presentation = present_modal_theme_editor(theme, f.app, *f.desktop, f.roles);
        f.app.step(0);
        f.select_role("ckv.tooltip");
        for (int tab = 0; tab < 5; ++tab) f.press(Key::Tab);
        f.type(" ");  // clear Bold
        const std::optional<Point> sample = f.sample("ckv.tooltip");
        CK_CHECK(sample.has_value());
        if (sample) CK_CHECK(f.style_at(*sample).attrs == Attr::Underline);
        const std::optional<Point> revert = f.find("Revert");
        CK_CHECK(revert.has_value());
        if (!revert) return;
        f.click(*revert);
        if (sample) CK_CHECK(f.style_at(*sample) == curly);
        // Enter would press the focused Revert again; the reader clicks OK.
        const std::optional<Point> ok = f.find(" OK ", revert->y);
        CK_CHECK(ok.has_value());
        if (ok) f.click(Point{ok->x + 1, ok->y});
        const std::optional<ThemeEditorResult> result = presentation.result();
        CK_CHECK(result.has_value() && result->theme.has_value());
        if (result && result->theme) CK_CHECK(result->theme->resolve(f.roles.tooltip) == curly);
    }
}

CK_TEST(the_theme_editor_is_operable_by_mouse_alone) {
    Fixture f;
    const Theme before = f.app.theme();
    ThemeEditorPresentation presentation = f.open();
    // The table's first row is the first role by name.
    const std::optional<Point> row = f.find("ckv.button.focused");
    CK_CHECK(row.has_value());
    if (!row) return;
    f.click(*row);
    CK_CHECK(f.sample("ckv.button.focused").has_value());
    // The table's heading names the column; the editor's label is below it.
    const std::optional<Point> heading = f.find("Background");
    const std::optional<Point> background = heading ? f.find("Background", heading->y + 1) : std::nullopt;
    CK_CHECK(background.has_value());
    if (!background) return;
    const std::optional<Point> default_kind = f.find("Default", background->y);
    CK_CHECK(default_kind.has_value() && default_kind->y == background->y);
    if (default_kind) f.click(*default_kind);
    const std::optional<Point> italic = f.find("Italic", background->y);
    CK_CHECK(italic.has_value());
    if (italic) f.click(*italic);
    // The dialog's own OK, beside Revert; the preview's sample OK is above it.
    const std::optional<Point> revert = f.find("Revert");
    const std::optional<Point> ok = revert ? f.find(" OK ", revert->y) : std::nullopt;
    CK_CHECK(ok.has_value());
    if (ok) f.click(Point{ok->x + 1, ok->y});
    CK_CHECK(presentation.completed());
    const std::optional<ThemeEditorResult> result = presentation.result();
    CK_CHECK(result.has_value() && result->theme.has_value());
    if (!result || !result->theme) return;
    Style expected = before.resolve(f.roles.button_focused);
    expected.bg = Color::default_color();
    expected.attrs = expected.attrs | Attr::Italic;
    CK_CHECK(result->theme->resolve(f.roles.button_focused) == expected);
}

CK_TEST(a_column_title_sorts_the_role_table_and_the_cursor_stays_on_its_role) {
    Fixture f;
    ThemeEditorPresentation presentation = f.open();
    auto* table = dynamic_cast<ckv::widgets::Table*>(f.app.focused());
    CK_CHECK(table != nullptr);
    if (table == nullptr) return;
    f.select_role("ckv.tooltip");
    const std::optional<ckv::widgets::TableCellRef> before = table->selected_cell();
    const std::optional<Point> heading = f.find("Background");
    CK_CHECK(heading.has_value());
    if (!heading) return;
    f.click(*heading);
    CK_CHECK(table->sort_column() == 3);
    ckv::widgets::TableModel* model = table->model();
    std::string previous;
    for (std::size_t row = 0; model != nullptr && row < model->row_count(); ++row) {
        const std::string background = model->cell({model->row_id_at(row), 3}).display;
        CK_CHECK(previous <= background);
        previous = background;
    }
    CK_CHECK(before.has_value() && table->selected_cell().has_value() && table->selected_cell()->row == before->row);
}
