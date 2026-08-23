// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "report_document.hpp"

#include <algorithm>

#include "reference_points.hpp"
#include "report_format.hpp"

namespace ckv::sysinfo {
namespace {

bool is_markdown(ReportFormat format) { return format == ReportFormat::Markdown; }

std::string heading(const std::string& text, ReportFormat format, int level) {
    if (is_markdown(format)) return "\n" + std::string(static_cast<std::size_t>(level), '#') + " " + text + "\n\n";
    // In text, a rule under the title, exactly as long as the title.
    return "\n" + text + "\n" + std::string(text.size(), level == 1 ? '=' : '-') + "\n\n";
}

std::string field_line(const std::string& name, const std::string& value, ReportFormat format) {
    if (name.empty() && value.empty()) return is_markdown(format) ? std::string() : std::string("\n");
    if (is_markdown(format)) return "| " + name + " | " + value + " |\n";
    std::string padded = name;
    padded.resize(std::max<std::size_t>(name.size(), 22), ' ');
    return padded + value + "\n";
}

std::string table_header(const std::vector<std::string>& columns, ReportFormat format) {
    if (!is_markdown(format)) return std::string();
    std::string head = "|";
    std::string rule = "|";
    for (const std::string& column : columns) {
        head += " " + column + " |";
        rule += "---|";
    }
    return head + "\n" + rule + "\n";
}

std::string row_line(const std::vector<std::string>& cells, ReportFormat format) {
    if (is_markdown(format)) {
        std::string line = "|";
        for (const std::string& cell : cells) line += " " + cell + " |";
        return line + "\n";
    }
    std::string line;
    for (std::size_t index = 0; index < cells.size(); ++index) {
        std::string cell = cells[index];
        if (index + 1 < cells.size()) cell.resize(std::max<std::size_t>(cell.size(), 20), ' ');
        line += cell;
    }
    return line + "\n";
}

}  // namespace

std::string default_report_name(ReportFormat format) {
    return is_markdown(format) ? "sysinfo-report.md" : "sysinfo-report.txt";
}

std::string compose_report(const SystemProbe& probe, const std::vector<BenchmarkResult>& results,
                           const std::string& terminal_report, ReportFormat format) {
    std::string out;
    out += heading("ckVision SysInfo report", format, 1);
    // No date. This program has no business inventing one, and the report
    // is about a machine rather than about a moment -- except for the
    // measurements, which say so themselves below.
    out += "Every figure below was read or measured by this program on the machine it ran on.\n";

    out += heading("System", format, 2);
    out += table_header({"Field", "Value"}, format);
    for (const std::vector<std::string>& row : system_rows(probe))
        out += field_line(row.at(0), row.at(1), format);

    out += heading("Memory", format, 2);
    const MemoryReport memory = probe.memory();
    out += (is_markdown(format) ? "" : memory_usage_text(memory) + "\n\n");
    out += table_header({"Category", "Size"}, format);
    for (const std::vector<std::string>& row : memory_rows(memory))
        out += field_line(row.at(0), row.at(1), format);
    if (is_markdown(format)) out += "\n" + memory_usage_text(memory) + "\n";

    out += heading("Volumes", format, 2);
    out += table_header({"Mounted on", "Filesystem", "Capacity", "Free", "Used"}, format);
    for (const std::vector<std::string>& row : volume_rows(probe.volumes())) out += row_line(row, format);

    if (!terminal_report.empty()) {
        out += heading("Terminal", format, 2);
        if (is_markdown(format)) out += "```\n" + terminal_report + "```\n";
        else out += terminal_report;
    }

    out += heading("Measurements", format, 2);
    out += measurement_caveat_text(probe.build());
    out += "\n";
    if (results.empty()) {
        out += "\nNothing was measured in this session.\n";
        return out;
    }

    for (const BenchmarkResult& result : results) {
        const BenchmarkDescriptor& descriptor = describe(result.id);
        out += heading(std::string(descriptor.title), format, 3);
        out += std::string(descriptor.explanation) + "\n\n";
        if (result.series.empty()) {
            out += table_header({"Figure", "Value"}, format);
            out += field_line("Measured", result.rate_text, format);
            out += field_line("Index", result.index_text, format);
            out += field_line("Index 1.0 is", format_rate(result.id, unit_rate(result.id)), format);
            const std::vector<ReferencePoint> points = reference_points_for(result.id);
            if (!points.empty()) {
                out += "\n";
                out += table_header({"Reference", "Index", "Arithmetic", "Source"}, format);
                for (const ReferencePoint& point : points)
                    out += row_line({std::string(point.label), format_decimal(point.rate / unit_rate(result.id), 1),
                                     std::string(point.basis), std::string(point.source)},
                                    format);
                out += is_markdown(format) ? "\nReference rows are published figures or arithmetic on published "
                                             "figures. This program did not measure them.\n"
                                           : "\nReference rows are published figures or arithmetic on published "
                                             "figures. This program did not measure them.\n";
            }
            continue;
        }
        out += table_header({"Point", "Measured", "Perfect"}, format);
        for (const SeriesPoint& point : result.series)
            out += row_line({point.label, point.value_text,
                             point.ideal > 0.0 ? format_decimal(point.ideal, 2) + "x" : std::string("-")},
                            format);
    }
    return out;
}

}  // namespace ckv::sysinfo
