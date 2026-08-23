// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The measurements, and what they are measurements of.
//
// Three rules hold everywhere in this file, and they are what make the
// numbers worth putting on a screen:
//
//   * A kernel is a pure function over its own memory. It touches no view,
//     no theme and no terminal, which is why it can be run on a worker
//     thread (benchmark_service.hpp) and tested for its ANSWER rather than
//     for its speed.
//   * The scale is this program's own, and says so. The classic diagnostic
//     tools drew your machine against a table of reference machines; this
//     one does not, because a bar labelled with a computer this program
//     never ran on is a measurement claim it cannot support. What it
//     compares instead is one kernel against another on the same scale,
//     and this run against the last one.
//   * A score is a fact about a machine's afternoon — its thermal state,
//     its other processes, its power source. Nothing in the test suite
//     asserts one.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"

namespace ckv::sysinfo {

enum class BenchmarkId {
    IntegerMix,
    FloatingPoint,
    MemoryBandwidth,
    // The two that answer with a curve rather than a number. They are
    // kernels like the others -- same catalogue, same picker, same worker,
    // same cancellation -- and differ only in what they hand back.
    CacheLatency,
    ThreadScaling,
    // The one kernel that writes to the reader's machine, and so the one
    // that does nothing at all unless it has been told where.
    DiskThroughput,
};

// What a run needs to know that the kernels cannot ask for themselves: how
// many threads this machine has, and whether -- and where -- the disk
// kernel has been given permission to write. Both are facts about the
// host, so both arrive from the application rather than being read behind
// its back.
struct RunOptions {
    int maximum_threads = 1;
    // Empty means the reader has not chosen a directory, and the disk
    // kernel then measures nothing rather than picking one itself.
    std::string scratch_directory;
    // How much the disk kernel writes, so the number the application shows
    // the reader beforehand and the number it actually writes are the same
    // number.
    static constexpr std::uint64_t kDiskBytes = 64ull * 1024 * 1024;
};

struct BenchmarkDescriptor {
    BenchmarkId id = BenchmarkId::IntegerMix;
    std::string_view key;       // stable, for commands and reports
    std::string_view title;     // as a chart row names it
    std::string_view rate_unit;  // what the measured rate is counted in
    // One line, for a list of six of them. The explanation below is what a
    // reader gets when they ask about this one in particular.
    std::string_view synopsis;
    // What the number means, and what it does not. Shown as help (WP-54)
    // and written into the exported report.
    std::string_view explanation;
};

const std::vector<BenchmarkDescriptor>& benchmark_catalogue();
const BenchmarkDescriptor& describe(BenchmarkId id);

// The rate that scores 1.0 on this program's index, per kernel. Stated as
// constants rather than buried in a division: the whole honesty of the
// chart rests on the reader being able to see what the scale is. They are
// chosen so that a machine of the mid-2020s scores in the tens, which is
// the range the classic tools' charts were drawn for.
inline constexpr double kIntegerUnitRate = 1e7;   // mix steps per second
inline constexpr double kFloatingUnitRate = 1e8;  // floating-point operations per second
inline constexpr double kMemoryUnitRate = 1e9;    // bytes per second of triad traffic
inline constexpr double kDiskUnitRate = 1e8;      // bytes per second read back from a file

double unit_rate(BenchmarkId id) noexcept;

// A rate in the unit its kernel counts, spelled the way that kernel spells
// it: "48.3 M steps/s", "812 MFLOPS", "3.42 GB/s". One place, so the
// window, the report and the fixture cannot each round it differently.
std::string format_rate(BenchmarkId id, double rate);

// One point of an analysis that has no single answer: a working-set size
// and its access latency, a thread count and its speedup.
struct SeriesPoint {
    std::string label;
    double value = 0.0;
    std::string value_text;
    // The value a perfect machine would have shown here, where such a
    // thing exists (ideal scaling is exactly the thread count). Absent
    // where it does not, because most questions have no perfect answer.
    double ideal = 0.0;
};

struct BenchmarkResult {
    BenchmarkId id = BenchmarkId::IntegerMix;
    double rate = 0.0;   // in the kernel's own unit
    double index = 0.0;  // rate / unit_rate(id)
    std::string rate_text;   // "3.4 GB/s", "812 MFLOPS"
    std::string index_text;  // "34.1"
    // Empty for a kernel that measures one thing. A kernel that measures a
    // curve fills this instead, and the chart draws one bar per point.
    std::vector<SeriesPoint> series;
    // What the series is a series of, on the chart's group heading.
    std::string series_caption;
};

// --- Kernels -------------------------------------------------------------
//
// Public so their answers can be tested. Each returns something the caller
// consumes, which is also what stops a compiler from deleting the work.

// A dependent xorshift64 chain folded into a checksum. Dependent on
// purpose: each step needs the previous one's result, so the loop measures
// how fast the machine can do one thing after another rather than how many
// independent things it can start at once.
std::uint64_t integer_mix(std::uint64_t seed, std::uint64_t steps) noexcept;

// One xorshift64 step, exposed so a test can predict integer_mix's answer
// instead of recording whatever it happened to produce.
std::uint64_t xorshift64(std::uint64_t state) noexcept;

// Row-major n x n, the naive triple loop -- deliberately naive: this is a
// measurement of the machine, and a blocked or vectorised kernel would
// measure how well it was written for one cache size.
std::vector<double> multiply_matrices(const std::vector<double>& left, const std::vector<double>& right,
                                      std::size_t n);

// STREAM's triad: a = b + scalar * c, over arrays far larger than any
// cache, so what it measures is the path to memory.
void stream_triad(std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c,
                  double scalar) noexcept;

// A ring of `elements` slots, each holding the index of the next, arranged
// as one cycle through every slot in an order no prefetcher can follow.
// Built with Sattolo's algorithm, which produces a single cycle rather than
// an arbitrary permutation -- an arbitrary one would break into shorter
// cycles and the walk would revisit a handful of hot slots forever.
std::vector<std::uint32_t> make_pointer_chase(std::size_t elements, std::uint64_t seed);

// Walks the ring `steps` times from slot 0 and returns where it stopped,
// which is what stops the walk from being deleted for having no effect.
std::uint32_t walk_pointer_chase(const std::vector<std::uint32_t>& ring, std::size_t steps) noexcept;

// --- Running them --------------------------------------------------------

// The seam that lets a test drive the run loop without waiting for real
// work: BenchmarkService owns a BenchmarkRunner& and knows nothing about
// what running a kernel involves.
class BenchmarkRunner {
public:
    virtual ~BenchmarkRunner() = default;

