// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "editor_app.hpp"

#include "../example_about.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::editor_example {
namespace {
std::string document_name(std::string_view path) {
    const std::size_t separator = path.find_last_of("/\\");
    return std::string(path.substr(separator == std::string_view::npos ? 0U : separator + 1U));
}
}  // namespace

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
        if (file_controller_.save() != widgets::EditorFileStatus::Ok)
            show_file_error("Could not save " + document_name(file_controller_.path()) + ". The document remains open.");
    }});
    const ui::CommandId undo = commands.declare({.key = "editor.undo", .title = "&Undo", .category = "Edit", .chord = "Ctrl+Z", .handler = [this] { (void)editor_->perform(widgets::EditorCommand::Undo); }});
    const ui::CommandId redo = commands.declare({.key = "editor.redo", .title = "&Redo", .category = "Edit", .chord = "Ctrl+Y", .handler = [this] { (void)editor_->perform(widgets::EditorCommand::Redo); }});
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
    // A checkable command: the menu row shows the registry's checked state,
    // so it reads the editor itself and can never disagree with it.
    const ui::CommandId toggle_line_numbers = commands.declare({.key = "editor.toggle-line-numbers", .title = "Line &Numbers", .category = "View", .handler = [this] {
        editor_->set_show_line_numbers(!editor_->show_line_numbers());
    }});
    commands.set_checked_predicate(toggle_line_numbers, [this] { return editor_->show_line_numbers(); });
    const ui::CommandId find_selection = commands.declare({.key = "editor.find-selection", .title = "&Find Selection", .category = "Search", .chord = "Ctrl+F", .handler = [this] {
        (void)editor_->use_selection_as_search_query();
    }});
    // With the Replace dialog open, Find Next searches for what its fields
    // say, so the dialog's button and F3 are one command.
    find_next_ = commands.declare({.key = "editor.find-next", .title = "Find &Next", .category = "Search", .chord = "F3", .handler = [this] {
        apply_replace_query();
        if (!editor_->find_next() && replace_window_ != nullptr) report_no_match();
    }});
    const ui::CommandId replace = commands.declare({.key = "editor.replace", .title = "&Replace...", .category = "Search", .chord = "Ctrl+R", .handler = [this] {
        show_replace_dialog();
    }});
    // The Replace dialog's own verbs. They act on what its fields say, so
    // they are available only while it is open, and no menu lists them.
    replace_next_ = commands.declare({.key = "editor.replace-next", .title = "&Replace", .category = "Search",
                                      .visibility = ui::CommandVisibility::Hidden, .handler = [this] { replace_next(); }});
    replace_all_ = commands.declare({.key = "editor.replace-all", .title = "Replace &All", .category = "Search",
                                     .visibility = ui::CommandVisibility::Hidden, .handler = [this] { request_replace_all(); }});
    const ui::CommandId open_yaml = commands.declare({.key = "editor.open-yaml-sample", .title = "Open &YAML sample", .category = "File", .handler = [this] {
        request_open_sample("config.yaml");
    }});
    const ui::CommandId open_json = commands.declare({.key = "editor.open-json-sample", .title = "Open &JSON sample", .category = "File", .handler = [this] {
        request_open_sample("settings.json");
    }});
    const ui::CommandId open_bash = commands.declare({.key = "editor.open-bash-sample", .title = "Open &Bash sample", .category = "File", .handler = [this] {
        request_open_sample("sample.sh");
    }});
    const ui::CommandId open_plain = commands.declare({.key = "editor.open-plain-sample", .title = "Open &plain-text sample", .category = "File", .handler = [this] {
        request_open_sample("notes.txt");
    }});
    const auto scheme_command = [this, &commands](ui::CommandDescriptor descriptor,
                                                    auto factory, int index) {
        descriptor.handler = [this, factory, index] {
            application_.set_theme(factory(application_.roles(), roles_));
            active_scheme_ = index;
        };
        return commands.declare(std::move(descriptor));
    };
    const ui::CommandId classic_scheme = scheme_command(
        {.key = "editor.scheme.classic", .title = "&Classic", .category = "View"},
        ui::make_classic_theme, 0);
    const ui::CommandId dark_scheme = scheme_command(
        {.key = "editor.scheme.dark", .title = "&Dark", .category = "View"},
        ui::make_dark_theme, 1);
    const ui::CommandId light_scheme = scheme_command(
        {.key = "editor.scheme.light", .title = "&Light", .category = "View"},
        ui::make_light_theme, 2);
    const ui::CommandId mono_scheme = scheme_command(
        {.key = "editor.scheme.mono", .title = "&Mono", .category = "View"},
        ui::make_mono_theme, 3);
    const auto scheme_item = [this](ui::CommandId command, int index) {
        return widgets::MenuItem::command(widgets::CommandPresentation{command})
            .with_mark_provider([this, index] {
                return active_scheme_ == index ? widgets::MenuMark::RadioOn : widgets::MenuMark::RadioOff;
            });
    };
    commands.set_enabled_predicate(save, [this] { return file_controller_.has_path() && document_->modified(); });
    commands.set_enabled_predicate(undo, [this] { return document_->can_undo(); });
    commands.set_enabled_predicate(redo, [this] { return document_->can_redo(); });
    commands.set_enabled_predicate(cut, [this] { return editor_->selection().has_value() && !editor_->read_only(); });
    commands.set_enabled_predicate(copy, [this] { return editor_->selection().has_value(); });
    commands.set_enabled_predicate(paste, [this] { return !editor_->read_only(); });
    commands.set_enabled_predicate(find_selection, [this] { return editor_->selection().has_value(); });
    commands.set_enabled_predicate(find_next_, [this] {
        return replace_window_ != nullptr || editor_->search_match_count() != 0U;
    });
    commands.set_enabled_predicate(replace, [this] { return window_ != nullptr && !editor_->read_only(); });
    commands.set_enabled_predicate(replace_next_, [this] { return replace_window_ != nullptr; });
    commands.set_enabled_predicate(replace_all_, [this] { return replace_window_ != nullptr; });

    widgets::MenuBarItem file{"&File", {widgets::MenuItem::command(widgets::CommandPresentation{save}),
                                          widgets::MenuItem::submenu("Open &Sample",
                                                            {widgets::MenuItem::command(widgets::CommandPresentation{open_yaml}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_json}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_bash}),
                                                             widgets::MenuItem::command(widgets::CommandPresentation{open_plain})}),
                                          widgets::MenuItem::separator(),
                                          widgets::MenuItem::command(widgets::CommandPresentation{
                                              commands.standard().help, "&About..."}),
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
                                              widgets::MenuItem::command(widgets::CommandPresentation{find_next_}),
                                              widgets::MenuItem::separator(),
                                              widgets::MenuItem::command(widgets::CommandPresentation{replace})}};
    widgets::MenuBarItem view{"&View", {
        widgets::MenuItem::command(widgets::CommandPresentation{toggle_line_numbers}),
        widgets::MenuItem::separator(),
        scheme_item(classic_scheme, 0),
        scheme_item(dark_scheme, 1),
        scheme_item(light_scheme, 2),
        scheme_item(mono_scheme, 3)}};
    desktop_->dock_top(std::make_unique<widgets::MenuBar>(std::vector<widgets::MenuBarItem>{
        std::move(file), std::move(edit), std::move(search), std::move(view)}));
    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{commands.standard().focus_next}},
                       widgets::StatusLineItem{widgets::CommandPresentation{save}},
                       widgets::StatusLineItem{widgets::CommandPresentation{find_next_}},
                       widgets::StatusLineItem{widgets::CommandPresentation{commands.standard().quit}}});
    desktop_->dock_bottom(std::move(status));

    auto window = std::make_unique<widgets::Window>("Editor — " + document_name(file_controller_.path()));
    window->set_bounds(Rect{2, 2, 74, 20});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);
    auto editor = std::make_unique<widgets::TextEditor>(document_, &profiles_);
    editor->set_file_name(document_name(file_controller_.path()));
    editor->set_show_line_numbers(true);
    editor->set_wrap_mode(widgets::WrapMode::Word);
    editor_ = editor.get();
    editor_->set_status_changed_handler([this](const widgets::EditorStatus&) { refresh_status(); });
    // The editor's context menu: a right click (or Ctrl+click) where the
    // pointer is, and the Menu key or Shift+F10 at the caret. Its rows are
    // the Edit menu's own commands, so a row that cannot act now -- Cut and
    // Copy with nothing selected -- is shown and greyed exactly as there.
    editor_->set_context_menu_handler([this, cut, copy, paste, find_selection](widgets::TextEditor&, Point cell) {
        (void)widgets::show_context_menu(
            {widgets::MenuItem::command(widgets::CommandPresentation{cut}),
             widgets::MenuItem::command(widgets::CommandPresentation{copy}),
             widgets::MenuItem::command(widgets::CommandPresentation{paste}),
             widgets::MenuItem::separator(),
             widgets::MenuItem::action("Select &All", [this] { (void)editor_->perform(widgets::EditorCommand::SelectAll); }),
             widgets::MenuItem::command(widgets::CommandPresentation{find_selection})},
            cell, application_, *desktop_);
    });
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
                                ckv::examples::about_text(
                                    "A shared document with a gutter and an editable source view."));
}

