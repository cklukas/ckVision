<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Portable terminal lifecycle — 2026-10-03

Owner-directed adopter support, implementation verified for release 0.1.9. This supplements
the embedded terminal architecture's child ownership and bounded close rules.

The term::TerminalSubsession interface exposes nonblocking
request_termination() and request_kill() for hosts that time their own grace
period while continuing to drain all sessions. POSIX sends its existing
process-group signals; Windows delivers Control-C through private ConPTY,
then TerminateJobObject when escalation is explicitly requested. Neither
request waits for exit or closes transport handles. Requests are idempotent
and harmless after exit, close or launch failure. Native resources remain
owned until close(); drain() observes exit and preserves final output.

The selective snapshot overload belongs on the portable term interface as
well. Every implementation must honor the inclusion flags at construction;
copying a full history and then deleting it defeats the API's purpose.

Acceptance: native stubborn-child cases exercise requests through the base
interface, prove the child remains running during graceful handling, prove
another session still accepts input, and then observe whole-job termination.
POSIX tests continue to prove the same requests through the shared interface.
Selected snapshots must omit excluded history and rasters while
retaining scalars. Launch-failure and repeated/after-close requests are safe.
Strict three-platform builds, lifecycle suites, provenance gates, sanitizers,
full regression and documentation gates precede release.

Candidate-byte gates and the compiled escalation mutation are recorded in
[portable-terminal-lifecycle-validation.md](portable-terminal-lifecycle-validation.md).
Publication verifies the filtered public snapshot independently.

## Separate upstream readiness gap

The Windows outer terminal currently throws when its combined wait set
exceeds `MAXIMUM_WAIT_OBJECTS`. A multi-terminal adopter needs more sources
than this (private terminal sessions each contribute several handles).
Resolve scalable native readiness inside ckVision, not by narrowing an
adopter's terminal limit or periodically ignoring some sessions. This remains
open and is not a capability claimed by the lifecycle release.
