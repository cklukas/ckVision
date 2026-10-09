# Fuzzing ckVision parsers

WP-33 supplies opt-in libFuzzer targets for the stateful or externally-fed
parsers whose ordinary unit tests cannot cover their input space:

- `fuzz_input_decoder`: terminal input, bracketed paste, capability replies,
  and recovery boundaries;
- `fuzz_text`: UTF-8 decoding, grapheme segmentation, and width clipping;
- `fuzz_golden`: golden-dump parsing and canonical round trips;
- `fuzz_osc`: OSC terminator neutralization; and
- `fuzz_virtual_display`: the bounded incremental VT/Sixel output decoder;
- `fuzz_terminal_emulator`: the private child VT/DCS/Sixel emulator;
- `fuzz_editor_document`: revisioned document mutations; and
- `fuzz_syntax_profile`: line highlighting by the standard syntax profiles;
- `fuzz_theme_format`: saved-theme text parsing and canonical round trips; and
- `fuzz_todo_codec`: bounded TODO workspace JSON decoding and canonical
  round trips over raw input bytes;
- `fuzz_progress`: task hierarchy mutations, finite aggregate fractions,
  custom frame validation and narrow display painting.

The checked-in files under `fuzz/corpus/` are permanent regression seeds. They
use `\e`, `\a`, and `\xNN` spelling for ESC, BEL, and arbitrary bytes; the
fuzz targets expand those spellings only so text files can retain reviewable
protocol boundaries. Ordinary generated mutations are still fed as raw bytes.

## Invariants and seed corpora

A target that only runs its parser proves the parser does not crash. The
security-relevant targets also state what their output must be, and abort on
any input that breaks it:

| Target | Invariant checked on every input | Adversarial seeds in `fuzz/corpus/<target>/` |
|---|---|---|
| `fuzz_input_decoder` | Only a bracketed paste becomes a `TextEvent`. Paste text is well-formed UTF-8 with no control but tab and line feed: no other C0, no DEL, no C1 (D-040). The same bytes framed as one paste and arriving inside its quiet period decode to paste text and nothing else, so no key, mouse, focus or capability event can be synthesized from them. A disconnect always ends a paste. | embedded and nested paste delimiters, an embedded end marker followed by a command chord (Ctrl+Q), legacy, CSI-u and modifyOtherKeys chords inside a paste, OSC 52 and ST-terminated OSC tails, C1, DEL, CR and NUL inside a paste, overlong and surrogate UTF-8, focus, mouse and probe replies inside a paste, a kitty release tail, an unterminated paste, a truncated CSI in and out of a paste, an overlong CSI and an overlong OSC reply |
| `fuzz_osc` | `text::sanitize_osc_text` returns well-formed UTF-8 with no C0 or C1 control code point and no DEL. | raw and UTF-8 C1 terminators (0x9C, U+009C, U+009B), CAN, SUB, DEL, tab, line feed and NUL, a nested OSC with both terminators, malformed UTF-8 |
| `fuzz_text` | Segmentation always advances; borrowed clipping matches owning clipping and never exceeds its width; display validation exactly matches unchanged sanitization; `sanitize_display_text` leaves no control at all and `sanitize_clipboard_text` none but tab and line feed, both as well-formed UTF-8. | combining marks, a ZWJ emoji, malformed UTF-8, and a line mixing an erase-display CSI, an OSC 52 write, C1, DEL, NUL, tab and line feed |
| `fuzz_virtual_display` | The raster plane keeps its geometry, and any window title the model accepted holds nothing `text::sanitize_osc_text` would remove. | Sixel geometry and truncation, OSC 0/22/52 as ckVision emits them, an OSC 0 title carrying U+009C, an OSC 52 payload that is not base64, a truncated CSI |
| `fuzz_terminal_emulator` | The cell buffer matches the grid and diagnostics stay bounded. | alternate buffer, bounded OSC, scroll regions, Sixel, OSC strings ended by raw and UTF-8 C1 ST, a truncated then overlong CSI, paste-mode toggles and delimiters in child output |
| `fuzz_golden` | Every accepted dump reserializes canonically. | minimal and raster dumps, the hostile-display-text golden, and a grid row carrying ESC, 0x9C and BEL |
| `fuzz_progress` | Every available fraction remains within zero and one after hierarchy/state/count mutations; custom frames are validated before narrow painting. | hierarchy operation bytes and Unicode frame sequences |
| `fuzz_theme_format` | A refused theme text names why and yields nothing; every accepted one serializes to a text that reads back to itself, role for role. | a minimal theme, underline refinements beside an unknown role, CR, tab, ESC and NUL inside role lines, a role named twice, two spaces between fields, and an unterminated last line |

