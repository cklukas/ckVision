// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "report_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace ckv::sysinfo {
namespace {

// Integer digits, without a stream and without a locale.
std::string decimal_digits(std::uint64_t value) {
    if (value == 0) return "0";
    char buffer[24];
    int index = static_cast<int>(sizeof(buffer));
    while (value != 0 && index > 0) {
        buffer[--index] = static_cast<char>('0' + static_cast<int>(value % 10));
        value /= 10;
    }
    return std::string(buffer + index, static_cast<std::size_t>(static_cast<int>(sizeof(buffer)) - index));
}

std::string two_digits(std::int64_t value) {
    const std::string digits = decimal_digits(static_cast<std::uint64_t>(value));
    return digits.size() >= 2 ? digits : "0" + digits;
}

struct Scaled {
    std::uint64_t whole = 0;
    std::uint64_t tenths = 0;
};

// `value` scaled by `divisor`, with one fractional digit, rounded to
// nearest. Done in integers throughout: the values are byte counts up to
// petabytes, and a double loses the low bits of those before the division
// that would hide it.
Scaled scale_with_tenth(std::uint64_t value, std::uint64_t divisor) {
    Scaled scaled{value / divisor, 0};
    const std::uint64_t remainder = value % divisor;
    scaled.tenths = (remainder * 10 + divisor / 2) / divisor;
    // Rounding the tenth can carry into the whole, and the caller must see
    // that: a carry at the top of a unit means the next unit up is the one
    // to show, or 1048575 bytes prints as "1024.0 KiB".
    if (scaled.tenths >= 10) {
        scaled.tenths = 0;
        ++scaled.whole;
    }
    return scaled;
}

std::string scaled_text(const Scaled& scaled) {
    return decimal_digits(scaled.whole) + "." + decimal_digits(scaled.tenths);
}

}  // namespace

std::string format_bytes(std::uint64_t bytes) {
    static constexpr std::string_view kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    std::uint64_t divisor = 1;
    std::size_t unit = 0;
    while (unit + 1 < std::size(kUnits) && bytes / divisor >= 1024) {
        divisor *= 1024;
        ++unit;
    }
    if (unit == 0) return decimal_digits(bytes) + " B";
    Scaled scaled = scale_with_tenth(bytes, divisor);
    if (scaled.whole >= 1024 && unit + 1 < std::size(kUnits)) {
        divisor *= 1024;
        ++unit;
        scaled = scale_with_tenth(bytes, divisor);
    }
    return scaled_text(scaled) + " " + std::string(kUnits[unit]);
}

