<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Captured process helpers

The shared invocation model supports terminal children, captured helpers and
independently living processes. These are separate ownership contracts: a
terminal is not a private byte pipe, and releasing a helper's descendants is
not a claim that those descendants have escaped the caller's ancestor jobs.

## One invocation model

`core::ProcessLaunchSpec` describes a program's invocation independently of
whether its standard streams are connected to a terminal or to private byte
pipes. `TerminalLaunchSpec` inherits these fields and adds its terminal profile
and required terminal exit policy. Both platform terminal backends already use
the shared native preparation code.

`program(executable, arguments)` names the executable without a PATH search;
arguments do not include argv[0]. An empty `argv0` uses the executable name.
`working_directory` defaults to `/` and must be explicitly set to a usable
native directory. `ProcessEnvironmentPolicy::InheritAndOverride` starts from
the parent's environment and replaces named entries; `ExplicitOnly` starts
empty. Environment names must be nonempty, contain no `=`, and not repeat.
Windows additionally compares names case-insensitively.

`windows_command_processor(executable, command)` explicitly selects cmd syntax.
An absent command opens interactive cmd with AutoRun disabled; a present command,
including empty text, executes and exits. Command text is trusted code, not data
to be escaped. It cannot be combined with an argument vector or argv[0]; POSIX
refuses this form. Windows uses `/d` or `/d /s /c` and native image separators,
while ordinary argument data uses the existing CRT encoder. No shell is guessed.

The common pure validator rejects NULs in every native string, invalid names and
contradictory forms before spawning. POSIX permits opaque byte paths/arguments;
Windows requires valid UTF-8 and enforces native command-line limits. The native
backends share one environment/argv builder per platform, not a helper-specific
copy of the terminal's rules.

## Captured execution contract

`core::ProcessRunner` is injected and has no default/global instance. Its
`run(request)` synchronously borrows the request's opaque binary `input` only
for that call. Input is stdin, never part of argv, and is closed after the last
byte, including empty input. Each output stream has an independent capture
limit; zero means retain nothing while draining. Excess data sets `truncated`.
These bytes are private diagnostic captures, not terminal output.

The idle-progress budget must be positive. Only positive byte transfer resets
it; repeated wakeups, interruptions and retry answers do not. Native adapters
borrow a monotonic `Clock`, keep each pump bounded and introduce no persistent
I/O worker thread. Completion follows root exit and already-ready diagnostics,
not EOF held open by a background descendant.

The caller must choose `ReleaseOnSuccess` or `TerminateOnCompletion`; an
unspecified descendant policy is an invalid request. Successful release permits
a clipboard holder to survive its root. Failure/timeout always cleans the owned
group/job. POSIX containment is a process group, not a claim about daemons that
deliberately leave it; Windows containment is an assigned job, with the root
suspended until assignment and only private stdio handles whitelisted.

`ProcessRunResult` distinguishes invalid request, launch failure, I/O failure,
idle timeout, unavailable exit status and completed root exit. The native error
retains its domain/code; the exit status retains either a normal exit code
(including every Windows DWORD) or a POSIX signal. Absence is not zero.
`successful()` requires a completed result, all input transferred, no native
error, and a known normal zero exit. Stdout/stderr captures and their truncation
remain independent of that verdict.

## Native construction

### Executable preflight on Windows

Added in ckVision0.1.18. This native inspection is separate from shell selection
and from the process launch, captured-helper and detached-lifetime APIs.

`term::inspect_windows_process_image(absolute_utf8_path)` reads bounded PE/COFF
headers without executing the file or searching PATH. It distinguishes invalid
paths, unavailable observations, malformed/non-process images, unsupported
architectures and header-compatible candidates. DLLs and non-Windows subsystems
are refused. The declared machine is checked against host user-mode support,
not the architecture of the inspecting application. Windows11 uses
`GetMachineTypeAttributes`; the Windows10 baseline checks the native machine
and documented x86 compatibility using `IsWow64Process2`.

This matters for an x64 application on ARM64: an observed system `cmd.exe`
returns error193 from `GetBinaryTypeW` in the x64 caller, yet launching the
same image succeeds. It is therefore not a reliable cross-architecture
preflight. No shell names, server roles or application policy enter inspection.

