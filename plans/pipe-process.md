<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Captured non-terminal helper processes

Status: Implementation landed; hosted diagnosis, release and adoption open · 2026-10-04 · D-132

A terminal application can export selected text through a reader-configured
helper without giving that helper its screen, control channels, or selected
text as public command-line arguments. This is a scoped platform service,
not an embedded terminal, a network transport, or a general process manager.
Neither a PTY nor ConPTY is an appropriate substitute for binary stdin pipes.

## Contract and ownership

Core carries the shared process invocation values and typed result plus an
injected ProcessRunner contract. Term implements the actual POSIX/Windows
runner with an explicitly borrowed monotonic Clock. There is no environment,
clock, filesystem or native-handle access in core, and no global runner.

Extract the existing terminal launch's executable, argv/argv0, explicit
Windows command-processor selection, cwd and environment policy into one
shared core ProcessLaunchSpec. TerminalLaunchSpec consumes that same value
model and retains terminal-only profile/exit policy. Do not duplicate native
argument/environment construction or introduce a shell-guessing heuristic.
All callers, native boundary validators, tests and public documentation move
together; no pre-1.0 compatibility shim is required.

The request supplies immutable binary stdin separately from the invocation,
an idle-progress budget and per-stream capture limits. Stdout and stderr are
private byte captures, never forwarded to the host terminal. The service
closes stdin immediately after all input is written, including empty input.
Excess captured bytes are drained and counted as truncated, not retained or
allowed to block the child. Each pump pass is bounded and checks root exit
and the deadline before returning to more output.

Results distinguish launch/validation failure, input/output failure, idle
timeout, unavailable exit status and completed root exit. Native errors retain
their domain and code. Input byte count and optional root exit status travel
independently; success requires all input transferred and a known zero exit.
Failure and absent observations are not successful zero-valued results.

The budget resets only when positive byte transfer occurs, not on wakeups,
EINTR, EAGAIN or repeatedly signaled sources. All timing uses the injected
monotonic Clock. Failed setup and timeouts clean owned resources and reap the
root; no persistent I/O thread or zombie is left behind.

The caller must explicitly choose successful-descendant behavior. Releasing
successful descendants supports a clipboard holder that outlives its root;
cleanup of owned descendants is the other policy. Unspecified policy fails
before launch. Failure/timeout never takes the successful-release path.
Completion follows root exit plus the already-ready bounded diagnostics,
not EOF from a background descendant that keeps a capture pipe open.

Windows owns a new job and assigns the suspended root before resuming it.
Its inherited handle whitelist contains exactly the child stdin/stdout/stderr
ends; parent ends and arbitrary caller handles are never inherited. Use owned
overlapped endpoints and event readiness, not blocking WriteFile on an
unbounded synchronous pipe or a terminal-session transport. Successful
release must deliberately remove kill-on-close behavior before dropping the
job. Setup/assignment failures are typed errors, not uncontained fallback.

POSIX owns a dedicated process group, resets child signals, sets private
stdio and closes non-child endpoints before exec. macOS uses atomic
close-by-default spawn actions; other POSIX hosts use fork/exec with a private
launch-error channel. Neither launch path loops across a million possible
descriptors on macOS. Cleanup is honestly scoped
to that group: a trusted daemon that deliberately creates a new session has
left it, unlike a Windows job member. Do not call parent-PID guesses a kernel
containment boundary. Successful release must not terminate a clipboard
daemon. Exec/cwd failures retain their actual error through a private launch
error channel (or the native spawn result); exit127 is not the only launch diagnostic.

## Required acceptance

1. Shared invocation factories/validation and result availability have pure
   core tests and a deterministic injected runner; native cmd syntax and
   ordinary Unicode argv retain their existing positive/negative coverage.
2. Real binary child fixtures preserve embedded NULs, invalid UTF-8 and large
   input. Exact stdin must be observed by the child, not inferred from writes.
3. Simultaneous stdout/stderr floods exceed both caps without deadlock; their
   retained prefixes and truncation are asserted independently. Final root
   diagnostics are retained without waiting on a background holder's EOF.
4. Empty input, early stdin close, nonzero root exit, missing executable,
   invalid cwd, failed native setup and unknown exit status produce distinct
   results and never corrupt the terminal's output or inherited modes.
5. An idle child times out and is reaped; a slow progressing child survives
   multiple idle-budget intervals. Wake-only traffic cannot reset its budget.
6. Root success with a real surviving descendant is tested under both explicit
   policies. Failure/timeout cleans the owned group/job. Assert identities
   and liveness before and after; a failed launch cannot satisfy a teardown
   assertion vacuously.
7. A known inheritable sentinel handle/descriptor remains absent in the real
   child. Repeated executions keep handle/fd counts bounded and root cleanup
   complete. No persistent helper thread appears.
8. Compile actual production negatives for inheritance, deadline, cap draining
   and successful-descendant policy. Verify build exit and changed binary
   identity before tests; byte-exact restore and full repeats follow.
9. All normal library gates, fuzz/benchmarks, sanitizers and public API/docs
   gates pass on clean committed source before release. A downstream terminal
   application then tests configured helper success/failure through its actual
   copy action, including internal-copy retention and visible diagnostics.

## Published authority

Behavior is derived from this repository's D-039/D-078 and native API
contracts, not another application's implementation:

- Microsoft [redirected child stdio](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output),
  [explicit inherited handle lists](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute),
  and [job limit information](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information).
- POSIX fork/exec, dup2, pipe, poll, waitpid and process-group contracts.

The shared invocation types, injected runner/result contract, reusable native
launch preparation and both private-pipe backends landed in b1a4ccd and filtered
public744faa. Real-child cases cover native inheritance, descendant ownership,
timeouts/root reaping and actual POSIX pipe-resource failure. The external fixture
is an ordinary strict-warning executable without sanitizer instrumentation;
the library and native contract remain fully instrumented. This separation
keeps sanitizer-runtime startup/shutdown latency out of the external progress
fixture without relaxing its deadline. Actual production negatives prove the
inheritance, deadline, output-draining and descendant-policy assertions.
The native acceptance and compiled production negatives pass locally on the
named Mac, Linux and Windows hosts. Hosted macOS CI37232274765 repeats the
progressing-helper failure; its cause remains open. Final committed-source
gates, a green hosted verdict, release0.1.16 and downstream release adoption
remain required. Local passing measurements do not resolve that hosted failure.
