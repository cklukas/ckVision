// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "benchmark.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <thread>
#include <utility>

#include "report_format.hpp"

namespace ckv::sysinfo {
namespace {

// Work quanta, chosen so one pass takes a few tens of milliseconds on a
// machine of this era: long enough that the clock's resolution is
// irrelevant, short enough that Esc feels immediate. A slower machine
// takes longer rather than measuring less, which is the right way round.
constexpr std::uint64_t kIntegerSteps = 20'000'000;
constexpr std::size_t kMatrixOrder = 256;
constexpr std::size_t kStreamElements = 4'000'000;  // 32 MiB per array, three arrays

constexpr double kNanosPerSecond = 1e9;

// The working sets the latency walk steps through: from inside the
// smallest first-level cache to far outside the largest last-level one,
// four times bigger each step, so every boundary the machine has falls
// between two of them and shows as a step in the chart.
constexpr std::size_t kLatencySets[] = {
    4u * 1024, 16u * 1024, 64u * 1024, 256u * 1024, 1024u * 1024, 4u * 1024 * 1024,
    16u * 1024 * 1024, 64u * 1024 * 1024,
};
constexpr std::size_t kLatencySteps = 2'000'000;

std::vector<double> seeded_matrix(std::size_t n, double offset) {
    std::vector<double> matrix(n * n);
    for (std::size_t index = 0; index < matrix.size(); ++index)
        matrix[index] = static_cast<double>((index * 7 + 3) % 13) + offset;
    return matrix;
}

std::string rate_text_for(BenchmarkId id, double rate) {
    switch (id) {
        case BenchmarkId::IntegerMix: return format_decimal(rate / 1e6, 1) + " M steps/s";
        case BenchmarkId::FloatingPoint: return format_decimal(rate / 1e6, 0) + " MFLOPS";
        case BenchmarkId::MemoryBandwidth: return format_decimal(rate / 1e9, 2) + " GB/s";
        // The two that answer with a curve have no single rate to spell.
        case BenchmarkId::CacheLatency:
        case BenchmarkId::ThreadScaling: break;
    }
    return std::string(kNotReported);
}

}  // namespace

const std::vector<BenchmarkDescriptor>& benchmark_catalogue() {
    static const std::vector<BenchmarkDescriptor> catalogue = {
        {BenchmarkId::IntegerMix, "integer", "Integer mix", "mix steps per second",
         "A dependent chain of shifts and folds: how fast this machine does one "
         "small thing after another. It says nothing about how many cores you have "
         "-- it uses one -- and little about wide vector work."},
        {BenchmarkId::FloatingPoint, "float", "Floating point", "floating-point operations per second",
         "A naive 256x256 double-precision matrix multiply. Deliberately naive: a "
         "tuned kernel would measure how well it was written for one cache size "
         "rather than what the machine can do."},
        {BenchmarkId::MemoryBandwidth, "memory", "Memory bandwidth", "bytes per second",
         "STREAM's triad over arrays far larger than any cache, so what it measures "
         "is the road to memory rather than the caches beside it. It allocates "
         "about 96 MiB while it runs."},
        {BenchmarkId::CacheLatency, "latency", "Cache latency", "nanoseconds per access",
         "A dependent walk through working sets from 4 KiB to 64 MiB, in an order no "
         "prefetcher can follow. The steps in the chart are this machine's cache "
         "boundaries: each one is a level the walk has just fallen out of."},
        {BenchmarkId::ThreadScaling, "scaling", "Thread scaling", "speedup over one thread",
         "The integer kernel on one thread, then two, then four, up to this machine's "
         "core count, each doing the same work per thread. Where the measured bar "
         "falls short of the shaded one is where the cores stopped being independent."},
    };
    return catalogue;
}

const BenchmarkDescriptor& describe(BenchmarkId id) {
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue())
        if (descriptor.id == id) return descriptor;
    return benchmark_catalogue().front();
}

