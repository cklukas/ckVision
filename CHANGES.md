<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Changes

The installed SDK advertises compatibility within its major and minor version
line. See [D-164](DECISIONS.md#d-164--regular-sdk-version-checks-for-the-21-api-transition-2026-11-23)
for the 2.1 transition.

## 2.1.0 — 2026-11-22

### Upgrade from 2.0

Request `find_package(ckvision 2.1 CONFIG REQUIRED)` and rebuild every
consumer with the 2.1 SDK. `InputLine` now uses `on_change` and
`on_activate` in place of `on_edited` and `on_accept`. `CellGrid` and
`Scrollbar` use `on_change` for the former `on_changed` and
`on_position_changed` events. See [the API index](docs/api-index.md) for
the callback names and signatures of other controls.

### New Features & Improvements

- **Path dialog lifetime:** The shared file and folder dialog releases its
  directory session and disconnects its borrowed list model while the
  window layout is still valid, including destruction without a close
  callback.
- **New widgets & dialog fields (`WP-101`, `WP-110`):** Added `CheckBox`,
  `GroupBox`, `Separator`, `Spacer`, `Toast`, `InfoBar`, `Expander`, `Link`,
  and `ContextMenu`, plus `DialogResult` keyed accessors and `Password`,
  `Slider`, and `SpinBox` dialog `FieldKind`s.
- **Unified widget ergonomics (`WP-109`):** Standardized value-change callbacks
  on `on_change` / `on_activate` and `set_role_override()` across interactive
  widgets; added `Slider` size hints/orientation/getters, `SpinBox` size hints
  and `step()`, `PropertyInspector` vertical scrolling, and `Table`/`TreeView`
  multi-selection; split `common_components.cpp` into per-widget TUs.
- **Layout, focus & theme (`WP-100`, `WP-108`):** Added `Grid` `TrackSpec`
  weights & padding, `Dock`/`Overlay` size hints, `Row`/`Column` padding and
  `set_spec()`, `make_item<T>()` container helpers, `Theme::set_fallback()`,
  context-scoped `CommandRegistry::bind_key()`, and hidden/disabled focus
  routing guards.
- **i18n, RTL & accessibility (`WP-111`):** Completed `UiStrings` wiring and
  live `on_strings_changed()` propagation, Unicode mnemonics, `LayoutDirection`
  (`Ltr`/`Rtl`), and `View` semantic `AccessibleInfo`.
- **Core, scene & terminal performance/parity (`WP-97`–`WP-99`, `WP-106`,
  `WP-107`):** Fixed `FileSystem` relative/Win32-extended path normalization,
  `WindowsTerminal` parity/`SetConsoleCtrlHandler` restoration, `PosixTerminal`
  `SIGTERM`/`SIGHUP` handling, `InputDecoder` DCS/PM/SOS/SS3/paste framing,
  deterministic `Image::content_id()`, damage-driven `Presenter`, `Compositor`
  `same_content()` & row-band damage, and `SixelEncoder` overflow LUT.
- **Exports, fuzzing & CI (`WP-102`, `WP-103`, `WP-112`, `WP-115`):** Complete
  explicit `CVISION_API` visibility exports (`CXX_VISIBILITY_PRESET hidden`),
  installed `ckvision.pc` (`pkg-config`), 6 fuzzers (`sixel_decoder`, `base64`,
  `capability_report`, `search_filter`, `key_chord`, `date_time`), and hardened
  Windows/macOS/Linux CI.
- **360-review remediation (`WP-114`–`WP-122`):** Fixed `WindowsTerminal`
  compile/parity gaps, `TreeView`/`Expander` UAFs, `TerminalEmulator::resize()`
  CoW, `SixelEncoder` alpha compositing, `InputDecoder::drain()` invalid APC
  containment, interned `"ckv.notice.*"` roles, `ContextMenu` modal capture,
  `InputLine` password clipboard/history isolation, `Grid` `TrackKind::Auto`
  and live `Row`/`Grid` RTL layout, `enabled_in_tree()`/`KeyAction::Release`
  guards, and stale 1.x doc snippets.

## 2.0.0 — 2026-11-16

### Breaking Changes

See [docs/migration-2.0.md](docs/migration-2.0.md) for the complete `1.4 -> 2.0`
migration guide and mechanical client upgrade checklist.

- **Compact 32-byte `Cell` & split-index color encoding (WP-85):** Split
  palette/truecolor encoding without legacy `Color` helpers; `static_assert(sizeof(Cell) == 32)`.
- **Zero-legacy `Theme` & `RoleRegistry` (WP-86):** Replaced `ClassicPalette`
  and `Theme::style(RoleId)` shims with `RoleRegistry`, `StandardRoles`, and
  `Theme::resolve(RoleId)` across all themes and widgets.
- **Unified `Application::step()` & zero-overhead `FunctionRef` (WP-87):**
  Removed legacy `process_events()` / `render()` splits and `std::function` hot-path allocs.
- **Clean-slate 2.0 visual language & theme contrast (WP-88..WP-92):**
  Unified `MenuBar` / `DropdownMenu`, `StatusLine` (`CommandPresentation`),
  `Window` / `Desktop` chrome (`make_standard_window_menu()`, `Alt+-` control
  menu, `Alt+0` Window List, `F12` / `Ctrl+Shift+P` `CommandPalette`),
  `TextEditor` / `EditorWindow` / `Dialog` UX, and WCAG-verified `classic`,
  `dark`, `light`, and `mono` palettes.
- **Fast `O(damage)` `Presenter`, `DECSTBM` scroll-intent, OSC 8 & Sixel LRU (WP-94):**
  Delta-SGR `std::to_chars`, `ColorDepth`-degraded underline colors,
  scroll-region hardware scroll hints, and 8 MiB Sixel LRU cache.
- **Data views, unified `ListItem`, `FileDialog`, `Table` & `TerminalView` (WP-95):**
  `ListItem::caption()` + `ListItem::borrowed()`, left-click selection gating,
  unified `DirectoryService` picker, preserved typed filenames, natural/typed
  `Table` sorting & resize, and `TerminalView` diagnostic banner & scrollback selection.
- **Pure modular SDK headers, `app.strings()`, compile-safe layouts (WP-96):**
  Relocated 9 internal headers to `src/`, split `common_components.hpp` into 9
  focused headers, protected raw `View::add_child`/`add`/`make` on layout
  containers, added `Window::set_detach_on_close(bool)` and bounds-free `Splitter`
  constructor, and unified localization on `Application::strings()` (`UiStrings`)
  and `Application::standard_roles()`.

## 1.4.0 — 2026-10-22

### Changed

- The TODO example and `ck-sysmon` moved to [CK Utilities](https://github.com/cklukas/ckUtilities), where they continue as
  `ck-todo` and `ck-sysmon` with their tests, goldens, fuzzers and guides.
  Packages now carry seventeen example programs (D-151).
- `ckvision_gallery --version` prints the installed release; it is the
  packages' smoke test and the Windows Start-menu entry.

## 1.3.0 — 2026-10-21

### Added

- Native Windows entry points for every example, including the native SysInfo probe and explicit terminal capability policy.
- Complete SDK/example DEB, RPM, archives, Homebrew formula and Windows MSI packages.
- Raspberry Pi V3D engine activity and shared GPU buffer accounting through native C++ sysfs/debugfs reads.

- `ck-sysmon`, a separate desktop resource monitor with local and passwordless
  SSH hosts, CPU/RAM/GPU bars, histories and loaded Ollama models. Menus provide
  host checkboxes, theme selection, About and a clock/calendar. Theme, metric
  toggles and host choices save automatically. The layout fills the terminal
  without spacer rows and keeps Ollama information compact.
- Optional Apple unified GPU activity and driver memory counters through public
  IOKit calls, with explicit unified-RAM labels and strict missing-value handling.
- Reproducible monitor screenshots, a user guide and native packages: Homebrew,
  Linux DEB/RPM/archives and Windows per-user MSI/ZIP, with install verification
  in the applicable CI lanes. The framework SDK remains a separate install surface.


### Fixed

- A file followed without a `LogWake` target could take up to the
  reconciliation interval (1 s by default) to show an appended line, even
  though the native notification had arrived. `LogFollower::next_deadline()`
  reported the reconciliation deadline while the session already had a
  pending change hint, so `LogController` did not step the lane. It now
  reports the hint as immediate work. Seen on Windows, where one append raises
  a single directory notification; with a wake target the hint already kicked
  the lane.

## 1.2.1 — 2026-10-20

A patch release. It fixes a Windows deadlock in the process services and
makes the 1.2.0 test suite pass on every platform and sanitizer lane CI
runs. Code written for 1.2.0 builds unchanged. 1.2.0 was tagged but its CI
did not pass, so it has no GitHub release; use 1.2.1.

### Fixed

- On Windows, a streamed child process could hang forever when its output
  arrived faster than one pump call drained it and nothing was taking from
  the mailboxes, for example under a `DropOldest` policy with no consumer.
  The pump ended its pass with no read outstanding and then waited for a
  signal that could not come, while the child stayed blocked on its full
  pipe. It now keeps a read outstanding. POSIX was not affected.

### Build

- Visual Studio builds compile each project's sources on every core
  (`/MP`). A clean Windows build of the repository takes about 4 minutes
  instead of 10 on a 12-thread machine.

### Tests

- Four tests asserted one interleaving of correct concurrent code, or
  depended on the host's temporary directory or sleep granularity, and failed
  intermittently or on one platform: the process-stream cancellation case,
  the Log Monitor export worker and growing-file cases, and the POSIX
  drive-like path case. Each now states only what the code guarantees. The
  fuzz lane builds every fuzz target, including the three log fuzzers added
  in 1.2.0.

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
