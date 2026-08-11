---
title: ckVision Platform Services
author: C. Klukas
date: 2026-08-09
format: report
description: Host-provided terminal, clock, filesystem, clipboard, and deterministic test services.
---

# Platform services

The UI library is intentionally host-driven. Your application chooses the
terminal backend and clock; filesystem and clipboard operations are explicit
services. This keeps UI code deterministic and makes a real application and a
headless test share the same view graph.

| Service | Interactive POSIX host | Deterministic test host | Client use |
|---|---|---|---|
| terminal | `term::PosixTerminal` | `term::HeadlessTerminal` | construct `Application`; run/poll/present |
| clock | `term::PosixClock` | `ManualClock` | timers and application time |
| clipboard | `TerminalClipboardWriter` | Application internal clipboard | text editor controls |
| filesystem | `term::PosixFileSystem` | `MemoryFileSystem` | File Browser, file/directory dialogs |

The File Browser accepts `FileSystem&`, so its master/detail wiring is
identical against a real disk and the deterministic tree used for screenshots.
It never lets TreeView or ListView query the disk themselves.

`FileEditorController` uses the same injected boundary for `read_file()`,
`fingerprint()`, and `write_file_atomic()`. A save supplies the fingerprint it
loaded; a changed-on-disk file returns a conflict rather than being silently
overwritten. `MemoryFileSystem` implements these operations for deterministic
editor lifecycle tests. See [Editor](editor.md) for the document/controller
composition.

<!-- ckvision-snippet source="examples/filebrowser/filebrowser_app.cpp" lines="100-139" -->
```cpp
    auto tree = std::make_unique<widgets::TreeView>();
    tree_ = tree.get();

    widgets::TreeNode root;
    root.label = root_path_;
    root.user_data = root_path_;
    populate_children(fs_, root);  // eager for the root only, so it can start expanded
    root.expanded = true;
    std::vector<widgets::TreeNode> roots;
    roots.push_back(std::move(root));
    tree_->set_roots(std::move(roots));

    auto file_list = std::make_unique<widgets::ListView>();
    file_list_ = file_list.get();

    tree_->on_selection_changed = [this](widgets::TreeNode&) {
        refresh_file_list_for_selected_node();
    };
    // Lazy population (M10/WP-22): fires the first time the user
    // expands a node whose children were never listed.
    tree_->on_expand_request = [this](widgets::TreeNode& node) { populate_children(fs_, node); };

    // Splitter (M10/WP-19) replaces the old fixed 50/50 Row: the split
    // starts at the same exact 50/50 ratio (Splitter's own default),
    // and Left/Right now lets the user adjust it.
    auto content = std::make_unique<widgets::Splitter>(window->content_rect(), std::move(tree),
                                                         std::move(file_list));
    splitter_ = content.get();
    window->set_content(std::move(content));

    // The selected directory's full path, shown live on the window's
    // own bottom border via Window::add_frame_overlay — exactly the
    // "current line in a text input window" pattern the overlay slot
    // exists for, applied here to "current directory" instead.
    auto path_label = std::make_unique<widgets::Label>(root_path_);
    path_label->set_role_override(roles_.static_text, roles_.label_mnemonic);  // static-text styling, not a form label
    path_label_ = window->add_frame_overlay(std::move(path_label), widgets::FrameSlot{});

    window_ = desktop_->add_window(std::move(window));
    refresh_file_list_for_selected_node();
```
<!-- /ckvision-snippet -->

![Deterministic File Browser capture](generated/screenshots/filebrowser-initial.svg)

Terminal profiles describe what the host is known to support; they are not a
license to inspect environment variables inside a widget. See
[terminal profiles](terminal-profiles.md), [graphics](graphics.md), and
[input decoding](input-decoder.md) for the protocol-facing reference material.
