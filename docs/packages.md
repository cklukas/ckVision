---
description: Install ckVision's framework SDK and example programs from DEB, RPM, TGZ, Windows MSI/ZIP or Homebrew packages.
---

# Install packages

The `packages` GitHub workflow builds Linux x86_64 and ARM64 DEB/RPM/TGZ,
macOS Apple Silicon and Intel TGZ plus a verified Homebrew formula, and Windows
x64 and ARM64 ZIP/MSI. Successful main builds retain CI artifacts; matching
version tags publish them as release assets. The self-hosted primary ARM64
lane builds Linux DEB/RPM/TGZ offline and retains the same checksums and tests.
ckVision packages contain the framework library, public headers, CMake exports,
example sources and all example programs supported on the target platform.

macOS distributions require macOS26 or newer. Its standard library provides the
floating-point `std::from_chars` operations required by the framework. Both Mac
package architectures are built and tested against that deployment baseline.

Verify the adjacent `.sha256` file before installation. For Debian/Raspberry Pi,
download the native-architecture DEB and run `sudo apt-get install ./ckvision_*.deb`.
The packages built on Ubuntu24.04 support its runtime baseline; the self-hosted
Debian13 ARM64 package targets Debian13, including the tested Raspberry Pi and
uConsole. Install a Windows MSI for the machine's architecture by opening it,
or use `msiexec /i PACKAGE.msi`. It installs for the current user, registers a
reversible user PATH entry and needs no separate Visual C++ runtime. ZIPs are
portable alternatives; add their `bin` directory to PATH yourself.
For development, use `find_package(ckvision CONFIG REQUIRED)` and link
`ckvision::cvision`. Set `CMAKE_PREFIX_PATH` to the installation prefix when
needed. Windows SDK packages use the static MSVC runtime: configure consumer
Release builds with `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`.

On macOS, download the generated `ckvision.rb` formula from the release or
verified CI artifact. Register a local package tap once, then install it:

```sh
brew tap-new --no-git ckvision/packages
cp ckvision.rb "$(brew --repository ckvision/packages)/Formula/ckvision.rb"
brew install ckvision/packages/ckvision
brew test ckvision/packages/ckvision
```

On Intel Macs, Homebrew may have no prebuilt CMake package. Install that build
dependency from source with `brew install --build-from-source cmake` before
installing ckVision. The Intel CI lane verifies this installation path.

The formula pins an immutable source archive and its SHA256 checksum, builds
all supported example programs and installs the complete framework SDK,
public headers, CMake exports and example sources. Package removal leaves
user files intact. `ckvision_gallery --version` verifies the installed release.
The build respects Homebrew's job limit; set `HOMEBREW_MAKE_JOBS` when a smaller
build should leave resources available for other work.

The TODO and system-monitor applications that earlier releases installed as
examples (`ckvision_todo`, `ck-sysmon`) are now part of [CK Utilities](https://github.com/cklukas/ckUtilities), as `ck-todo`
and `ck-sysmon`.
