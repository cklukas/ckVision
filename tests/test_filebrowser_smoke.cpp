// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// End-to-end smoke test for the File Browser example
// (examples/filebrowser/filebrowser_app.cpp), driven headlessly
// against a scripted MemoryFileSystem exactly the way the interactive
// main.cpp drives it against the real disk — the master-detail
// pattern (TreeView selection driving a ListView's contents) is
// exercised through the real Application::dispatch/step pipeline, not
// a simplified stand-in.
#include "cktest.hpp"
#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "filebrowser_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MemoryFileSystem;
using ckv::ui::Application;

namespace {
MemoryFileSystem make_scripted_tree() {
    MemoryFileSystem fs;
    fs.add_directory("/root");
    fs.add_directory("/root/alpha");
    fs.add_file("/root/alpha/a1.txt");
    fs.add_file("/root/alpha/a2.txt");
    fs.add_directory("/root/beta");
    fs.add_file("/root/beta/b1.txt");
    fs.add_directory("/root/beta/nested");
    fs.add_file("/root/readme.txt");  // a file directly under root, not a directory
    return fs;
}

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    MemoryFileSystem fs = make_scripted_tree();
    ckv::filebrowser::FileBrowserApp browser{app, fs, "/root"};
};
}  // namespace

CK_TEST(the_tree_starts_with_root_selected_and_the_file_list_showing_its_direct_children) {
    Fixture f;
    // "/root" itself has one file (readme.txt) and two subdirectories
    // (alpha, beta) as direct entries.
    CK_CHECK(f.browser.selected_directory() == "/root");
    const auto& items = f.browser.file_list()->items();
    CK_CHECK(items.size() == 3);
}

CK_TEST(directories_in_the_file_list_are_suffixed_with_a_trailing_slash) {
    Fixture f;
    const auto& items = f.browser.file_list()->items();
    bool found_alpha_dir = false;
    for (const auto& item : items)
        if (item == "alpha/") found_alpha_dir = true;
    CK_CHECK(found_alpha_dir);
}

CK_TEST(navigating_the_tree_to_a_different_folder_updates_the_file_list) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});  // root -> alpha (first child)
    CK_CHECK(f.browser.selected_directory() == "/root/alpha");
    const auto& items = f.browser.file_list()->items();
    CK_CHECK(items.size() == 2);
    bool found_a1 = false, found_a2 = false;
    for (const auto& item : items) {
        if (item == "a1.txt") found_a1 = true;
        if (item == "a2.txt") found_a2 = true;
    }
    CK_CHECK(found_a1 && found_a2);
}

CK_TEST(navigating_to_a_folder_with_a_nested_subdirectory_shows_it_suffixed) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});  // alpha
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});  // beta
    CK_CHECK(f.browser.selected_directory() == "/root/beta");
    const auto& items = f.browser.file_list()->items();
    bool found_nested = false;
    for (const auto& item : items)
        if (item == "nested/") found_nested = true;
    CK_CHECK(found_nested);
}

CK_TEST(the_first_frame_renders_both_panes_and_the_menu_status_chrome) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("File Browser") != std::string::npos);
    CK_CHECK(bytes.find("File") != std::string::npos);   // menu bar
    CK_CHECK(bytes.find("Quit") != std::string::npos);   // status line
    CK_CHECK(bytes.find("root") != std::string::npos);   // tree root label
}

CK_TEST(the_frame_overlay_shows_the_currently_selected_directorys_full_path) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("/root") != std::string::npos);
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("/root/alpha") != std::string::npos);
}

CK_TEST(alt_x_quits) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(!f.browser.should_quit());
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(f.browser.should_quit());
}

CK_TEST(tab_moves_focus_from_the_tree_to_the_file_list_pane) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.focused() == f.browser.tree());
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}});
    CK_CHECK(f.app.focused() == f.browser.file_list());
}
