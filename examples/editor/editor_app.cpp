// Copyright (c) 2026 C. Klukas. All rights reserved.
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
namespace {

constexpr ui::CommandId kUndo = ui::kFirstApplicationCommandId + 100;
constexpr ui::CommandId kRedo = ui::kFirstApplicationCommandId + 101;
constexpr ui::CommandId kCut = ui::kFirstApplicationCommandId + 102;
constexpr ui::CommandId kCopy = ui::kFirstApplicationCommandId + 103;
constexpr ui::CommandId kPaste = ui::kFirstApplicationCommandId + 104;
constexpr ui::CommandId kToggleWrap = ui::kFirstApplicationCommandId + 105;
constexpr ui::CommandId kFindSelection = ui::kFirstApplicationCommandId + 106;
constexpr ui::CommandId kFindNext = ui::kFirstApplicationCommandId + 107;
constexpr ui::CommandId kSave = ui::kFirstApplicationCommandId + 108;
constexpr ui::CommandId kOpenYamlSample = ui::kFirstApplicationCommandId + 109;
constexpr ui::CommandId kOpenJsonSample = ui::kFirstApplicationCommandId + 110;
constexpr ui::CommandId kOpenBashSample = ui::kFirstApplicationCommandId + 111;
constexpr ui::CommandId kOpenPlainSample = ui::kFirstApplicationCommandId + 112;

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

    application_.commands().add(ui::CommandDescriptor{kSave, "&Save", "File", {}, "Ctrl+S", [this] {
        (void)file_controller_.save();
    }});
    application_.commands().add(ui::CommandDescriptor{kUndo, "&Undo", "Edit", {}, "Ctrl+Z", [this] { document_->undo(); }});
    application_.commands().add(ui::CommandDescriptor{kRedo, "&Redo", "Edit", {}, "Ctrl+Y", [this] { document_->redo(); }});
    application_.commands().add(ui::CommandDescriptor{kCut, "Cu&t", "Edit", {}, "Ctrl+X", [this] { (void)editor_->cut_selection_to_clipboard(); }});
    application_.commands().add(ui::CommandDescriptor{kCopy, "&Copy", "Edit", {}, "Ctrl+C", [this] { (void)editor_->copy_selection_to_clipboard(); }});
    application_.commands().add(ui::CommandDescriptor{kPaste, "&Paste", "Edit", {}, "Ctrl+V", [this] { (void)editor_->paste_from_clipboard(); }});
    // All three modes, in the order a reader wants them: off for source and
    // logs, word for prose, character for content with no word structure to
    // respect. One key cycles them so the difference is easy to see.
    application_.commands().add(ui::CommandDescriptor{kToggleWrap, "&Word Wrap", "Edit", {}, "Alt+W", [this] {
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
    application_.commands().add(ui::CommandDescriptor{kFindSelection, "&Find Selection", "Search", {}, "Ctrl+F", [this] {
        (void)editor_->use_selection_as_search_query();
    }});
    application_.commands().add(ui::CommandDescriptor{kFindNext, "Find &Next", "Search", {}, "F3", [this] {
        (void)editor_->find_next();
    }});
    application_.commands().add(ui::CommandDescriptor{kOpenYamlSample, "Open &YAML sample", "File", {}, "", [this] {
        (void)open_sample("config.yaml");
    }});
    application_.commands().add(ui::CommandDescriptor{kOpenJsonSample, "Open &JSON sample", "File", {}, "", [this] {
        (void)open_sample("settings.json");
    }});
    application_.commands().add(ui::CommandDescriptor{kOpenBashSample, "Open &Bash sample", "File", {}, "", [this] {
        (void)open_sample("sample.sh");
    }});
    application_.commands().add(ui::CommandDescriptor{kOpenPlainSample, "Open &plain-text sample", "File", {}, "", [this] {
        (void)open_sample("notes.txt");
    }});
    application_.commands().set_enabled_predicate(kSave, [this] { return file_controller_.has_path() && document_->modified(); });
    application_.commands().set_enabled_predicate(kUndo, [this] { return document_->can_undo(); });
    application_.commands().set_enabled_predicate(kRedo, [this] { return document_->can_redo(); });
    application_.commands().set_enabled_predicate(kCut, [this] { return editor_->selection().has_value() && !editor_->read_only(); });
    application_.commands().set_enabled_predicate(kCopy, [this] { return editor_->selection().has_value(); });
    application_.commands().set_enabled_predicate(kPaste, [this] { return !editor_->read_only(); });
    application_.commands().set_enabled_predicate(kFindSelection, [this] { return editor_->selection().has_value(); });
    application_.commands().set_enabled_predicate(kFindNext, [this] { return editor_->search_match_count() != 0U; });

    widgets::MenuBarItem file{"&File", {widgets::MenuItem{widgets::CommandPresentation{kSave}},
                                          widgets::MenuItem{"Open &Sample", ui::kInvalidCommand, {}, false,
                                                            widgets::MenuItemCheck::None,
                                                            {widgets::MenuItem{widgets::CommandPresentation{kOpenYamlSample}},
                                                             widgets::MenuItem{widgets::CommandPresentation{kOpenJsonSample}},
                                                             widgets::MenuItem{widgets::CommandPresentation{kOpenBashSample}},
                                                             widgets::MenuItem{widgets::CommandPresentation{kOpenPlainSample}}}},
                                          widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
                                          widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kQuit}}}};
    widgets::MenuBarItem edit{"&Edit", {widgets::MenuItem{widgets::CommandPresentation{kUndo}},
                                          widgets::MenuItem{widgets::CommandPresentation{kRedo}},
                                          widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
                                          widgets::MenuItem{widgets::CommandPresentation{kCut}},
                                          widgets::MenuItem{widgets::CommandPresentation{kCopy}},
                                          widgets::MenuItem{widgets::CommandPresentation{kPaste}},
                                          widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
                                          widgets::MenuItem{widgets::CommandPresentation{kToggleWrap}}}};
    widgets::MenuBarItem search{"&Search", {widgets::MenuItem{widgets::CommandPresentation{kFindSelection}},
                                              widgets::MenuItem{widgets::CommandPresentation{kFindNext}}}};
    desktop_->dock_top(std::make_unique<widgets::MenuBar>(std::vector<widgets::MenuBarItem>{
        std::move(file), std::move(edit), std::move(search)}));
    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kFocusNext}},
                       widgets::StatusLineItem{widgets::CommandPresentation{kSave}},
                       widgets::StatusLineItem{widgets::CommandPresentation{kFindNext}},
                       widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kQuit}}});
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

    application_.commands().set_handler(ui::std_commands::kQuit, [this] { application_.request_quit(); });
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
