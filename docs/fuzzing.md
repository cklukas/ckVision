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
- `fuzz_syntax_profile`: syntax-profile parsing and compilation; and
- `fuzz_todo_codec`: bounded TODO workspace JSON decoding and canonical
  round trips over raw input bytes.

The checked-in files under `fuzz/corpus/` are permanent regression seeds. They
use `\e`, `\a`, and `\xNN` spelling for ESC, BEL, and arbitrary bytes; the
fuzz targets expand those spellings only so text files can retain reviewable
protocol boundaries. Ordinary generated mutations are still fed as raw bytes.

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
cmake --build build-fuzz --target fuzz_input_decoder fuzz_text fuzz_golden fuzz_osc fuzz_virtual_display fuzz_terminal_emulator fuzz_editor_document fuzz_syntax_profile fuzz_todo_codec
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
