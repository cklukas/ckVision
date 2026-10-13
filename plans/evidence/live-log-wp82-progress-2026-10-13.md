<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# WP-82 native-source evidence — 2026-10-13

Implementation is complete; WP-82 remains gate pending. Native Windows acceptance and
final gates remain pending under the owner's load-deferral instruction.
D-144 and the live-log architecture derive behavior from installed Apple help,
RFC 8259, systemd documentation and Microsoft Event Log API references; no
prohibited implementation source was consulted.

## Implemented composition

Capability-probed Apple NDJSON and discovered journalctl use private continuous
pipes. Unknown JSON fields/redaction remain raw record bytes. Frame size,
nesting, scalar metadata, publication capacity and per-turn framing are bounded.
Malformed/oversized/incomplete input produces explicit adapter notices. stderr
is a private bounded diagnostic. Source limits are explicit and distinct from
local view predicates. Retry never claims durable or gap-free history.

Windows discovers Application/System and bounded enabled operational channels.
Eight prestarted workers own pull subscriptions, bounded XML/system/message/
bookmark rendering, and one publisher-metadata cache each. Quiet workers wait
on subscription/cancellation signals. Source retirement does not join. Missing
message resources retain raw XML plus a render error. Explicit supplied
bookmarks use strict resume semantics and retain stale/native errors. All view
and model mutation remains on the app thread.

## Verified scope

- macOS strict-warning ASan/UBSan demo/fixture builds pass.
- **64 focused Mac cases pass**: 23 demo/parser/native-replay cases and 41
  tab/follower regressions. **8/8 focused CTest gates pass**, including both
  provenance gates and layer/license checks.
- **23 Linux ASan/UBSan cases pass**, including file/atomic-export workflows
  and six native replay frames, from a separate hashed source snapshot.
- The same headless app graph observed tagged real macOS os_log and Linux
  syslog/journal events, their timestamps, local filter publication and details,
  admitted another tagged event while paused, resumed and retired the source.
- Six native goldens cover Apple/journal/Windows metadata, channel denial,
  stale resume and missing-message fallback, alongside ten portable app states.
  Replay does not establish native Windows correctness.

Apple help was observed returning EX_USAGE 64 with valid NDJSON help; the pure
capability regression accepts that only for the explicit help probe. The native
case waits for query publication, rather than assuming a busy loop publishes it.
Broad loaded-host Apple traffic showed explicit output loss; controlled source
scope separates subscription acceptance from broad-source throughput. A Linux
capability attempt returned unavailable under load; the later native attempt
passed. Neither transient unavailability nor skipped execution is counted as a
pass.

The [JSON record](live-log-wp82-progress-2026-10-13.json) retains source/output
hashes, target/configuration, times, exact build work and test/native output.
The Linux clean build took 749.67s for 134 actions, the replay increment 209.69s
for 17 actions, and the two-unit diagnostic/native increment 416.15s. Logs
identify compiled/relinked work and unchanged snapshot hashes; these loaded
host durations are not performance evidence. The final Mac configure/no-work
invocation took 1.91s: changed Windows source is absent from the Mac target, and
compiled inputs/output hashes match. Mac scratch uses mounted S8T;
Linux uses the recorded permitted shared PRO-BLADE scratch and explicit TMPDIR.

## Deferred acceptance

The Windows setup operation stopped before producing a build record. Its owned
root has source/transfer files and no known running owned build. A later
read-only checksum observation timed out; no successful Windows compilation or
native event result is claimed. Existing unrelated VM work is preserved. The
observed Mac host load was 54.47/55.26/46.40; Windows strict/native validation is
carried forward under the owner's load-deferral instruction.

Mac/Linux snapshots contain pre-existing completion work excluded from the
live-log commit. The clean committed-tree matrix, installed-client/fuzz gates,
TSan/resource stability, measured peak memory/CPU/idle/input/visibility budgets
and actual native windows remain WP-84 obligations. No real demo window was
launched. Computer use previously denied macOS Terminal access, so native-window
acceptance remains pending. WP-82 is not Done.
