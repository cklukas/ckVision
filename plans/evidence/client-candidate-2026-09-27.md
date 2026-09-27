<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Mac 0.1.8 client candidate — 2026-09-27

This is a technical candidate, not the owner's release approval. Its selected
source revision is `b4783d09679ba3cc311b2a7b76d88167e264b712` on `main`.
The tracked tree had no changes when packaging began. The unrelated untracked
`.claude/` directory was not an input to the build or the handoff script.

## Build and gates

- Host: the pinned Mac13,1, macOS 27.0 reference host described in
  [the p99 record](p99-baseline-2026-09-26.md).
- Output: `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build`;
  Release, AppleClang 21, examples and tests enabled, warnings as errors.
- The full build containing the current code completed before the candidate
  run; the `ckvision_hello` executable was written at `18:34:32Z`. The later
  commits `d99e65e` and `b4783d0` changed documentation only. A build refresh
  from `19:01:15Z` to `19:01:15Z` reported `ninja: no work to do`; that short
  duration was investigated through the source diff and unchanged build graph,
  and is not presented as a fresh compile.
- Full current-tree Mac CTest: `19:01:26Z`–`19:01:42Z`, **218/218** passed,
  including `provenance_check`, `provenance_checker_self_test`, install and
  bundle smoke tests, and `theme_switch_work_gate`. The current build log has
  no compiler warnings. The earlier full-build timing and source fingerprint
  are in [the theme harness record](theme-switch-harness-2026-09-27.md).
- Relevant current source and asset fingerprints: `CMakeLists.txt`
  `e3bc0e9d5608e27ced484124c092e63d9221bd364491b4d883b7831ec12d25f0`;
  `docs/client-handoff.md`
  `14187002f72ac1bb141924d7c17fd7e7abc63116b82a82efd3be7e9451bba06b`;
  `ckvision_hello` executable
  `fcc35ee41518479811dd73384d1b39ba9cd94959ae8d830f7c3a0f2b87d01cb1`.

## Package and identity

`tools/package_client_bundle.sh` ran from `19:02:09Z` to `19:02:14Z` with
`TMPDIR` set to the unique task directory on `/Volumes/PRO-BLADE/tmp`.
No comparable package-duration baseline is documented; the five-second run is
supported by its successful install, capture-generation log and archive checks.

| Artifact | Exact path or value |
|---|---|
| Bundle | `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/ckvision-client-0.1.8-b4783d0` |
| Archive | `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/ckvision-client-0.1.8-b4783d0.tar.gz` |
| Archive timestamp | `2026-09-27T19:02:14Z` |
| Archive size | 25,880,876 bytes |
| Archive SHA-256 | `3ba0ae0483a2366e48eb4bb09f8317a6b84899f0942cfcedbf11ac1d508dfcba` |

`gzip -t` and a full `tar -tzf` listing succeeded. The archive contains the
installed `ckvision_hello`, `ckvision_echo`, `ckvision_gallery`,
`ckvision_rootdialog`, the terminal example, source documentation, and 46 fresh
SVG captures. The installed `ckvisionConfigVersion.cmake` declares `0.1.8` and
`ckvisionTargets.cmake` defines `ckvision::cvision`. The installed Hello binary
has the same SHA-256 as the verified build executable, and the copied handoff
guide has the same SHA-256 as the committed source document.

The archive is retained for the owner's interactive acceptance and release
decision. That review and sign-off remain open.