void EditorApp::refresh_status() {
    if (status_ == nullptr || editor_ == nullptr) return;
    const widgets::EditorStatus state = editor_->status();
    status_->set_text("Ln " + std::to_string(state.line) + ", Col " + std::to_string(state.column) +
                      (state.overwrite ? " OVR" : " INS") + (state.modified ? " *" : ""));
}

widgets::EditorFileStatus EditorApp::open_sample(std::string_view path) {
    return open_sample_with_options(path, {});
}

widgets::EditorFileStatus EditorApp::open_sample_with_options(std::string_view path,
                                                               widgets::EditorOpenOptions options) {
    const widgets::EditorFileStatus status = file_controller_.open(std::string(path), options);
    if (status == widgets::EditorFileStatus::Ok && editor_ != nullptr) {
        editor_->set_file_name(document_name(file_controller_.path()));
        if (window_ != nullptr) window_->set_title("Editor — " + document_name(file_controller_.path()));
        refresh_status();
    }
    return status;
}

void EditorApp::request_open_sample(std::string path) {
    if (window_ == nullptr || open_confirmation_ || close_confirmation_ || file_error_) return;
    if (!document_->modified()) {
        if (open_sample(path) != widgets::EditorFileStatus::Ok)
            show_file_error("Could not open " + document_name(path) + ". The current document remains open.");
        return;
    }
    open_confirmation_.emplace(widgets::present_modal_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Confirm, "Save changes",
                                      "Save changes to " + document_name(file_controller_.path()) + " before opening " +
                                          document_name(path) + "?",
                                      widgets::MessageBoxButtons::YesNoCancel}));
    open_confirmation_->set_completion_handler([this, target = std::move(path)](widgets::MessageBoxResult result) {
        widgets::EditorFileStatus status = widgets::EditorFileStatus::Ok;
        if (result == widgets::MessageBoxResult::Yes) {
            status = file_controller_.save();
            if (status == widgets::EditorFileStatus::Ok) status = open_sample(target);
        } else if (result == widgets::MessageBoxResult::No) {
            status = open_sample_with_options(
                target, widgets::EditorOpenOptions{.modified_document = widgets::EditorOpenModifiedPolicy::Discard});
        }
        open_confirmation_.reset();
        if (status != widgets::EditorFileStatus::Ok)
            show_file_error("Could not save or open " + document_name(target) + ". The current document remains open.");
    });
}

