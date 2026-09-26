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
    // OkCancel, `yes` and `no` for the YesNo variants. `cancel` is also the
    // dismiss button of the file dialog and the directory picker.
    std::string ok = "OK";
    std::string cancel = "Cancel";
    std::string yes = "Yes";
    std::string no = "No";
    // The dismiss button of the help viewer, the terminal report and the
    // window list.
    std::string close = "Close";
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
    // Introduces a topic's curated cross-references at the foot of its page.
    // They are prose, not a second list: every topic they name is already
    // present in the viewer's own index pane.
    std::string help_see_also = "See also:";
};

// The built-in English table, the default argument of every standard dialog
// factory. It is one immutable instance, so the reference stays valid for the
// life of the process.
const StandardStrings& english_standard_strings() noexcept;

}  // namespace ckv::widgets
