# Clean-room derivation record

This record is the D-012/WP-23 authority bridge for material that was
rewritten during the 2026-08-08 provenance audit. It names only ckVision's
approved specifications and executable evidence; it is not an external-source
comparison.

## Hello example

| Retained behavior | Approved specification | Executable evidence |
|---|---|---|
| A one-screen desktop application demonstrates menu, status, and dialog construction | VISION.md “Easy things easy”; ROADMAP.md M9 | `examples/hello/`, `tests/test_hello_golden.cpp` |
| Alt+G opens the greeting dialog and Alt+X exits | `docs/hello-example.md` behavior contract; ROADMAP.md M9 | `alt_g_opens_the_greeting_dialog`, `alt_x_quits_via_the_status_line_shortcut` |
| The dialog is modal; Esc and each of its four buttons dismiss it | DECISIONS.md D-021; VISION.md modal-dialog capability | `while_the_greeting_dialog_is_open_background_accelerators_are_scoped_out`, `every_greeting_button_dismisses_the_dialog` |
| Initial and dialog appearance remain stable | DECISIONS.md D-014 | `tests/golden/hello_initial.dump`, `tests/golden/hello_greeting.dump` |
| The example remains compact | ROADMAP.md M9 | `tools/check_hello_line_budget.py`, `hello_line_budget` CTest |

## Review controls

The repository-level `provenance_check` CTest gate scans tracked and
non-ignored text artifacts for source-comparison, fidelity, and transcription
claims. Its paired `provenance_checker_self_test` proves both rejection and
clean-room acceptance examples. The checker is a narrow guard, not evidence
of provenance by itself; reviewers must apply the checklist in AGENTS.md.

## Quarantined material

The former external-framework conversion chapter and its dependent comments
were removed from the working tree. The current Hello documentation is
`docs/hello-example.md`; its content is defined by the records above.
