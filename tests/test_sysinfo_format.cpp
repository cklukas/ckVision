// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's number-to-text rules. This is where the example is
// tested exactly: a pane can be forgiven a column width, but not a wrong
// unit, a wrong rounding, or a zero standing where the host said nothing.
#include <cmath>
#include <limits>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "report_format.hpp"

using ckv::sysinfo::battery_text;
using ckv::sysinfo::bytes_or_absent;
using ckv::sysinfo::count_or_absent;
using ckv::sysinfo::decimal_or_absent;
using ckv::sysinfo::duration_or_absent;
using ckv::sysinfo::format_bytes;
using ckv::sysinfo::format_decimal;
using ckv::sysinfo::format_duration;
using ckv::sysinfo::format_hertz;
using ckv::sysinfo::format_percent;
using ckv::sysinfo::hertz_or_absent;
using ckv::sysinfo::kNotReported;
using ckv::sysinfo::memory_usage_text;
using ckv::sysinfo::text_or_absent;
using ckv::sysinfo::used_fraction;

CK_TEST(bytes_are_reported_in_binary_units) {
    CK_CHECK(format_bytes(0) == "0 B");
    CK_CHECK(format_bytes(1023) == "1023 B");
    CK_CHECK(format_bytes(1024) == "1.0 KiB");
    CK_CHECK(format_bytes(1536) == "1.5 KiB");
    CK_CHECK(format_bytes(16ULL * 1024 * 1024 * 1024) == "16.0 GiB");
    CK_CHECK(format_bytes(1024ULL * 1024 * 1024 * 1024 * 1024) == "1.0 PiB");
}

// The rounding carry that turns 1023.97 KiB into 1024.0 KiB: correct
// arithmetic, and a unit no reader has ever seen written down.
CK_TEST(a_rounding_carry_moves_up_a_unit_instead_of_printing_1024) {
    CK_CHECK(format_bytes(1024 * 1024 - 1) == "1.0 MiB");
    CK_CHECK(format_bytes(1024 * 1024 - 1024 * 100) == "924.0 KiB");
}

