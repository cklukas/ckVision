# Native process-resource observation validation

Checkpoint: version 0.1.15 isolated candidate; not landed or released · 2026-10-04

The candidate is based on private commit `931cdff8`. No statement here
attributes these new resource APIs to the published 0.1.14 release.

## Versioned candidate gates

All five configurations now use the same frozen source/asset manifest:
`5ec3410e75479bcc4e51174353ae1e73a42eb7e2eec2dd10d931432cac629eaf`.
Version declarations, literal tests, all sixteen report goldens and the
actually regenerated one-glyph report SVG are included.

| Host/configuration, examples ON | Result |
|---|---|
| macOS Apple Clang strict Release | 226/226, CTest exit 0, 42.99 s; benchmark exit 0 |
| Debian 13 ARM64 GCC 13.3 Release/FORTIFY | 226/226, CTest exit 0, 56.96 s; benchmark exit 0 |
| Windows 11 25H2 ARM64 MSVC strict Release | 218/218, CTest exit 0, 72.18 s; benchmark exit 0 |
| macOS LLVM 23 Debug ASan/UBSan plus ten fuzz corpus entries | 236/236, CTest exit 0, 198.17 s |
| macOS LLVM 23 Debug TSan | 225/225, CTest exit 0, 360.91 s |

All compile logs have zero warnings. Source and asset hashes are checked
before and after every lane. Windows's version-definition rebuild takes
126.000 s (12:57:20.158–12:59:26.162 UTC), versus the same output's previous
152.356 s version rebuild. Linux's comparable 166-action version rebuild
takes 316 s, versus 340 s previously. Mac ASan/UBSan performs 398 actions
in 85 s and TSan 386 actions in 70 s, broader than their previous version-only
176/166-action rebuilds. Mac Release's final relink is 66 actions/2 s after
its version-report generator has already rebuilt 100 actions/22 s; the short
final elapsed time is not represented as a clean build.

Final validation-document bytes postdate that manifest. Clean committed-source
and filtered public-snapshot gates are still required before release. None of
these upstream gates proves downstream application statistics acceptance.

## Earlier normal gates

| Host/configuration | Frozen input-manifest SHA256 | Result |
|---|---|---|
| macOS Apple Clang strict Release, examples ON | `cbfcb685ea6d8c43ded670c1c71741e13697c0dfe7fbece57782e8464d4c88f1` | 226/226, CTest exit 0, 40.55 s |
| Debian 13 ARM64 GCC 13.3 Release/FORTIFY, examples ON | `9ccad0439eaa9c8788a603377763cf342cfe1d02b3e6fe213c8feffa75fca887` | 226/226, CTest exit 0, 59.96 s; benchmark exit 0 |
| Windows 11 25H2 ARM64 MSVC strict Release, examples ON | `35b2b05d70f1c0c1466db1310dee99a652e83c01273b61b0e7ae168868f80363` | 218/218, CTest exit 0, 61.66 s; benchmark exit 0 |

These are different frozen candidates: later native-only fixture changes
are not claimed compiled by the older Unix gates. All share the same new
portable process identity/default-resource contract. The same-input versioned
gates above supersede these as candidate evidence, while preserving their scope.

Mac clean all-target compilation performed 492 actions in 126 s, output
`mac-native-stats.CiLCeC`; no comparable clean-duration baseline was claimed.
The API-index repair then required no Unix compilation. Linux's shared-header
rebuild performed 492 actions in 1715 s (12:08:53–12:37:28 UTC), not a
regression against its narrower prior 166-action/340 s version rebuild.
Native shared-header compilation took 563.353 s; the final restored full
relink took 8.796 s (12:33:40.858–12:33:49.666 UTC). Strict warning gates
remain enabled. Compact records retain actual output paths and binary hashes.

## Executable native contracts

Six dedicated native cases cover resident versus merely committed memory,
invalid full-width identities/null-job refusal, reduced job query rights,
nested owned jobs, exiting CPU workers, growing membership beyond the initial
16-member buffer, and denied member memory with unavailable totals. The
ConPTY suite queries the same public core seam on a real root/descendant,
then verifies empty-job memory and no PID after escalation, and Gone after
close. Neither special host processes nor unreadable members are filtered out
to manufacture a successful total.

Two actual compiled native mutations are rejected: replacing lifetime job CPU
by the sum of current live CPU fails the exiting-worker monotonic assertions;
substituting PrivateUsage's virtual commit charge for resident memory fails
the untouched-commit reservation assertion. Both builds exit 0, both CTest
runs exit 8 with the intended failures, and both restored targeted gates exit
0. Source hashes differ under mutation and match exactly after restoration.
The subsequent all-target rebuild and 218-entry sweep pass on restored bytes.

## Failed instruments and corrected fixtures

- The first full Mac gate was 224/226: both failures required the new installed
  headers in the API index. Adding those entries passes the full repeat; no
  documentation assertion was weakened.
- The first Mac compilation was dispatched while its manifest generator was
  still writing. Its initial fingerprint was partial. The complete 4760-entry
  manifest was checked before/after the repeat; the original partial hash is
  retained, never represented as the full-input fingerprint.
- CREATE_NO_WINDOW workers acquired native conhost processes inside the owned
  jobs. PID/image diagnostics proved those additional members, which the
  sampler correctly counted. Non-terminal fixtures now request DETACHED_PROCESS;
  exact member-count assertions remain unchanged.
- A member DACL alone did not deny reads on the named host. The disposable
  contract process's scoped debugging-privilege fixture makes the denial
  effective, independently checks OpenProcess refusal, and restores its exact
  previous token state. The library never changes a privilege. All partial
  counter/absence assertions remain.
- The first mutation driver compiled but stopped before testing because its
  Select-String arguments selected a nonexistent source-basename path. Its
  finally block restored source, not compiled output. The corrected driver
  first rebuilds and tests the restored baseline, then performs the mutations
  and the final full gate. The first attempt is not negative-test evidence.

No live user-facing application, downstream frame/wire statistics integration,
or published package acceptance follows from these library/fixture gates.
