<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Terminal host integration

`ui::Application` is an instance-owned UI loop. A process may host several
applications when each has its own `term::Terminal` session; two applications
must not share one terminal session. The application object and its view tree
belong to the thread that calls `step()` or `run()`.

## Driving the loop

For a standalone terminal program, construct one platform clock, pass that same
object to one platform terminal and one `Application`, then call `run()`. The
clock must outlive the terminal. On POSIX use `PosixClock` and `PosixTerminal`; on Windows use `WindowsClock` and `WindowsTerminal`. `run()` does not install a shutdown or SIGINT handler. `PosixTerminal` owns only the terminal session handlers described below. Input remains poll-driven; applications do not need to set their terminal descriptor non-blocking.

## Platform-service composition

Native process hosts can use the pure functions in `term/windows_argv.hpp`
to encode wide argv for `CreateProcessW`. `windows_argv_command_line()`
handles the first filename token separately from ordinary arguments and
returns no value for embedded NULs, an empty/quoted image token, or the
32767-code-unit native limit. The returned string is mutable. This follows
Microsoft's CRT parser; it is not quoting for cmd or PowerShell command text.
ConPTY's native adapter consumes the same encoder (D-129); explicit cmd
commands continue to use `TerminalLaunchSpec::windows_command_processor()`.

Windows hosts accept wide native arguments and pass UTF-8 to the shared
application. `term/windows_text.hpp` supplies `windows_utf8()` and
`windows_utf16()` for this composition (D-130). Both return owned optional
strings, preserve empty values and explicit embedded NULs, and reject malformed
Unicode/native length failures instead of using a locale or ANSI code page.
They do not normalize text. Filename and process adapters separately reject
NULs where a native zero-terminated API requires it.

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

On Windows, use `WindowsClock` and `WindowsTerminal` in the same composition.
The terminal requires console stdin/stdout with VT mode support; construction
fails with an actionable diagnostic when those handles are redirected or the
host refuses VT input/output. It checks that the host retained both VT mode
flags, distinguishes output, input, and UTF-8 setup failures, and names the
modern host or legacy-mode setting needed for a VT refusal. Windows Console
Host with "Use legacy console" enabled rejects the VT output mode, so
construction fails with `VT output cannot be enabled (Win32 87)` followed by
that advice, before any frame is written. It saves and restores the original
console modes
and code pages, decodes VT input with the same `InputDecoder`, and uses native
`CF_UNICODETEXT` for clipboard export. The first frame uses conservative
capabilities; a bounded response window may refine Sixel, pixel mouse,
colors, geometry, and the keyboard protocol. A verified kitty reply triggers a
session-owned flag push and readback; restoration pops that stack entry before
leaving the alternate screen. A silent host returns to cell mouse coordinates
and legacy keyboard input when the window expires. The named Windows Terminal
1.24 Stable host lacks kitty support; 1.25 Preview advertises it.
Resize notifications are read from the console input buffer because VT
supplies no equivalent. Bracketed paste enters as a paste-marked text event;
native `CF_UNICODETEXT` is used for export. The named VM's live Stable and Preview sessions deliver SGR cell-mouse
press/release events; both report a positive 10×20 cell-pixel metric and Sixel,
but neither confirms SGR pixel-mouse mode 1016. The backend therefore keeps
cell mouse input and leaves `Capabilities::pixel_mouse` false. A live gallery
click opens its modal and redraws the partially occluded Sixel image after
dismissal. Microsoft's [mode 1016 feature request](https://github.com/microsoft/terminal/issues/18591)
is still open. A live Preview round trip exported a known Unicode string through
`CF_UNICODETEXT` and returned the same bytes as a paste-marked Ctrl+V event.
A mode-1016 pixel report delayed beyond the bounded probe can itself establish
the capability when the cell metric is known; the backend then re-enables
mode 1016 so later events retain pixel coordinates. A late query reply alone
does not establish the capability.
On the signed-in Windows 11 ARM64 WezTerm GUI, this path delivered a held
left-button down, motion, and up with both cell and pixel coordinates. The
installed Windows Terminal Stable and Preview builds still use cell mouse.
A native Windows private-ConPTY integration test supplies positive mode-1016
and cell-size replies, then verifies the backend delivers left press, motion,
and release with both pixel and derived cell coordinates. This proves the
verified-host code path. Pixel-mouse acceptance on a capable live Windows
host, abnormal-exit restoration, and complete three-platform acceptance
remain WP-37 work.

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
descriptor and the private wake descriptor; for `WindowsTerminal` it contains
the console input handle and a private wake event. Child terminal readiness
sources follow either outer backend. `Application::next_timer_deadline_nanos()`
reports the earliest application timer, if any. The host waits on those handles
alongside its own sources until the earlier of its deadline and that timer, then
calls `step(clock.now_nanos())`. It must not close, reconfigure, or retain the
handles beyond the next `wait_handles()` call, an application/session mutation,
or the terminal/session lifetimes. Deterministic headless and replay
backends expose no outer-backend handles and are stepped directly; an attached
POSIX child session still contributes its own borrowed PTY handle.

