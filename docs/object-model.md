---
title: ckVision Object Model
author: C. Klukas
date: 2026-08-09
format: report
description: Ownership, focus, event delivery, layout, and painting in a ckVision application.
---

# Object model

ckVision uses one explicit UI tree. There is no hidden global UI registry and
no widget creates its own terminal connection. That gives an application a
clear answer to “who owns this?” and makes the same graph runnable under a
real terminal or deterministic test terminal.

```text
Application
|-- CommandRegistry, Theme, root View, injected host services
`-- root View
    `-- Desktop
        |-- top dock: MenuBar
        |-- bottom dock: StatusLine
        |-- windows in z-order
        |   `-- Window -> content View -> layouts/controls
        `-- popup or modal dialog
```

| Object | Ownership/insertion | Focus and events | Paint and resize | Header |
|---|---|---|---|---|
| `Application` | Host constructs it from `Terminal`, `Clock`, and optional clipboard. | Central dispatch: modal/focused view first, then command bindings. | Composes the root into a terminal frame; dispatches terminal resize. | `include/cvision/ui/application.hpp` |
| `View` | Parent receives `std::unique_ptr<View>` through `add_child`. | A view opts into tab focus and may consume key/text/mouse input. | Parent bounds define its coordinate space; children paint in tree order. | `include/cvision/ui/view.hpp` |
| `Desktop` | App inserts it beneath `Application::root()`. | Activates windows, manages transient popup/modal state. | Re-pins docks and reclamps windows on resize. | `include/cvision/widgets/desktop.hpp` |
| `Window` | Desktop takes ownership with `add_window`. | Its content participates in normal focus traversal. | Draws frame/shadow and gives its content the inner rectangle. | `include/cvision/widgets/window.hpp` |
| Layout | Window/content parent owns it. | Layouts are normally not focused. | Allocates child bounds during layout/resizes. | `include/cvision/ui/layout.hpp` and related headers |

## The common construction sequence

The layouts example shows the ordinary explicit construction style: make a
Desktop, transfer it to the root, then build chrome and windows.

<!-- ckvision-snippet source="examples/layouts/layouts_app.cpp" lines="21-33" -->
```cpp

LayoutsApp::LayoutsApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_chrome();
    build_window();

    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_focus(splitter_);
```
<!-- /ckvision-snippet -->

## Modal versus modeless surfaces

Windows on a Desktop are modeless: users may activate, move, resize, tile, and
cycle them. A standard dialog presentation is modal: input is scoped to the
dialog subtree until it completes, then focus returns to the invoking view.
Use a dialog result/completion callback instead of making the caller own a
dialog window. See [dialogs and commands](dialogs-and-commands.md).

## Paint and data flow

Your code mutates widget state through public methods such as `set_text`,
`set_items`, or `set_fraction`. That invalidates the affected view. On the next
application step, views paint into the composed scene and the Presenter sends
only the changed terminal cells/raster regions. Clients need not manually flush
or manually repaint individual widgets.

For resizing, choose a layout container or a `DesktopGrowPolicy` before using
manual bounds. Manual bounds are appropriate for the fixed placements in a
small dialog; `Row`, `Column`, `Grid`, `Dock`, `AnchorPane`, `Overlay`, and
`Splitter` express the intended relationship under changing terminal sizes.
