// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A compact ckVision-owned integration example: desktop, menu/status chrome,
// modal greeting dialog, and golden coverage. See docs/hello-example.md.
#pragma once

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::hello {

class HelloApp {
public:
    explicit HelloApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }

private:
    void greeting_box();

    ui::Application& app_; ui::StandardRoles roles_; widgets::Desktop* desktop_ = nullptr;
};

}  // namespace ckv::hello