`fuzz_editor_document`, `fuzz_syntax_profile` and `fuzz_todo_codec` read
documents, profiles and JSON rather than terminal streams; their corpora carry
malformed UTF-8, malformed JSON and oversized input. `fuzz_syntax_profile`
requests each standard profile by id and requires every span the JSON, YAML,
Bash, Markdown and SQL profiles emit to be non-empty, to lie within its line
and to have both ends on grapheme-cluster boundaries; its seeds include the
input on which the JSON profile once split a two-byte character into two
spans.

Whether pasted text can dispatch an application command is decided at the
decoder: `Application` binds commands only to `KeyEvent` chords, and a paste
reaches it only as `TextEvent`. The framed-paste invariant above therefore
covers the command route for every input the fuzzer generates, without an
`Application` in the loop. `embedded_paste_terminator_and_ctrl_chord_cannot_execute_an_application_command`
(`tests/test_paste_security.cpp`) checks the same route end to end through
`Application` with a declared Ctrl+Q command, and pins the recovered frame in
`tests/golden/paste_recovery.dump`.

Seeds stay small (a few hundred bytes at most) so the corpus replay remains a
quick regression gate; libFuzzer mutates from them, it does not need them to
be exhaustive.

## Bounded CI corpus

The GitHub Actions `fuzz` lane configures Ubuntu Clang with
`CKVISION_BUILD_FUZZERS=ON` and `CKVISION_SANITIZE=address,undefined`, builds
all targets, and replays every checked-in seed file once through the
libFuzzer entrypoint. The lane uses a 4 KiB input limit, two-second per-input
timeout, and 512 MiB RSS limit. It is an executable regression gate, not a
claim of exhaustive fuzzing.

Run the equivalent locally on an LLVM/Clang installation (on macOS, use an
installed LLVM toolchain rather than Xcode's AppleClang):

```sh
cmake -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
  -DCKVISION_BUILD_FUZZERS=ON -DCKVISION_SANITIZE=address,undefined
cmake --build build-fuzz --target fuzz_input_decoder fuzz_text fuzz_golden fuzz_osc fuzz_virtual_display fuzz_terminal_emulator fuzz_editor_document fuzz_syntax_profile fuzz_theme_format fuzz_todo_codec fuzz_progress
ctest --test-dir build-fuzz --output-on-failure -L fuzz-corpus
```

AppleClang does not ship libFuzzer's link runtime, so that compiler
configuration fails early with an explicit diagnostic; it is not silently
skipped. The regular ASan/TSan test configurations remain supported there.

## Finding promotion and extended runs

When a fuzz run finds a failure, first minimize it with the target's normal
libFuzzer `-minimize_crash=1` workflow. Commit the resulting smallest input to
the matching `fuzz/corpus/<target>/` directory and add a named unit or golden
regression that states the behavioral contract. Do not merely add a crash file:
the named regression is the durable explanation, while the corpus seed protects
the parser path.

Owner-run extended campaigns use the same sanitizer configuration with a larger
`-runs` budget or wall-clock `-max_total_time` and pass a writable working
corpus directory before the checked-in seed directory; their command, commit,
corpus size, sanitizer, platform, and finding count are recorded in the
associated release evidence. Crashes must never be accepted through sanitizer
recovery.
