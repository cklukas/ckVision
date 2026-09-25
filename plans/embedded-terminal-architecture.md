# Embedded terminal architecture

## Product promise

ckVision 0.1 includes a resizable `TerminalView` that hosts a real child
terminal session inside a ckVision window. A client can run an explicit command
(for example Bash, another terminal application, or a separate ckVision
application) and receive normal terminal input, resize behavior, alternate
screen operation, and Sixel graphics without allowing the child to corrupt or
paint outside the parent application's scene.

This is not a byte-forwarding shortcut. The child never writes to the outer
terminal. It writes to a private PTY/ConPTY; ckVision emulates that session
off-screen and re-renders the resulting cells and raster content through the
ordinary scene compositor. The outer presenter is the only component that ever
writes to the user's terminal. This document is binding for WP-45 through
WP-50.

## Scope

### Included

- A `TerminalView` widget and explicit `TerminalSubsession` service for a
  single interactive child process, with resize, focus, input forwarding,
  scrollback, selection/copy, lifecycle state, and visible diagnostics.
- POSIX PTYs on macOS/Linux and ConPTY on Windows, each through the `term`
  layer. The child receives an independently sized terminal, not the parent's
  controlling tty.
- A defensive off-screen terminal emulator implementing the documented
  xterm-compatible profile required by the shipped integration sample,
  full-screen terminal applications, and ckVision child applications:
  UTF-8, cursor controls, SGR, erase/insert/delete, scrolling regions, DEC
  private modes, alternate screen, title/query policy, mouse, bracketed paste,
  and Sixel.
- Decoded child Sixel images converted to ckVision `Image`/raster regions,
  clipped and composited inside the view exactly like native `ImageView` and
  `Canvas` content.
- An explicit child capability profile and deterministic test emulator. The
  profile defines reported cells/pixels, color, graphics, keyboard/mouse, and
  query responses; it is never guessed from the parent environment.

### Deliberately excluded

- A host shell, process manager, remote terminal/network protocol, terminal
  multiplexer, or an unrestricted pass-through to the outer terminal.
- Arbitrary unimplemented control-sequence behavior. Unsupported control data
  is safely ignored or reported in the child-session diagnostics according to
  a documented policy; it is never emitted outward.
- Kitty graphics in the child terminal for this delivery. Child Sixel is the
  required raster protocol because it can be decoded to an ordinary `Image`
  and re-composited safely. A later capability may add another child graphics
  decoder under the same containment model.

## Containment model

```text
child process → private PTY/ConPTY → terminal emulator → TerminalView paint
                                              │                   │
                                              └─ cells + RGBA ────┘
                                                                  ↓
                                                 scene compositor / outer presenter
                                                                  ↓
                                                        user's terminal
```

The parser owns a primary and alternate cell buffer, cursor, modes, palette,
scrollback, and a raster plane for exactly one child session. It consumes child
bytes as data. No child escape sequence, OSC, DCS, or graphics payload reaches
the outer terminal writer. `TerminalView` paints only its content rectangle;
the scene compositor clips its cells and raster slices to the window, then
handles occlusion, shadows, scrolling, resize damage, and final Sixel emission
for the outer terminal.

This gives the required no-spill invariants:

1. A child cursor move, erase, title change, palette update, or private mode
   alters only its private emulator state.
2. Child raster placement is relative to the child buffer. It is translated,
   clipped, and occluded only after `TerminalView` paints it into the scene.
3. Child clear/reset/alternate-screen transitions remove affected child raster
   regions before the next composed frame; stale pixels cannot remain.
4. A parent window move, resize, overlap, shadow, or close causes normal scene
   damage/composition. It cannot leave child cells or Sixel pixels behind.
5. The same script under `NoGraphics` paints the child's mandatory textual
   fallback and emits no outer raster pixels.

## Layer and ownership design

`term` owns `TerminalSubsession`, child process launch, PTY/ConPTY adapters,
wait handles, byte I/O, capability replies, and the off-screen terminal
emulator. Its snapshots use core value types: terminal cells, cursor/mode
state, images, and diagnostics. It does not depend on `scene`, `ui`, or
`widgets`.

`ui::Application` accepts explicitly attached subsessions and multiplexes their
readiness with the outer terminal's wait handles. It drains each session using
a bounded byte/work budget and posts a normal damage request; a noisy child
cannot starve keyboard or paint processing. `widgets::TerminalView` borrows one
subsession, maps view-relative input to it, and paints a snapshot through a
clipped `scene::Painter`. Session-change notifications traverse only the live
view tree, so detaching a view removes it from future delivery; the owning
application keeps the session alive until all consumers are gone. No global
child registry or persistent transport worker is permitted; D-072 allows a
short-lived, per-session native close thread while the owner drains ConPTY
output on Windows.

