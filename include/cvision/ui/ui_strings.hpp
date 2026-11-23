// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ckv::ui {

// Unified table of all user-visible strings owned by Application (`app.strings()`)
// and read by ckVision's widgets and standard dialog factories. Applications
// customize or translate UI copy once via `app.set_strings(...)` instead of
// passing per-dialog string tables.
struct CVISION_TYPE_API UiStrings {
    // --- Standard dialog buttons -----------------------------------------
    std::string ok = "OK";
    std::string cancel = "Cancel";
    std::string yes = "Yes";
    std::string no = "No";
    std::string close = "Close";
    std::string switch_to_window = "&Switch To";
    std::string close_window = "Close &Window";
    std::string back = "Back";
    std::string open = "Open";
    std::string save = "Save";
    std::string select = "Select";
    std::string filter = "Filter";
    std::string show_hidden = "Show Hidden";
    std::string hide_hidden = "Hide Hidden";
    std::string previous_page = "Previous page";
    std::string next_page = "Next page";
    std::string refresh_directory = "Refresh";
    std::string completion_help = "Right: complete | Tab: focus | Enter directory: navigate";
    std::string copy_to_clipboard = "&Copy to clipboard";

    // --- Standard dialog titles & prose ----------------------------------
    std::string open_file_title = "Open File";
    std::string save_file_title = "Save File";
    std::string select_directory_title = "Select Directory";
    std::string window_list_title = "Window List";
    std::string terminal_report_title = "Terminal report";
    std::string help_title = "Help";
    std::string select_date_title = "Select Date";
    std::string select_time_title = "Select Time";
    std::string help_see_also = "See also:";

    // --- Theme editor ----------------------------------------------------
    std::string theme_editor_title = "Edit Theme";
    std::string theme_role = "Role";
    std::string theme_sample = "Sample";
    std::string theme_foreground = "&Foreground";
    std::string theme_background = "&Background";
    std::string theme_attributes = "A&ttributes";
    std::string theme_color_default = "Default";
    std::string theme_color_palette = "Palette";
    std::string theme_color_rgb = "RGB";
    std::string theme_bold = "Bold";
    std::string theme_dim = "Dim";
    std::string theme_italic = "Italic";
    std::string theme_underline = "Underline";
    std::string theme_reverse = "Reverse";
    std::string theme_strike = "Strike";
    std::string theme_underline_shape = "&Underline";
    std::string theme_underline_color = "&Line colour";
    std::string theme_shape_straight = "Straight";
    std::string theme_shape_double = "Double";
    std::string theme_shape_curly = "Curly";
    std::string theme_shape_dotted = "Dotted";
    std::string theme_shape_dashed = "Dashed";
    std::string theme_revert = "&Revert";
    std::string theme_preview_label = "Name:";
    std::string theme_preview_text = "Ada";
    std::string theme_preview_option = "Remember";
    std::string theme_preview_first = "Alpha";
    std::string theme_preview_second = "Beta";

    // --- Date & time widgets ---------------------------------------------
    std::vector<std::string> month_names{"January", "February", "March",     "April",   "May",      "June",
                                         "July",    "August",   "September", "October", "November", "December"};
    std::vector<std::string> weekday_names{"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    std::string am = "AM";
    std::string pm = "PM";
    std::string no_date = "— no date —";
    std::string not_a_date = "Not a date.";
    std::string invalid_year = "invalid";

    // --- Property inspector ----------------------------------------------
    std::string whole_number = "Must be a whole number";
    std::string number = "Must be a number";
    std::string at_least = "Must be at least";
    std::string at_most = "Must be at most";

    // --- Wizard ----------------------------------------------------------
    std::string wizard_back = "< Back";
    std::string wizard_next = "Next >";
    std::string wizard_finish = "Finish";
    std::string wizard_cancel = "Cancel";
    // Formats the step indicator for 1-based page `current` of `count`.
    std::function<std::string(std::size_t current, std::size_t count)> wizard_step =
        [](std::size_t current, std::size_t count) {
            return "Step " + std::to_string(current) + " of " + std::to_string(count);
        };

    // --- Desktop, window & command palette -------------------------------
    std::string command_palette_placeholder = "Type a command or action...";
    // Default SearchBox prompt prefix.
    std::string search_box_prompt = "Search ";
    // Diagnostic shown when editing a read-only Table cell.
    std::string table_cell_read_only = "This cell is read-only";
    // Messages shown when the terminal is smaller than the minimum floor.
    std::string terminal_too_small = "Terminal too small";
    std::string resize_to_at_least = "Resize to at least";
    std::string too_small_prefix = "Too small:";
    std::string too_small_need = "need ";
    std::string window_menu_restore = "&Restore";
    std::string window_menu_move = "&Move";
    std::string window_menu_size = "&Size";
    std::string window_menu_minimize = "Mi&nimize";
    std::string window_menu_maximize = "Ma&ximize";
    std::string window_menu_close = "&Close";
};

// The built-in English table, valid for the life of the process.
CVISION_API const UiStrings& english_ui_strings() noexcept;

}  // namespace ckv::ui