void EditorApp::show_file_error(std::string message) {
    if (file_error_ || window_ == nullptr) return;
    file_error_.emplace(widgets::present_modal_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Error, "File operation failed",
                                      std::move(message), widgets::MessageBoxButtons::Ok}));
    file_error_->set_completion_handler([this](widgets::MessageBoxResult) { file_error_.reset(); });
}

void EditorApp::request_close() {
    if (window_ == nullptr || !document_->modified() || close_confirmation_) return;
    close_confirmation_.emplace(widgets::present_modal_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Confirm, "Save changes",
                                      "Save changes to " + document_name(file_controller_.path()) + "?",
                                      widgets::MessageBoxButtons::YesNoCancel}));
    close_confirmation_->set_completion_handler([this](widgets::MessageBoxResult result) {
        if (window_ == nullptr) {
            close_confirmation_.reset();
            return;
        }
        bool save_failed = false;
        switch (result) {
            case widgets::MessageBoxResult::Yes:
                if (file_controller_.request_close(widgets::EditorCloseChoice::Save) == widgets::EditorFileStatus::Ok)
                    (void)window_->close();
                else
                    save_failed = true;
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
        if (save_failed)
            show_file_error("Could not save " + document_name(file_controller_.path()) + ". The document remains open.");
    });
}

