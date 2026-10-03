// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The pinned-host full-screen theme-switch limit in the architecture §8.
// Timing starts after set_theme() has invalidated the tree and ends after
// Application::step() has repainted, composed, diffed and emitted one frame.
// The terminal sink deliberately does no PTY I/O or virtual-display parsing.
#include <sys/utsname.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/version.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

#ifndef CKV_THEME_SWITCH_BUILD_TYPE
#define CKV_THEME_SWITCH_BUILD_TYPE "unknown"
#endif

namespace {

using Clock = std::chrono::steady_clock;
constexpr ckv::Size kGrid{200, 60};
constexpr std::int64_t kLimitNanos = 5'000'000;
constexpr double kBaselineTolerance = 0.05;

class FrameSink final : public ckv::term::Terminal {
public:
    ckv::term::Capabilities capabilities() const noexcept override {
        ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
        caps.color_depth = ckv::term::ColorDepth::TrueColor;
        return caps;
    }
    ckv::Size size() const noexcept override { return kGrid; }
    std::vector<ckv::term::TerminalEvent> poll(std::int64_t) override { return {}; }
    void write(std::string_view bytes) override {
        ++writes_;
        bytes_ += bytes.size();
    }
    void set_title(std::string_view) override {}
    void bell() override {}
    void write_clipboard(std::string_view) override {}

    void reset() noexcept {
        writes_ = 0;
        bytes_ = 0;
    }
    std::size_t writes() const noexcept { return writes_; }
    std::size_t bytes() const noexcept { return bytes_; }

private:
    std::size_t writes_ = 0;
    std::size_t bytes_ = 0;
};

struct Options {
    std::size_t iterations = 1'000;
    std::size_t warmup = 40;
    std::string output;
    std::string commit = "unknown";
    std::int64_t baseline_p99 = 0;
    bool verify_only = false;
};

std::optional<std::int64_t> positive_number(std::string_view text) {
    std::int64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value <= 0)
        return std::nullopt;
    return value;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view flag = argv[index];
        if (flag == "--verify-only") {
            options.verify_only = true;
            continue;
        }
        if (++index >= argc) return std::nullopt;
        const std::string_view value = argv[index];
        if (flag == "--iterations" || flag == "--warmup" || flag == "--baseline-p99-ns") {
            const std::optional<std::int64_t> number = positive_number(value);
            if (!number) return std::nullopt;
            if (flag == "--iterations") options.iterations = static_cast<std::size_t>(*number);
            if (flag == "--warmup") options.warmup = static_cast<std::size_t>(*number);
            if (flag == "--baseline-p99-ns") options.baseline_p99 = *number;
        } else if (flag == "--output") {
            options.output = value;
        } else if (flag == "--commit") {
            options.commit = value;
        } else {
            return std::nullopt;
        }
    }
    if (!options.verify_only && options.output.empty()) return std::nullopt;
    return options;
}

std::string json_string(std::string_view value) {
    std::string escaped = "\"";
    for (const unsigned char byte : value) {
        if (byte == '"' || byte == '\\') escaped += '\\';
        if (byte < 0x20) {
            char control[7];
            std::snprintf(control, sizeof control, "\\u%04x", static_cast<unsigned>(byte));
            escaped += control;
        } else {
            escaped += static_cast<char>(byte);
        }
    }
    return escaped + '"';
}

std::int64_t percentile(const std::vector<std::int64_t>& sorted, std::size_t percent) {
    const std::size_t rank = (sorted.size() * percent + 99) / 100;
    return sorted[std::max<std::size_t>(rank, 1) - 1];
}

struct HostLoad {
    std::array<double, 3> averages{};
    bool known = false;
};

HostLoad read_load() {
    HostLoad load;
    load.known = ::getloadavg(load.averages.data(), 3) == 3;
    return load;
}

void write_load_json(std::FILE* file, const HostLoad& load) {
    if (load.known)
        std::fprintf(file, "[%.2f, %.2f, %.2f]", load.averages[0], load.averages[1], load.averages[2]);
    else
        std::fputs("null", file);
}

}  // namespace

