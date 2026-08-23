// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/editor_window.hpp"

#include "cvision/widgets/static_text.hpp"

namespace ckv::widgets {

EditorWindow::EditorWindow(std::string title, std::shared_ptr<EditorDocument> document, FileSystem& filesystem,
                           SyntaxProfileRegistry* profiles)
    : Window(title), base_title_(std::move(title)), controller_(std::move(document), filesystem) {
    auto editor = std::make_unique<TextEditor>(controller_.document(), profiles);
    editor_ = editor.get();
    editor_->set_show_line_numbers(true);
    editor_->set_status_changed_handler([this](const EditorStatus&) { refresh_chrome(); });
    set_content(std::move(editor));
    status_ = add_frame_overlay(std::make_unique<StaticText>("Ln 1, Col 1"), FrameSlot{Edge::Bottom, ui::Alignment::End});
    observer_ = controller_.document()->subscribe([this](const DocumentChange&) { refresh_chrome(); });
    close_request = [this] { return !controller_.modified(); };
    refresh_chrome();
}

EditorWindow::~EditorWindow() {
    if (observer_ != 0U) controller_.document()->unsubscribe(observer_);
}

EditorFileStatus EditorWindow::open(std::string path, EditorOpenOptions options) {
    const EditorFileStatus status = controller_.open(std::move(path), options);
    if (status == EditorFileStatus::Ok) {
        editor_->set_file_name(controller_.path());
        refresh_chrome();
    }
    return status;
}

EditorFileStatus EditorWindow::save() {
    const EditorFileStatus status = controller_.save();
    refresh_chrome();
    return status;
}

EditorFileStatus EditorWindow::save_as(std::string path, EditorSaveAsPolicy policy) {
    const EditorFileStatus status = controller_.save_as(std::move(path), policy);
    if (status == EditorFileStatus::Ok) editor_->set_file_name(controller_.path());
    refresh_chrome();
    return status;
}

EditorFileStatus EditorWindow::request_close(EditorCloseChoice choice) {
    const EditorFileStatus status = controller_.request_close(choice);
    refresh_chrome();
    return status;
}

std::string EditorWindow::display_name(std::string_view path) {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string_view::npos ? std::string(path) : std::string(path.substr(separator + 1U));
}

void EditorWindow::refresh_chrome() {
    if (editor_ == nullptr || status_ == nullptr) return;
    const EditorStatus state = editor_->status();
    status_->set_text("Ln " + std::to_string(state.line) + ", Col " + std::to_string(state.column) +
                      (state.overwrite ? " OVR" : " INS") + (state.modified ? " *" : ""));
    const std::string name = controller_.has_path() ? display_name(controller_.path()) : base_title_;
    set_title(name + (state.modified ? " *" : ""));
}

}  // namespace ckv::widgets
