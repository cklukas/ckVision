<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Mac 0.1.8 client candidate `7ecd1d9` — 2026-09-27

This is a technical candidate, not the owner's release approval. Its selected
source revision is `7ecd1d9d221a26e7a81bf334978e3ef92f85e81d` on `main`.
The tracked tree had no changes when packaging began. The unrelated untracked
`.claude/` directory was not an input to the build or handoff script.

## Build and gates

- Host: the pinned Mac13,1, macOS 27.0 reference host described in
  [the p99 record](p99-baseline-2026-09-26.md).
- Output: `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build`;
  Release, AppleClang 21, examples and tests enabled, warnings as errors.
- The current-source Root Dialog entry point had SHA-256
  `0d69def897ef806b26d97c2fa95dddd3d2ffac2df425f2c2076d6eba4b86b031`
  and `examples/CMakeLists.txt` had
  `2c237da86f6811dc7f56c0189ff1e6e1d73a7b90123ba9dca7b64e777ae325b8`.
  Both files were written at `20:10:47Z`; the Mac `ckvision_rootdialog`
  executable was built afterward at `20:11:27Z`, with SHA-256
  `7b51d67f1683ab001fa66278efa7dd11b2b8145f955d8ed760a8e40412d735a5`.
  The unchanged Hello executable retained SHA-256
  `fcc35ee41518479811dd73384d1b39ba9cd94959ae8d830f7c3a0f2b87d01cb1`.
- The current-source Mac Release suite passed **218/218**, including
  `provenance_check`, `provenance_checker_self_test`, install and bundle smoke
  tests, and `theme_switch_work_gate`. The Root Dialog change also passed
  **218/218** on Linux ARM64 and **204/204** on Windows ARM64. Those three
  builds had zero warnings; the [Windows record](windows-rootdialog-2026-09-27.md)
  has clean-build timing and the live form run. After the plan edits,
  `doc_links` and both provenance gates passed again on Mac.
- Other relevant unchanged fingerprints: `CMakeLists.txt`
  `e3bc0e9d5608e27ced484124c092e63d9221bd364491b4d883b7831ec12d25f0`;
  `docs/client-handoff.md`
  `14187002f72ac1bb141924d7c17fd7e7abc63116b82a82efd3be7e9451bba06b`.

## Package and identity

`tools/package_client_bundle.sh` ran from `20:48:05Z` to `20:48:09Z`, with
`TMPDIR` set to the unique package directory on `/Volumes/PRO-BLADE/tmp`.
The previous comparable package took five seconds; this run took about four.
The script installed the SDK and examples, regenerated captures, copied source
documentation and created the archive.

| Artifact | Exact path or value |
|---|---|
| Bundle | `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/ckvision-client-0.1.8-7ecd1d9` |
| Archive | `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/ckvision-client-0.1.8-7ecd1d9.tar.gz` |
| Archive timestamp | `2026-09-27T20:48:09Z` |
| Archive size | 25,881,287 bytes |
| Archive SHA-256 | `6d958afeda9a3da9e2665f0f4a5f6efea05347ed1093c5f86616a08d9554acfd` |

`gzip -t` and a complete `tar -tzf` listing of 979 entries succeeded. The
archive contains installed Hello, Echo, Gallery, Root Dialog and terminal
examples, source documentation, and 46 fresh SVG captures. The installed
`ckvisionConfigVersion.cmake` declares `0.1.8` and `ckvisionTargets.cmake`
defines `ckvision::cvision`. The installed Root Dialog and Hello binaries
have the same SHA-256 as their verified build executables, and the copied
handoff guide has the same SHA-256 as its committed source.

The earlier `b4783d0` candidate, SHA-256
`3ba0ae0483a2366e48eb4bb09f8317a6b84899f0942cfcedbf11ac1d508dfcba`,
is superseded and its bundle/archive were removed after this candidate was
verified. The current archive is retained for the owner's interactive
acceptance and release decision. That review and sign-off remain open.
