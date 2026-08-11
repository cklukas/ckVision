# Embedded terminal delivery target and acceptance plan

## Client target

The delivered public path lets a ckVision application create a normal,
resizable `Window`, place a `TerminalView` in its content, and explicitly launch
a child command. The terminal view behaves as a well-contained terminal: users
can work in Bash or another terminal application, the child resizes with the
window, and a nested ckVision application renders and receives input normally.
Sixel emitted by the child appears only within that content rectangle and is
correctly clipped, occluded, moved, resized, shadowed, and removed by the
parent application.

The binding design is [embedded-terminal-architecture.md](embedded-terminal-architecture.md).
This document defines the delivery order and observable proof for WP-45 through
WP-50.

The intended client shape is:

```cpp
term::TerminalLaunchSpec shell = term::TerminalLaunchSpec::program(
    "/bin/bash", {"--noprofile", "--norc"});
shell.profile = term::embedded_xterm_sixel_profile();

term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(shell));
auto terminal = std::make_unique<widgets::TerminalView>(session);
window->set_content(std::move(terminal));
```

Names may improve during the design package, but the explicit launch, ownership,
profile, and containment semantics may not be weakened.

## Delivery sequence

| Work package | Deliverable | Exit proof |
|---|---|---|
| WP-45 | Core terminal emulator, child capability profile, snapshot/raster model, parser limits, and D-042 architecture record | Unit/property/fuzz tests for cells, cursor/modes, UTF-8, control recovery, alternate buffers, and resource limits |
| WP-46 | POSIX PTY session/process adapter, application readiness integration, lifecycle protocol, and basic `TerminalView` | Real macOS/Linux PTY scripts for Bash, exit, focus, input, resize, and two isolated sessions |
| WP-47 | Complete interactive `TerminalView` behavior: mouse/paste/query policy, scrollback/selection, window composition, and nested ckVision child path | Public keyboard/mouse/resize/modal scripts and cell goldens for normal/alternate buffer interaction |
| WP-48 | Child Sixel decoder, raster-to-scene bridge, no-spill containment, outer graphics/no-graphics behavior, and hostile-payload hardening | Decoded-pixel goldens proving clipping/occlusion/clear/resize; parser fuzzing and resource-limit tests |
| WP-49 | Terminal example application, public guide, diagnostics, and end-to-end acceptance harness | Documented runnable Bash/full-screen/nested-ckVision/Sixel scenarios with generated screenshots |
| WP-50 | Windows ConPTY child-session adapter and three-platform embedded-terminal matrix | Named Windows Terminal/ConPTY evidence plus byte-identical platform-independent goldens and degraded rows |

WP-45 through WP-49 are platform-independent in their contracts and may proceed
on the present macOS/Linux host. WP-50 is explicitly host-blocked until a
Windows/Windows Terminal environment is available. WP-37 consumes WP-50 rather
than treating a general backend test as a substitute for embedded-terminal
acceptance.

## Current evidence map

The present POSIX/headless implementation keeps each acceptance proof next to
the public path it exercises:

| Package | Executable evidence in this tree |
|---|---|
| WP-45 | `tests/test_terminal_emulator.cpp`, `tests/test_terminal_cell_golden.cpp`, `tests/golden/terminal_cells.visual`, `tests/golden/terminal_child_sixel.visual`, and `fuzz/fuzz_terminal_emulator.cpp` |
| WP-46 | `tests/test_posix_terminal_subsession.cpp` (Bash, alternate-screen, exit, resize, and two-session cases), `tests/posix_terminal_launch_contract.cpp`, `Application::wait_handles()`'s combined external set, and `src/ui/application.cpp`'s bounded readiness drain |
| WP-47 | `tests/test_terminal_view.cpp`, `tests/terminal_redraw_contract.cpp`, and the nested-child cases in `tests/test_posix_terminal_subsession.cpp` |
| WP-48 | `tests/test_virtual_display_visual_golden.cpp`, `tests/test_terminal_view.cpp`, `tests/test_virtual_display.cpp`, `tests/fixtures/snake.six`, and the outer pixel-containment assertion in the retained-window case |
| WP-49 | `examples/terminal`, `docs/embedded-terminal.md`, `tools/docgen/capture_terminal_screenshots.cpp` (shell, full-screen, nested, menu, and Sixel captures), and the `terminal_visual_capture` CTest |
| WP-50 | Intentionally pending a real Windows/Windows Terminal host; the non-Windows build reports an explicit unavailable adapter state and is not acceptance evidence |

The map is an audit aid, not a substitute for the complete release matrix:
the final global gate still requires the named Windows host evidence for WP-50.

## Acceptance matrix

| Capability | Required proof |
|---|---|
| Isolation | Tests capture outer presenter bytes and prove no child escape/control/Sixel payload was forwarded; cells/pixels outside the view are unchanged except ordinary parent composition |
| Process lifecycle | Explicit argv/cwd/environment, spawn error, child exit, graceful close, configured escalation, parent teardown, and two simultaneous children are deterministic and leak-free |
| Terminal compatibility | UTF-8, SGR, cursor/edit/erase, scroll regions, alternate buffer, documented DEC modes, query replies, bracketed paste, and child mouse profile have fixtures and public PTY scripts |
| Input/focus | Focused keyboard/text/paste/mouse events reach only the selected child; parent border/chrome input remains local; reserved escape returns parent command focus |
| Resize | The child sees the exact content cell/pixel extent through its PTY/ConPTY; resize storms preserve buffer, cursor, and raster ownership without stale cells/pixels |
| Nested ckVision | A separately launched ckVision app runs against the child endpoint, handles its own session modes/resize/input, and is visually confined to the parent window |
| Sixel | Child Sixel is decoded to bounded image state, re-composited through the parent scene, and passes moving/overlap/shadow/clear/close pixel goldens; `NoGraphics` emits no raster pixels |
| Security/robustness | Malformed/oversized escapes, OSC/DCS, Sixel, query loops, output floods, process crashes, and reentrancy cannot hang, over-allocate, cross the outer-terminal boundary, or corrupt another session |
| Public usability | `examples/terminal` documents full source, object hierarchy, launch policies, input escape, diagnostics, Bash/nested ckVision/Sixel screenshots, and the exact test/build path |
| Global release | Fuzz/sanitizer/performance bounds, macOS/Linux/Windows PTY/ConPTY runs, all themes, and WP-38 traceability hold before M11 closes |

## Non-negotiable constraints

- Never forward raw child bytes to the outer `Terminal`; only the outer
  presenter may write there.
- Never attach a child application to the parent application's outer terminal
  session. A child receives a distinct PTY/ConPTY endpoint.
- Never derive child terminal capabilities or launch environment by hidden
  reads of the parent process environment. The adapter receives explicit
  launch/profile values.
- Never expose a child OSC, clipboard, title, hyperlink, notification, device
  query, or graphics request without an explicit policy decision and a
  containment-preserving translation.
- Never treat a decorative screenshot or a raw-byte substring as evidence for
  graphics containment. Sixel acceptance requires decoded pixel-plane goldens
  that assert both the child window and unaffected parent regions.

## Release handoff

WP-49 supplies the complete POSIX/headless implementation evidence. WP-50
supplies the Windows-specific child session evidence. WP-37 records the full
three-platform host matrix and WP-38 independently audits every terminal view
state, scheme, graphics/degradation path, public reference page, and example
scenario before release acceptance.
