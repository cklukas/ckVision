# ckVision Engineering Standard

These rules bind all work in this repository, human or agent.

## Quality bar

Clean, professional, elegant code — no workarounds, no hacks, no quick
paths, no deliberate follow-up debt. When a fix feels hacky, it is; find
the design-level fix. Pre-1.0 there is no backward compatibility: fix the
design, update all callers and tests in the same change, remove what is
superseded. Do not optimize for the smallest diff.

## Provenance rule (binding, no exceptions)

Never open, read, quote, or transcribe source code of Turbo Vision or any
of its ports and derivatives — not to "check how they did it", not for a
constant, not for anything. ckVision is implemented exclusively from its
own documents (VISION, ARCHITECTURE, plans, goldens) and from published
standards and documentation: ECMA-48, the xterm control-sequences
reference, kitty protocol specifications, Microsoft's console/VT
documentation, Unicode standards and reports (UAX #11, #29, TR#51), and
platform API references. Behavioral conventions of classic desktops may be
described from documentation and observation, never lifted from code.

### Provenance review checklist

Before a change is accepted, author and reviewer confirm that its behavior is
derived from the authority chain above, an approved published reference, or a
documented observation — never a prior implementation's source. Record the
ckVision specification and executable evidence for re-derived behavior; do not
present a comparison, a port, or outside-implementation faithfulness as evidence. Run the mandatory
`provenance_check` and `provenance_checker_self_test` CTest gates. Those checks
are lexical guards, not a substitute for this review.

## Architecture discipline

- The layer model and dependency rules in ARCHITECTURE.md §1 are binding;
  include-direction checks in CI enforce them.
- No global mutable state in library design — no singletons, no hidden
  registries. The single sanctioned process-level facility is the
  async-signal-safe terminal-session registry (DECISIONS.md D-024).
- Zero mandatory external dependencies. Adding any dependency — including
  vendored code and test-only tools — is a DECISIONS.md matter first.
- Determinism: no wall-clock reads and no locale in library code; no
  environment, filesystem, or clock access in core/scene/ui/widgets —
  platform impurities enter only through injected interfaces and
  explicit backend construction (D-039); identical inputs must yield
  byte-identical frames on every platform.
- The performance charter (ARCHITECTURE.md §8) is part of correctness:
  budget regressions are failures.

## Verification gates (definition of done)

In order: clean build with zero warnings (warnings are errors) on macOS,
Linux, and Windows targets; all unit, golden, and script tests green;
fuzz targets build and run their CI corpus; ASan/UBSan (and TSan where
threads exist) green; benchmarks within budget; documentation updated for
every public-surface change. A feature without golden coverage and
reference documentation is not done.

## Repository practice

- Trunk-only: commit to `main`, linear history, no feature branches.
- Planning documents (VISION, ARCHITECTURE, ROADMAP, DECISIONS, plans/)
  are the project's authority chain, in that order. Code that contradicts
  them is wrong; if the documents are wrong, change the documents first
  (DECISIONS.md records the change) and then the code.
- Keep every artifact self-contained: no references to other projects'
  private repositories, plans, or internals. Naming other projects is
  acceptable only for public, factual statements (e.g. adopters).
- Every source, build, and tooling file begins with the copyright line
  `Copyright (c) 2026 C. Klukas. All rights reserved.` in the file's
  comment syntax, followed by `SPDX-License-Identifier: MIT` on the next
  line — D-OPEN-1 was resolved by the owner on 2026-08-20: **MIT** (the
  standard MIT license, not MIT-0; `LICENSE` at the repo root holds the
  authoritative text), with contribution terms in `CONTRIBUTING.md` (DCO
  sign-off, an explicit patent grant, and a relicense grant to the owner).
  The copyright line stays verbatim; the SPDX line beneath it is the
  license statement, swept by ckmux's `tools/add_spdx.py`.
- Commit messages: one-line summary, then what and why, then verification
  evidence (test/benchmark counts). Never commit with failing gates.
- The owner's own commits carry no `Signed-off-by:` line. The DCO in
  `CONTRIBUTING.md` is the certification an outside contributor gives; the
  owner (Dr. Christian Klukas) develops this project himself and certifies
  nothing to himself. Do not add a sign-off, and do not raise the question.
