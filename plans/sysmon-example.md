# ck-sysmon — desktop system monitor

The separate `ck-sysmon` example complements the existing SysInfo benchmark
application. It draws directly through Desktop::set_background_painter, with
File / Exit, checkable View host items, Help / About, and clickable status
commands for CPU, RAM, GPU utilization, GPU memory, and history. The local machine starts selected and can be unchecked like a remote host.
An empty selection displays guidance to select a host in View. Each reachable passwordless configured SSH alias is selected
on its first successful probe; a reader's subsequent choice is preserved.
Unavailable aliases remain visible, unchecked and disabled, with a reason.

All implementation is example-owned; there is no public library change or new
mandatory dependency. An injected monitor source isolates host discovery and
sampling from deterministic model, layout and painting. Native C++ collectors
use Mach/sysctl on macOS, procfs/sysfs on Linux, and Win32/DXGI/PDH on Windows.
Optional NVML is dynamically loaded, never required. Unsupported measurements
remain absent, never plausible zeroes. CPU is an interval utilization; RAM is
total minus available (macOS free plus inactive pages (free already includes speculative pages)). GPU
memory means capacity occupied, distinct from memory-controller utilization.

SSH aliases come from concrete Host entries and positive literal Match host /
originalhost criteria in user/system configuration and Include files; patterns are not invented into hostnames. OpenSSH remains the
configuration authority for connections (User, Port, IdentityFile, ProxyJump,
Match etc.). All connections are noninteractive, bounded, with host-key checking
preserved, no forwarding and no remote helper installation. Prefer an installed
`ck-sysmon --sample` C++ collector. If absent, query standard host tools in a
single read-only remote command. Failed queries invalidate old measurements.
The remote collector format is bounded, versioned text, parsed strictly.

A worker owns probes and subprocesses; UI timers request work without waiting.
Results are delivered with Application::post behind a weak lifetime token.
Shutdown cancels queued work, joins workers and withdraws UI callbacks. Local
sampling and remote requests run independently; one slow host cannot delay local
refresh. Histories retain a bounded number of samples and gaps for missing data.

Horizontal percentage bars compare named resources on one fixed 0–100% scale;
vertical block sparklines show recent time, oldest to newest. Responsive cards
use columns only when they have readable width. A paged grid keeps all selected
hosts accessible on small terminals. GPU rows keep stable device identities and
have explicit overflow paging rather than silently dropping devices. No raster
or Sixel rendering is used. Tests cover discovery/parsing, failure and recovery,
selection, toggles, pagination, resize, missing data, lifecycle and deterministic
frames. Native samples and real interactive terminal inspection supplement
headless evidence; they are not interchangeable.

Provenance: ckVision Desktop, command registry, menu/status and message-box APIs;
OpenSSH ssh_config(5)/ssh(1); Linux procfs and DRM sysfs documentation; Apple Mach,
sysctl and Metal public APIs; Microsoft GetSystemTimes, GlobalMemoryStatusEx,
DXGI and PDH documentation; NVIDIA NVML/nvidia-smi reference. The owner's Windows
GPU monitoring script is a documented observation of NVML and PDH usage, not a
third-party implementation. No prohibited implementation source is consulted.

Additional requested surfaces: the menu-bar trailing ClockView opens the existing
CalendarDropdown; its local date/time providers are injected by the host. Ollama
is queried only through loopback `/api/ps` on the target, without launching it.
Loaded model names and size_vram are optional content, bounded and paginated with
the resource rows. HTTP/API failures are distinguished from an empty model list.

Windows linked-display adapter observation: the owner’s two physical GPUs share
one DXGI LUID and have separate `_phys_0` and `_phys_1` PDH instances. Physical
PnP keys, resolved through Configuration Manager PCI bus/address properties,
join each physical index to NVML without assuming any enumeration order. Hex
casing in PDH names is normalized before matching.

