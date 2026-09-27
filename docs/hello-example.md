---
title: Hello Example Verification Appendix
author: C. Klukas
date: 2026-08-08
format: report
description: Verification appendix for the complete Hello tutorial.
---

# Hello Example: verification appendix

Start with the [complete Hello tutorial](tutorial-hello.md). It contains the
full compilable source split by file, object hierarchy, explanation of command
presentation and modal lifetime, and generated initial/menu/dialog screenshots.
This page keeps the compact behavioral and test contract for maintainers and
reviewers; it is not the primary learning path.

`examples/hello` is a compact ckVision integration example. It is authored
and specified inside this repository: an application shell with Desktop, File
menu, status line, a greeting command, and a modal dialog. It is deliberately
small enough to exercise ordinary public application construction without
becoming a second widget specification.

## Behavior contract

- Alt+G opens a modal `Hello, World!` dialog.
- F10 activates the menu bar on `File`, and Enter drops it open. The same quit
  command presents as `Exit` in that menu and `Quit` in the status line; both
  surfaces execute the one registered handler, whether `Exit` is chosen by
  keyboard or the status item is clicked.
- Both surfaces show the chord the command is bound to now: rebinding it at
  runtime changes the status line on the next frame and the menu the next time
  it opens, and the new chord quits while the old one no longer does.
- The dialog is an Info message box containing `How are you?` and its standard
  Ok action.
- Esc and Ok dismiss the dialog. While it is open, the background Alt+X command
  is not active; it is restored after the dialog closes. The handler registers a
  typed non-blocking completion without retaining a raw `Window*`.
- Alt+X requests application exit.

The design sources are the vision's one-screen application principle,
the decision log D-012/D-014/D-021, and the roadmap M9. The authoritative visual
contract is the set of checked-in golden frames, not a comparison with an
external application.

## Verification

`tests/test_hello_golden.cpp` drives the real public application path and
checks the initial and dialog frames against `tests/golden/hello_initial.dump`
and `tests/golden/hello_greeting.dump`. It separately verifies modal command
scoping, button dismissal, and the exit shortcut. Its scripts inject every key
and click through `HeadlessTerminal` and `Application::step`: F10, Enter, Up
and Enter choose `File → Exit` (`f10_file_exit_by_keyboard_runs_the_one_quit_handler`),
a click on the status line's `Quit` runs the same handler
(`clicking_the_status_line_quit_runs_the_same_quit_handler`), the open File
menu is pinned in `tests/golden/hello_file_menu.dump`, and a runtime rebind of
the quit command to Ctrl+Q is pinned in `tests/golden/hello_rebound.dump`
(status line) and `tests/golden/hello_rebound_menu.dump` (File menu)
(`a_runtime_rebind_reaches_the_docked_status_line_and_menu_and_the_new_chord_quits`).
`tools/docgen/generate_hello_golden.cpp` writes all five frames, and the
`generated_golden_bytes` gate regenerates and compares them on every host. `tests/test_wp35.cpp` covers
the command-presentation split, application shell helper, context activation,
command retraction, and per-subtree theme override. The `hello_line_budget`
and `example_hygiene` CTest gates keep the example under the 60-line M9 limit
and reject old constructor-plumbing/cast/duplicate-command patterns.

`tools/docgen/capture_hello_screenshots.cpp` drives the same path to produce
the screenshots included with the example applications documentation.
