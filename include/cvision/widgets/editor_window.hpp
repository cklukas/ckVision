// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Optional reusable Window composition for a TextEditor and its injected file
// lifecycle. Clients may use the lower-level document/controller separately.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "cvision/widgets/file_editor_controller.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class FrameText;

// A Window whose content is a TextEditor, with line numbers shown, and whose file lifecycle is
// a FileEditorController over the same document. The title is the file name (the last path
// component) once a file has been opened or saved as, and the constructor's title before that;
// " *" is appended while the document is modified. A frame overlay at the bottom edge, aligned
// to its end, reads "Ln L, Col C" followed by INS or OVR and " *" when modified.
//
// The window installs a close_request that vetoes close() while the document is modified, so a
// stray close never loses work, unless request_close has answered Ok at the document's current
// revision: after the application settled the question (Save succeeded, or Discard was chosen),
// close() proceeds, until a further change of the text asks it again.
class EditorWindow final : public Window {
public:
    // `title` is the caption while no file is open. `document` is shared by the controller and
    // the editor; a null one is replaced by a new empty document. `filesystem` must outlive the
    // window. `profiles` is the registry the editor detects syntax profiles from, and must
    // outlive the window; null gives the editor its own standard set.
    EditorWindow(std::string title, std::shared_ptr<EditorDocument> document, FileSystem& filesystem,
                 SyntaxProfileRegistry* profiles = nullptr);
    ~EditorWindow() override;

    // The shared document, the file controller, and the editor, which the window owns as its
    // content. Prefer the window's own file operations below to the controller's: only they
    // refresh the title after a change of path or clean state, and only open and save_as hand
    // the editor the file name its syntax profile is detected from.
    const std::shared_ptr<EditorDocument>& document() const noexcept { return controller_.document(); }
    FileEditorController& controller() noexcept { return controller_; }
    const FileEditorController& controller() const noexcept { return controller_; }
    TextEditor& editor() noexcept { return *editor_; }
    const TextEditor& editor() const noexcept { return *editor_; }

    // The controller's operations of the same names, followed by a refresh of the title and the
    // status overlay (for open, only on Ok). On Ok, open and save_as also give the editor the
    // file's path as its file name, which re-detects its syntax profile. request_close answers
    // whether the caller may close and does not close the window itself; an Ok answer lets a
    // following close() through (see the class comment).
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
    FrameText* status_ = nullptr;
    EditorDocument::ObserverId observer_ = 0;
    // The revision at which request_close last answered Ok, if any.
    std::optional<DocumentRevision> close_settled_revision_;
};

}  // namespace ckv::widgets
