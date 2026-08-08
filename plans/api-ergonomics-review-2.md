# ckVision — Historical API Ergonomics and Integration Review #2

This file is a deliberately reduced historical index. Its former detailed
landing narrative was removed from the working tree during WP-23 because it
made unsupported external-source comparisons and contradicted the current
60-line Hello gate. It is not an implementation specification, an acceptance
record, or an executable queue.

The authoritative current execution queue is
[corrective-assurance.md](corrective-assurance.md). Developers must use its
first `In progress` row, otherwise its first `Ready` row, for “work on the
next open WP.” The current authority chain is VISION.md, ARCHITECTURE.md,
ROADMAP.md, DECISIONS.md, and then the corrective plan.

## Historical findings retained as index entries

| Earlier review area | Current corrective owner | Current status |
|---|---|---|
| Root sizing and resize integration | WP-24 | Reopened |
| Pointer-target lifetime and buried-window activation | WP-25 | Reopened |
| Desktop/window ownership and dialog attachment | WP-26 | Reopened |
| Run loop, modality, terminal session, and blocking API | WP-27 | Reopened |
| Platform-service boundary | WP-28 | Reopened |
| Hostile-paste contract | WP-29 | Reopened |
| Unicode and grapheme-safe clipping | WP-30 | Reopened |
| Damage, retained paint, backing stores, and budgets | WP-31 | Reopened |
| Portable test infrastructure and CI | WP-32 | Reopened |
| Sixel/no-graphics virtual display and capture | WP-32A | Partial landing; reopened |
| Fuzzing and sanitizer program | WP-33 | Reopened |
| Widget catalog closure | WP-34A through WP-34C | Reopened |
| Theme, command, example ergonomics, and Hello acceptance | WP-35 | Reopened |
| Layout/dialog/tree literal acceptance | WP-36 | Reopened |
| Windows backend and three-platform matrix | WP-37 | Open / host-blocked |
| Golden, script, reference, and release-gate closure | WP-38 | Blocked by queue |

## Historical evidence policy

Earlier commits remain version-control history, but their descriptions are not
specification evidence. A current work package may cite a prior landing only
after re-deriving its behavior from an approved ckVision authority and adding
current executable evidence. The re-derivation for the rewritten Hello example
is recorded in [clean-room-derivation.md](clean-room-derivation.md).

## Superseded Hello note

The Hello example's current behavior and test evidence live in
[docs/hello-example.md](../docs/hello-example.md). Its executable budget is
60 non-comment, non-blank lines across `hello_app.hpp`, `hello_app.cpp`, and
`main.cpp`, enforced by `hello_line_budget`. The current behavior contract is
pinned by `tests/test_hello_golden.cpp` and its two D-014 golden frames.
