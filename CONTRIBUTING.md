# Contributing to ckVision

ckVision is licensed under the [MIT License](LICENSE). Contributions are
welcome, under the terms below — they exist so that what you may do with
this library, and what this library may do with your contribution, is
written down rather than assumed.

## Contribution terms

By submitting a contribution to this repository you agree that:

1. **License.** Your contribution is licensed under the MIT License, the
   same license this project is distributed under. This writes down the
   "inbound = outbound" custom instead of relying on it.
2. **Patent grant.** You grant the project, its maintainer, and every
   recipient of this software a perpetual, worldwide, non-exclusive,
   royalty-free, irrevocable patent license to make, use, sell, offer for
   sale, import, and otherwise transfer your contribution alone and in
   combination with this project, covering any patent claims you control
   that are necessarily infringed by your contribution.
3. **Relicensing.** You grant the project owner (Dr. Christian Klukas) the
   right to relicense your contribution, alone or as part of this project,
   under other license terms, including commercial terms. The public MIT
   license of anything already released is never revoked by this — it
   exists so the library can also be offered under additional terms without
   tracking down every past contributor.

If you cannot agree to these terms, do not submit the contribution.

## Developer Certificate of Origin

Every commit must carry a `Signed-off-by:` line with your real name and a
working email address (`git commit -s`). The sign-off certifies the
[Developer Certificate of Origin 1.1](https://developercertificate.org/):
that you wrote the contribution, or otherwise have the right to submit it
under this project's license. Commits without a sign-off are not merged.

## Provenance (binding — read before writing a line)

ckVision derives terminal behavior from published standards and documented
black-box observation only. Contributions are held to the same rule: never
port or transcribe code from other terminal emulators, multiplexers, or TUI
frameworks (Turbo Vision and its ports included). Standards documents,
protocol specifications, and other projects' *manuals* are fine; their
source is not.

## The practical bar

Every change lands with a test that fails without it. The build is
zero-warning and green under ASan/UBSan with examples ON. Documentation is
updated in the same commit when behavior changes, and a change to a public
API says so. Commit messages carry real verification evidence — what you ran
and what it showed — not "all tests pass".

## Test selection

The header-only runner in `include/cvision/testing/cktest.hpp` supports
`--filter` (case-name substring), `--suite` (source basename), `--case`
(exact case name), and `--shard index/count`. `CKTEST_FILTER` supplies the
default substring filter; an explicit `--filter` replaces it. An empty or
absent environment default selects all cases. The runner owns its default
for the entire run, so a test changing the environment cannot alter later
selection. A selection matching no cases returns 2 rather than a false green.

Native MSVC adopters can include this runner with `/W4 /WX` without defining
`_CRT_SECURE_NO_WARNINGS`. The Windows CRT environment copy is freed by scoped
ownership; a copy failure is diagnosed and returns 2 before any case runs.
`CK_CHECK` accepts literal and constexpr conditions without MSVC constant-if
warnings. It evaluates the condition once, preserves contextual negation,
and reports a false value at the caller's original expression/file/line.
