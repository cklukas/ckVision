<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Captured process helpers

Development status: shared launch preparation, the injected core contract and
native captured-pipe runners are implemented in an isolated candidate under
verification for version 0.1.16. Full gates and downstream integration remain; this is not yet a
released helper capability.

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
