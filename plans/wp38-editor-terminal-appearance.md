<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# WP-38 editor and contained-terminal appearance audit — 2026-09-25

This is the editor/contained-terminal slice of WP-38's state × scheme ×
graphics-profile matrix. It inventories current executable and visual evidence;
it does not promote the full appearance matrix or WP-38 to Done. The four
built-in schemes are classic, dark, light, and mono. Paths below name
checked-in artifacts; executable producers are identified where a capture
alone would otherwise be weak evidence.

| Surface and state | Existing executable or byte-exact evidence | Current visual capture | Remaining WP-38 gap |
|---|---|---|---|
| Editor search, selection, and caret across all four schemes | `tests/test_editor_smoke.cpp` drives the public View commands and compares `editor_search.dump`, `editor_search_dark.dump`, `editor_search_light.dump`, and `editor_search_mono.dump`; `tools/docgen/generate_editor_goldens.cpp` generates them | `docs/generated/screenshots/editor-search.svg` shows one scheme | **Done** for four-scheme symbolic search state; additional pixel/state variants remain in the broader matrix |
| Editor initial and close-confirm states | `tests/test_editor_smoke.cpp` compares `editor_initial.dump` and `editor_close_confirm.dump` | `editor-initial.svg`, `editor-json.svg`, `editor-close-confirm.svg` | Dark/light/mono byte-exact initial and close-confirm states are **Open** |
| Terminal initial shell across four schemes | `tests/test_terminal_app.cpp` verifies the View menu entries and executes their commands with two open windows across all four schemes; this is scripted behavior, not four byte-exact frames | `terminal-initial.svg`, `terminal-initial-dark.svg`, `terminal-initial-light.svg`, `terminal-initial-mono.svg` | Scheme-specific symbolic and RGBA goldens are **Open** |
| Terminal full-screen child and nested ckVision | `tests/test_terminal_app.cpp`, `tests/test_terminal_view.cpp`, and the POSIX/Windows child-session suites cover public and private interaction; the named Windows Terminal GUI run is in [WP-50 evidence](windows-wp50-evidence.md) | `terminal-full-screen.svg`, `terminal-nested.svg` | State × scheme byte-exact visual captures are **Open** |
| Contained Sixel and no-graphics fallback | `tests/test_virtual_display_visual_golden.cpp` and the generated `contained_no_graphics.dump`/`.rgba` pair pin the declared default-profile scene; [WP-50 evidence](windows-wp50-evidence.md) records both live Windows Terminal Sixel and Console Host fallback | `terminal-sixel.svg`, `terminal-no-graphics.svg` | The initial contained scene now has paired symbolic/RGBA goldens under all four schemes in both Sixel and no-graphics profiles; later state × scheme/profile combinations remain **Open** |
| Contained move, resize, overlap, shadow, clear, repaint, close, and two-child replacement | `tests/test_virtual_display_visual_golden.cpp` plus paired `contained_*.dump`/`.rgba` goldens cover the default scheme/profile; `tests/check_generated_goldens.py` enumerates the portable manifest | Generated terminal scenario captures and [Windows live evidence](windows-wp50-evidence.md) cover representative states | The default-profile cell/pixel pairs are **Done**; four-scheme and wide-text/shrink combinations remain **Open** |

The existing `generated_golden_bytes` gate covers the named checked-in files,
not an automatically generated Cartesian product of every state and scheme.
The broader widget/chrome appearance rows in [WP-38](corrective-assurance.md#wp-38--golden-script-reference-and-release-gate-closure)
remain outside this slice. This audit makes the unfilled combinations explicit
before adding captures; it is not a reason to rerun completed live Windows
scenarios without a relevant code change.

Runtime scheme switching initially left retained desktop/window surfaces in
the previous colors. `Application::set_theme()` now invalidates the subtree;
the four generated contained-scene dumps differ by scheme while the decoded
Sixel plane remains the same image. The no-graphics profile has a transparent
pixel plane under every scheme.

At `2646d0a`, the full macOS CTest suite passed 177/177. A fresh Linux
Release build from the committed source and a clean Windows ARM64 Release
build from a hash-verified archive each generated the same 63 terminal
graphics files and six editor files as the checked-in goldens. This is
three-host byte identity for this 69-file editor/terminal slice; the full
79-file manifest and the broader WP-38 release gates still need their own
three-host audit. The Terminal example now exposes live scheme changes in its
View menu; the two-window command path is scripted.
