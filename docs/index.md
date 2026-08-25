---
title: ckVision
---

**ckVision** is a modern C++ library for building full terminal user
interfaces — windows, menus, dialogs, widgets, embedded terminals and
graphics — with deterministic, headless-testable behaviour.

- Repository: [github.com/cklukas/ckVision](https://github.com/cklukas/ckVision)
- Releases: [v0.1.0 and later](https://github.com/cklukas/ckVision/releases)
- Built with ckVision: [ckmux](https://github.com/cklukas/ckmux), a terminal
  multiplexer with a visible interface

## Start here

- [Getting started](getting-started.md) — build, link, and a first application
- [Hello tutorial](tutorial-hello.md) — a complete application, step by step
- [Example applications](example-apps.md) — the `examples/` tree as a guided tour
- [SysInfo example](sysinfo-example.md) — injected host facts, benchmarks,
  charts, help, and report export
- [TODO example](todo-example.md) — a complete persistent Kanban application,
  custom views, editor notes, and multi-instance conflicts
- [Hello example verification appendix](hello-example.md)

## Concepts

- [Object model](object-model.md) — views, windows, the desktop, events
- [Layout guide](layout-guide.md) — sizing, anchoring, and resize behaviour
- [Themes and rendering](themes-and-rendering.md)
- [Dialogs and commands](dialogs-and-commands.md)
- [Standard commands](standard-commands.md) — the command registry, v1

## Widgets and views

- [Widget gallery](widget-gallery.md) — every widget, with screenshots
- [Data views](data-views.md)
- [Editor](editor.md)
- [Flow view](flow-view.md)
- [Embedded terminal](embedded-terminal.md) — a real PTY inside a window

## Terminal and graphics

- [Graphics](graphics.md) — Sixel and raster output
- [Terminal host integration](terminal-host-integration.md)
- [Terminal capability profiles](terminal-profiles.md)
- [Input decoder](input-decoder.md) — keys, mouse, and paste, v1
- [Text width and grapheme segmentation](text-width.md)

## Integrating and embedding

- [Platform services](platform-services.md)
- [Client handoff](client-handoff.md)
- [Client API index](api-index.md)

## Quality and internals

- [Performance verification](performance.md)
- [Fuzzing ckVision parsers](fuzzing.md)
- [Golden dump format](golden-format.md)
- [Documentation coverage](coverage.md)
