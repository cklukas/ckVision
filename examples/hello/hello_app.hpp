// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// A faithful ckVision port of Turbo Vision's own canonical minimal
// example application (Borland's "hello.cpp" tutorial: THelloApp — a
// desktop, a menu bar with one custom command, a status line, and a
// greeting dialog with static text and four buttons). Built for two
// purposes: (1) a golden regression test pinning ckVision's rendered
// output for a well-known, independently-specified small app shape,
// and (2) docs/turbo-vision-conversion.md's worked side-by-side
// comparison of the same application in both frameworks.
//
// This is a PORT — the same behavior expressed in ckVision's own
// idiom, not a transliteration. See the doc comment on greeting_box()
// for the one remaining (non-observable) structural difference from
// the original.
#pragma once

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::hello {

class HelloApp {
public:
    explicit HelloApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }

private:
    void greeting_box();

    ui::Application& app_;
    ui::StandardRoles roles_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::MenuBar* menu_ = nullptr;
};

}  // namespace ckv::hello
