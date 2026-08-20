// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "cvision/widgets/file_editor_controller.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/syntax_profile.hpp"

namespace ckv::widgets {
class Desktop;
class StaticText;
class TextEditor;
class Window;
}  // namespace ckv::widgets

namespace ckv::editor_example {

class EditorApp {
public:
    explicit EditorApp(ui::Application& application);

    widgets::TextEditor* editor() const noexcept { return editor_; }
    widgets::Window* window() const noexcept { return window_; }
    const std::shared_ptr<widgets::EditorDocument>& document() const noexcept { return document_; }
    // The File/Samples menu calls the same public workflow. Exposing it keeps
    // the example's profile-detection proof directly scriptable as well.
    widgets::EditorFileStatus open_sample(std::string_view path);

private:
    ui::Application& application_;
    ui::StandardRoles roles_;
    MemoryFileSystem filesystem_;
    std::shared_ptr<widgets::EditorDocument> document_;
    widgets::FileEditorController file_controller_;
    widgets::SyntaxProfileRegistry profiles_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::TextEditor* editor_ = nullptr;
    widgets::StaticText* status_ = nullptr;
    std::optional<widgets::MessageBoxPresentation> close_confirmation_;

    void refresh_status();
    void request_close();
};

}  // namespace ckv::editor_example