Outer backends also include the decoder's own quiet deadlines and resolve
them after every wait result. A continuously readable IPC source cannot
postpone Escape or guarded-paste completion.

`Application::wake()` makes the POSIX wake descriptor ready, so a host wait
returns promptly without synthesizing input. Worker threads use
`Application::post()` for UI work; it queues the work and wakes the terminal.

`Application::run_until(done)` is the outer-loop convenience for a caller that
owns the loop. A quit request wins over a later completion check: if it is
already present, or arrives while the preceding `step()` dispatches input,
timers, or posted work, `run_until` returns `false` without invoking `done`
again. A host shutdown policy can therefore not accidentally turn into a
successful dialog or task completion callback.

The blocking standard-dialog helpers (`exec_modal_dialog`, `exec_modal_message_box`,
`exec_modal_file_dialog`, and `exec_modal_directory_picker`) treat that interruption as
deterministic cancellation. If their dialog is still attached, they detach it
and remove its modal scope before returning the factory's cancellation fallback.
This is a host shutdown path, so it deliberately does not invoke the dialog's
vetoable user-close callback.

## Capability probing

### Native host diagnostics

Both `PosixTerminal` and `WindowsTerminal` accept `set_output_capture(callback)`
and `set_graphics_trace(trace)`, and expose `mouse_reports_seen()`. These are
host-injected observations on the explicitly chosen native backend, not
environment-controlled library settings. Capture receives ordered output
attempts before writes, from installation onward: probe, frame, sanitized title,
bell and normal restoration bytes. Constructor output precedes installation;
native Windows clipboard exports are not terminal output. An empty callback
disables capture, and callback exceptions do not suppress native output or
restoration. POSIX fatal signal-handler output remains signal-safe and cannot
call arbitrary capture code.

The borrowed trace sink/clock must outlive the terminal. Each settled probe
window emits one capability summary, using the same vocabulary on both OSes;
disabled tracing constructs no summary. Mouse-report counts come from the
session's InputDecoder, including decoded SGR reports rather than guesses
based on application callbacks. Recording a native terminal still forwards
the real output/input; its native capture stays in place. Replay uses its
recorded operation stream without creating another native session.

`FileTraceSink::open` accepts UTF-8 filenames on Windows and uses a wide
native open, not the current ANSI code page. Unicode names preserve their
exact spelling in both truncate and append mode. Empty filenames, embedded
NULs and malformed Windows UTF-8 return no sink; Unix filenames otherwise
retain their native bytes. Binary log output keeps LF endings on both hosts.
This follows Microsoft's documented
[narrow versus wide filename handling](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fopen-wfopen?view=msvc-170),
not a host-side trace implementation.

`PosixTerminal` presents its first frame from the constructor-supplied
baseline profile; it never waits for a probe response. By default it then
queries the dynamic foreground/background colors (OSC 10/11), synchronized-
output mode (DECRQM 2026), color-scheme notification mode (DECRQM 2031),
Primary Device Attributes (DA1) for its Sixel advertisement,
XTSMGRAPHICS for finite Sixel color-register and geometry limits,
the active SGR-pixel mouse mode (DECRQM 1016), and character-cell pixel size
(XTWINOPS 16). The backend enables SGR-pixel mode for the session, but exposes
pixel mouse coordinates only when a positive cell-size reply and evidence of
active mode arrive. The latter may be DECRPM 1016 or a live SGR report beyond
the known cell grid; the two facts may arrive in either order. Replies received within the bounded
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
  exactly. Separate PTY tests stop a session with a real job-control
  `SIGTSTP` and compare its resumed entry sequence with the one it sent at
  start-up, byte for byte: the alternate screen (1049), the kitty keyboard
  push, SGR any-motion mouse (1003/1006) with pixel mode (1016), bracketed
  paste (2004), focus reports (1004) and colour-scheme notifications (2031).
  A probing session's resumed probe traffic, including its synchronized-output
  query (2026), must match its start-up probe traffic as well. The entry and
  restore sequences are one ledger, so a kitty keyboard entry the session
  adopted at runtime is popped before the alternate screen is left (kitty
  keeps one stack per screen) and pushed again, in the same place, on resume.
  If the tty changed size while stopped, its resize event
  precedes the capability event in that same poll batch, so the very first
  resumed frame uses the new dimensions. The PTY suite separately creates two
  live sessions, simulates the restored post-stop terminal state, and verifies
  that one continuation signal re-enters raw mode and the correct entry
  sequence for both before each reports its own capability change.