void EditorApp::show_replace_dialog() {
    if (window_ == nullptr || replace_window_ != nullptr) return;
    // The text the reader most likely means: a selection on one line, which
    // is how Ctrl+F picks its query too, and otherwise the query in use.
    const widgets::EditorSearchQuery& current = editor_->search_query();
    std::string query = current.text;
    if (const auto selected = editor_->selection()) {
        std::string text = document_->text(*selected);
        if (!text.empty() && text.find('\n') == std::string::npos) query = std::move(text);
    }

    widgets::DialogDescriptor descriptor;
    descriptor.title = "Replace";
    descriptor.fields.push_back(widgets::FieldDescriptor{"&Find:", std::move(query), nullptr});
    descriptor.fields.push_back(widgets::FieldDescriptor{"Replace &with:", last_replacement_, nullptr});
    widgets::FieldDescriptor match_case{"Match &case", "", nullptr};
    match_case.kind = widgets::FieldKind::Check;
    match_case.initial_checked = current.case_sensitive;
    descriptor.fields.push_back(std::move(match_case));
    widgets::FieldDescriptor whole_word{"Whole words &only", "", nullptr};
    whole_word.kind = widgets::FieldKind::Check;
    whole_word.initial_checked = current.whole_word;
    descriptor.fields.push_back(std::move(whole_word));
    // The buttons run the registry's commands, the same ones F3 reaches, so
    // what a button does is declared once, with its enablement.
    descriptor.buttons.push_back(widgets::ButtonDescriptor{
        "Find &Next", widgets::ButtonRole::Neutral, [this] { (void)application_.execute_command(find_next_); }});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{
        "&Replace", widgets::ButtonRole::Accept, [this] { (void)application_.execute_command(replace_next_); }});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{
        "Replace &All", widgets::ButtonRole::Neutral, [this] { (void)application_.execute_command(replace_all_); }});
    descriptor.buttons.push_back(widgets::ButtonDescriptor{"Close", widgets::ButtonRole::Dismiss, [this] {
        if (replace_window_ != nullptr) (void)replace_window_->close();
    }});

    // Materialized and hosted here rather than through present_modal_dialog: a
    // presented descriptor dialog closes on its default button, and this one
    // stays open while the reader replaces one match after another.
    widgets::MaterializedDialog dialog = widgets::materialize_dialog(descriptor);
    widgets::InputLine* const find_field = dialog.inputs[0];
    widgets::InputLine* const replace_field = dialog.inputs[1];
    widgets::CheckGroup* const match_case_field = dialog.checks[2];
    widgets::CheckGroup* const whole_word_field = dialog.checks[3];
    ui::View* const initial_focus = dialog.initial_focus;

    auto window = std::make_unique<widgets::Window>(descriptor.title);
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                              roles_.dialog_background);
    window->set_content_margin(1, 1);
    window->set_resizable(false);
    window->set_content(std::move(dialog.root));
    // Enter, from any field, is the default button; Esc is Close.
    window->accept_request = [this] { (void)application_.execute_command(replace_next_); };
    window->cancel_request = [this] {
        if (replace_window_ != nullptr) (void)replace_window_->close();
    };
    window->on_closed = [this] {
        widgets::Window* const closing = replace_window_;
        replace_window_ = nullptr;
        replace_find_ = nullptr;
        replace_with_ = nullptr;
        replace_match_case_ = nullptr;
        replace_whole_word_ = nullptr;
        if (closing != nullptr) widgets::schedule_self_detach(*closing, application_);
    };
    // Along the desktop's bottom edge, not centred over the document: the
    // reader has to see the match a press of Replace is about to change, and
    // the document starts at the top.
    const Rect area = desktop_->content_area();
    const int width = std::min(area.width, window->horizontal_size_hint().preferred);
    const int height =
        std::min(area.height, std::max(window->vertical_size_hint().preferred, window->height_for_width(width)));
    window->set_bounds(Rect{area.x + (area.width - width) / 2, area.y + area.height - height, width, height});

    replace_window_ = desktop_->present_modal(widgets::WindowHandle{std::move(window), initial_focus}, application_);
    if (replace_window_ == nullptr) return;
    replace_find_ = find_field;
    replace_with_ = replace_field;
    replace_match_case_ = match_case_field;
    replace_whole_word_ = whole_word_field;
}

