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
    std::string ok = "OK";
    std::string cancel = "Cancel";
    std::string yes = "Yes";
    std::string no = "No";
    std::string close = "Close";
    std::string back = "Back";
    std::string open = "Open";
    std::string save = "Save";
    std::string select = "Select";
    std::string filter = "Filter";
    std::string show_hidden = "Show Hidden";
    std::string hide_hidden = "Hide Hidden";
    // Mnemonic included: the terminal report's focus starts in its text
    // viewport, so without Alt+C the copy action is reachable only by Tab.
    std::string copy_to_clipboard = "&Copy to clipboard";

    std::string open_file_title = "Open File";
    std::string save_file_title = "Save File";
    std::string select_directory_title = "Select Directory";
    std::string window_list_title = "Window List";
    std::string terminal_report_title = "Terminal report";
    std::string help_title = "Help";
    // Introduces a topic's curated cross-references at the foot of its page.
    // They are prose, not a second list: every topic they name is already
    // present in the viewer's own index pane.
    std::string help_see_also = "See also:";
};

const StandardStrings& english_standard_strings() noexcept;

}  // namespace ckv::widgets
