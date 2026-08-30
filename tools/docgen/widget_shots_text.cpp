// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Views that show text: the read-only ones, the editable ones, and the
// flowed document. The full editor application has its own captures
// (docs/editor.md); what these figures add is the widget on its own,
// with the settings named beside it.
#include "widget_shots.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/widgets/editor_window.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/text_view.hpp"
#include "widget_stage.hpp"

namespace ckv::docgen {
namespace {

void shot_text_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto view = std::make_unique<widgets::TextView>();

    // ckvision-doc: textview
    view->set_text(
        "TextView shows text the reader cannot edit: a log, a report, a help "
        "page.\n"
        "It wraps, scrolls, and carries OSC 8 hyperlinks.\n"
        "\n"
        "Open the \x1B]8;;https://cklukas.github.io/ckVision/\x1B\\documentation "
        "site\x1B]8;;\x1B\\ for the rest.");
    view->set_wrap_mode(widgets::WrapMode::Word);
    view->set_vertical_scrollbar_policy(widgets::ScrollbarPolicy::Auto);
    view->on_link_activate = [](const std::string& target) { (void)target; };
    // ckvision-doc-end: textview

    widgets::TextView* text = view.get();
    stage.window_with_content("Notes", Rect{18, 5, 44, 10}, std::move(view));
    text->set_current_link(0);
    stage.focus(text);
    stage.step();
    stage.save_window(dir, "widget-textview");
}

void shot_memo(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto memo = std::make_unique<widgets::Memo>();

    // ckvision-doc: memo
    memo->set_text(
        "Memo is the multi-line field of a form: a description, a commit "
        "message, an address.\n"
        "It edits, wraps, selects, and talks to the clipboard, but it holds "
        "its own text rather than a shared document -- for a real editor, use "
        "TextEditor.");
    memo->set_wrap_mode(widgets::WrapMode::Word);
    memo->set_vertical_scrollbar_policy(widgets::ScrollbarPolicy::Auto);
    // ckvision-doc-end: memo

    widgets::Memo* view = memo.get();
    stage.window_with_content("Description", Rect{18, 5, 44, 10}, std::move(memo));
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-memo");
}

void shot_flow_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto flow = std::make_unique<widgets::FlowView>();

    // ckvision-doc: flowview
    widgets::FlowBlock heading;
    heading.content.push_back(widgets::FlowText{"Release 0.4", Attr::Bold, std::nullopt});

    widgets::FlowBlock body;
    body.content.push_back(widgets::FlowText{"FlowView lays out a document of ", Attr{}, std::nullopt});
    body.content.push_back(widgets::FlowText{"styled runs", Attr::Underline, std::nullopt});
    body.content.push_back(widgets::FlowText{", line breaks and inline images, and wraps them to its own width. See ", Attr{}, std::nullopt});
    body.content.push_back(widgets::FlowText{"the flow view guide", Attr{}, std::string("flow-view.md")});
    body.content.push_back(widgets::FlowText{" for the document model.", Attr{}, std::nullopt});

    widgets::FlowDocument document;
    document.blocks = {std::move(heading), std::move(body)};
    flow->set_document(std::move(document));
    flow->on_link_activate = [](const std::string& target) { (void)target; /* follow it */ };
    // ckvision-doc-end: flowview

    widgets::FlowView* view = flow.get();
    stage.window_with_content("Help", Rect{18, 5, 44, 10}, std::move(flow));
    view->set_current_link(0);
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-flowview");
}

void shot_text_editor(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: texteditor
    widgets::SyntaxProfileRegistry profiles;
    widgets::register_standard_syntax_profiles(profiles);

    auto document = std::make_shared<widgets::EditorDocument>(
        "{\n"
        "  \"name\": \"ckvision\",\n"
        "  \"version\": \"0.4.0\",\n"
        "  \"headless\": true,\n"
        "  \"widgets\": 51\n"
        "}\n");

    auto editor = std::make_unique<widgets::TextEditor>(document, &profiles);
    editor->set_file_name("package.json");   // the profile detector reads this
    editor->set_show_line_numbers(true);
    editor->set_wrap_mode(widgets::WrapMode::None);
    editor->set_vertical_scrollbar_policy(widgets::ScrollbarPolicy::Auto);
    editor->set_search_query(widgets::EditorSearchQuery{"ckvision", false, false});
    // ckvision-doc-end: texteditor

    widgets::TextEditor* view = editor.get();
    stage.window_with_content("package.json", Rect{16, 4, 48, 12}, std::move(editor));
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-texteditor");
}

void shot_editor_window(const std::filesystem::path& dir) {
    WidgetStage stage;
    static MemoryFileSystem files;
    files.add_directory("/notes");
    files.add_file("/notes/release.md",
                   "# Release 0.4\n\n"
                   "- every widget has a figure\n"
                   "- every figure has the code that drew it\n");

    // ckvision-doc: editorwindow
    static widgets::SyntaxProfileRegistry profiles;
    widgets::register_standard_syntax_profiles(profiles);

    auto window = std::make_unique<widgets::EditorWindow>(
        "release.md", std::make_shared<widgets::EditorDocument>(), files, &profiles);
    window->set_bounds(Rect{14, 4, 52, 13});
    window->open("/notes/release.md");
    window->editor().set_show_line_numbers(true);
    widgets::EditorWindow* editor_window = stage.desktop().add<widgets::EditorWindow>(
        std::move(window));
    // ckvision-doc-end: editorwindow

    stage.focus(&editor_window->editor());
    stage.step();
    stage.save(dir, "widget-editorwindow", Rect{13, 3, 55, 16});
}

}  // namespace

void capture_text_shots(const std::filesystem::path& dir) {
    shot_text_view(dir);
    shot_memo(dir);
    shot_flow_view(dir);
    shot_text_editor(dir);
    shot_editor_window(dir);
}

}  // namespace ckv::docgen