- Its fatal-signal restoration handler (`SIGSEGV`, `SIGBUS`, `SIGFPE`,
  `SIGILL` and `SIGABRT`) restores every registered terminal session before
  the process terminates. The PTY gate verifies both the alternate-screen
  restore bytes and the original canonical/echo mode flags for two
  simultaneously live sessions. For each of `SIGSEGV`, `SIGBUS`, `SIGFPE` and
  `SIGILL`, it also verifies that a child dying by that signal writes exactly
  the restore ledger an ordinary exit writes, and nothing after it: the
  library has no diagnostic of its own for a plain fatal signal. This is
  process cleanup, not an application-level signal-routing mechanism.

Ordinary C++ scope exit, including exception unwinding, uses the same terminal
restore path before control reaches the caller's catch handler. The PTY suite
checks the restoration sequence precedes that handler's observable output.

Application event, draw, timer, posted-work, command, focus, and loop-predicate
callbacks are noexcept-in-effect. A callback exception is a contract violation:
the POSIX backend restores every live terminal session, writes a fixed diagnostic
to stderr, and terminates with `SIGABRT`. The PTY suite points the child's
stderr at the same PTY its session writes to, so restore bytes and diagnostic
arrive as one ordered stream, and verifies that every session-restore byte
precedes the diagnostic. Backends that own terminal state
must implement the same ordering through `Terminal::terminate_after_callback_failure`.

An always-on `CKV_ASSERT` uses the same POSIX D-024 path. Its expression,
source path, and line are published as immutable metadata before `SIGABRT`; the
fatal handler restores every session and then prints the complete assertion
diagnostic to stderr. The PTY suite proves this ordering on the same single
channel, independently from the callback-failure case.

## Diagnostics

`Application::diagnostics()` is the application-owned `DiagnosticsSink`. It
always retains messages in its own buffer and can additionally forward them to
one owned, injected sink for structured host logging. At `Application`
destruction, it first ends the attached terminal session and only then emits
the buffered `trace`, `debug`, `info`, `warning`, or `error` lines through the
terminal backend's diagnostic channel. Therefore neither ordinary diagnostics
nor an injected observer require an application to write stderr while the
alternate screen is active. The PTY suite checks that order for
`PosixTerminal` on one channel: the child's stderr is the PTY its session
writes to, and every restore byte arrives before the diagnostic.
`HeadlessTerminal` deliberately keeps that final
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

## OSC emission safety

