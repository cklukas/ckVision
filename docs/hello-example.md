---
title: Hello Example
author: C. Klukas
date: 2026-08-08
format: guide
description: The ckVision-owned behavior contract and verification evidence for the compact Hello example.
---

# Hello Example

`examples/hello` is a compact ckVision integration example. It is authored
and specified inside this repository: a desktop with menu and status chrome,
a greeting command, and a modal dialog. It is deliberately small enough to
exercise ordinary public application construction without becoming a second
widget specification.

## Behavior contract

- Alt+G opens a modal `Hello, World!` dialog.
- The dialog contains the `How are you?` label and four independently
  focusable dismiss buttons: Terrific, Ok, Lousy, and Cancel.
- Esc and every button dismiss the dialog. While it is open, the background
  Alt+X command is not active; it is restored after the dialog closes.
- Alt+X requests application exit, and F10 activates the menu bar.

The design sources are VISION.md's one-screen application principle,
DECISIONS.md D-012/D-014/D-021, and ROADMAP.md M9. The authoritative visual
contract is the pair of checked-in golden frames, not a comparison with an
external application.

## Verification

`tests/test_hello_golden.cpp` drives the real public application path and
checks the initial and dialog frames against `tests/golden/hello_initial.dump`
and `tests/golden/hello_greeting.dump`. It separately verifies modal command
scoping, button dismissal, and the exit shortcut. The `hello_line_budget`
CTest gate counts non-comment, non-blank lines across the three example source
files and fails above the 60-line M9 limit.

`tools/docgen/capture_hello_screenshots.cpp` drives the same path to produce
the screenshots included with the example applications documentation.
