# ckVision — framework SDK and example applications

These packages contain ckVision's C++20 framework library, public headers,
exported CMake package, example sources and all example executables supported
on the target platform. The ckVision Gallery is the Start-menu entry on Windows.

Use Homebrew on macOS (`brew install ckvision/packages/ckvision` after adding the
release's verified formula), `sudo apt-get install ./ckvision_*.deb` on Debian,
or the native architecture's per-user MSI on Windows. Windows ZIP archives
contain the same complete payload. MSI adds bin to user PATH and removes that
entry on uninstall.

For your own project:

```cmake
find_package(ckvision CONFIG REQUIRED)
target_link_libraries(your_program PRIVATE ckvision::cvision)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is not searched
by CMake. Windows packages use the static MSVC runtime; configure consumers
with `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` and build Release.
See `share/ckvision/examples/log_clients` for complete consumer examples.

Run `ckvision_gallery --version` to confirm the installed release without
opening a terminal session.

The TODO and system-monitor applications that earlier releases shipped as
examples are now part of CK Utilities, as `ck-todo` and `ck-sysmon`.
