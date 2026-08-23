<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Terminal host integration

`ui::Application` is an instance-owned UI loop. A process may host several
applications when each has its own `term::Terminal` session; two applications
must not share one terminal session. The application object and its view tree
belong to the thread that calls `step()` or `run()`.

## Driving the loop

For a standalone terminal program, construct one `PosixClock`, pass that same
object to one `PosixTerminal` and one `Application`, then call `run()`. The
clock must outlive the terminal. `run()` does not install a shutdown or SIGINT
handler. `PosixTerminal` owns only the terminal session handlers described
below. Input remains poll-driven; applications do not need to set their
terminal descriptor non-blocking.

## Platform-service composition

Platform contracts use ordinary, explicitly owned instances; there is no
process-wide service locator. `Clock`, `FileSystem`, and `ClipboardWriter` are
core contracts. POSIX adapters live in `term`: `PosixClock`,
`PosixFileSystem`, and `TerminalClipboardWriter`. A terminal owns poll, wait
handles, and wake because those are properties of that particular terminal
session, not a generic application service.

For a standalone POSIX application, compose clipboard export with the terminal
explicitly:

```cpp
ckv::term::PosixClock clock;
ckv::term::PosixTerminal terminal(clock);
ckv::term::TerminalClipboardWriter clipboard(terminal);
ckv::ui::Application app(terminal, clock, clipboard);
```

`Application` also has the compact two-argument constructor; it owns an
equivalent terminal-backed clipboard adapter for that instance. The
three-argument form is for a native host clipboard or a deterministic test
double. It borrows the supplied bridge, so the bridge must outlive the
application. `MemoryClipboardWriter`, `ManualClock`, and `MemoryFileSystem`
provide deterministic, in-memory composition with no filesystem, environment,
or wall-clock access. Two applications use distinct terminal sessions and
distinct service instances; no service state crosses from one to the other.

Clipboard import remains an input event marked as paste. ckVision never issues
an OSC 52 read request; the injected bridge is export-only.

Normal presentation and session writes are lossless under output backpressure:
if the supplied terminal endpoint reports that it is temporarily unwritable,
the POSIX backend waits for writability and resumes the same byte sequence
rather than discarding a suffix. An embedding host that supplies such an
endpoint must therefore continue consuming terminal output; otherwise the
owning UI thread is intentionally backpressured with it.

An embedding host drives the application by calling `step()` on its owning
thread. `Application::wait_handles()` exposes a borrowed, read-only set of
backend-native wait sources; for `PosixTerminal` it contains the terminal input
descriptor and the private wake descriptor, followed by the readiness sources
of every attached child terminal session. `Application::next_timer_deadline_nanos()`
reports the earliest application timer, if any. The host waits on those handles
alongside its own sources until the earlier of its deadline and that timer, then
calls `step(clock.now_nanos())`. It must not close, reconfigure, or retain the
handles beyond the next `wait_handles()` call, an application/session mutation,
or the terminal/session lifetimes. Deterministic headless and replay
backends expose no outer-backend handles and are stepped directly; an attached
POSIX child session still contributes its own borrowed PTY handle.

`Application::wake()` makes the POSIX wake descriptor ready, so a host wait
returns promptly without synthesizing input. Worker threads use
`Application::post()` for UI work; it queues the work and wakes the terminal.

`Application::run_until(done)` is the outer-loop convenience for a caller that
owns the loop. A quit request wins over a later completion check: if it is
already present, or arrives while the preceding `step()` dispatches input,
timers, or posted work, `run_until` returns `false` without invoking `done`
again. A host shutdown policy can therefore not accidentally turn into a
successful dialog or task completion callback.

The blocking standard-dialog helpers (`exec_dialog`, `exec_message_box`,
`exec_file_dialog`, and `exec_directory_picker`) treat that interruption as
deterministic cancellation. If their dialog is still attached, they detach it
and remove its modal scope before returning the factory's cancellation fallback.
This is a host shutdown path, so it deliberately does not invoke the dialog's
vetoable user-close callback.

## Capability probing