On Windows, the ConPTY-facing pipe handles use synchronous I/O as required by
`CreatePseudoConsole`, while the host ends are overlapped named-pipe handles.
The adapter exposes their completion events as borrowed `WindowsHandle`
readiness sources and consumes output only through `drain()`'s byte budget.
Child startup explicitly supplies null standard handles with
`STARTF_USESTDHANDLES`; otherwise a console host launched from a redirected
parent can duplicate the parent's streams into the child, bypassing ConPTY
([Microsoft Terminal discussion](https://github.com/microsoft/terminal/discussions/15814)).
D-073 keeps the inbox ConPTY API as the dependency-free baseline. The
Windows adapter looks for Microsoft's optional `conpty.dll` next to its
executable and uses that runtime's create/resize/close calls for the whole
session when present; the architecture-matched `OpenConsole.exe` remains in
the package's prescribed subdirectory. The DLL stays loaded until the owned
close operation joins. Without it, the effective child profile has
`sixel=false`, because the named inbox host consumed child Sixel before the
adapter pipe. This is a platform capability decision; core and UI do not
inspect the filesystem or a process-global registry.

No persistent transport thread or raw child-byte forwarding is added. A
short-lived, owned close thread runs only `ClosePseudoConsole` after the child
exits, while the owner continues draining the final output (D-072). A child is assigned
to a private Windows job before its first thread resumes, so bounded teardown
can end descendants as well as the initial process. Pending I/O is cancelled
and observed before its buffers are released. The output sink is closed before
`ClosePseudoConsole`, which may otherwise block while emitting its final frame
on older Windows releases. These choices follow Microsoft's
[ConPTY session](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session),
[close](https://learn.microsoft.com/en-us/windows/console/closepseudoconsole),
[named-pipe I/O](https://learn.microsoft.com/en-us/windows/win32/ipc/synchronous-and-overlapped-input-and-output),
and [job object](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
contracts.

`TerminalLaunchSpec` is an explicit value: executable plus argument vector,
working directory, declared child environment additions/replacements, terminal
profile, initial cell/pixel dimensions, and exit policy. It does not invoke a
shell implicitly. Convenience factories may provide an explicitly named shell
only when the client asks. `TerminalSubsession` owns the child handle and owns
the close protocol: request graceful termination, bounded injectable wait,
then escalation only if the client selected that policy. Exit status is an
observable value, never an exception escaping the event loop.

## Input, resize, and nested ckVision applications

When focused, `TerminalView` encodes ordinary keyboard, text, paste, and
eligible mouse events according to the child's declared profile. Border/title
operations remain parent window operations. A configurable, reserved local
escape chord (default `Ctrl+Alt+Space`) returns command focus to the parent and
is never sent to the child; clients may also expose a menu command to do so.
Mouse reporting is forwarded only when the child has enabled it and the pointer
is within the content rectangle. Clipboard export, OSC title, hyperlinks,
notifications, and device queries all use explicit allow/deny/translate
policies; none may silently act on the outer terminal.

On a view resize, the application first changes the emulator dimensions and
pixel metrics, then resizes the PTY/ConPTY and delivers the platform-native
resize notification to the child. The child sees exactly the content rectangle,
not frame/shadow cells. A ckVision child therefore creates its own terminal
session on the slave endpoint, receives its normal terminal capabilities and
resize events, and presents into the private byte stream; it has no path to the
parent's outer session. The parent's real terminal stays under the parent
application's lifecycle ledger throughout.

## Sixel and raster handling

The child Sixel decoder is a resource-bounded parser derived from the published
Sixel grammar. It validates dimensions, palette operations, repeat counts,
payload length, and placement against `TerminalSubsessionOptions` limits. It
decodes into an RGBA `Image` with a declared cell anchor and opaque fallback
cells, not into an outer terminal byte sequence. Child cell/pixel geometry is
used to map image coordinates to the current view; resizing invalidates and
rebuilds only necessary child raster placement.

The scene sees each child image as a regular, view-owned raster region. The
existing compositor's visibility slicing governs partial occlusion and outer
Sixel re-encoding. Child output cannot bypass that path. If the outer profile
has no graphics, the child emulator still maintains its state but the view uses
the stored fallback cells. If the child profile does not advertise Sixel, its
applications see that deterministic capability and choose their own fallback.

## Defensive parser and process policy

Terminal output is hostile input. The emulator imposes explicit limits on
input and output ring size, escape/control-string size, printable run length, terminal
dimensions, scrollback lines/bytes, image pixels/palette entries, graphics per
frame, and parser work per application step. Over-limit data yields a stable
diagnostic/placeholder and recovery at a documented sequence boundary; it never
allocates without bound, blocks the owner thread, or leaks a partial sequence to
the outer terminal. Fuzzing covers all parser states, control-string recovery,
Sixel payloads, resize races, and process-exit ordering.

The emulator answers only the documented query subset from its declared child
profile. Unknown queries have a deterministic no-response or documented reply.
OSC 52 import/export is denied by default; child title and notification requests
are stored as child metadata only unless the client's policy explicitly maps
them into parent UI. The parent process's ambient environment is not read to
construct the child profile; launch environment is an explicit adapter input.

## Required evidence

WP-45 through WP-50 must add unit/property/fuzz tests for emulator and parser
state, public-path scripts for child lifecycle/input/resize/focus, real PTY and
ConPTY smokes, and cell plus decoded-pixel goldens. Required scenarios include
Bash/readline, a full-screen alternate-buffer sample, a nested ckVision child,
Sixel clipped by a moving/resized/overlapping window, `NoGraphics` fallback,
child crash/exit, resource-limit recovery, and two simultaneous subsessions.
The final evidence must show both that the child content is correct inside its
window and that every cell/pixel outside that rectangle remains owned by the
parent scene.
