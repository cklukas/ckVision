// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include "cvision/core/directory_service.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/dialog_presentation.hpp"

namespace ckv::widgets { class Desktop; class InputLine; class StaticText; }
namespace ckv::completion_demo {
// One real graph used by the native executable and deterministic scene tests.
// The explicitly supplied directory service outlives this graph/Application.
class CompletionApp final {
public:
    CompletionApp(ui::Application& app, DirectoryService& directories, std::string initial_directory);
    ~CompletionApp();
    void open_file();
    void open_folder();
    widgets::InputLine& command_field() noexcept { return *command_; }
    widgets::Desktop& desktop() noexcept { return *desktop_; }
private:
    widgets::PendingDialogs pending_;
    std::shared_ptr<void> lifetime_ = std::make_shared<int>(0);
    ui::Application& app_;
    DirectoryService& directories_;
    ui::StandardRoles roles_;
    std::string initial_directory_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::InputLine* command_ = nullptr;
    widgets::StaticText* result_ = nullptr;
};
} // namespace ckv::completion_demo
