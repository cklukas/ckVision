# ckVision

**ckVision** is a windowed terminal-UI framework for C++20 — overlapping
movable windows, drop-down menus, modal dialogs, complete keyboard *and* mouse
interaction, themes, and first-class Sixel graphics, drawn through a
damage-tracked pipeline that repaints only what changed.

It takes the interaction grammar of the classic desktop-in-a-terminal
applications of the early 1990s and rebuilds it on modern grounds: modern C++,
modern terminals, modern library architecture.

```cpp
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);

    ckv::ui::Application app(terminal, clock, clipboard);
    MyApp my_app(app);          // your view tree
    app.run();
}
```

The host owns the terminal, the clock, and the clipboard and passes them in.
Nothing in the library reaches for a global, and nothing reads the wall clock
behind your back — which is also why the whole framework can be driven
headlessly in a test.

## What you get

- **Windows and menus** — a `Desktop` with z-order, activation, cycling, tile
  and cascade; windows with frame chrome, move/resize, zoom and a vetoable
  close protocol; a menu bar with mnemonics, shadows, context menus and
  light-dismiss.
- **Widgets** — Label, StaticText, Button, InputLine (validators, input masks,
  password echo, history), CheckGroup, RadioGroup, ListView, TreeView,
  TextView, Memo, Table, Scrollbar, ScrollViewport, ImageView, Canvas,
  Splitter, StatusLine, plus message boxes, a file open/save dialog, a
  directory picker, a window list and a help viewer.
- **Dialogs as data** — declarative descriptors that materialize into a
  validated dialog, with accept-veto and Esc-cancel.
- **Graphics** — a public-protocol Sixel encoder, cropped raster overlay that
  respects window occlusion, and pixel-precise SGR-Pixels mouse input.
- **Terminal handling** — an incremental input decoder covering legacy, kitty
  and modifyOtherKeys key encodings, SGR mouse, sanitized bracketed paste,
  focus events and capability probe replies; a presenter that diffs cells,
  degrades color depth to what the host has, and emits synchronized output.
- **Themes** — a flat, interned-role theme system with Dark, Light and Mono
  schemes.
- **A text editor core** — revisioned documents, `TextEditor`, language
  profiles, incremental syntax caching, search and replace, and a safe file
  workflow.
- **Backends** — POSIX (real termios, signals, PTY-tested), headless, and
  record/replay. macOS is the verified platform; Linux builds and is close
  behind, with GCC's stricter warning set still being worked through;
  Windows/ConPTY is a target, not yet a claim.

Zero mandatory dependencies. C++20. `find_package(ckvision)` gives you one
target: `ckvision::cvision`.

## Status

Version 0.1, pre-release. The framework is substantially implemented and
substantially tested — the suite covers unit behavior, byte-exact golden
output, PTY contracts, fuzzed decoders, allocation budgets, and visual
captures. CI builds it on macOS, Linux and Windows plus dedicated Address,
UndefinedBehavior and Thread sanitizer lanes; macOS is the lane held green
today, and the others are being brought to it.

It is not yet a finished 1.0. No milestone has been signed off against its
full written acceptance criteria: cross-platform gates, some performance and
security evidence, and parts of the documentation are still open. The public
API is stable enough to build real applications on — the example apps are real
applications — but expect it to move before a tagged release.

## Build

Needs a C++20 compiler and CMake 3.25+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/examples/ckvision_hello
```

Run the suite:

```bash
ctest --test-dir build --output-on-failure
```

Install and consume from another project:

```bash
cmake --install build --prefix /your/prefix
```

```cmake
find_package(ckvision CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ckvision::cvision)
```

## Examples

`examples/` are built, tested and screenshot from the same object graphs the
documentation describes — they are the fastest way in.

| Example | Shows |
|---|---|
| `hello` | The smallest complete host: terminal, clock, clipboard, view tree, loop |
| `gallery` | Every widget, keyboard and mouse navigation, Sixel images |
| `filebrowser` | `TreeView` driving `ListView` over the real filesystem |
| `layouts` | Row/Column layout and resize behavior |
| `forms` | Dialogs, validation, focus restoration, wizards |
| `graphics` | `ImageView`, `Canvas`, and graceful degradation without Sixel |
| `editor` | The document editor: profiles, search/replace, file workflow |
| `terminal` | An embedded terminal in a window |
| `workbench` | A larger multi-window application |

## Documentation

Start with [getting started](docs/getting-started.md), then the complete
[Hello tutorial](docs/tutorial-hello.md) and the
[object model](docs/object-model.md).

| Document | Content |
|---|---|
| [getting-started.md](docs/getting-started.md) | Build, install, and the minimal host application |
| [tutorial-hello.md](docs/tutorial-hello.md) | Complete Hello source, hierarchy, and walkthrough |
| [object-model.md](docs/object-model.md) | Application/view/Desktop/window ownership, focus, events, painting |
| [layout-guide.md](docs/layout-guide.md) | Choosing a layout container, and resize behavior |
| [widget-gallery.md](docs/widget-gallery.md) | Every public widget with usage and screenshots |
| [dialogs-and-commands.md](docs/dialogs-and-commands.md) | Commands, dialogs, validation, focus restoration, wizards |
| [standard-commands.md](docs/standard-commands.md) | The standard command identifiers |
| [themes-and-rendering.md](docs/themes-and-rendering.md) | Theme roles and the render model |
| [graphics.md](docs/graphics.md) | ImageView, Canvas, and Sixel/no-graphics behavior |
| [editor.md](docs/editor.md) | Revisioned documents, TextEditor, profiles, search/replace |
| [embedded-terminal.md](docs/embedded-terminal.md) | Running a terminal inside a window |
| [data-views.md](docs/data-views.md) | Table and list data binding |
| [flow-view.md](docs/flow-view.md) | The flow view |
| [platform-services.md](docs/platform-services.md) | The terminal/clock/filesystem/clipboard host boundary |
| [terminal-profiles.md](docs/terminal-profiles.md) | Known terminals and their capabilities |
| [terminal-host-integration.md](docs/terminal-host-integration.md) | Embedding ckVision in an existing event loop |
| [input-decoder.md](docs/input-decoder.md) | Key, mouse, paste and probe-reply coverage |
| [text-width.md](docs/text-width.md) | Unicode width and grapheme handling |
| [api-index.md](docs/api-index.md) | Curated public-header index |
| [example-apps.md](docs/example-apps.md) | What each example demonstrates and how it is tested |
| [hello-example.md](docs/hello-example.md) | Hello verification appendix and golden evidence |
| [golden-format.md](docs/golden-format.md) | The golden capture format |
| [performance.md](docs/performance.md) | Allocation and cost gates, and the p99 procedure |
| [fuzzing.md](docs/fuzzing.md) | Fuzz targets and corpora |
| [coverage.md](docs/coverage.md) | Machine-checked docs-to-header/example/test traceability |
| [client-handoff.md](docs/client-handoff.md) | Producing an installable SDK and example bundle |

Regenerate the documentation visuals and rendered outputs with
`tools/docgen/generate_docs.sh`.

## Provenance

ckVision shares no code and no API with Turbo Vision or any port or derivative
of it, or with any other prior framework. It is inspired by what those
programs *did* — the interaction grammar a user could see and learn — and its
behavior is derived from published standards (ECMA-48, xterm ctlseqs, the
kitty protocol specs, Unicode UAX #11/#29, terminfo(5), POSIX) and from
documented black-box observation of terminals. Contributions are held to the
same rule; see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE). Copyright (c) 2026 Dr. Christian Klukas.
