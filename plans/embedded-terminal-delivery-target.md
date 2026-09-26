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
on the present macOS/Linux host. A named external-drive Windows ARM64 VM is
available for WP-50 development. Its inbox and optional-modern ConPTY suites
pass, including contained child Sixel through the modern runtime. A clean
Windows ARM64 build and live Terminal Preview session show paste, nested
ckVision input/exit, contained Sixel, a clicked File menu, concurrent children,
individual close, nonzero child exit, isolated private-host failure/recovery,
and app-wide teardown/relaunch. A newer live two-window pass also proves
distinct command streams and preserved private scrollback across window
switches; the documented File-menu pointer drag launches a fresh child. The
same verified GUI shows the Sixel text fallback under Windows Console Host.
The live cell-mouse route passes in Stable and Preview; Preview also passes
a held-motion nested-child route, a parent mouse-grip resize with a measured
private ConPTY size change, and live Sixel clipping plus private-child cleanup
after a mouse resize and individual close. The retained scene now dims
child raster pixels under higher-window shadows; three paired cell/RGBA
states cover overlap, coincident-shadow union, and exact restoration. The
three-platform 79-file manifest passes byte for byte, and a live Preview
File menu visibly dims a contained Sixel raster and restores it when closed.
A guarded live Preview title-bar drag moves that raster with 57,186 interior
RGB pixels matching exactly after translation while the vacated parent region
is uncovered.
A live child `cls` clears 46,280 sampled raster pixels without changing
78,480 sampled pixels outside the raster; closing that child frees its private host, and reopening
allocates a fresh host whose raster matches 46,020 known-good pixels exactly.
A real covering-window resize also exposed and corrected a fractional-scale
raster seam: a decoded-Sixel regression and fresh macOS/Linux/Windows full
suites pass, and the new Preview GUI preserves 36,640/36,640 exposed pixels
and restores 59,640/59,640 after close.
In another signed-in Preview run, a held Sixel-window resize shows its
mandatory cell fallback, as the documented gesture policy requires; the
raster and right-of-picture child content then restore 59,640/59,640
and 89,846/89,846 pixels exactly, and the same private PowerShell accepts a new command.
A strict live Console Host fixture records a complete degraded cell-mouse
press/release. A separate modern-ConPTY integration case verifies two concurrent
red/green rasters, peer close, survivor input, and graphics reallocation; the
inbox runtime passes the corresponding text-fallback lifecycle. Neither installed Windows Terminal host advertises SGR pixel-mouse
mode. A native private-ConPTY case now verifies mode-1016-plus-cell-metric
promotion and dual-space press, motion, and release through the real Windows
backend. A private-ConPTY abrupt-child-exit case also verifies that a
graphics peer survives and a replacement session allocates after the
terminated child is closed; the inbox runtime passes its text fallback. A new
native resource-limit case likewise rejects an oversized Sixel payload,
recovers to a valid picture, and leaves a concurrent graphics peer usable;
the modern suite passes 21/21 after a new concurrent resize-storm case; the full Windows matrix passes 164/164.
The same route now passes in the signed-in Preview GUI: forced child loss
retains its last Sixel frame, closing the exited window removes it, the
File menu allocates a new graphics child, and the original cmd peer accepts
new input. The reopened image and right-of-picture child content match the pre-loss
capture at 59,640/59,640 and 89,846/89,846 sampled pixels. Three genuine
outer-desktop strips also match at 23,120/23,120 pixels across that route.
A separate live Preview test deliberately triggers .NET FailFast in the
contained Sixel PowerShell. Windows runtime, Application Error, and Error
Reporting events confirm the fatal path; the GUI and original cmd survive,
the image remains until the exited window closes, a fresh private child
restores the image exactly, and the original cmd accepts new input. The same
three outside-window strips match at 23,120/23,120 through every phase.
A live resource-limit test now writes more than 4 MiB of Sixel from a
contained PowerShell, observes the visible limit diagnostic and following
text, renders a valid red picture in that same session, verifies cmd peer
input, then closes and reopens a normal Sixel demo with exact raster
restoration. Outside-window desktop strips remain unchanged throughout.
Live pixel mouse, other crash classes, the remaining isolation and cell/pixel
scenarios, other degraded paths, and the release matrix remain open. WP-37
consumes WP-50 rather than treating a general backend test as a substitute for
embedded-terminal acceptance.

## WP-50 named Windows scenario status — 2026-09-25

