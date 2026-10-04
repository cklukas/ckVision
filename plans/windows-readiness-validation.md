<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Native Windows readiness and quiet input deadlines

Status: full candidate platform gates pass; clean commit and publication pending.

D-126 supplies an instance-owned `WindowsWaitSet` in the terminal layer.
`WindowsTerminal` consumes this API instead of a 64-handle Win32 wait.
Registration lifetime, cancellation, retired generation keys and rearming
belong to the library, not to its adopters. No production wait worker thread,
global registry or periodically polled subset is introduced.

Ten native cases exercise 130 sources whose state changes after registration,
duplicates, empty sets, rearming, hot-source fairness, 200 cancellation races,
invalid and closed handles, independent instances, owned handle cleanup,
process exit after registration, and public WindowsTerminal console input,
wake and extra-source integration. The console case also checks that lone
Escape resolves under an indefinite caller deadline without subsequent input.
The focused native candidate passed all ten cases; this is not a live GUI or
an adopter's complete native application acceptance.

Adopter testing exposed a separate shared input contract gap: lone Escape
was not advertised by `InputDecoder::next_timeout_nanos()`. On POSIX, decoder
quiet deadlines were serviced only when the native wait timed out, so a
continuously readable extra source could indefinitely defer Escape. The
decoder now advertises the earliest quiet deadline, and POSIX services it
after every wait outcome. The Windows adapter already serviced the timeout;
advertising it also prevents an indefinite Windows wait from hiding Escape.

Two deterministic new regressions compile and fail on original bytes:
`lone_esc_advertises_its_deadline_and_clears_it_after_resolution_or_completion`
and `posix_terminal_resolves_escape_while_an_extra_source_remains_readable`.
The latter changes the environment after Escape is read, leaving an extra
pipe readable while an injected clock crosses the exact quiet deadline.
Both pass with the correction. No adopter timeout was relaxed or bypass added.

## Candidate gates

Run by ckmux-windows Codex against source/assets manifest SHA256
`a6b465865f651d08afba7d2fc4c452cf2af6e19869a746ebe379f857a5beab23`:

- macOS Apple Clang strict Release, examples ON: 222/222 CTest entries,
  exit 0, 43.23 s at parallel 2. The first run was 221/222 because the report
  screenshot still named the old version; the real capture tool regenerated
  only that asset. Version 0.1.12 and all sixteen report goldens agree.
- Debian 13 ARM64 GCC 13.3 strict Release: 222/222, exit 0, 23.03 s,
  parallel 2; build 41 s. Benchmark gate exit 0.
- Linux Clang 19 Debug ASan/UBSan and fuzz corpus: 232/232, exit 0,
  126.91 s, parallel 2; build 129 s.
- Linux GCC 13.3 Debug TSan: 221/221, exit 0, 280.29 s, parallel 2;
  build 99 s. Its checked inventory differs from the other configurations.
- Windows 11 25H2 ARM64 MSVC strict Release, examples ON: 211/211,
  CTest exit 0, 50.33 s, parallel 2; benchmark gate exit 0. Build
  01:49:58.363Z–01:55:14.345Z, 315.981 s. Aggregate test executable SHA256
  `5c82c377108470a1f1cf1f9247cd876f758fe1b595e86becd28ce3df373f075f`;
  readiness contract executable SHA256
  `9b4a07074c4b6e6677eb8c8027f0ab547500462081ad8334446690894ab80040`.

The Mac benchmark gate also exits 0. Linux fingerprints match again after
all three full suites. The native driver's baseline text mistakenly labels
a prior 92.89 s CTest duration as a build duration; it is not used as a
build baseline. The comparable earlier broad native rebuild took 395.705 s,
as recorded in the command-launch plan. Focused one-TU mutation builds and
no-work builds need their own timing baselines, not this broad-rebuild figure.

Native MSVC ARM64 strict Release compiled negative tests remove rearming or
drop source sets exceeding 64. Each changed source hash, successful build and
fresh executable are checked before execution; each intended named case
prints failing assertions and exits 1. Restoration checks the original hash
and rebuilds before the full native gate. An earlier 64-limit mutation threw
an uncaught injected exception: its intended case ran and terminated, but the
driver rejected the absence of a printed FAIL. That attempt remains recorded
separately; it is not the completed assertion-based negative gate.

Exact outputs and build records remain in the task-owned mac-ckvision-shell,
native release and Linux ckvision-release/ckvision-asan/ckvision-tsan
configurations. Records include source fingerprints, UTC start/end times,
elapsed durations and executable hashes. No visual acceptance claim follows
from these headless runs. This private evidence note follows the candidate
freeze; the final clean committed gate must also verify its bytes.
