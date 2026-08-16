<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Client handoff

This guide produces a local SDK and example bundle from one configured build.
It is designed for a developer who needs an exact, inspectable artifact rather
than a collection of paths inside a working tree.

## Build, verify, and bundle

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
tools/package_client_bundle.sh build /tmp/ckvision-client
```

The command refuses an existing destination. It writes both
`/tmp/ckvision-client/` and `/tmp/ckvision-client.tar.gz`. The directory
contains:

- `sdk/`: headers, `libcvision`, CMake package metadata, and runnable examples
  in `sdk/bin/`;
- `docs/source/`: the versioned documentation source; and
- `docs/generated/screenshots/`: fresh SVG evidence from the same build.

The installed package is consumed with
`find_package(ckvision CONFIG REQUIRED)` and
`target_link_libraries(my_app PRIVATE ckvision::cvision)`. Point a client at
the staged SDK with `-DCMAKE_PREFIX_PATH=/absolute/path/to/sdk`.

`ctest --test-dir build -R '(install_package_smoke|client_bundle_smoke)'`
stages an installation, builds and runs an independent package consumer, then
checks that the handoff archive contains `ckvision_terminal` and its generated
terminal documentation capture.

## Terminal example troubleshooting

`ckvision_terminal` starts an explicit interactive child: `$SHELL` when the
environment names one, `/bin/sh` otherwise. Its demo environment sets
`TERM=xterm-256color` and `COLORTERM=truecolor` and nothing else — `PATH`,
`PS1` and the rest are inherited, so the shell's own startup files decide the
prompt and where tools such as `btop` resolve from. A tool that runs in the
reader's ordinary terminal therefore runs here too, and a prompt that looks
wrong is a shell configuration to look at rather than something the example
imposed. A library client still supplies its child environment explicitly.

If text looks misaligned, use a monospaced terminal font and reset the
terminal's cell metrics. Runtime graphics are enabled only after the terminal
confirms a usable pixel geometry; changing font size or window size withdraws
that evidence until fresh geometry arrives. The Sixel demo remains safe in a
no-graphics terminal: it renders a deterministic text fallback instead of
forwarding child graphics bytes.

Use **File → New Terminal** for another isolated shell, and **Window → Tile**
or **Window → Cascade** to arrange them. Closing a terminal window closes its
private session and process group; closing the whole application performs the
same bounded teardown. If a test fixture remains after an interrupted manual
run, close the application normally rather than killing only the outer
terminal emulator.