std::string format_hertz(std::uint64_t hertz) {
    if (hertz >= 1'000'000'000) return scaled_text(scale_with_tenth(hertz, 1'000'000'000)) + " GHz";
    if (hertz >= 1'000'000) return decimal_digits((hertz + 500'000) / 1'000'000) + " MHz";
    if (hertz >= 1'000) return decimal_digits((hertz + 500) / 1'000) + " kHz";
    return decimal_digits(hertz) + " Hz";
}

std::string format_duration(std::int64_t seconds) {
    if (seconds < 0) return std::string(kNotReported);
    const std::int64_t days = seconds / 86'400;
    const std::int64_t hours = (seconds % 86'400) / 3'600;
    const std::int64_t minutes = (seconds % 3'600) / 60;
    const std::int64_t rest = seconds % 60;
    const std::string clock = two_digits(hours) + ":" + two_digits(minutes) + ":" + two_digits(rest);
    return days > 0 ? decimal_digits(static_cast<std::uint64_t>(days)) + "d " + clock : clock;
}

std::string format_decimal(double value, int decimals) {
    if (!std::isfinite(value) || decimals < 0 || decimals > 9) return std::string(kNotReported);
    const bool negative = value < 0.0;
    double magnitude = negative ? -value : value;
    std::uint64_t scale = 1;
    for (int step = 0; step < decimals; ++step) scale *= 10;
    // Above this the double has no fractional bits left to print, so the
    // decimals would be an invention of the conversion.
    if (magnitude * static_cast<double>(scale) >= 9.0e18) return std::string(kNotReported);
    const std::uint64_t scaled = static_cast<std::uint64_t>(magnitude * static_cast<double>(scale) + 0.5);
    std::string text = decimal_digits(scaled / scale);
    if (decimals > 0) {
        std::string fraction = decimal_digits(scaled % scale);
        text += ".";
        text.append(static_cast<std::size_t>(decimals) - fraction.size(), '0');
        text += fraction;
    }
    return negative && scaled != 0 ? "-" + text : text;
}

std::string format_percent(std::uint64_t part, std::uint64_t whole) {
    if (whole == 0) return std::string(kNotReported);
    return decimal_digits((part * 100 + whole / 2) / whole) + "%";
}

std::string text_or_absent(const std::optional<std::string>& value) {
    return value.has_value() && !value->empty() ? *value : std::string(kNotReported);
}

std::string bytes_or_absent(const std::optional<std::uint64_t>& value) {
    return value.has_value() ? format_bytes(*value) : std::string(kNotReported);
}

std::string count_or_absent(const std::optional<int>& value) {
    if (!value.has_value()) return std::string(kNotReported);
    return *value < 0 ? std::string(kNotReported) : decimal_digits(static_cast<std::uint64_t>(*value));
}

std::string hertz_or_absent(const std::optional<std::uint64_t>& value) {
    return value.has_value() ? format_hertz(*value) : std::string(kNotReported);
}

std::string duration_or_absent(const std::optional<std::int64_t>& seconds) {
    return seconds.has_value() ? format_duration(*seconds) : std::string(kNotReported);
}

std::string decimal_or_absent(const std::optional<double>& value, int decimals) {
    return value.has_value() ? format_decimal(*value, decimals) : std::string(kNotReported);
}

namespace {

std::vector<std::string> field_row(std::string name, std::string value) {
    return {std::move(name), std::move(value)};
}

// A blank row. These tables are read as groups of related facts, and a gap
// is how a reader sees the group boundary without the table needing a
// second kind of row to draw a rule with.
std::vector<std::string> spacer_row() { return {"", ""}; }

}  // namespace

std::string power_source_text(const PowerReport& power) {
    switch (power.source) {
        case PowerReport::Source::Battery: return "battery";
        case PowerReport::Source::Line: return "line power";
        case PowerReport::Source::Unknown: break;
    }
    return std::string(kNotReported);
}

std::string battery_text(const PowerReport& power) {
    if (!power.charge_percent.has_value()) return std::string(kNotReported);
    std::string text = count_or_absent(power.charge_percent) + "%";
    if (power.charging.has_value()) text += *power.charging ? ", charging" : ", not charging";
    if (power.remaining_seconds.has_value()) text += ", " + format_duration(*power.remaining_seconds) + " left";
    return text;
}

std::string memory_usage_text(const MemoryReport& memory) {
    if (!memory.total_bytes.has_value() || !memory.available_bytes.has_value())
        return std::string(kNotReported);
    const std::uint64_t total = *memory.total_bytes;
    const std::uint64_t available = std::min(*memory.available_bytes, total);
    const std::uint64_t used = total - available;
    return format_bytes(used) + " of " + format_bytes(total) + " used (" + format_percent(used, total) + ")";
}

double used_fraction(const std::optional<std::uint64_t>& total,
                     const std::optional<std::uint64_t>& free_bytes) {
    // An absent figure, or a pair that disagrees (see volume_rows), leaves
    // the bar empty: a bar cannot draw "not reported", which is why the
    // text beside it always names the numbers it was given.
    if (!total.has_value() || !free_bytes.has_value() || *total == 0 || *free_bytes > *total) return 0.0;
    return static_cast<double>(*total - *free_bytes) / static_cast<double>(*total);
}

std::string measurement_caveat_text(const BuildReport& build) {
    if (build_is_optimized(build))
        return "Measurements are indicative: they describe this machine as it is right now, "
               "not as a certification.";
    if (build.build_type.empty())
        return "* This build does not say how it was compiled, so its scores cannot be "
               "attributed to this machine rather than to the build.";
    // The asterisk answers the one on every measured bar above it. "Greatly
    // reduced" is not a hedge: the same kernels on the same machine score
    // several times lower unoptimized, so a reader comparing a Debug score
    // with anything at all is comparing compilers.
    return "* " + build.build_type +
           " build: these scores are greatly reduced. An unoptimized binary measures the build, "
           "not the machine.";
}

std::string measured_bar_marker(const BuildReport& build) {
    return build_is_optimized(build) ? std::string() : std::string(" *");
}

std::vector<std::vector<std::string>> system_rows(const SystemProbe& probe) {
    const HostReport host = probe.host();
    const ProcessorReport processor = probe.processor();
    const PowerReport power = probe.power();
    const BuildReport build = probe.build();

    std::vector<std::vector<std::string>> rows;
    rows.push_back(field_row("Host name", text_or_absent(host.host_name)));
    rows.push_back(field_row("User", text_or_absent(host.user_name)));
    rows.push_back(field_row("Operating system", text_or_absent(host.os_name)));
    rows.push_back(field_row("Kernel", text_or_absent(host.kernel)));
    rows.push_back(field_row("Architecture", text_or_absent(host.architecture)));
    rows.push_back(field_row("Uptime", duration_or_absent(host.uptime_seconds)));
    rows.push_back(field_row("Load (1 min)", decimal_or_absent(host.load_average_1m, 2)));
    rows.push_back(spacer_row());
    rows.push_back(field_row("Processor", text_or_absent(processor.brand)));
    rows.push_back(field_row("Physical cores", count_or_absent(processor.physical_cores)));
    rows.push_back(field_row("Logical cores", count_or_absent(processor.logical_cores)));
    rows.push_back(field_row("Performance cores", count_or_absent(processor.performance_cores)));
    rows.push_back(field_row("Efficiency cores", count_or_absent(processor.efficiency_cores)));
    rows.push_back(field_row("Nominal frequency", hertz_or_absent(processor.nominal_hz)));
    rows.push_back(field_row("L1 data cache", bytes_or_absent(processor.l1d_cache_bytes)));
    rows.push_back(field_row("L2 cache", bytes_or_absent(processor.l2_cache_bytes)));
    rows.push_back(field_row("L3 cache", bytes_or_absent(processor.l3_cache_bytes)));
    rows.push_back(spacer_row());
    rows.push_back(field_row("Power source", power_source_text(power)));
    rows.push_back(field_row("Battery", battery_text(power)));
    rows.push_back(spacer_row());
    rows.push_back(field_row("Compiler", build.compiler));
    rows.push_back(field_row("Standard library", build.standard_library));
    rows.push_back(field_row("Language", build.cxx_standard));
    rows.push_back(field_row("Build", text_or_absent(build.build_type)));
    rows.push_back(field_row("Pointer size", count_or_absent(build.pointer_bits) + " bit"));
    rows.push_back(field_row("Byte order", build.little_endian ? "little endian" : "big endian"));
    return rows;
}

std::vector<std::vector<std::string>> memory_rows(const MemoryReport& memory) {
    std::vector<std::vector<std::string>> rows;
    rows.push_back(field_row("Total", bytes_or_absent(memory.total_bytes)));
    rows.push_back(field_row("Available", bytes_or_absent(memory.available_bytes)));
    if (!memory.detail.empty()) {
        rows.push_back(spacer_row());
        // The host's own accounting, in the host's own words: a program
        // that renamed these into a common vocabulary would be reporting
        // its own arithmetic as the machine's measurement.
        for (const auto& [name, value] : memory.detail) rows.push_back(field_row(name, format_bytes(value)));
    }
    rows.push_back(spacer_row());
    rows.push_back(field_row("Swap total", bytes_or_absent(memory.swap_total_bytes)));
    rows.push_back(field_row("Swap used", bytes_or_absent(memory.swap_used_bytes)));
    return rows;
}

std::vector<std::vector<std::string>> volume_rows(const std::vector<VolumeReport>& volumes) {
    std::vector<std::vector<std::string>> rows;
    rows.reserve(volumes.size());
    for (const VolumeReport& volume : volumes) {
        std::string mount = volume.mount_point;
        if (volume.read_only) mount += " (ro)";
        // More free space than capacity is not a bug in either figure:
        // an APFS volume reports its own size and its container's free
        // space, and on a shared container the second is routinely the
        // larger. There is no used share to state, so none is stated.
        std::string used = std::string(kNotReported);
        if (volume.capacity_bytes.has_value() && volume.free_bytes.has_value() &&
            *volume.capacity_bytes > 0 && *volume.free_bytes <= *volume.capacity_bytes)
            used = format_percent(*volume.capacity_bytes - *volume.free_bytes, *volume.capacity_bytes);
        rows.push_back({std::move(mount), text_or_absent(volume.filesystem),
                        bytes_or_absent(volume.capacity_bytes), bytes_or_absent(volume.free_bytes),
                        std::move(used)});
    }
    return rows;
}

std::string volume_usage_text(const VolumeReport& volume) {
    return volume.mount_point + ": " + bytes_or_absent(volume.free_bytes) + " free of " +
           bytes_or_absent(volume.capacity_bytes);
}

}  // namespace ckv::sysinfo
