# Native Windows filesystem — D-123

Implement the existing injected `FileSystem` contract in `term`, not in an
application. UTF-8 names convert strictly to Win32 wide paths; embedded NUL,
invalid UTF-8, device namespaces, alternate data streams and wildcard paths
are rejected, as are control characters, reserved device basenames (including
superscript COM/LPT digits), and trailing spaces/periods that the Windows shell
cannot address reliably. Drive/UNC roots survive normalization, joining and parent
navigation. Relative paths resolve at this native boundary against the current
directory; extended drive/UNC spelling supports paths beyond MAX_PATH.

Queries and reads follow filesystem links. A read fingerprints its opened file
handle before and after the byte read and excludes ordinary concurrent write
handles with native sharing rules. A replaced path cannot pair one file's
bytes with another file's revision. Fingerprints include volume/file identity,
size, last-write and change times, plus a streamed SHA-256 digest through the
Windows system crypto service. Timestamp equality alone cannot prove unchanged
bytes: two fast same-size native writes can occupy the same timestamp tick.
Fingerprint-only queries stream bytes without retaining the file in memory.
Reads hash the same chunks they return, between opened-handle metadata checks.
The token is opaque revision evidence, not a portable content-addressing API.

Atomic writes serialize cooperating instances/processes using a persistent
`.ckvision-write.lock` in the target directory and `LockFileEx`. That basename
is reserved. Check Any/create-only/matching expectations while holding the
lock; create a random exclusive temporary sibling, preserve an existing file's
DACL, write/flush complete bytes, and publish with native rename-by-handle.
No remove-then-rename or copy-over-target fallback is permitted. Close the write
handle before reporting the final target fingerprint (Win32 write timestamps
are only guaranteed after close). Failed unpublished temporaries are marked
for deletion through their owned handle. Success leaves only the target and
the persistent directory lock, never a pile of temporary siblings.

Writes do not follow a final-component reparse point, overwrite a directory,
or override native read-only/sharing/access restrictions. Other metadata and
alternate streams are not part of this whole-byte-file service. The new file
has its creator's owner and the preserved DACL, or inherited directory DACL
for creation. No elevated privilege is required or silently enabled.

The lock is cooperative, like the POSIX adapter: noncooperating path replacement
and mapped-memory writers are outside its conditional-write serialization
guarantee. No retry thread, hidden global state, environment lookup or clock
sampling is added. Native directory/file impurities remain injected in term.

## Required evidence

- strict native MSVC build; Unicode names/content, empty/binary contents,
  directory listing, drive/UNC parent navigation, invalid inputs and long paths;
- public FileSystem expectations and changed same-size/native-write revision;
- sharing denial preserves old content, then succeeds after the holder closes;
- native read-only denial preserves old content/attributes and recovers after
  attributes change; hard links share identity until replacing one name;
- concurrent cooperating writers yield one matching/create-only winner;
- readers under replacement observe only a complete old or new byte sequence,
  with matching opened-file revisions; preserved DACL and temporary cleanup;
- editor/file-dialog public workflows and POSIX/portable/sanitizer regression
  gates; documentation, package exposure and release consumption.

## Candidate validation — 2026-10-03

Run by ckmux-windows Codex on isolated `987e0b68` plus the owned filesystem
and explicit-test-scratch deltas; no unrelated progress-widget changes included.

- Windows 11 25H2 ARM64, MSVC 2022 strict Release: 209/209 CTest suites,
  exit 0, 117.90 s at parallel 2. Native filesystem suite contains 16 cases.
  Matching-revision, DACL-preservation and content-digest mutations each
  compiled successfully and failed their intended assertions. Original source
  SHA256 was restored and verified before the successful focus/full gates.
  The final full incremental build ran 18:09:11.213–18:09:34.414 UTC, 23.192 s,
  at parallel 4; changed test objects and library-dependent links were rebuilt,
  not an 880.171 s / 371-object clean Windows build. Output:
  `C:/Users/klukas/ckmux-windows-20261003-01/release`.
  System-backend source SHA256:
  `48b6682eaaa84dbc3c3d309d54b7eca62de106ea2e8c8232c98cd7352033f664`.
  Native input-manifest SHA256:
  `68ec57b477b6d3f5b528e32bf2957f648f89f5975bcf6d0150a293901dca857a`.
- Apple Clang strict Release, examples enabled: 221/221, exit 0, 44.92 s
  at parallel 2. Clean build 18:01:24–18:02:54 UTC, 90 s, 485 Ninja actions;
  the preceding project's documented clean baseline was 484 actions/102.718 s.
  Output `/Volumes/PRO-BLADE/tmp/ckmux-windows-20261003-01/mac-ckvision-filesystem`.
  Subsequent Windows-only digest changes did not alter the POSIX compile graph
  or test executable (`f1464fea44378b7203e1457c7d00b39823300a9f7afd100f88f85b6c9e0ff980`).
  The updated documentation/provenance/license/inventory/scratch/conformance
  selection passed 12/12, 21.80 s. Benchmark `--gate` passes.
- Debian 13 ARM64 GCC 13.3 strict Release: 221/221, exit 0, 28.31 s;
  Clang 19 Debug ASan/UBSan plus fuzz: 231/231, exit 0, 152.34 s;
  Clang 19 Debug TSan: 220/220, exit 0, 309.40 s. Parallel 2 throughout.
  Outputs under `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-{release,asan,tsan}`.
  These portable/POSIX gates precede the Windows-only digest addition; no native
  Windows sanitizer or live application/display verdict is claimed.

The strengthened path case exposed Win32 canonicalization hiding trailing
spaces/periods; raw components now validate before resolution. A restored run
also exposed identical timestamps after a same-size write. The fixed regression
suppresses native write/change-time updates explicitly, checks unchanged times,
and proves content-based revision/conflict detection without sleeps.

The first gate driver used a nonexistent `--smoke` benchmark switch after all
tests passed. Correct `--gate` invocations independently passed on both native
Windows and Mac; that failed driver exit is not a successful aggregate verdict.
Evidence above is candidate validation, not a future release, SMB network-share
test, native Windows x64 sanitizer result or ckmux application acceptance.
