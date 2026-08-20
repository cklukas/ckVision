// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "editor_app.hpp"

#include <memory>
#include <vector>

#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::editor_example {
EditorApp::EditorApp(ui::Application& application)
    : application_(application), roles_(ui::intern_standard_roles(application.roles())),
      document_(std::make_shared<widgets::EditorDocument>()), file_controller_(document_, filesystem_) {
    application_.theme() = ui::make_classic_theme(application_.roles(), roles_);
    widgets::register_standard_syntax_profiles(profiles_);
    filesystem_.add_file("config.yaml",
                         "name: ckVision\nversion: 0.1\nenabled: true\ndescription: This is a deliberately long YAML value that demonstrates stable editor word wrapping at the viewport edge.\n");
    filesystem_.add_file("settings.json", "{\n  \"name\": \"ckVision\",\n  \"enabled\": true\n}\n");
    filesystem_.add_file("sample.sh", "#!/usr/bin/env bash\necho \"ckVision\"\n");
    filesystem_.add_file("notes.txt", "Plain text fallback has no declared source grammar.\n");
    (void)open_sample("config.yaml");

    auto desktop = std::make_unique<widgets::Desktop>(application_.root().bounds());
    desktop_ = desktop.get();
    application_.root().add_child(std::move(desktop));

    // Every command declares the key it is known by; the registry assigns
    // the id, so this example never writes one and cannot collide with
    // the framework's own commands or another library's.
    ui::CommandRegistry& commands = application_.commands();
    const ui::CommandId save = commands.declare({.key = "editor.save", .title = "&Save", .category = "File", .chord = "Ctrl+S", .handler = [this] {
        (void)file_controller_.save();
    }});
    const ui::CommandId undo = commands.declare({.key = "editor.undo", .title = "&Undo", .category = "Edit", .chord = "Ctrl+Z", .handler = [this] { document_->undo(); }});
    const ui::CommandId redo = commands.declare({.key = "editor.redo", .title = "&Redo", .category = "Edit", .chord = "Ctrl+Y", .handler = [this] { document_->redo(); }});
    const ui::CommandId cut = commands.declare({.key = "editor.cut", .title = "Cu&t", .category = "Edit", .chord = "Ctrl+X", .handler = [this] { (void)editor_->cut_selection_to_clipboard(); }});
    const ui::CommandId copy = commands.declare({.key = "editor.copy", .title = "&Copy", .category = "Edit", .chord = "Ctrl+C", .handler = [this] { (void)editor_->copy_selection_to_clipboard(); }});
    const ui::CommandId paste = commands.declare({.key = "editor.paste", .title = "&Paste", .category = "Edit", .chord = "Ctrl+V", .handler = [this] { (void)editor_->paste_from_clipboard(); }});
    // All three modes, in the order a reader wants them: off for source and
    // logs, word for prose, character for content with no word structure to
    // respect. One key cycles them so the difference is easy to see.
    const ui::CommandId toggle_wrap = commands.declare({.key = "editor.toggle-wrap", .title = "&Word Wrap", .category = "Edit", .chord = "Alt+W", .handler = [this] {
        switch (editor_->wrap_mode()) {
            case widgets::WrapMode::None:
                editor_->set_wrap_mode(widgets::WrapMode::Word);
                break;
            case widgets::WrapMode::Word:
                editor_->set_wrap_mode(widgets::WrapMode::Character);
                break;
            case widgets::WrapMode::Character:
                editor_->set_wrap_mode(widgets::WrapMode::None);
                break;
        }
    }});
    const ui::CommandId find_selection = commands.declare({.key = "editor.find-selection", .title = "&Find Selection", .category = "Search", .chord = "Ctrl+F", .handler = [this] {
        (void)editor_->use_selection_as_search_query();
    }});
    const ui::CommandId find_next = commands.declare({.key = "editor.find-next", .title = "Find &Next", .category = "Search", .chord = "F3", .handler = [this] {
        (void)editor_->find_next();
    }});
    const ui::CommandId open_yaml = commands.declare({.key = "editor.open-yaml-sample", .title = "Open &YAML sample", .category = "File", .handler = [this] {
        (void)open_sample("config.yaml");
    }});
    const ui::CommandId open_json = commands.declare({.key = "editor.open-json-sample", .title = "Open &JSON sample", .category = "File", .handler = [this] {
        (void)open_sample("settings.json");
    }});
    const ui::CommandId open_bash = commands.declare({.key = "editor.open-bash-sample", .title = "Open &Bash sample", .category = "File", .handler = [this] {
        (void)open_sample("sample.sh");
    }});
    const ui::CommandId open_plain = commands.declare({.key = "editor.open-plain-sample", .title = "Open &plain-text sample", .category = "File", .handler = [this] {
        (void)open_sample("notes.txt");
    }});
    commands.set_enabled_predicate(save, [this] { return file_controller_.has_path() && document_->modified(); });
    commands.set_enabled_predicate(undo, [this] { return document_->can_undo(); });
    commands.set_enabled_predicate(redo, [this] { return document_->can_redo(); });
    commands.set_enabled_predicate(cut, [this] { return editor_->selection().has_value() && !editor_->read_only(); });
    commands.set_enabled_predicate(copy, [this] { return editor_->selection().has_value(); });
    commands.set_enabled_predicate(paste, [this] { return !editor_->read_only(); });
    commands.set_enabled_predicate(find_selection, [this] { return editor_->selection().has_value(); });
    commands.set_enabled_predicate(find_next, [this] { return editor_->search_match_count() != 0U; });

    widgets::MenuBarItem file{"&File", {widgets::MenuItem::command(widgets::CommandPresentation{save}),
                                          widgets::MenuItem::submenu("Open &Sample",
                                                            {widgets::MenuItem::command(widgets::CommandPresentation{open_yaml}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_json}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_bash}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_plain})}),
                                          widgets::MenuItem::separator(),
                                          widgets::MenuItem::command(widgets::CommandPresentation{commands.standard().quit})}};
    widgets::MenuBarItem edit{"&Edit", {widgets::MenuItem::command(widgets::CommandPresentation{undo}),
                                          widgets::MenuItem::command(widgets::CommandPresentation{redo}),
                                          widgets::MenuItem::separator(),
                                          widgets::MenuItem::command(widgets::CommandPresentation{cut}),
                                          widgets::MenuItem::command(widgets::CommandPresentation{copy}),
                                          widgets::MenuItem::command(widgets::CommandPresentation{paste}),
                                          widgets::MenuItem::separator(),
                                          widgets::MenuItem::command(widgets::CommandPresentation{toggle_wrap})}};
    widgets::MenuBarItem search{"&Search", {widgets::MenuItem::command(widgets::CommandPresentation{find_selection}),
                                              widgets::MenuItem::command(widgets::CommandPresentation{find_next})}};
    desktop_->dock_top(std::make_unique<widgets::MenuBar>(std::vector<widgets::MenuBarItem>{
        std::move(file), std::move(edit), std::move(search)}));
    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{commands.standard().focus_next}},
                       widgets::StatusLineItem{widgets::CommandPresentation{save}},
                       widgets::StatusLineItem{widgets::CommandPresentation{find_next}},
                       widgets::StatusLineItem{widgets::CommandPresentation{commands.standard().quit}}});
    desktop_->dock_bottom(std::move(status));

    auto window = std::make_unique<widgets::Window>("Editor — config.yaml");
    window->set_bounds(Rect{2, 2, 74, 20});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);
    auto editor = std::make_unique<widgets::TextEditor>(document_, &profiles_);
    editor->set_file_name("config.yaml");
    editor->set_show_line_numbers(true);
    editor->set_wrap_mode(widgets::WrapMode::Word);
    editor_ = editor.get();
    editor_->set_status_changed_handler([this](const widgets::EditorStatus&) { refresh_status(); });
    window->set_content(std::move(editor));
    status_ = window->add_frame_overlay(std::make_unique<widgets::StaticText>("Ln 1, Col 1"),
                                        widgets::FrameSlot{widgets::Edge::Bottom, ui::Alignment::End});
    window->close_request = [this] {
        request_close();
        return !document_->modified();
    };
    window->on_closed = [this] { window_ = nullptr; };
    window_ = desktop_->add_window(std::move(window));
    refresh_status();

    commands.set_handler(commands.standard().quit, [this] { application_.request_quit(); });
    application_.set_focus(editor_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(application_, *desktop_, roles_,
                                "ckVision Editor example",
                                "A shared document with a gutter and an editable source view.");
}

