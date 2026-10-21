# ckVision monitor, native examples and complete distribution — 2026-10-21

The standalone monitor has strict protocol2 accounting for Apple unified RAM
and V3D buffer objects. Both Pi hosts expose readable V3D engine counters and
buffer statistics; the installed helpers use C++ and preserve cumulative counters
for UI interval calculation. Busiest-engine activity is never a sum; buffers have
no invented VRAM capacity. Apple data is joined by exact Metal registryID and
read from optional validated IOKit snapshots. Driver figures remain explicit.

All nineteen examples compile on Windows. Native Windows contracts verify the
SysInfo probe, Unicode arguments, all eighteen interactive starts/draw/quit
routes, and explicit observed/effective terminal capability separation. Root
Dialog cancellation returns1 by design. The existing Terminal demo's plain-VT
contract exits through its parent File menu, as focused keys belong to its shell.

Complete packages contain all binaries, public headers, the framework library,
CMake exports and example sources. Actual extracted packages and installed SDKs
are exercised by an external CMake HeadlessTerminal/Application consumer. Windows
MSI payload bytes match ZIP; a custom path survives installer costing and all
package files/PATH are removed on uninstall. The final user install is separate.
Both Pi hosts have verified DEB installs; workstation has a verified per-user MSI.
Homebrew installation and CI release publication follow the filtered snapshot.

Validation:263 macOS,262 Linux ARM64 and256 Windows x64 Release CTests;263 complete
ASan/UBSan CTests, then7 targeted checks after host ports;5 TSan checks after the
ports;15 fuzz corpora and10,000 monitor inputs. Both provenance gates pass.
Strict documentation builds35 pages with286 assets. Screenshots use the actual
Presenter and independently decoded HeadlessTerminal, matched to committed goldens.
They do not claim physical native-window visibility. Full CI matrix outcomes and
published installation artifacts are recorded after they complete.

The [receipt](sysmon-distribution-2026-10-21.json) retains source/asset fingerprints,
exact outputs, timing/configuration and hashes, plus investigated stale builds,
unsupported AppleClang fuzzing, an overloaded test retry and resolved package
issues. Earlier receipts retain previous baselines; no duration is invented.
