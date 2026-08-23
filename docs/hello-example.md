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
- F10 activates the `File` menu. The same quit command presents as `Exit` in
  that menu and `Quit` in the status line; both surfaces execute the one
  registered handler.
- The dialog is an Info message box containing `How are you?` and its standard
  Ok action.
- Esc and Ok dismiss the dialog. While it is open, the background Alt+X command
  is not active; it is restored after the dialog closes. The handler registers a
  typed non-blocking completion without retaining a raw `Window*`.
- Alt+X requests application exit.

The design sources are the vision's one-screen application principle,
the decision log D-012/D-014/D-021, and the roadmap M9. The authoritative visual
contract is the pair of checked-in golden frames, not a comparison with an
external application.

## Verification

`tests/test_hello_golden.cpp` drives the real public application path and
checks the initial and dialog frames against `tests/golden/hello_initial.dump`
and `tests/golden/hello_greeting.dump`. It separately verifies modal command
scoping, button dismissal, and the exit shortcut. `tests/test_wp35.cpp` covers
the command-presentation split, application shell helper, context activation,
command retraction, and per-subtree theme override. The `hello_line_budget`
and `example_hygiene` CTest gates keep the example under the 60-line M9 limit
and reject old constructor-plumbing/cast/duplicate-command patterns.

`tools/docgen/capture_hello_screenshots.cpp` drives the same path to produce
the screenshots included with the example applications documentation.