double unit_rate(BenchmarkId id) noexcept {
    switch (id) {
        case BenchmarkId::IntegerMix: return kIntegerUnitRate;
        case BenchmarkId::FloatingPoint: return kFloatingUnitRate;
        case BenchmarkId::MemoryBandwidth: return kMemoryUnitRate;
        // A curve is not on the index scale: there is no single rate to
        // divide, and a 1.0 here would put a meaningless bar on the chart.
        case BenchmarkId::CacheLatency:
        case BenchmarkId::ThreadScaling: break;
    }
    return 1.0;
}

std::uint64_t xorshift64(std::uint64_t state) noexcept {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

std::uint64_t integer_mix(std::uint64_t seed, std::uint64_t steps) noexcept {
    std::uint64_t state = seed;
    std::uint64_t checksum = 0;
    for (std::uint64_t step = 0; step < steps; ++step) {
        state = xorshift64(state);
        // The FNV-1a prime: a fold that carries every bit of the state into
        // the checksum, so nothing in the chain can be skipped.
        checksum = (checksum * 0x100000001b3ULL) ^ state;
    }
    return checksum;
}

std::vector<std::uint32_t> make_pointer_chase(std::size_t elements, std::uint64_t seed) {
    // A ring of fewer than two slots has no cycle to walk; it is not a
    // degenerate chase, it is not a chase.
    if (elements < 2) return std::vector<std::uint32_t>(elements, 0);
    std::vector<std::uint32_t> ring(elements);
    for (std::size_t index = 0; index < elements; ++index) ring[index] = static_cast<std::uint32_t>(index);
    // Sattolo's algorithm: the swap partner is drawn from BELOW the current
    // position, never equal to it, which is exactly what makes the result
    // one cycle through every slot instead of several short ones.
    std::uint64_t state = seed == 0 ? 1 : seed;
    for (std::size_t index = elements - 1; index > 0; --index) {
        state = xorshift64(state);
        const std::size_t partner = static_cast<std::size_t>(state % index);
        std::swap(ring[index], ring[partner]);
    }
    // ring[i] currently holds a permutation; turn it into "the slot after
    // i" by following the permutation's own cycle.
    std::vector<std::uint32_t> next(elements);
    for (std::size_t index = 0; index + 1 < elements; ++index)
        next[ring[index]] = ring[index + 1];
    if (elements > 0) next[ring[elements - 1]] = ring[0];
    return next;
}

std::uint32_t walk_pointer_chase(const std::vector<std::uint32_t>& ring, std::size_t steps) noexcept {
    if (ring.empty()) return 0;
    std::uint32_t at = 0;
    for (std::size_t step = 0; step < steps; ++step) at = ring[at];
    return at;
}

std::vector<double> multiply_matrices(const std::vector<double>& left, const std::vector<double>& right,
                                      std::size_t n) {
    std::vector<double> product(n * n, 0.0);
    for (std::size_t row = 0; row < n; ++row) {
        for (std::size_t inner = 0; inner < n; ++inner) {
            const double scale = left[row * n + inner];
            for (std::size_t column = 0; column < n; ++column)
                product[row * n + column] += scale * right[inner * n + column];
        }
    }
    return product;
}

void stream_triad(std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c,
                  double scalar) noexcept {
    const std::size_t count = std::min(a.size(), std::min(b.size(), c.size()));
    for (std::size_t index = 0; index < count; ++index) a[index] = b[index] + scalar * c[index];
}

void MeasuredBenchmarkRunner::set_maximum_threads(int threads) noexcept {
    maximum_threads_ = std::max(1, threads);
}

BenchmarkResult MeasuredBenchmarkRunner::run(BenchmarkId id, const std::atomic<bool>& cancelled) const {
    if (id == BenchmarkId::CacheLatency) return run_cache_latency(cancelled);
    if (id == BenchmarkId::ThreadScaling) return run_thread_scaling(cancelled);

    BenchmarkResult result;
    result.id = id;

    // Allocated once, outside the timed passes: this measures the kernel,
    // not the allocator.
    std::vector<double> left;
    std::vector<double> right;
    std::vector<double> a;
    std::vector<double> b;
    std::vector<double> c;
    double work_per_pass = 0.0;
    switch (id) {
        case BenchmarkId::IntegerMix:
            work_per_pass = static_cast<double>(kIntegerSteps);
            break;
        case BenchmarkId::FloatingPoint:
            left = seeded_matrix(kMatrixOrder, 1.0);
            right = seeded_matrix(kMatrixOrder, 2.0);
            // Every output element costs one multiply and one add.
            work_per_pass = 2.0 * static_cast<double>(kMatrixOrder) * static_cast<double>(kMatrixOrder) *
                            static_cast<double>(kMatrixOrder);
            break;
        case BenchmarkId::MemoryBandwidth:
            a.assign(kStreamElements, 0.0);
            b.assign(kStreamElements, 1.0);
            c.assign(kStreamElements, 2.0);
            // Two arrays read and one written, whatever the cache does in
            // between; this is the traffic the kernel asks for.
            work_per_pass = 3.0 * static_cast<double>(kStreamElements) * static_cast<double>(sizeof(double));
            break;
        // Handled above, before any of this was set up.
        case BenchmarkId::CacheLatency:
        case BenchmarkId::ThreadScaling: break;
    }

    // A warm-up pass, not timed: the first touch of a fresh allocation is a
    // page fault per page, and timing that would measure the operating
    // system's memory manager under the name of this machine's memory.
    std::uint64_t sink = 0;
    double checksum = 0.0;
    const auto one_pass = [&] {
        switch (id) {
            case BenchmarkId::IntegerMix:
                sink ^= integer_mix(0x9e3779b97f4a7c15ULL ^ sink, kIntegerSteps);
                break;
            case BenchmarkId::FloatingPoint: {
                const std::vector<double> product = multiply_matrices(left, right, kMatrixOrder);
                checksum += product.front() + product.back();
                break;
            }
            case BenchmarkId::MemoryBandwidth:
                stream_triad(a, b, c, 3.0);
                checksum += a.front() + a.back();
                break;
            case BenchmarkId::CacheLatency:
            case BenchmarkId::ThreadScaling: break;
        }
    };
    one_pass();

    std::int64_t best_nanos = std::numeric_limits<std::int64_t>::max();
    for (int pass = 0; pass < kPasses; ++pass) {
        if (cancelled.load(std::memory_order_relaxed)) return result;
        const std::int64_t start = clock_.now_nanos();
        one_pass();
        const std::int64_t elapsed = clock_.now_nanos() - start;
        if (elapsed > 0) best_nanos = std::min(best_nanos, elapsed);
    }

    // A clock that never moved measures nothing, and a rate divided out of
    // zero elapsed time is the kind of number this program exists not to
    // print.
    if (best_nanos == std::numeric_limits<std::int64_t>::max()) return result;

    result.rate = work_per_pass * kNanosPerSecond / static_cast<double>(best_nanos);
    result.index = result.rate / unit_rate(id);
    result.rate_text = rate_text_for(id, result.rate);
    result.index_text = format_decimal(result.index, 1);

    // The two sinks exist so no pass can be deleted for having no effect.
    // Consuming them here costs nothing measurable and cannot be elided.
    if (sink == 1 && checksum == 1.0) result.rate += 0.0;
    return result;
}

BenchmarkResult MeasuredBenchmarkRunner::run_cache_latency(const std::atomic<bool>& cancelled) const {
    BenchmarkResult result;
    result.id = BenchmarkId::CacheLatency;
    result.series_caption = "nanoseconds per dependent access - shorter is better";

    std::uint32_t sink = 0;
    for (const std::size_t bytes : kLatencySets) {
        if (cancelled.load(std::memory_order_relaxed)) return result;
        const std::size_t elements = bytes / sizeof(std::uint32_t);
        const std::vector<std::uint32_t> ring = make_pointer_chase(elements, 0x2545f4914f6cdd1dULL);

        // Warmed, then timed: the first walk pays for page faults and for
        // filling the cache being measured, which is the opposite of what
        // this measures.
        sink ^= walk_pointer_chase(ring, std::min<std::size_t>(kLatencySteps, elements * 4));
        std::int64_t best_nanos = std::numeric_limits<std::int64_t>::max();
        for (int pass = 0; pass < kPasses; ++pass) {
            if (cancelled.load(std::memory_order_relaxed)) return result;
            const std::int64_t start = clock_.now_nanos();
            sink ^= walk_pointer_chase(ring, kLatencySteps);
            const std::int64_t elapsed = clock_.now_nanos() - start;
            if (elapsed > 0) best_nanos = std::min(best_nanos, elapsed);
        }
        if (best_nanos == std::numeric_limits<std::int64_t>::max()) continue;

        const double nanos_per_access = static_cast<double>(best_nanos) / static_cast<double>(kLatencySteps);
        result.series.push_back(
            SeriesPoint{format_bytes(bytes), nanos_per_access, format_decimal(nanos_per_access, 1) + " ns", 0.0});
    }

    if (!result.series.empty())
        result.rate_text = result.series.front().value_text + " at " + result.series.front().label + ", " +
                           result.series.back().value_text + " at " + result.series.back().label;
    if (sink == 0xFFFFFFFFu) result.rate += 0.0;  // the walk's result, consumed
    return result;
}

BenchmarkResult MeasuredBenchmarkRunner::run_thread_scaling(const std::atomic<bool>& cancelled) const {
    BenchmarkResult result;
    result.id = BenchmarkId::ThreadScaling;
    result.series_caption = "speedup over one thread - shaded is perfect scaling";

    // Thread counts that double: 1, 2, 4, ... up to the machine's own, with
    // the machine's own always included even when it is not a power of two.
    std::vector<int> counts;
    for (int threads = 1; threads < maximum_threads_; threads *= 2) counts.push_back(threads);
    counts.push_back(maximum_threads_);

    // A quantum per thread, so every configuration does the same work per
    // thread and the comparison is of time, not of arithmetic.
    constexpr std::uint64_t kStepsPerThread = 8'000'000;
    double single_thread_nanos = 0.0;
    for (const int threads : counts) {
        if (cancelled.load(std::memory_order_relaxed)) return result;
        std::int64_t best_nanos = std::numeric_limits<std::int64_t>::max();
        for (int pass = 0; pass < kPasses; ++pass) {
            if (cancelled.load(std::memory_order_relaxed)) return result;
            std::vector<std::uint64_t> sinks(static_cast<std::size_t>(threads), 0);
            const std::int64_t start = clock_.now_nanos();
            {
                std::vector<std::thread> workers;
                workers.reserve(static_cast<std::size_t>(threads) - 1);
                for (int worker = 1; worker < threads; ++worker)
                    workers.emplace_back([&sinks, worker] {
                        sinks[static_cast<std::size_t>(worker)] =
                            integer_mix(0x9e3779b97f4a7c15ULL + static_cast<std::uint64_t>(worker),
                                        kStepsPerThread);
                    });
                sinks[0] = integer_mix(0x9e3779b97f4a7c15ULL, kStepsPerThread);
                for (std::thread& worker : workers) worker.join();
            }
            const std::int64_t elapsed = clock_.now_nanos() - start;
            if (elapsed > 0) best_nanos = std::min(best_nanos, elapsed);
            if (sinks[0] == 1) best_nanos += 0;  // the work's result, consumed
        }
        if (best_nanos == std::numeric_limits<std::int64_t>::max()) continue;

        // Work scales with the thread count, so the same wall-clock time
        // for twice the threads is a speedup of two.
        const double nanos = static_cast<double>(best_nanos);
        if (single_thread_nanos == 0.0) single_thread_nanos = nanos;
        const double speedup = single_thread_nanos * static_cast<double>(threads) / nanos;
        // One thread is the baseline, so its "perfect" bar would be the
        // same bar drawn twice.
        result.series.push_back(SeriesPoint{std::to_string(threads) + (threads == 1 ? " thread" : " threads"),
                                            speedup, format_decimal(speedup, 2) + "x",
                                            threads == 1 ? 0.0 : static_cast<double>(threads)});
    }

    if (!result.series.empty())
        result.rate_text = result.series.back().value_text + " on " + result.series.back().label;
    return result;
}

}  // namespace ckv::sysinfo