`PosixTerminal` presents its first frame from the constructor-supplied
baseline profile; it never waits for a probe response. By default it then
queries the dynamic foreground/background colors (OSC 10/11), synchronized-
output mode (DECRQM 2026), color-scheme notification mode (DECRQM 2031),
Primary Device Attributes (DA1) for its Sixel advertisement,
XTSMGRAPHICS for finite Sixel color-register and geometry limits,
the active SGR-pixel mouse mode (DECRQM 1016), and character-cell pixel size
(XTWINOPS 16). The backend enables SGR-pixel mode for the session, but exposes
pixel mouse coordinates only when both its active-mode and positive cell-size
replies arrive; their order is immaterial. Replies received within the bounded
250 ms probe window, measured on that injected clock, refine
the instance's capabilities and arrive through `poll()` as a
`CapabilityChangedEvent`; no reply, or a late reply, silently leaves the
baseline in place. A resize or terminal resume starts a fresh window. Hosts
with a curated or forced profile pass `false` for
`enable_capability_probes` in `PosixTerminal`'s constructor to keep that
profile authoritative.

Before a fresh window starts, the backend withdraws any runtime-probed cell
pixel metric and pixel-mouse capability. A resized terminal may have changed
font or display scale, so old pixel-to-cell conversion is never used while the
new metric is unknown. Pixel mouse returns only after the new window has
independently confirmed both DECRPM 1016 and XTWINOPS 16; a disabled-probe
profile retains its explicit host-supplied values unchanged.

The response protocols do not carry a request identifier. ckVision therefore
fences any reply sequence already incomplete when a new probe window begins:
its final bytes are consumed safely but cannot refine the new capabilities.
A complete delayed response that first arrives after the boundary is
indistinguishable from a timely fresh response on these published protocols;
the 250 ms deadline is an evidence policy, not a causal identifier. Hosts that
need stronger certainty use an explicit authoritative profile with probing
disabled.

For synchronized output, the backend briefly enables DEC mode 2026, queries
its active state, and immediately resets it. This makes a positive DECRPM 2026
response support evidence while leaving the baseline frame unbuffered; the
presenter later enables and resets the mode around individual frames only when
that capability has been verified. The session ledger also carries the reset
so every abnormal restoration path is safe.

While probes are enabled, `PosixTerminal` enters DEC mode 2031 and restores it
on every terminal-session exit path. A positive DECRQM 2031 report enables
live `CSI ? 997 ; 1 n` (dark) / `CSI ? 997 ; 2 n` (light) notifications for
the session. OSC 11 is the direct background-based hint; OSC 10 supplies an
inverse-contrast fallback only when OSC 11 has not answered. Both remain
deadline-bounded; only that verified notification stream is accepted after the
250 ms window.

When a terminal reports a finite Sixel geometry maximum, the presenter keeps
the raster's mandatory text fallback instead of emitting an oversized image.
Likewise, a finite color-register maximum bounds the Sixel encoder's palette.

For the named conservative multiplexer and Linux-console baselines, see
[Terminal capability profiles](terminal-profiles.md). Those are host-selected
profiles, never library-side environment detection.

An explicit `KeyboardProtocol::Kitty` profile makes the POSIX session push
kitty's disambiguation and event-type enhancements (`CSI > 3 u`) when it enters the
alternate screen and pop that exact saved state (`CSI < u`) before leaving it.
This uses kitty's documented stack mechanism, so ckVision never guesses a
pre-existing keyboard state. `KeyboardProtocol::ModifyOtherKeys` is instead a
host assertion: xterm's public modification control does not provide an
equivalent state stack, so the library does not overwrite a host setting it
cannot restore exactly. See the [kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/)
and [xterm control-sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).

## Signals and shutdown

Shutdown policy is host-owned. In particular, ckVision never treats `SIGINT`
as an application command and no POSIX signal handler calls an `Application`
method directly. A host that maps an interrupt to graceful shutdown records
the signal using its own async-signal-safe mechanism (for example, a flag plus
a host-owned pipe), then on the owning thread calls `request_quit()`.
`request_quit()` wakes the terminal wait itself, so no separate `wake()` call
is required for shutdown.

`PosixTerminal` installs only the terminal-session machinery it needs:

