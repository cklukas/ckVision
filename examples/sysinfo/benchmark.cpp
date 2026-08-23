// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "benchmark.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

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

BenchmarkResult MeasuredBenchmarkRunner::run(BenchmarkId id, const std::atomic<bool>& cancelled) const {
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

}  // namespace ckv::sysinfo
