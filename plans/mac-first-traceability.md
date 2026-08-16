<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Mac-first traceability record

This record is the WP-38-MAC acceptance artifact. It maps the current Mac
delivery work to its executable evidence and records the exact gates run for
the baseline being committed.

## Package evidence

| Package | Implementation evidence | Acceptance evidence |
|---|---|---|
| WP-23–WP-36C | `ARCHITECTURE.md`, `DECISIONS.md`, `include/`, `src/`, and the established widget/example plans | Full CTest suite, layer/provenance/documentation gates, existing package goldens and scripts |
| WP-39 | `include/cvision/widgets/editor_document.hpp`, `src/widgets/editor_document.cpp` | `tests/test_editor_document.cpp`, editor goldens, editor fuzz corpus |
| WP-40 | `include/cvision/widgets/text_editor.hpp`, `src/widgets/text_editor.cpp` | `tests/test_text_editor.cpp`, `tests/test_editor_smoke.cpp`, editor visual captures |
| WP-41–WP-42 | Syntax cache/profile implementation and standard profiles | `tests/test_syntax_cache.cpp`, `tests/test_syntax_profile.cpp`, syntax fuzz corpus |
| WP-43–WP-44 | Search, file controller, editor window, example and documentation | `tests/test_editor_search.cpp`, `tests/test_file_editor_controller.cpp`, editor smoke/golden tests, `docs/editor.md` |
| WP-45 | Private terminal emulator, capability profile, bounded parser and raster state | `tests/test_terminal_emulator.cpp`, `tests/test_terminal_cell_golden.cpp`, terminal fuzz corpus, `tests/fixtures/snake.six` |
| WP-46 | POSIX PTY session, bounded readiness drain, lifecycle and resize handling | `tests/test_posix_terminal_subsession.cpp`, `tests/posix_terminal_launch_contract.cpp` |
| WP-47 | Interactive TerminalView, input/focus/mouse/paste/scrollback and nested child path | `tests/test_terminal_view.cpp`, `tests/terminal_redraw_contract.cpp`, nested-child PTY cases |
| WP-48 | Sixel decoder, scene raster bridge, clipping, fallback and hostile-input bounds | `tests/test_virtual_display_visual_golden.cpp`, terminal view raster tests, Sixel limits/recovery tests |
| WP-49 | Runnable terminal example, menus/windows, docs and generated captures | `examples/terminal`, `docs/embedded-terminal.md`, `terminal_visual_capture`, terminal app tests |
| WP-53 | Installable CMake package, runnable example handoff, client archive, and independent consumer | `tests/install_package_smoke.cmake`, `tests/client_bundle_smoke.cmake`, `tools/package_client_bundle.sh`, `docs/client-handoff.md` |
| WP-54 | Versioned release audit, public API/example evidence index, limits, and acceptance checklist | `docs/release-audit.md`, `docs/api-index.md`, `plans/example-coverage.md` |

## Gate record

| Gate | Command/evidence | Result |
|---|---|---|
| Build | `cmake --build build -j2` | Passed with the configured Mac toolchain |
| Unit/integration/golden suite | `ctest --test-dir build --output-on-failure` | 133/133 passed |
| Installed consumer and handoff archive | `ctest --test-dir build -R '(install_package_smoke|client_bundle_smoke)' --output-on-failure` | Passed |
| Terminal fuzz corpus | `ctest --test-dir build-fuzz-llvm -L fuzz-corpus --output-on-failure` | 8/8 passed with Homebrew LLVM |
| Terminal sanitizer suite | `ctest --test-dir build-asan-llvm -R 'suite_test_(terminal_emulator|terminal_view|posix_terminal_subsession|menu|terminal_cell_golden|presenter)' --output-on-failure` | 6/6 passed with ASan/UBSan |
| Layer/provenance | `python3 tools/check_layers.py`; `python3 tools/check_provenance.py --root .` | Passed |
| Documentation | `python3 tools/docgen/check_docs.py --root . --coverage --links --screenshots` | Passed |
| Working-tree hygiene | `git diff --check` | Passed |

## Mac acceptance statement

The current baseline has executable Mac evidence for the editor and contained
terminal work, including malformed-input recovery, process lifecycle,
alternate-screen behavior, nested child containment, Sixel/no-graphics
degradation, menu interaction, resize, focus, and documentation captures.
The next packages are now focused on finishing quality and release readiness,
not adding unverified feature claims.

## Known limitations

The strict full sanitizer build still exposes pre-existing missing-designated-
initializer warnings in unrelated test sources. The terminal sanitizer lane
is green; the warning backlog is tracked for WP-52 rather than hidden by a
global warning-policy change.
