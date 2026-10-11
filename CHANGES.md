<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Changes

ckVision follows semantic versioning from 1.0.0 onward. Fixes are patch
releases, compatible additions are minor releases, and incompatible changes to
the public C++ or CMake package surface require a new major release.

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