    // Runs one kernel and returns its result. `cancelled` may become true
    // at any moment; a long kernel is expected to look at it and return
    // early, and what it returns then is ignored.
    virtual BenchmarkResult run(BenchmarkId id, const RunOptions& options,
                                const std::atomic<bool>& cancelled) const = 0;
};

// The real thing. Takes the injected clock rather than reading one: the
// application boundary owns time here as everywhere else, and a benchmark
// that reached for a global clock would be the one place in this example
// that broke its own rule.
class MeasuredBenchmarkRunner final : public BenchmarkRunner {
public:
    explicit MeasuredBenchmarkRunner(const Clock& clock) noexcept : clock_(clock) {}

    BenchmarkResult run(BenchmarkId id, const RunOptions& options,
                        const std::atomic<bool>& cancelled) const override;

    // Each kernel is run this many times and the FASTEST pass is reported.
    // Not the mean: the slow passes are the ones that were interrupted by
    // something else on the machine, and averaging them in measures the
    // interruption. The fastest pass is the closest this machine came to
    // doing only this.
    static constexpr int kPasses = 3;

private:
    BenchmarkResult run_cache_latency(const std::atomic<bool>& cancelled) const;
    BenchmarkResult run_thread_scaling(const RunOptions& options, const std::atomic<bool>& cancelled) const;
    BenchmarkResult run_disk_throughput(const RunOptions& options, const std::atomic<bool>& cancelled) const;

    const Clock& clock_;
};

}  // namespace ckv::sysinfo
