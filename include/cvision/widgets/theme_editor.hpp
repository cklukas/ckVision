// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Theme editor: the standard dialog that lists a theme's roles from its flat
// role table, edits a role's colours and attributes with typed editors, shows
// the edit live in a preview of representative widgets, and returns the edited
// theme as a value (the widget catalog "Theme editor", ROADMAP M11). It applies
// nothing and saves nothing: the application installs the theme it gets back
// with Application::set_theme, and persists it — ui::serialize_theme gives the
// text — wherever it keeps its settings.
#pragma once

#include <functional>
#include <optional>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

// The answer of a theme editor.
struct ThemeEditorResult {
    // The edited theme when the reader accepted, built over the same registry
    // as the theme the editor opened with; empty when they cancelled. A role
    // the reader never touched resolves exactly as it did before.
    std::optional<ui::Theme> theme;
};
// The handle present_modal_theme_editor returns; see DialogPresentation.
using ThemeEditorPresentation = DialogPresentation<ThemeEditorResult>;

// Builds the theme editor window, unattached. It edits a copy of `theme`; the
// theme itself is never changed.
//
// The window holds, from the top:
//   * the role table: every role of the theme's registry, sorted by name so
//     that each family (`ckv.window.*`) reads as one group, with the style it
//     resolves to — a sample drawn in that style, then its foreground and
//     background in the theme text's colour spelling (ui::format_color) and
//     its attributes by name. A column title sorts the table by that column;
//   * the editors for the role under the table's cursor: for the foreground
//     and for the background, a choice between the terminal's default colour,
//     a palette entry and an RGB colour with a field for the value (a decimal
//     index 0-255, or #RRGGBB); a check box per attribute; and the
//     underline's own two editors, a choice among the UnderlineShape values
//     and a colour editor of the same kind-and-value form for the colour the
//     rule is drawn in, both enabled only while Underline is checked;
//   * the live preview: the role's own name drawn in its style, and a label,
//     a field, two buttons, a check box and a list, all drawn with the edited
//     theme;
//   * the OK, Cancel and Revert buttons.
//
// Every change takes effect in the table and the preview at once. Choosing
// another colour kind converts the colour: a palette entry to the RGB value it
// names, an RGB colour to the nearest palette entry, and the default to the
// colour it stands for — the entry a terminal conventionally uses for a
// foreground (7) or a background (0), and for the underline the text's own
// colour, which a default underline follows. A value field admits only what
// its kind can spell — decimal digits for a palette index, '#' and
// hexadecimal digits for RGB — and while it does not hold a valid value it
// shows as invalid and changes nothing. Clearing Underline also drops the
// underline's shape and colour, as the Style contract asks, and disables
// their editors. Revert puts the role back to the style it had when the
// editor opened, underline included.
//
// OK (and Enter where the focused control leaves it unhandled) fires
// `on_result` with the edited theme; Cancel and Escape fire it with an empty
// result. It fires exactly once and then the window closes, restores focus to
// `restore_focus_to` (which may be nullptr) and schedules its own detach. The
// handle's initial_focus is the role table. The theme's registry must outlive
// the window; the labels it keeps from `strings` are copied, so `strings`
// need only live for the call. The window is not resizable, and asks for
// 78x22 cells: as much of an 80x24 terminal as a menu bar, a status line and
// the window's shadow leave.
WindowHandle make_theme_editor(const ui::Theme& theme, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(ThemeEditorResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

// Builds the editor (make_theme_editor, restoring focus to whatever was
// focused when called) and presents it modally on `desktop` without a nested
// loop. Completion occurs only after detachment; an accepted theme wins,
// while Cancel, close, external detach and quit resolve to an empty result.
// `strings` is copied.
[[nodiscard]] ThemeEditorPresentation present_modal_theme_editor(const ui::Theme& theme, ui::Application& app,
                                                            Desktop& desktop, const ui::StandardRoles& roles,
                                                            const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