`WindowsProcessImageInfo::compatible()` is not loader success, publisher
authentication or readiness. It does not resolve imports, validate all image
contents or prevent the file from changing after inspection. The eventual
launch operation remains authoritative and must handle failure independently.

Authority: Microsoft's [PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format),
[machine support query](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getmachinetypeattributes)
and [Windows on Arm emulation](https://learn.microsoft.com/en-us/windows/arm/apps-on-arm-x86-emulation).

### Independently living Windows processes

`core::DetachedProcessLauncher` is a separate injected boundary, implemented on
Windows by `term::WindowsDetachedProcessLauncher`. It uses `ProcessLaunchSpec`
and the same native preparation, inherits no handles, and detaches the console.
It requests job breakaway but creates the child suspended. Before resuming,
`IsProcessInJob(child, nullptr)` must report no remaining job: an inner job's
permission alone is insufficient. Restrictive ancestry is an explicit failure,
not a fallback that reports a daemon started and later loses it with its caller.

`DetachedProcessResult` preserves the failure stage, native error, full-width
identity and separate cleanup state/error. Rejected created children have not
run application code; cleanup terminates and waits at most five seconds.
`successful()` requires a verified resumed child. Success releases process
ownership, but does not imply application readiness. No POSIX detached backend
is claimed by this Windows-specific implementation. Captured-helper descendant
release does not claim independence from the caller's ancestor jobs.

### Captured helpers

Include `cvision/term/process_runner.hpp`, construct a `NativeProcessRunner`
with the application's existing monotonic `Clock`, and inject it through the
core interface. The clock must outlive the runner and advance during real
native waits. This is a synchronous host-boundary call, not an asynchronous
UI task: the application chooses where a bounded helper call belongs.

POSIX owns nonblocking pipes and a dedicated process group. Only stdin,
stdout and stderr survive exec. macOS uses its atomic close-by-default spawn
file actions rather than one close syscall per possible descriptor; cwd/exec
errors are returned by the native spawn call. Other POSIX hosts use a private
close-on-exec error channel instead of reducing cwd/exec errors to exit127.
The macOS directory action supports macOS10.15 onward and selects the standard
replacement on macOS26; its deprecated spelling is only the older-host fallback.
The calling thread's
SIGPIPE mask is temporarily protected without replacing the process-global
signal disposition, then restored. Windows owns three overlapped parent pipe
endpoints, current-user-only ACLs, unique per-call names and a suspended root
assigned to a kill-on-close job before it runs. Only its three child stdio ends
are on the inherited-handle whitelist. Successful release clears kill-on-close;
every other path retains owned-job cleanup. Win32 and NTSTATUS errors have
distinct domains; neither is a POSIX errno.

Portable tests use an injected fake to check this value contract. Native terminal
tests retain real argument, cwd/environment and child-lifecycle coverage;
real captured-pipe fixtures cover binary transfer, independent capped floods,
timeouts/progress, launch/exit failures, inheritance and descendant policies.
POSIX tests also force real pipe-resource failure, verify timeout-root reaping,
and deliver repeated signal wakeups without extending the idle budget. The
external child fixture is an ordinary strict-warning executable; sanitizer
instrumentation remains on the library and test parent, not an external
runtime's startup/exit latency. Final platform gates and downstream integration
remain required before release.

## Continuous process output

`core::ProcessStreamService` supplies private continuous stdout/stderr, with
`term::NativeProcessStreamService` owning 1–8 prestarted workers. Each active
session owns one root and its process group/job. It reuses ProcessLaunchSpec,
native argument/environment/cwd preparation, stdio inheritance and the same
private pump as NativeProcessRunner. No terminal emulation or host-terminal
forwarding is involved. A quiet stream has no idle timeout.

```cpp
#include <cvision/term/process_stream.hpp>
#include <cvision/widgets/log_stream_receiver.hpp>

// Construct outside dispatch. clock is a thread-safe injected native clock;
// wake can be a shared LogActivity with an enclosing TerminalLogWake target.
ckv::term::NativeProcessStreamService service(clock, 2, wake);
ckv::core::ProcessStreamRequest request;
request.launch = ckv::core::ProcessLaunchSpec::program("/absolute/helper",
    {"--follow", "argument data"});
request.launch.working_directory = "/explicit/directory";
request.stdout_source = 1;
request.stderr_source = 2;
auto opened = service.start(std::move(request));
// Handle Invalid/Busy; Opened owns a session. Retain it until both receivers
// are detached/destroyed, and retire it before destroying service.
auto session = std::move(opened.session);
ckv::widgets::LogModel output_model, error_model;
ckv::widgets::LogStreamReceiver output(output_model, session->stdout_mailbox());
ckv::widgets::LogStreamReceiver errors(error_model, session->stderr_mailbox());
// A supplied-time controller schedules bounded receiver/filter/search turns.
```

The example assumes an Opened result before accessing session. Requests require
distinct nonzero source IDs and a nonzero epoch. Arguments are ordinary data;
explicit Windows command-processor syntax remains a separate launch choice.
Validation first limits work/storage: at most 64 argv and 128 environment
entries, 64 KiB aggregate strings, independent valid mailbox options and a
1–16384-byte chunk fitting both queues. Invalid returns its validation reason;
Busy means all worker slots are occupied or briefly contended. Retry through
ordinary yielding app scheduling. Source selection may allocate bounded state,
but start performs no process launch, pipe read or worker join.

Each output has an independent fixed mailbox/overflow policy. A backpressured
endpoint retains one native chunk and one reusable publication packet; the
other endpoint keeps progressing. Try-lock contention yields through a short
retry deadline; capacity/cancellation wakes join the native wait. Drop policies
use the mailbox's positional gap accounting. An already accepted packet is
not duplicated on retry. Stdout/stderr identities and sequence order remain
separate; their arrival order does not establish original cross-stream causality.
Compose each mailbox with the shipped receiver/model/filter/controller from
[Live logs](live-logs.md), including UTF-8 boundaries and safe display handling.

`status(result)` is a nonblocking try-copy. Preparing, Running, Draining,
Completed, Cancelled, LaunchFailed and IoFailed are distinct. Completed preserves
the root's full normal/signal exit observation, including nonzero exits; it
does not imply success. Native errors preserve their domain/code. Publication
failure and an execution exception have separate failure tags. Root identity,
bytes observed per stream, held-publication bytes and retired staging are
diagnostics. Source-native timestamps are not inferred from arrival time.

EOF closes each mailbox independently and flushes its model even while the
root or other pipe remains active. EOF flags mean positively observed EOF;
`tail_unverified` distinguishes forced closure after root completion without
proven continuity. Root exit terminates owned descendants and drains buffered
ready bytes, without waiting for a surviving descendant to close an endpoint.
macOS process events, Linux pidfd where supported and Windows root handles
avoid idle polling; `root_notifications=false` exposes the 20 ms fallback on
unsupported readiness routes. Root exit/error and pipe-end publications remain
observable when a data queue is full.

Windows continuous workers wait on the root process, pending pipe operations
and the shared cancellation/capacity event. They consume that event after a
signalled wait; quiet streams have neither the finite helper's five-second
idle timeout nor a periodic timeout wake. A short retry is reserved for mutex
contention, while full queues wait for capacity. This follows the native
[multiple-object wait contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects).

`cancel()` and session destruction signal cancellation without joining. The
worker terminates/reaps its owned helper, retires queued/staged bytes and emits
cancellation separately from overflow loss. Service shutdown joins outside UI
dispatch. Detach/destroy borrowed receivers before releasing session state.
`start()` is nonblocking: `Busy` also covers a worker's short ownership handoff,
so callers retry on a later scheduling turn. `Invalid` is a structural rejection
and is not retried unchanged. Native tests follow the same admission contract;
they do not assume a newly constructed worker's mutex is immediately available.
Clocks and wake targets outlive the service and any surviving queue use. POSIX
process-group ownership does not sandbox a helper deliberately escaping that
group; use trusted source helpers. Windows owns a job with kill-on-close.

`allocated_bytes()` reports fixed per-session queue/publication/native-pump
workspace. Include service workers/stacks, transient launch/environment storage
and OS pipe buffers separately when budgeting/measuring total peak memory.
The inherited environment follows the shared launch contract; ExplicitOnly
lets an application choose its whole environment. No mandatory dependency is
added. Native stream/finite-runner fixtures cover real byte transfer, independently
blocked sinks, argument data, per-pipe EOF, overflow, errors and owned cleanup;
the final three-platform/thread/resource matrix remains a release obligation.
