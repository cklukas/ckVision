<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Changes

ckVision follows semantic versioning from 1.0.0 onward. Fixes are patch
releases, compatible additions are minor releases, and incompatible changes to
the public C++ or CMake package surface require a new major release.

## Unreleased

## 1.2.0 — 2026-10-20

A minor release: live log viewing, inline completion and responsive
directory dialogs, SVG screenshots as public API, and a Sixel cursor fix
that matters to any application embedding a terminal. Every change is an
addition; code written for 1.1.0 builds unchanged.

### Added

- Live logs. `widgets::LogModel` keeps a bounded, read-only log and decodes a
  byte stream into it incrementally; `LogView` and `LogPanel` show it with
  follow and inspect modes, `LogFilter` and `LogSearch` filter and find within
  it without blocking the view, and `LogFollower` and `LogController` compose
  them into a following file viewer. Sources implement the contracts in
  `<cvision/core/log_source.hpp>` and deliver through `core::LogMailbox`;
  `term::NativeLogFileService` observes and reads growing files natively.
  The [live logs guide](docs/live-logs.md) and the installed
  `examples/log_clients` recipes show the composition; the Log Monitor example
  is the full application.
- `core::ProcessStreamService`, with `term::NativeProcessStreamService`, runs a
  child process and streams its output in bounded pieces, independently of the
  terminal subsession model.
- `Application::set_frame_delivery_policy(FrameDeliveryPolicy::Bounded)` paces
  every frame, small text included, behind one completion question, for
  applications whose output arrives continuously. `Adaptive` stays the default.
- Inline completion. `InputLine` takes an optional completion provider and
  shows its suggestion as a subdued suffix (role `input_completion`); Right
  accepts it as one undoable edit and Tab keeps moving focus. `SearchBox` and
  an editable `ComboBox` can reuse their field's provider, and `SearchBox`
  takes a custom prompt.
- `core::DirectoryService`, with a native implementation in `term`, gives file
  and folder dialogs filtered, paged discovery and path completion. Enter in
  the path field navigates absolute and relative directories, `..` included,
  outside UI dispatch; very large directories no longer stall typing or
  cancelling. `FileSystem::open_directory` reads a directory one entry at a
  time.
- `scene::render_svg_snapshot` renders any `FrameView`, with an optional Sixel
  raster plane, as a self-contained SVG screenshot. It was the private tooling
  behind ckVision's own figures; applications can now publish screenshots of
  themselves the same way. Its output does not depend on the process locale.
- Smaller additions: `TabControl::remove_tab` and `on_change`,
  `next_text_row` for allocation-free wrapping one row at a time, and
  `ListItem::borrowed_text` so a model can lend long captions without copying
  them during paint. The Completion Lab example demonstrates completion.

### Fixed

- `TerminalEmulator` now leaves the cursor on a child's Sixel picture's last
  row, in the column the picture began in, as a Sixel-capable host does. It
  stayed on the picture's first row, so the newline and prompt a program
  printed after its picture were written over the picture's second row, and
  a host drawing the terminal showed a black bar across the picture.

## 1.1.0 — 2026-10-11

### Added

- `ckv::term::TerminalLost`, a `std::system_error`, reports that the terminal
  a session runs on is gone. `PosixTerminal` and `WindowsTerminal` throw it
  when output can no longer be written or console input can no longer be read.
  Code that already catches `std::system_error` keeps working unchanged.

### Fixed

- A terminal that goes away no longer ends the process with `SIGABRT`.
  `Application::step()` and `run_until()` used to treat the backend's
  session-ending exception as an application-callback failure. The process
  aborted with "application callback threw", and macOS showed a crash report.
  Every callback boundary now lets `TerminalLost` through to the caller, even
  when a callback was writing to the terminal as it went away. That
  includes painting, ringing the bell and setting the title. Exceptions that
  application code throws itself still end the process as before.
- `PosixTerminal` notices when its input hangs up (end of file or a read error
  in raw mode). It delivers whatever the input still held, a recovered paste
  included, and then throws `TerminalLost`. Before, a session that drew
  nothing waited forever on input that could no longer arrive.

### Upgrading

Catch `ckv::term::TerminalLost` around the loop that drives `Application`. Let
the `Application` and the terminal unwind, which restores the terminal, and
report `what()` afterwards. A program without such a handler ends through
`std::terminate`, just as it would for any other uncaught exception.

## 1.0.0 — 2026-10-09

First release under the semantic-versioning commitment. The public C++ and
CMake package API is the 1.x compatibility baseline.