While one or more POSIX terminal sessions are live, the D-024 registry owns
the dispositions for those terminal-session signals. It restores the host's
saved dispositions after the final session ends. An embedding host must not
replace those same dispositions during a live ckVision terminal session.

- `SIGWINCH` interrupts a blocked POSIX poll. A terminal reports a resize
  only when its own tty geometry changed, so a resize from one PTY cannot
  resize another application.
- `SIGTSTP` restores every registered session before the process stops.
  `SIGCONT` restores raw mode and terminal entry sequences, then causes each
  resumed terminal to emit a capability-change event. `Application` handles
  that event by invalidating and fully re-presenting the frame. The PTY
  acceptance test compares the pre-suspend and resumed presenter frame bytes
  exactly; session-entry and probe traffic are deliberately outside that
  frame comparison. If the tty changed size while stopped, its resize event
  precedes the capability event in that same poll batch, so the very first
  resumed frame uses the new dimensions. The PTY suite separately creates two
  live sessions, simulates the restored post-stop terminal state, and verifies
  that one continuation signal re-enters raw mode and the correct entry
  sequence for both before each reports its own capability change.
- Its fatal-signal restoration handler restores every registered terminal
  session before the process terminates. The PTY gate verifies both the
  alternate-screen restore bytes and the original canonical/echo mode flags
  for two simultaneously live sessions. This is process cleanup, not an
  application-level signal-routing mechanism.

Ordinary C++ scope exit, including exception unwinding, uses the same terminal
restore path before control reaches the caller's catch handler. The PTY suite
checks the restoration sequence precedes that handler's observable output.

Application event, draw, timer, posted-work, command, focus, and loop-predicate
callbacks are noexcept-in-effect. A callback exception is a contract violation:
the POSIX backend restores every live terminal session, writes a fixed diagnostic
to stderr, and terminates with `SIGABRT`. The PTY suite verifies that every
session-restore byte precedes that diagnostic. Backends that own terminal state
must implement the same ordering through `Terminal::terminate_after_callback_failure`.

An always-on `CKV_ASSERT` uses the same POSIX D-024 path. Its expression,
source path, and line are published as immutable metadata before `SIGABRT`; the
fatal handler restores every session and then prints the complete assertion
diagnostic to stderr. The PTY suite proves this ordering independently from the
callback-failure case.

## Diagnostics

`Application::diagnostics()` is the application-owned `DiagnosticsSink`. It
always retains messages in its own buffer and can additionally forward them to
one owned, injected sink for structured host logging. At `Application`
destruction, it first ends the attached terminal session and only then emits
the buffered `trace`, `debug`, `info`, `warning`, or `error` lines through the
terminal backend's diagnostic channel. Therefore neither ordinary diagnostics
nor an injected observer require an application to write stderr while the
alternate screen is active. `HeadlessTerminal` deliberately keeps that final
emission in-memory; tests use an injected sink to inspect it without host I/O.
`RecordingTerminal` records each post-restore diagnostic as an ordinary terminal
operation. `ReplayTerminal` retains its replayed diagnostic bytes in memory and
never emits them to the host process, so operation-by-operation replay also
covers the final lifecycle diagnostic channel.
A PTY contract also places `RecordingTerminal` around a live `PosixTerminal`:
an actual raw-PTY key batch and a forwarded output operation are then replayed
solely from the captured initial state and operation stream. This complements
the deterministic headless application round trip; replay itself never opens
or borrows the original terminal session.

## Multi-application boundary

`wake()`, terminal input, resize observation, focus, modal state, commands,
and quit requests are all application-local. The PTY acceptance test creates
two live `PosixTerminal`/`Application` pairs in one process and verifies that
input, wake, resize, and quit activity for the first does not mutate the
second. A process-wide fatal signal remains intentionally process-wide.

## Local multiplexer smoke observation

The dependency-free PTY tests are the portable acceptance evidence. As a
supplementary, non-gating observation on 2026-08-08, the built Hello example
was launched in fresh detached sessions of tmux 3.6b and GNU Screen 4.00.03 on
the local macOS host. In each case its documented Alt+X Quit command ended the
application and the detached session. This proves only the basic launch and
quit path on those host versions; it does not replace the conservative-profile
tests or claim support for unprobed extensions.
