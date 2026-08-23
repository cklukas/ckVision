// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Optional reusable Window composition for a TextEditor and its injected file
// lifecycle. Clients may use the lower-level document/controller separately.
#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "cvision/widgets/file_editor_controller.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class StaticText;

class EditorWindow final : public Window {
public:
    EditorWindow(std::string title, std::shared_ptr<EditorDocument> document, FileSystem& filesystem,
                 SyntaxProfileRegistry* profiles = nullptr);
    ~EditorWindow() override;

    const std::shared_ptr<EditorDocument>& document() const noexcept { return controller_.document(); }
    FileEditorController& controller() noexcept { return controller_; }
    const FileEditorController& controller() const noexcept { return controller_; }
    TextEditor& editor() noexcept { return *editor_; }
    const TextEditor& editor() const noexcept { return *editor_; }

    EditorFileStatus open(std::string path, EditorOpenOptions options = {});
    EditorFileStatus save();
    EditorFileStatus save_as(std::string path, EditorSaveAsPolicy policy = EditorSaveAsPolicy::FailIfExists);
    EditorFileStatus request_close(EditorCloseChoice choice);

private:
    void refresh_chrome();
    static std::string display_name(std::string_view path);

    std::string base_title_;
    FileEditorController controller_;
    TextEditor* editor_ = nullptr;
    StaticText* status_ = nullptr;
    EditorDocument::ObserverId observer_ = 0;
};

}  // namespace ckv::widgets
