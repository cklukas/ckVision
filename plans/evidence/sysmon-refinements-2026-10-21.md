# ck-sysmon refinements — 2026-10-21

Local is now a selectable View item. Desired selections survive refreshes and
relaunches; the empty selection has a golden. SSH discovery extracts positive
literal Match host/originalhost destinations. The missing rpi4 destination was
named in a Match criterion rather than a Host entry. Passwordless probes of
rpi4 and uconsole succeed; both Linux fallback samples report four processors.
CPU remains aggregate, with a processor count, as requested.

Standard (bright) is the default, with a mutually exclusive Dark choice. Chrome
and charts recolor together. Theme, all six metric toggles and desired host
selections save to the per-user settings file on each change and restore before
probing. The injected FileSystem supplies atomic conditional writes, error and
conflict reporting. Tests cover codec rejection, auto-save, a new application
instance, native disk publication/reload, and preserving concurrent changes.

The menu accessory places the application summary before the existing clock,
retaining its calendar anchor and keyboard walk. No separate title or hint row
remains. Cards span the vertical content band, without empty rows before,
between or after borders. Ollama uses one status row and one row per model,
with allocation on the model row. Six reviewed goldens cover wide/narrow/small
layouts, both themes, an empty selection and stacked cards. Fixed-data SVG/PNG
previews were inspected; they do not establish what a native terminal window
shows. No new interactive process was launched; the old copy needs a restart.

Final zero-warning builds and complete CTest suites pass on macOS (257/257),
Windows x64 (251/251) and Linux aarch64 (257/257). ASan/UBSan passes all 3,875 unit
cases plus 20 monitor/provenance/corpus CTests. TSan passes all 24 monitor/config
cases with native sample and both provenance gates. The monitor fuzzer includes
settings parsing and runs 10,000 inputs without findings. Standard scene and
performance gates pass within the complete platform suites.

The [machine-readable receipt](sysmon-refinements-2026-10-21.json) records source,
configuration and golden hashes, exact outputs, start/end/durations and
executable hashes. It retains the resolved compile/hygiene failures and rejected
intermediate ASan manifest audit; no failing final gate is hidden. The ASan
no-work rebuild was investigated with source/object/executable timestamp ordering
and final input hashes. TSan timing includes configure, an extra executable and
concurrent sanitizer work; its 404 build steps were checked.

Behavior derives from ckVision’s APIs, the example plan and published
OpenSSH/platform references. No prohibited implementation source was consulted.
Disposable sanitizer builds and test artifacts were removed after preserving
logs and build records. Current runnable Release builds remain.
