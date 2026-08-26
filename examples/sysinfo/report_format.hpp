// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Presentation of the SysInfo reports: pure functions from numbers to the
// text a pane shows. They exist as their own translation unit because they
// are the part of this application that is worth testing exactly — an
// off-by-one in a unit or a rounding rule is a wrong number on screen, and
// a wrong number is the only failure this program cannot survive.
//
// Deliberately locale-free and hand-rolled rather than routed through
// iostreams or printf: the same bytes must render the same way on every
// host, and a golden that depends on the ambient locale is a golden that
// fails on somebody else's machine for a reason nobody can see.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "system_probe.hpp"

namespace ckv::sysinfo {

// What a field says when the host did not answer. One spelling, in one
// place, so "the machine did not say" never reads as a measurement.
inline constexpr std::string_view kNotReported = "not reported";

// Binary units, because every operating system in this program's reach
// reports memory in powers of two and a program that silently switched to
// powers of ten would disagree with its own host by 7%.
std::string format_bytes(std::uint64_t bytes);

// Rounded to the unit a reader would say out loud: "3.20 GHz", "800 MHz".
std::string format_hertz(std::uint64_t hertz);

// "4d 03:22:10" once a day has passed, "03:22:10" before that.
std::string format_duration(std::int64_t seconds);

// Locale-independent fixed-point. A non-finite value has no decimal
// spelling, so it reports as absence rather than as "nan".
std::string format_decimal(double value, int decimals);

// `part` of `whole` as whole percent, rounded to nearest; empty `whole`
// yields absence rather than a division.
std::string format_percent(std::uint64_t part, std::uint64_t whole);

std::string text_or_absent(const std::optional<std::string>& value);
std::string bytes_or_absent(const std::optional<std::uint64_t>& value);
std::string count_or_absent(const std::optional<int>& value);
std::string hertz_or_absent(const std::optional<std::uint64_t>& value);
std::string duration_or_absent(const std::optional<std::int64_t>& seconds);
std::string decimal_or_absent(const std::optional<double>& value, int decimals);

// --- Composed panes ------------------------------------------------------
//
// The panes' contents as plain rows of text, composed here rather than in
// the views. Two reasons, and the second is the one that matters: a test
// can then assert what a pane says without rendering it, and the report
// this application exports is the same text the panes show rather than a
// second, drifting rendition of it.

std::string power_source_text(const PowerReport& power);
std::string battery_text(const PowerReport& power);

// "10.0 GiB of 16.0 GiB used (63%)".
std::string memory_usage_text(const MemoryReport& memory);

// The used share of a capacity, as a fraction for a bar. Zero when either
// figure is absent — a bar cannot show absence, which is why the text
// beside it always names the same numbers.
double used_fraction(const std::optional<std::uint64_t>& total,
                     const std::optional<std::uint64_t>& free_bytes);

// The line under a chart of measured bars. An unoptimized build measures
// the compiler's idleness rather than the machine, and the footnote is
// where the asterisk on those bars is answered.
std::string measurement_caveat_text(const BuildReport& build);

// The mark a measured bar's label carries so the footnote has something to
// point at: an asterisk where the build cannot be trusted for speed,
// nothing at all where it can.
std::string measured_bar_marker(const BuildReport& build);

// The first sentence of a longer explanation. What a list of six things can
// afford to say about each of them.
std::string first_sentence(std::string_view text);

// Field/value rows. A row of two empty strings is a group separator.
std::vector<std::vector<std::string>> system_rows(const SystemProbe& probe);
std::vector<std::vector<std::string>> memory_rows(const MemoryReport& memory);

// Mounted on / filesystem / capacity / free / used.
std::vector<std::vector<std::string>> volume_rows(const std::vector<VolumeReport>& volumes);

// "/: 384.0 GiB free of 512.0 GiB", or a stated absence for no volume.
std::string volume_usage_text(const VolumeReport& volume);

}  // namespace ckv::sysinfo