The following requested signed-in Windows Terminal scenarios are **Done** on
the named ARM64 VM. The observations and exact process/build evidence are in
[the WP-50 Windows record](windows-wp50-evidence.md). These row statuses do
not promote the complete WP-50 package or WP-37 release matrix.

| Scenario | Status | Evidence in the Windows record |
|---|---|---|
| Contained terminal in signed-in Windows Terminal Preview | **Done** | Live Preview GUI and clean verified GUI checkpoints |
| Nested ckVision input and exit | **Done** | Live Preview nested frame, `go` response, and return to its private shell |
| Child Sixel containment | **Done** | Live raster clipping, move, overlap/shadow, clear, individual close, and outside-window checks |
| No-graphics fallback | **Done** | Same verified GUI under Console Host displayed `[sixel]`; inbox ConPTY native fallback passed |
| Resize | **Done** | Live parent grip changed child ConPTY 64×17 → 57×15; held Sixel resize restored its image and child content; native two-child resize storm passed |
| Close, child loss, and fresh allocation | **Done** | Individual window close, nonzero child exit, forced and FailFast child loss, surviving peer input, and fresh private-host allocation |
| Resource-limit recovery | **Done** | Live 4.2 MiB Sixel rejection followed by valid image, peer input, close, and normal-demo restoration; native 128-byte limit case passed |
| Portable contained-scene goldens | **Done** | Contained-scene files, including child text over a partially erased picture, remain byte-identical within the current 95-file cell/RGBA/symbolic manifest on macOS, Linux, and Windows |

**WP-50 overall: Done.** Its ConPTY adapter, named live scenarios, contained
graphics and fallback, child lifecycle, alternate buffer, simultaneous
sessions, and platform-independent cells/pixels satisfy the package's stated
acceptance criteria. The installed Windows Terminal Stable and Preview hosts
do not advertise SGR pixel-mouse mode 1016; the native private ConPTY path
passes when that capability is supplied, but a real capable-host gesture is
still unverified. A signed-in mintty 3.8.3 `-P on` trial delivered a real held
drag only as SGR cell coordinates; its exact record is in
[WP-37 Windows evidence](windows-wp37-evidence.md). Capable-host pixel mouse,
legacy-host refusal, and the global release matrix remain WP-37/WP-38 work.

## Current evidence map

The present POSIX/headless implementation keeps each acceptance proof next to
the public path it exercises:

| Package | Executable evidence in this tree |
|---|---|
| WP-45 | `tests/test_terminal_emulator.cpp`, `tests/test_terminal_cell_golden.cpp`, `tests/golden/terminal_cells.visual`, `tests/golden/terminal_child_sixel.visual`, and `fuzz/fuzz_terminal_emulator.cpp` |
| WP-46 | `tests/test_posix_terminal_subsession.cpp` (Bash, alternate-screen, exit, resize, and two-session cases), `tests/posix_terminal_launch_contract.cpp`, `Application::wait_handles()`'s combined external set, and `src/ui/application.cpp`'s bounded readiness drain |
| WP-47 | `tests/test_terminal_view.cpp`, `tests/terminal_redraw_contract.cpp`, and the nested-child cases in `tests/test_posix_terminal_subsession.cpp` |
| WP-48 | `tests/test_virtual_display_visual_golden.cpp`, `tests/test_terminal_view.cpp`, `tests/test_virtual_display.cpp`, `tests/fixtures/snake.six`, the outer pixel-containment assertion in the retained-window case, and paired raw RGBA/cell goldens for real TerminalView move, resize, overlap, shadow/union/restore, clear, repaint, close, NoGraphics, and five-stage resize sequences |
| WP-49 | `examples/terminal`, `docs/embedded-terminal.md`, `tools/docgen/capture_terminal_screenshots.cpp` (shell, full-screen, nested, menu, and Sixel captures), and the `terminal_visual_capture` CTest |
| WP-50 | `src/term/windows_terminal_subsession.cpp`, `tests/test_windows_terminal_subsession.cpp`, and `tests/windows_terminal_child.cpp` provide the completed native adapter and passing inbox/modern ConPTY suites; `tools/docgen/generate_terminal_plane_goldens.cpp` and six `contained_pair_*` fixtures pin simultaneous child rasters, individual close, and replacement on all three platforms. [Named Windows evidence](windows-wp50-evidence.md) records live Preview lifecycle and private-host failure/recovery, Console Host fallback, no-enhanced-reply plain-VT ConPTY, and the repaired Sixel Demo prompt placement. True pixel mouse and the remaining matrix/release rows stay open under WP-37/WP-38. |

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