CK_TEST(frequencies_round_to_the_unit_a_reader_would_say) {
    CK_CHECK(format_hertz(3'200'000'000) == "3.2 GHz");
    CK_CHECK(format_hertz(800'000'000) == "800 MHz");
    CK_CHECK(format_hertz(2'400) == "2 kHz");
    CK_CHECK(format_hertz(0) == "0 Hz");
}

CK_TEST(durations_show_days_only_once_there_are_any) {
    CK_CHECK(format_duration(0) == "00:00:00");
    CK_CHECK(format_duration(3'661) == "01:01:01");
    CK_CHECK(format_duration(86'399) == "23:59:59");
    CK_CHECK(format_duration(357'730) == "4d 03:22:10");
    CK_CHECK(format_duration(-1) == std::string(kNotReported));
}

CK_TEST(decimals_are_fixed_point_and_locale_free) {
    CK_CHECK(format_decimal(1.25, 2) == "1.25");
    CK_CHECK(format_decimal(2.0, 2) == "2.00");
    CK_CHECK(format_decimal(3.14159, 3) == "3.142");
    CK_CHECK(format_decimal(-2.5, 1) == "-2.5");
    CK_CHECK(format_decimal(0.04, 1) == "0.0");
    // A value with no decimal spelling reports absence rather than "nan",
    // which reads on screen as a measurement of something.
    CK_CHECK(format_decimal(std::nan(""), 2) == std::string(kNotReported));
    CK_CHECK(format_decimal(std::numeric_limits<double>::infinity(), 2) == std::string(kNotReported));
}

CK_TEST(percentages_round_to_nearest_and_refuse_an_empty_whole) {
    CK_CHECK(format_percent(1, 3) == "33%");
    CK_CHECK(format_percent(2, 3) == "67%");
    CK_CHECK(format_percent(1, 2) == "50%");
    CK_CHECK(format_percent(0, 0) == std::string(kNotReported));
}

// The rule the whole probe boundary exists to keep: what the host did not
// say is not zero, not "unknown", and not an empty cell.
CK_TEST(an_absent_field_reports_absence_rather_than_a_plausible_zero) {
    CK_CHECK(text_or_absent(std::nullopt) == std::string(kNotReported));
    CK_CHECK(text_or_absent(std::string()) == std::string(kNotReported));
    CK_CHECK(bytes_or_absent(std::nullopt) == std::string(kNotReported));
    CK_CHECK(bytes_or_absent(std::optional<std::uint64_t>{0}) == "0 B");
    CK_CHECK(count_or_absent(std::nullopt) == std::string(kNotReported));
    CK_CHECK(count_or_absent(std::optional<int>{0}) == "0");
    CK_CHECK(hertz_or_absent(std::nullopt) == std::string(kNotReported));
    CK_CHECK(duration_or_absent(std::nullopt) == std::string(kNotReported));
    CK_CHECK(decimal_or_absent(std::nullopt, 2) == std::string(kNotReported));
}

CK_TEST(memory_usage_text_states_both_figures_it_divided) {
    ckv::sysinfo::MemoryReport memory;
    memory.total_bytes = 16ULL * 1024 * 1024 * 1024;
    memory.available_bytes = 6ULL * 1024 * 1024 * 1024;
    CK_CHECK(memory_usage_text(memory) == "10.0 GiB of 16.0 GiB used (63%)");

    memory.available_bytes = std::nullopt;
    CK_CHECK(memory_usage_text(memory) == std::string(kNotReported));
}

CK_TEST(a_bar_without_both_figures_shows_no_fill) {
    CK_CHECK(used_fraction(std::optional<std::uint64_t>{100}, std::optional<std::uint64_t>{25}) == 0.75);
    CK_CHECK(used_fraction(std::optional<std::uint64_t>{100}, std::nullopt) == 0.0);
    CK_CHECK(used_fraction(std::nullopt, std::optional<std::uint64_t>{25}) == 0.0);
    CK_CHECK(used_fraction(std::optional<std::uint64_t>{0}, std::optional<std::uint64_t>{0}) == 0.0);
    // More free than capacity is a disagreement between two host calls,
    // not a negative bar.
    CK_CHECK(used_fraction(std::optional<std::uint64_t>{100}, std::optional<std::uint64_t>{200}) == 0.0);
}

// A volume whose free space exceeds its capacity is what a shared APFS
// container looks like from inside one of its volumes. There is no used
// share to compute, and 0% would read as "empty" for a disk that is not.
CK_TEST(a_volume_pair_that_disagrees_reports_no_used_share_at_all) {
    ckv::sysinfo::VolumeReport volume;
    volume.mount_point = "/Volumes/Shared";
    volume.capacity_bytes = 6ULL * 1024 * 1024 * 1024 * 1024;
    volume.free_bytes = 10ULL * 1024 * 1024 * 1024 * 1024;

    const auto rows = ckv::sysinfo::volume_rows({volume});
    CK_CHECK(rows.at(0).at(2) == "6.0 TiB");
    CK_CHECK(rows.at(0).at(3) == "10.0 TiB");
    CK_CHECK(rows.at(0).at(4) == std::string(kNotReported));
    CK_CHECK(used_fraction(volume.capacity_bytes, volume.free_bytes) == 0.0);

    // The ordinary pair still divides.
    volume.free_bytes = 3ULL * 1024 * 1024 * 1024 * 1024;
    CK_CHECK(ckv::sysinfo::volume_rows({volume}).at(0).at(4) == "50%");
}

CK_TEST(a_read_only_volume_says_so_where_its_name_is) {
    ckv::sysinfo::VolumeReport volume;
    volume.mount_point = "/Volumes/Archive";
    volume.read_only = true;
    CK_CHECK(ckv::sysinfo::volume_rows({volume}).at(0).at(0) == "/Volumes/Archive (ro)");
}

// The sentence a benchmark window leads with, and the reason it is not one
// fixed sentence: a Debug build's numbers are about the build.
CK_TEST(an_unoptimized_build_says_so_before_it_shows_a_number) {
    ckv::sysinfo::BuildReport build;
    build.build_type = "Release";
    CK_CHECK(ckv::sysinfo::build_is_optimized(build));
    CK_CHECK(ckv::sysinfo::measurement_caveat_text(build).find("indicative") != std::string::npos);

    // An optimized build has nothing to footnote, so its bars carry no mark.
    CK_CHECK(ckv::sysinfo::measured_bar_marker(build).empty());

    build.build_type = "Debug";
    CK_CHECK(!ckv::sysinfo::build_is_optimized(build));
    CK_CHECK(ckv::sysinfo::measurement_caveat_text(build) ==
             "* Debug build: these scores are greatly reduced. An unoptimized binary measures the build, "
             "not the machine.");
    // The mark on the bar and the mark on the footnote are the same mark;
    // a footnote pointing at nothing is worse than no footnote.
    CK_CHECK(ckv::sysinfo::measured_bar_marker(build) == " *");
    CK_CHECK(ckv::sysinfo::measurement_caveat_text(build).rfind("*", 0) == 0);

    build.build_type.clear();
    CK_CHECK(!ckv::sysinfo::build_is_optimized(build));
    CK_CHECK(ckv::sysinfo::measured_bar_marker(build) == " *");
    CK_CHECK(ckv::sysinfo::measurement_caveat_text(build).find("does not say how it was compiled") !=
             std::string::npos);
}

CK_TEST(a_battery_reports_what_it_knows_and_no_more) {
    ckv::sysinfo::PowerReport power;
    CK_CHECK(battery_text(power) == std::string(kNotReported));
    power.charge_percent = 82;
    CK_CHECK(battery_text(power) == "82%");
    power.charging = false;
    CK_CHECK(battery_text(power) == "82%, not charging");
    power.remaining_seconds = 9'000;
    CK_CHECK(battery_text(power) == "82%, not charging, 02:30:00 left");
}