Three operating-system commands leave ckVision with caller-supplied text in
them: the window title, the clipboard export, and the hyperlinks the
Presenter puts around linked cells. Each is an OSC command string, which ends at ST — `ESC \` in a 7-bit
code, the single byte 0x9C in an 8-bit one (ECMA-48 §8.3.143) — and which the
xterm control-sequences reference also ends at BEL. CAN and SUB cancel a
sequence in progress, and ECMA-48 §8.3.89 admits no other control into a
command string. Text that carried any of these could end the string early and
have the rest read as new controls: a second title, a clipboard write, a
screen erase. The architecture §12 therefore requires every emission to reject
or escape them, and `text::sanitize_osc_text` is where that happens:

- It drops every C0 control (ESC, BEL, CAN, SUB, tab and line feed among
  them), DEL, and every C1 control code point (U+0080–U+009F, which include
  U+009C ST and U+009B CSI as a UTF-8 host meets them).
- It replaces malformed UTF-8, including a lone 0x9C or 0x9B byte, with
  U+FFFD, whose encoding has no byte below 0xA0. A character whose encoding
  merely contains such a byte as a continuation (U+201C is E2 80 9C) is kept.

The result is well-formed UTF-8 in which no 7-bit, 8-bit, or UTF-8 spelling of
a terminator or introducer survives. The emitters are:

- **Window title (OSC 0).** `Terminal::set_title` sends
  `osc_title_sequence(title)` (`cvision/term/osc_sequences.hpp`):
  `OSC 0 ; <sanitized title> BEL`. `PosixTerminal`, `WindowsTerminal` and
  `HeadlessTerminal` all send exactly these bytes. A title is one line, so its
  tab and line breaks are dropped too.
- **Clipboard export (OSC 52).** `PosixTerminal` and `HeadlessTerminal` send
  `osc_clipboard_sequence(text)`: `OSC 52 ; c ; <base64> BEL`, and only while
  `Capabilities::clipboard_write` is set. The payload is RFC 4648 base64,
  whose alphabet has no control byte, so the text needs no stripping and
  arrives on the clipboard unchanged, tabs and line breaks included. Clipboard
  import stays an input event marked as paste, sanitized by the input decoder.
  `WindowsTerminal` exports through the native clipboard instead.
- **Hyperlinks (OSC 8).** The Presenter sends `osc_hyperlink_open(target)`,
  `OSC 8 ; id=<hex> ; <target> ST`, before a run of linked cells and
  `OSC 8 ; ; ST` after it, and only while `Capabilities::hyperlinks` is set
  (see [Hyperlinks (OSC 8)](#hyperlinks-osc-8) below). A target is not
  sanitized but validated: it must already be a URI of printable ASCII with
  no space (`is_valid_hyperlink_target`), so it holds no control, DEL, C1
  code point or malformed byte, and one that is not is dropped — the text is
  shown without a link — rather than repaired into a different address. The
  check is made where a target enters a cell (`LinkTable` refuses it) and
  again by `osc_hyperlink_open`, which returns nothing for it.

`HeadlessTerminal`'s display models OSC 0, OSC 8 and OSC 52 strictly and
invalidates the capture for a title holding any control or malformed byte, a
hyperlink whose target is not valid or whose `id` is not the one ckVision
derives from it, or a payload that is not strict base64. The pinned bytes for a hostile title (ESC `\`,
BEL, a raw 0x9C, U+009C, CAN, SUB, DEL, a line break and a nested OSC 0) and
for hostile clipboard text are checked through `set_title` and
`write_clipboard` on `HeadlessTerminal`, through `RecordingTerminal`, and on a
live `PosixTerminal` over a real PTY.

### Hyperlinks (OSC 8)

A cell can be part of a hyperlink (D-088). `Painter::draw_text` takes an
optional target and writes it into every cell of the run, a wide glyph's
continuation column included; `TextView` does so for each link span whose
target is a terminal hyperlink. A cell stores the link as a small id into the
`LinkTable` of the grid that holds it — a `Surface`, the composed frame, the
decoded display — in storage its layout already had, so a cell without a link
costs nothing. Tables count their references and drop a target once no cell
uses it. Shadows and other style passes keep a cell's link; the compositor
carries it into the frame by target through occlusion and moves; golden dumps
record it as `link` runs ([golden format](golden-format.md)).

No protocol lets an application ask a terminal whether it renders OSC 8, so
`Capabilities::hyperlinks` is never probed. It is off in every curated
profile; a host that knows better sets it in an explicit `Capabilities` value
or through `CapabilityOverrides::hyperlinks`
([profiles](terminal-profiles.md)). Off, nothing changes: linked cells are
presented as the plain text they also are, byte for byte, and a change of
link alone repaints nothing.

On, the Presenter brackets every contiguous run of cells with one target in
exactly one open and one close. It closes at the end of every run, and so
before any cursor addressing — the re-address after a non-ASCII grapheme
(D-019), a new row, a raster, the final cursor — and a frame never ends with
a hyperlink open. The `id` parameter is `hyperlink_id(target)`, sixteen hex
digits of the target's FNV-1a hash, so the pieces of one link — a span wrapped
over two lines, a run split by a re-address, a half repainted by a later frame
— are one hyperlink to the host, which groups cells by id and target
together. Two spans with the same target are therefore one hyperlink too.
Links are compared by target from frame to frame, so a frame that renumbers
its table repaints nothing that did not change.

Everything else about a link stays inside ckVision: `TextView` still marks,
steps through and follows its links with Tab, Enter and the pointer, and a
terminal's own click on a hyperlink is the terminal's business.

## Windows native readiness

`term::WindowsWaitSet` is an instance-owned native adapter for a host that
combines borrowed console input, process handles and manual-reset readiness
events. It uses Microsoft's wait-completion packets and one completion port:
no worker threads, thread pool, 64-source limit or incomplete-source polling.
`WindowsTerminal::poll(deadline, extra_sources)` uses this same adapter.

Construct a set with the host's injected `Clock`, then call
`wait(absolute_deadline_nanos, sources)`. INT64_MAX means indefinitely;
finite fractional milliseconds round up, and an empty set returns immediately.
Each call updates membership and returns a batch of distinct completed
`WaitHandle`s. This is notification, not an atomic snapshot of all kernel
states. A still-signaled source is rearmed on the next call, never repeatedly
inside its own batch. Duplicates coalesce; zero/non-Windows entries are ignored.
Mutexes/semaphores are not readiness sources. The returned span belongs to the
instance and expires on the next wait/clear.

Keep borrowed sources live until a subsequent call omits them, or call
`clear()` before closing them. Removal retires a unique registration key
before cancellation and packet close, so a late completion cannot dereference
retired storage or masquerade as a replacement. A pending canceled packet is
never immediately reused. Native setup/wait failures throw `std::system_error`;
a failed wait clears registrations before propagating the error. No fallback
silently substitutes a bounded or threaded wait when the required API is absent.

The Windows regression target drives the real adapter and public terminal
path, including more than 64 sources, changes while registered, duplicate/hot
sources, rearming, cancellation races, native errors and handle cleanup.
These backend contracts do not by themselves certify an embedding app's
transport or user-facing native acceptance.

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