int main(int argc, char** argv) {
    const std::optional<Options> parsed = parse_options(argc, argv);
    if (!parsed) {
        std::fputs("usage: cvision_theme_switch_latency [--verify-only] [--iterations N] [--warmup N]\n"
                   "                                    --output FILE [--commit ID] [--baseline-p99-ns N]\n",
                   stderr);
        return 2;
    }
    const Options& options = *parsed;

    FrameSink terminal;
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::ApplicationShell shell(app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles),
                                                .always_dock_status_line = true});
    auto editor = std::make_unique<ckv::widgets::Window>("Editor");
    editor->set_bounds(ckv::Rect{4, 3, 128, 38});
    editor->set_content(std::make_unique<ckv::widgets::StaticText>(
        "A full-screen theme change repaints the desktop, its chrome and both retained windows."));
    shell.desktop().add_window(std::move(editor));
    auto notes = std::make_unique<ckv::widgets::Window>("Notes");
    notes->set_bounds(ckv::Rect{72, 21, 119, 34});
    notes->set_content(std::make_unique<ckv::widgets::StaticText>(
        "The second window overlaps the first and casts a shadow across its content."));
    shell.desktop().add_window(std::move(notes));

    const ckv::ui::Theme classic = ckv::ui::make_classic_theme(app.roles(), roles);
    const ckv::ui::Theme dark = ckv::ui::make_dark_theme(app.roles(), roles);
    if (classic.resolve(roles.desktop_background) == dark.resolve(roles.desktop_background)) {
        std::fputs("theme switch: the desktop background does not change between schemes\n", stderr);
        return 3;
    }
    app.step(0);  // establish the initial frame and Presenter history

    const std::size_t warmup = options.verify_only ? 0 : options.warmup;
    const std::size_t iterations = options.verify_only ? 4 : options.iterations;
    const HostLoad load_before = read_load();
    std::vector<std::int64_t> samples;
    samples.reserve(iterations);
    std::uint64_t bytes = 0;
    for (std::size_t index = 0; index < warmup + iterations; ++index) {
        app.set_theme(index % 2 == 0 ? dark : classic);
        terminal.reset();
        const Clock::time_point start = Clock::now();
        app.step(0);
        const Clock::time_point stop = Clock::now();
        if (terminal.writes() != 1 || terminal.bytes() == 0 ||
            app.last_compose_cells_touched() < static_cast<std::size_t>(kGrid.width * kGrid.height)) {
            std::fprintf(stderr,
                         "theme switch: iteration %zu did not recompose the full %dx%d frame and emit one diff "
                         "(writes=%zu, bytes=%zu, cells=%zu)\n",
                         index, kGrid.width, kGrid.height, terminal.writes(), terminal.bytes(),
                         app.last_compose_cells_touched());
            return 3;
        }
        if (index < warmup) continue;
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
        bytes += terminal.bytes();
    }
    if (options.verify_only) {
        std::printf("theme switch: %zu full-frame dark/classic transitions emitted one diff each\n", iterations);
        return 0;
    }

    std::sort(samples.begin(), samples.end());
    const std::int64_t p50 = percentile(samples, 50);
    const std::int64_t p90 = percentile(samples, 90);
    const std::int64_t p99 = percentile(samples, 99);
    const std::int64_t maximum = samples.back();
    const bool within_limit = maximum < kLimitNanos;
    const bool within_baseline = options.baseline_p99 == 0 ||
                                 static_cast<double>(p99) <=
                                     static_cast<double>(options.baseline_p99) * (1.0 + kBaselineTolerance);
    const HostLoad load_after = read_load();
    utsname host{};
    (void)::uname(&host);

    std::printf("theme switch: %zu measured full-frame transitions (+%zu warm-up), %dx%d, %s build\n",
                samples.size(), warmup, kGrid.width, kGrid.height, CKV_THEME_SWITCH_BUILD_TYPE);
    std::printf("p50 %.3f ms, p90 %.3f ms, p99 %.3f ms, max %.3f ms; %.0f bytes/frame\n",
                static_cast<double>(p50) / 1e6, static_cast<double>(p90) / 1e6,
                static_cast<double>(p99) / 1e6, static_cast<double>(maximum) / 1e6,
                static_cast<double>(bytes) / static_cast<double>(samples.size()));
    if (load_before.known && load_after.known)
        std::printf("load average before %.2f %.2f %.2f, after %.2f %.2f %.2f\n",
                    load_before.averages[0], load_before.averages[1], load_before.averages[2],
                    load_after.averages[0], load_after.averages[1], load_after.averages[2]);
    std::printf("max against 5 ms: %s; p99 against baseline: %s\n", within_limit ? "within" : "EXCEEDED",
                within_baseline ? "within" : "REGRESSED");

    std::FILE* const file = std::fopen(options.output.c_str(), "w");
    if (file == nullptr) {
        std::perror(options.output.c_str());
        return 3;
    }
    std::fprintf(file,
                 "{\n  \"tool\": \"cvision_theme_switch_latency\",\n  \"schema\": 1,\n"
                 "  \"ckvision_version\": %s,\n  \"commit\": %s,\n  \"build_type\": %s,\n"
                 "  \"compiler\": %s,\n  \"host\": {\"sysname\": %s, \"release\": %s, \"machine\": %s},\n",
                 json_string(ckv::version_string()).c_str(), json_string(options.commit).c_str(),
                 json_string(CKV_THEME_SWITCH_BUILD_TYPE).c_str(), json_string(__VERSION__).c_str(),
                 json_string(host.sysname).c_str(), json_string(host.release).c_str(), json_string(host.machine).c_str());
    std::fputs("  \"load_average_before\": ", file);
    write_load_json(file, load_before);
    std::fputs(",\n  \"load_average_after\": ", file);
    write_load_json(file, load_after);
    std::fputs(",\n", file);
    std::fprintf(file,
                 "  \"grid\": [%d, %d],\n  \"warmup\": %zu,\n  \"iterations\": %zu,\n"
                 "  \"p50_ns\": %lld,\n  \"p90_ns\": %lld,\n  \"p99_ns\": %lld,\n  \"max_ns\": %lld,\n"
                 "  \"mean_frame_bytes\": %.1f,\n  \"limit_max_ns\": %lld,\n  \"max_within_limit\": %s,\n",
                 kGrid.width, kGrid.height, warmup, samples.size(), static_cast<long long>(p50),
                 static_cast<long long>(p90), static_cast<long long>(p99), static_cast<long long>(maximum),
                 static_cast<double>(bytes) / static_cast<double>(samples.size()), static_cast<long long>(kLimitNanos),
                 within_limit ? "true" : "false");
    if (options.baseline_p99 != 0)
        std::fprintf(file, "  \"baseline_p99_ns\": %lld,\n  \"p99_within_baseline\": %s\n}\n",
                     static_cast<long long>(options.baseline_p99), within_baseline ? "true" : "false");
    else
        std::fputs("  \"baseline_p99_ns\": null,\n  \"p99_within_baseline\": null\n}\n", file);
    std::fclose(file);
    return within_limit && within_baseline ? 0 : 1;
}