void EditorApp::apply_replace_query() {
    if (replace_window_ == nullptr) return;
    // A query whose options changed still keeps the current match when that
    // text matches the new query too (TextEditor::set_search_query), so
    // setting it on every press never loses the match the reader is on.
    editor_->set_search_query(widgets::EditorSearchQuery{replace_find_->text(), replace_match_case_->checked(0),
                                                         replace_whole_word_->checked(0)});
}

void EditorApp::replace_next() {
    apply_replace_query();
    last_replacement_ = replace_with_->text();
    // The current match is the one the reader is looking at: it is replaced,
    // and the next one selected, so every press shows what the following one
    // will change before it changes it. Without a current match — the first
    // press — the press only finds one.
    (void)editor_->replace_current_search_match(last_replacement_);
    if (!editor_->find_next()) report_no_match();
}

void EditorApp::request_replace_all() {
    if (replace_prompt_) return;
    apply_replace_query();
    last_replacement_ = replace_with_->text();
    const std::size_t count = editor_->search_match_count();
    if (count == 0U) {
        report_no_match();
        return;
    }
    // One question before a change the reader cannot watch happen, stating
    // its extent. It is raised from inside the Replace dialog, so it is modal
    // on top of a modal, and answering it returns the focus there.
    replace_prompt_.emplace(widgets::present_modal_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Confirm, "Replace All",
                                      "Replace " + std::to_string(count) + (count == 1U ? " match" : " matches") +
                                          " of \"" + editor_->search_query().text + "\" with \"" +
                                          last_replacement_ + "\"?",
                                      widgets::MessageBoxButtons::YesNo}));
    replace_prompt_->set_completion_handler([this, replacement = last_replacement_](widgets::MessageBoxResult result) {
        replace_prompt_.reset();
        // One transaction, so a single Undo takes every replacement back.
        if (result == widgets::MessageBoxResult::Yes && window_ != nullptr)
            (void)editor_->replace_all_search_matches(replacement);
    });
}

void EditorApp::report_no_match() {
    if (replace_prompt_) return;
    // A press that finds nothing says so: silence cannot be told apart from
    // a press that never arrived.
    const std::string& query = editor_->search_query().text;
    replace_prompt_.emplace(widgets::present_modal_message_box(
        application_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Info, "Replace",
                                      query.empty() ? std::string("Type the text to find first.")
                                                    : "\"" + query + "\" does not occur in " +
                                                          document_name(file_controller_.path()) + ".",
                                      widgets::MessageBoxButtons::Ok}));
    replace_prompt_->set_completion_handler([this](widgets::MessageBoxResult) { replace_prompt_.reset(); });
}

}  // namespace ckv::editor_example
