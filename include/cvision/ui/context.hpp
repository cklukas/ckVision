// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Context: the flat {theme, roles, application} bundle a widget needs
// to resolve its own theme roles and (for the few widgets that need
// behavioral access — MenuBar, StatusLine) reach the owning
// Application, WITHOUT taking any of it as constructor parameters
// (the decision log D-028 — "constructors take content, not plumbing").
//
// Propagated exactly like View's existing DirtyRectSink/DetachSink:
// Application installs one on root() once, in its own constructor;
// View::add_child propagates whatever context its new parent already
// has to the whole subtree just attached, so a widget built in memory
// and attached in one shot (the common pattern throughout this
// library) resolves correctly the moment it lands under a live
// Application — see View::on_attached().
//
// Standalone use (headless unit tests that construct and draw a
// widget without ever building a full Application) remains fully
// supported: call View::set_context(...) directly on any view with a
// Context built from a bare Theme/RoleRegistry pair (`app` left
// null) — no Application object required. This is the load-bearing
// reason Context is a value type propagated by the SAME view-tree
// mechanism `Application` uses, rather than something only
// `Application` can produce.
#pragma once

#include "cvision/ui/theme.hpp"

namespace ckv::ui {

class Application;

struct Context {
    const Theme* theme = nullptr;
    RoleRegistry* roles = nullptr;  // non-const: intern() is a mutating lookup
    Application* app = nullptr;     // optional; only a few widgets dereference this

    // The one field every widget actually needs to resolve roles;
    // View uses this to decide whether a context is "set" at all.
    bool valid() const noexcept { return theme != nullptr && roles != nullptr; }
};

}  // namespace ckv::ui
