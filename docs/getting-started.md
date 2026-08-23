---
title: Getting Started with ckVision
author: C. Klukas
date: 2026-08-09
format: report
description: Build and run a small ckVision terminal application.
---

# Getting started

ckVision is a C++20 terminal UI library.  Link the `ckvision::cvision` target,
construct an `ui::Application` from a terminal and clock supplied by the host,
build a view tree, then let `Application::run()` own the ordinary interactive
loop. The examples are the most useful starting point because they are built,
tested, and screenshot from the exact same object graphs documented here.

## Build the library and examples

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/examples/ckvision_hello
```

## Install and consume the package

Install a built tree to a chosen prefix. The framework headers and CMake
package are always installed. When examples are enabled, the runnable examples
are installed in `bin/`, including `ckvision_terminal`.

```bash
cmake --install build --prefix "$PWD/ckvision-install"
./ckvision-install/bin/ckvision_editor_profile_sample
```

An application consuming that installation links the public target:

```cmake
find_package(ckvision CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ckvision::cvision)
```

For a local prefix, configure the client with
`-DCMAKE_PREFIX_PATH=/absolute/path/to/ckvision-install`. The
`install_package_smoke` CTest gate stages an installation, builds a separate
consumer through this exact `find_package` path, and runs it.

On macOS and Linux, the shipped POSIX terminal and clock are the normal host
adapters. On every platform, `HeadlessTerminal` and `ManualClock` provide a
deterministic test host. Do not let widgets read the clock, terminal, clipboard
or filesystem themselves: pass services through `Application` or the relevant
public API. See [platform services](platform-services.md).

## The smallest complete host

This is the actual interactive entry point from Hello. `HelloApp` owns the
application-specific view graph; the host owns the terminal, clock, clipboard,
and loop.

<!-- ckvision-snippet source="examples/hello/main.cpp" lines="1-18" -->
```cpp
// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Hello — a compact visual integration example; see
// docs/hello-example.md for its behavior contract and golden coverage.
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"

#include "hello_app.hpp"

int main() {
    ckv::term::PosixClock clock; ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard); ckv::hello::HelloApp hello(app);
    app.run(); return 0;
}
```
<!-- /ckvision-snippet -->

The next step is the [Hello tutorial](tutorial-hello.md). It explains every
object above, includes all of the app source, and shows the resulting frames.

## Choose an example by problem

| You need | Start with |
|---|---|
| A menu, status line, command, and message box | [Hello](tutorial-hello.md) |
| A general application shell with windows and a form | [Gallery](example-apps.md#gallery) |
| Resizable layout containers | [Layout guide](layout-guide.md) |
| Forms, validation, help, standard dialogs, and a wizard | [Dialogs and commands](dialogs-and-commands.md) |
| Editing, lists, trees, tables, tabs, and utility chrome | [Widget gallery](widget-gallery.md) |
| Images or custom raster drawing | [Graphics](graphics.md) |

## Client model in one minute

`Application` owns a root `View`. Your app inserts a `Desktop` beneath that
root. The Desktop owns top/bottom docks, windows, and transient popups. A
window owns one content view, and layout containers or ordinary `View` objects
own the controls below it. Ownership always flows through `std::unique_ptr`;
non-owning pointers are only cached for later interaction.

Input enters `Application`, which gives the focused/modal part of that tree
first chance to handle it, then resolves command key bindings. Painting flows
the other way: the tree paints into a frame, then the terminal presenter emits
the minimal terminal update. The [object model](object-model.md) gives the
complete picture.
