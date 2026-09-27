// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
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
class CheckGroup;
class Desktop;
class InputLine;
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
    // The desktop the window lives on — exposed for the same reason
    // window() is: the example's own behaviour is what the smoke test
    // drives, and where a minimized window is parked is part of it.
    widgets::Desktop* desktop() const noexcept { return desktop_; }
    const std::shared_ptr<widgets::EditorDocument>& document() const noexcept { return document_; }
    // The File/Samples menu calls the same public workflow. Exposing it keeps
    // the example's profile-detection proof directly scriptable as well.
    widgets::EditorFileStatus open_sample(std::string_view path);
    // The Replace dialog while it is open (Search > Replace..., Ctrl+R), and
    // null otherwise. Exposed like window(): whether the surface is on
    // screen is part of the example's behaviour.
    widgets::Window* replace_window() const noexcept { return replace_window_; }

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
    int active_scheme_ = 0;
    std::optional<widgets::MessageBoxPresentation> close_confirmation_;
    std::optional<widgets::MessageBoxPresentation> open_confirmation_;
    std::optional<widgets::MessageBoxPresentation> file_error_;
    // Replace All's question, or the report that there is nothing to find,
    // raised from inside the Replace dialog: modal on top of a modal.
    std::optional<widgets::MessageBoxPresentation> replace_prompt_;

    // The Replace dialog's commands. Its buttons run them through the
    // registry, so the dialog has no second copy of what each one does.
    ui::CommandId find_next_ = ui::kInvalidCommand;
    ui::CommandId replace_next_ = ui::kInvalidCommand;
    ui::CommandId replace_all_ = ui::kInvalidCommand;
    // The open Replace dialog and its fields, all null while it is closed.
    widgets::Window* replace_window_ = nullptr;
    widgets::InputLine* replace_find_ = nullptr;
    widgets::InputLine* replace_with_ = nullptr;
    widgets::CheckGroup* replace_match_case_ = nullptr;
    widgets::CheckGroup* replace_whole_word_ = nullptr;
    // What the reader last replaced with, offered again next time.
    std::string last_replacement_;

    void refresh_status();
    widgets::EditorFileStatus open_sample_with_options(std::string_view path, widgets::EditorOpenOptions options);
    void request_open_sample(std::string path);
    void show_file_error(std::string message);
    void request_close();
    void show_replace_dialog();
    void apply_replace_query();
    void replace_next();
    void request_replace_all();
    void report_no_match();
};

}  // namespace ckv::editor_example