void EditorApp::refresh_status() {
    if (status_ == nullptr || editor_ == nullptr) return;
    const widgets::EditorStatus state = editor_->status();
    status_->set_text("Ln " + std::to_string(state.line) + ", Col " + std::to_string(state.column) +
                      (state.overwrite ? " OVR" : " INS") + (state.modified ? " *" : ""));
}

widgets::EditorFileStatus EditorApp::open_sample(std::string_view path) {
    const widgets::EditorFileStatus status = file_controller_.open(std::string(path));
    if (status == widgets::EditorFileStatus::Ok && editor_ != nullptr) {
        editor_->set_file_name(file_controller_.path());
        refresh_status();
    }
    return status;
}

void EditorApp::request_close() {
    if (window_ == nullptr || !document_->modified() || close_confirmation_) return;
    close_confirmation_.emplace(widgets::present_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Confirm, "Save changes",
                                      "Save changes to config.yaml?", widgets::MessageBoxButtons::YesNoCancel}));
    close_confirmation_->set_completion_handler([this](widgets::MessageBoxResult result) {
        if (window_ == nullptr) return;
        switch (result) {
            case widgets::MessageBoxResult::Yes:
                if (file_controller_.request_close(widgets::EditorCloseChoice::Save) == widgets::EditorFileStatus::Ok)
                    (void)window_->close();
                break;
            case widgets::MessageBoxResult::No:
                if (file_controller_.request_close(widgets::EditorCloseChoice::Discard) == widgets::EditorFileStatus::Ok)
                    (void)window_->close();
                break;
            case widgets::MessageBoxResult::Cancel:
            case widgets::MessageBoxResult::Ok:
                break;
        }
        close_confirmation_.reset();
    });
}

}  // namespace ckv::editor_example