The monitor starts in Standard (bright) mode. View / Theme offers Standard and
Dark as mutually exclusive commands. Built-in themes recolor the entire shell,
calendar and dialogs; matching chart palettes recolor the desktop, frames,
headings, histories and load bars. Standard uses Classic chrome with a light
chart surface and darker semantic accents. Theme changes preserve selections,
metric toggles, histories and paging. Reviewed goldens cover both modes.

An injected settings binding restores theme, metric visibility and desired host
selections before sampling. Each option command saves immediately through a
host-owned per-user settings store. Reachability updates never overwrite desired
selections. The versioned settings codec is deterministic and strictly validated;
filesystem I/O uses the existing injected FileSystem and atomic conditional
writes. Missing settings use defaults; invalid/unreadable settings and save
failures have an on-desktop diagnostic. No environment access enters the app.
Only terminal hosts resolve the platform's user configuration path.

There is one footer: the clickable status line. No separate hint row is drawn
above it. Cards use the full height between menu and footer, without top, bottom or
inter-card blank rows. The application name, count and host page sit before the
clock in a shrinking menu accessory; the clock retains its keyboard walk and
calendar anchor. Ollama uses one status row plus one row per model, keeping the
allocation on the model row. Card detail pagination stays within the card.

Apple unified GPU telemetry uses optional PerformanceStatistics properties read
through public IOKit registry APIs. Metal registryID selects the exact accelerator
entry. On the observed M1 Max, Device Utilization % is a direct 0–100 sample,
and In use system memory is driver-reported whole-accelerator memory in bytes.
For unified-memory devices the chart explicitly labels that accounting as
unified RAM, using physical RAM as its shared pool denominator. It overlaps
the RAM chart and is not dedicated VRAM or an additional quantity to sum.
Alloc system memory is not a capacity and is not used. Driver keys are optional,
not an Apple stability promise; validate types, finiteness, sign and ranges.
No private IOReport entry points or process-local Metal allocation counters are
used. Missing keys stay unavailable. Collector protocol2 includes an explicit
GPU memory-kind field so remote rendering retains the same meaning.

Distribution: prepare the monitor as the 1.3.0 release addition. The complete ckVision packages contain the framework library, headers, exported
CMake package, developer client samples and every platform-supported example,
including ck-sysmon. GitHub builds
native Linux x86_64/ARM64 DEB/RPM/TGZ, macOS ARM64/Intel TGZ and verified Homebrew
formulas, and Windows x64/ARM64 ZIP/MSI. Windows packages use the static CRT and
a per-user MSI with reversible user PATH integration. The self-hosted primary
Linux ARM64 runner builds the formats available on that host, offline. Both CI
channels retain checksums and verify extracted packages by executing version
and native sample commands. Installed bytes must match the package payload.
Documentation screenshots use the real Presenter and decoded HeadlessTerminal
state, matched to committed goldens and gated through the screenshot manifest.
GitHub publication uses the existing filtered-snapshot process; development
history and planning documents remain private. Install with Homebrew on macOS,
apt-get on Debian ARM64, and MSI on Windows after package verification.

V3D telemetry uses discovered DRM driver identity. GPU_ENGINE protocol2 rows
carry device ID, one of bin/render/tfu/csd, timestamp ns and runtime ns. Rates
are calculated between successive accepted samples, keeping the native helper
stateless. The bar is labeled busiest engine. Buffer-object KiB accounting is
shown as GPU buffers (V3D), without capacity or percentage. Strict parsers reject
negative/overflowed/duplicate data and reset intervals. Offline or absent devices
break counter continuity. Tests cover snapshots, wire validation, interval
arithmetic, identity changes and a decoded-terminal screenshot/golden.

The owner requires all example binaries on Windows before release. Port the
remaining thin native entry points and implement WindowsSystemProbe. Packages
are monolithic complete SDK/example distributions; every platform must contain
all nineteen binaries. Preserve Spin's explicit capability controls by bringing
WindowsTerminal's override boundary to parity with POSIX/headless backends.

Native example diagnostics own their trace/capture resources longer than both
Application and terminal. Construct clock, diagnostics, terminal, then the app;
restoration is captured before diagnostics closes its files. The Windows native
contract checks restoration bytes for all18 interactive executables.
