// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace ckv::widgets {

// User-visible strings owned by ckVision's standard dialog factories.
// Applications pass a translated table when constructing dialogs; the
// default table preserves the built-in English UI without global mutable
// state or per-widget hardcoded labels.
struct StandardStrings {
    // Button captions follow. Each becomes a Button's text, so a translation
    // may carry an '&' mnemonic marker the way copy_to_clipboard does.
    //
    // The message box's answer buttons: `ok` for MessageBoxButtons::Ok and
    // OkCancel, `yes` and `no` for the YesNo variants. `ok` and `cancel` are
    // also the date and time dialogs' buttons, and `cancel` the dismiss
    // button of the file dialog, the directory picker and the window list.
    std::string ok = "OK";
    std::string cancel = "Cancel";
    std::string yes = "Yes";
    std::string no = "No";
    // The dismiss button of the help viewer and the terminal report.
    std::string close = "Close";
    // The window list's two buttons that act on the window under its cursor:
    // `switch_to_window` activates it (the default button), `close_window`
    // asks it to close.
    std::string switch_to_window = "&Switch To";
    std::string close_window = "Close &Window";
    // The help viewer's button that returns to the topic shown before the
    // current one.
    std::string back = "Back";
    // The file dialog's confirming button: `open` in FileDialogMode::Open,
    // `save` in the save mode.
    std::string open = "Open";
    std::string save = "Save";
    // The directory picker's confirming button.
    std::string select = "Select";
    // The file dialog's filter button. When the dialog was given filters the
    // button reads this word, ": ", and the active filter's label.
    std::string filter = "Filter";
    // The file dialog's toggle for names beginning with '.'. It names the
    // action a press would take: `show_hidden` while such entries are hidden,
    // `hide_hidden` while they are listed.
    std::string show_hidden = "Show Hidden";
    std::string hide_hidden = "Hide Hidden";
    // The terminal report's copy button.
    // Mnemonic included: the terminal report's focus starts in its text
    // viewport, so without Alt+C the copy action is reachable only by Tab.
    std::string copy_to_clipboard = "&Copy to clipboard";

    // Window titles of the standard dialogs, drawn in the frame's top border:
    // the file dialog in its open and save modes, the directory picker, the
    // window list, the terminal report and the help viewer.
    std::string open_file_title = "Open File";
    std::string save_file_title = "Save File";
    std::string select_directory_title = "Select Directory";
    std::string window_list_title = "Window List";
    std::string terminal_report_title = "Terminal report";
    std::string help_title = "Help";
    // Window titles of the standard date and time dialogs. Their other words
    // -- month and weekday names, the meridiem -- come from the
    // DateTimeLabels table each is given.
    std::string select_date_title = "Select Date";
    std::string select_time_title = "Select Time";
    // Introduces a topic's curated cross-references at the foot of its page,
    // each one a link the reader can follow like the links in the prose.
    std::string help_see_also = "See also:";

    // The theme editor. The title heads its window. `theme_role` and
    // `theme_sample` head the first two columns of its role table;
    // `theme_foreground`, `theme_background` and `theme_attributes` head the
    // other three and label the editors below the table, where their '&'
    // marks the mnemonic that reaches each editor (the table's headings drop
    // the marker). The underline's editors have labels of their own below.
    // `theme_sample` is also the text of the table's sample cells, drawn in
    // each role's style.
    std::string theme_editor_title = "Edit Theme";
    std::string theme_role = "Role";
    std::string theme_sample = "Sample";
    std::string theme_foreground = "&Foreground";
    std::string theme_background = "&Background";
    std::string theme_attributes = "A&ttributes";
    // The three kinds of colour a colour editor chooses between: the
    // terminal's own colour, a palette entry, and a 24-bit colour.
    std::string theme_color_default = "Default";
    std::string theme_color_palette = "Palette";
    std::string theme_color_rgb = "RGB";
    // The attribute choices, in the order of ckv::Attr's flags. The role
    // table lists a role's attributes by the same names.
    std::string theme_bold = "Bold";
    std::string theme_dim = "Dim";
    std::string theme_italic = "Italic";
    std::string theme_underline = "Underline";
    std::string theme_reverse = "Reverse";
    std::string theme_strike = "Strike";
    // The labels of the two underline editors below the attribute check
    // boxes, each with the '&' mnemonic that reaches its editor: the shape
    // the rule is drawn with, and the colour it is drawn in.
    std::string theme_underline_shape = "&Underline";
    std::string theme_underline_color = "&Line colour";
    // The underline shape choices, in the order of ckv::UnderlineShape.
    std::string theme_shape_straight = "Straight";
    std::string theme_shape_double = "Double";
    std::string theme_shape_curly = "Curly";
    std::string theme_shape_dotted = "Dotted";
    std::string theme_shape_dashed = "Dashed";
    // The button that puts the selected role back the way the editor found it.
    std::string theme_revert = "&Revert";
    // What the live preview shows: a field's label and text, a check box's
    // caption and a list's two rows, beside an `ok` and a `cancel` button.
    // Sample content only; the reader cannot focus or change any of it.
    std::string theme_preview_label = "Name:";
    std::string theme_preview_text = "Ada";
    std::string theme_preview_option = "Remember";
    std::string theme_preview_first = "Alpha";
    std::string theme_preview_second = "Beta";
};

// The built-in English table, the default argument of every standard dialog
// factory. It is one immutable instance, so the reference stays valid for the
// life of the process.
const StandardStrings& english_standard_strings() noexcept;

}  // namespace ckv::widgets
