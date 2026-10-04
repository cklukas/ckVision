<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Native host diagnostics validation

Status: candidate, not released · 2026-10-04

D-077/D-127 supplies concrete POSIX/Windows backend parity, not a second
decoder, global capture service or expanded core Terminal abstraction.
Native tests exercise actual console output, cumulative decoded reports,
record/replay, throwing/disabled sinks, idempotent normal restoration and
two real ConPTY environment changes. Portable exact-text goldens protect
the common capability-summary vocabulary, including overrides and keyboards.

## Preliminary evidence, before the final 0.1.13 preparation

Run by the Windows-port Codex session on isolated ckVision `81198549` plus
its diagnostics delta. Native Windows11 ARM64 MSVC strict Release focused
gate: two CTest entries, six diagnostics and ten readiness cases, exit0,
2.29s. Source/asset manifest hash begins `6ea31680`. A compiled capture/trace
removal changes the native implementation hash, builds successfully, reaches
the named capture/settlement/ConPTY cases and fails intended assertions,
exit1; original bytes are restored and rebuilt. This is library component
evidence, not a user-facing terminal walkthrough or complete adopter port.

Linux GCC13.3 Release and Clang19 Debug ASan/UBSan each pass six focused
entries and the real POSIX resize/settlement case. That selection contains
the POSIX terminal, record/replay, API comments and provenance checks. It
does not contain `layer_check` or the documentation header inventory; a regex
spelling `include_direction` selected neither. Full gates remain mandatory.

## Retained failed instruments and corrections

- The first native fixture expected a legacy console viewport API to change
  the observed terminal size. It did not on the named host. Replace that
  assumption with a real pseudoconsole host resize and assert the child event,
  size and settled trace, then a second resize with tracing disabled.
- The first pseudoconsole fixture omitted its required child exit policy.
  Launch failed before the behavior assertion. State TerminateAfterGrace
  explicitly; preserve the failed run rather than interpreting it as trace
  behavior.
- The initial full native gate passed211/212 but rejected relative library
  includes. Move the shared formatter to the established installed internal
  header convention; keep the architectural checker unchanged.
- The following Mac gate passed220/222 and native gate210/212: both missing
  documentation inventory checks rejected the new internal header's omitted
  API-index entry. Add that entry, not an exemption. The final versioned
  preparation also includes its screenshot and16 report goldens before
  repeating every complete gate.

The final 0.1.13 source/asset freeze includes these corrections and three
portable summary-golden cases. Its required full inventories are Mac223,
Linux Release223, Linux ASan/UBSan plus fuzz233, Linux TSan222 and native
Windows213. These are expected inventories, not success claims. The release,
clean-commit, filtered publication and GitHub CI gates remain open until their
actual logs and exact commits are checked.
