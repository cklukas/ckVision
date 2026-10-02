<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Expanded appearance matrix sanitizer deadline — 2026-10-02

The complete widget implementation adds four-scheme appearance coverage for all
thirteen families. The first final TSan suite passed 216 tests but timed out
appearance_matrix at its 120-second deadline. Running that test alone with the
same deadline also timed out. Neither run produced a race report.

The exact complete check was then measured outside CTest with a 600-second
measurement guard. It passed the complete pinned-golden and coverage checks in 130.731 seconds.
The exit code was 0. Inputs remained frozen throughout the measurement, and
the executable retained its recorded identity. The archive SHA-256 is
`a6697c01b8ba8214f526e18a0cc74159a8a06bc8e43b94e6fc92cdb8aa0883ba`.

D-119 therefore assigns this functional test a 300-second TSan deadline. Other
configurations retain 120 seconds. No benchmark budget, sanitizer, golden or
coverage assertion is weakened. The final reconfigured TSan suite passed
217/217 in 169.81 seconds. The documentation note triggered the lexical
provenance guard in an intermediate run; its wording was corrected and the
complete suite was rerun successfully.
