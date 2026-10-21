# ckVision — framework SDK and example applications

These packages contain ckVision's C++20 framework library, public headers,
exported CMake package, example sources and all example executables supported
on the target platform. `ck-sysmon` is the desktop system monitor example.

Use Homebrew on macOS (`brew install ckvision/packages/ckvision` after adding the
release's verified formula), `sudo apt-get install ./ckvision_*.deb` on Debian,
or the native architecture's per-user MSI on Windows. Windows ZIP archives
contain the same complete payload. MSI adds bin to user PATH and removes that
entry on uninstall. Settings remain in the user's configuration directory.

For your own project:

```cmake
find_package(ckvision CONFIG REQUIRED)
target_link_libraries(your_program PRIVATE ckvision::cvision)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is not searched
by CMake. Windows packages use the static MSVC runtime; configure consumers
with `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` and build Release.
See `share/ckvision/examples/log_clients` for complete consumer examples.

Run `ck-sysmon --version` or `ck-sysmon --sample` for noninteractive diagnostics.
The monitor reads local CPU/RAM and optional native GPU driver data. It discovers
SSH aliases and uses passwordless, strict-host-key SSH without changing login
configuration. An installed remote C++ helper is preferred; fixed system-tool
scripts are a fallback. Ollama is queried only on the target host's loopback.

V3D devices report the busiest GPU engine and GPU buffer-object sizes when
sysfs/debugfs counters are readable. Buffers share system RAM and have no
separate VRAM capacity. The app never escalates privileges or changes access.
Apple unified-memory usage is explicitly driver-reported shared RAM. Missing
metrics remain unavailable. View controls hosts and themes; status commands
toggle metrics. Choices are saved automatically.
