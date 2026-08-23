// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's kernels and the thread that runs them.
//
// Not one assertion here is about how long anything took. A benchmark
// score is a fact about a machine's afternoon; what a test can hold the
// program to is that the kernels compute the right answer, that a run
// reports what it did, and that a cancel stops it. The last of those is
// proven by holding a scripted kernel at a known point rather than by
// sleeping and hoping, which is the difference between a test of
// cancellation and a test of this machine's scheduler.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"

#include "benchmark.hpp"
#include "benchmark_service.hpp"
#include "fixed_benchmark_runner.hpp"

using ckv::ManualClock;
using ckv::sysinfo::BenchmarkId;
using ckv::sysinfo::BenchmarkResult;
using ckv::sysinfo::BenchmarkService;
using ckv::sysinfo::FixedBenchmarkRunner;
using ckv::sysinfo::integer_mix;
using ckv::sysinfo::multiply_matrices;
using ckv::sysinfo::stream_triad;
using ckv::sysinfo::xorshift64;
using ckv::ui::Application;

namespace {

constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

struct Harness {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    FixedBenchmarkRunner runner;
    BenchmarkService service{app, runner};

    std::shared_ptr<int> token = std::make_shared<int>(0);
    std::vector<BenchmarkResult> results;
    std::vector<BenchmarkService::Progress> progress;
    int finished = 0;
    bool cancelled = false;

    ckv::sysinfo::RunOptions options;

    bool start(std::vector<BenchmarkId> plan) {
        return service.start(std::move(plan), options, std::weak_ptr<void>(token),
                             [this](BenchmarkService::Progress step) { progress.push_back(step); },
                             [this](BenchmarkResult result) { results.push_back(result); },
                             [this](bool was_cancelled) {
                                 ++finished;
                                 cancelled = was_cancelled;
                             });
    }

    // Posted work is delivered by the run loop, not by the worker; this is
    // the loop, standing still.
    void drain() {
        service.wait_until_idle();
        for (int turn = 0; turn < 8; ++turn) app.step(0);
    }
};

}  // namespace

CK_TEST(the_integer_kernel_computes_the_chain_a_reader_can_work_out_by_hand) {
    // Zero steps is zero work and an untouched checksum.
    CK_CHECK(integer_mix(12345, 0) == 0);

    // One step is one xorshift, folded into an empty checksum.
    const std::uint64_t first = xorshift64(12345);
    CK_CHECK(integer_mix(12345, 1) == first);

    // Two steps is the fold, which is what makes the chain dependent: the
    // second state cannot be computed before the first.
    const std::uint64_t second = xorshift64(first);
    CK_CHECK(integer_mix(12345, 2) == ((first * kFnvPrime) ^ second));

    CK_CHECK(integer_mix(999, 1000) == integer_mix(999, 1000));
    CK_CHECK(integer_mix(999, 1000) != integer_mix(1000, 1000));

    // xorshift64's fixed point, and why the measured runner seeds with a
    // constant that is not it: from zero the chain never leaves zero, and
    // the loop would measure a machine doing nothing.
    CK_CHECK(xorshift64(0) == 0);
    CK_CHECK(integer_mix(0, 1'000) == 0);
}

CK_TEST(the_matrix_kernel_multiplies_matrices) {
    // Identity: the product is the other operand, entry for entry.
    const std::vector<double> identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
    const std::vector<double> value{1, 2, 3, 4, 5, 6, 7, 8, 9};
    CK_CHECK(multiply_matrices(identity, value, 3) == value);
    CK_CHECK(multiply_matrices(value, identity, 3) == value);

    // Ones times ones: every entry is n, whatever n is.
    const std::vector<double> ones(16, 1.0);
    const std::vector<double> product = multiply_matrices(ones, ones, 4);
    CK_CHECK(product.size() == 16);
    for (const double entry : product) CK_CHECK(entry == 4.0);

    // And one worked by hand, because identity and ones are the two cases
    // a transposed index would still pass.
    const std::vector<double> left{1, 2, 3, 4};
    const std::vector<double> right{5, 6, 7, 8};
    const std::vector<double> expected{1 * 5 + 2 * 7, 1 * 6 + 2 * 8, 3 * 5 + 4 * 7, 3 * 6 + 4 * 8};
    CK_CHECK(multiply_matrices(left, right, 2) == expected);
}

CK_TEST(the_triad_kernel_computes_b_plus_scalar_times_c) {
    std::vector<double> a(4, 0.0);
    const std::vector<double> b{1.0, 2.0, 3.0, 4.0};
    const std::vector<double> c{10.0, 20.0, 30.0, 40.0};
    stream_triad(a, b, c, 3.0);
    CK_CHECK((a == std::vector<double>{31.0, 62.0, 93.0, 124.0}));

    // A shorter operand bounds the loop rather than running off the end.
    std::vector<double> wide(6, -1.0);
    stream_triad(wide, b, c, 1.0);
    CK_CHECK(wide[3] == 44.0);
    CK_CHECK(wide[4] == -1.0);
}

CK_TEST(a_run_delivers_one_result_per_kernel_and_says_when_it_is_done) {
    Harness h;
    CK_CHECK(h.start({BenchmarkId::IntegerMix, BenchmarkId::FloatingPoint, BenchmarkId::MemoryBandwidth}));
    h.drain();

    CK_CHECK(h.results.size() == 3);
    CK_CHECK(h.results[0].id == BenchmarkId::IntegerMix);
    CK_CHECK(h.results[2].id == BenchmarkId::MemoryBandwidth);
    CK_CHECK(h.finished == 1);
    CK_CHECK(!h.cancelled);
    CK_CHECK(!h.service.running());

    // Progress never goes backwards and ends at the whole plan.
    CK_CHECK(!h.progress.empty());
    std::size_t seen = 0;
    for (const BenchmarkService::Progress& step : h.progress) {
        CK_CHECK(step.total == 3);
        CK_CHECK(step.completed >= seen);
        seen = step.completed;
    }
    CK_CHECK(seen == 3);
}

CK_TEST(a_cancelled_run_stops_between_kernels_and_charts_nothing_it_did_not_finish) {
    Harness h;
    h.runner.hold();
    CK_CHECK(h.start({BenchmarkId::IntegerMix, BenchmarkId::FloatingPoint, BenchmarkId::MemoryBandwidth}));

    // The worker is now parked inside the first kernel, and will still be
    // there when cancel() arrives -- no timing involved.
    h.runner.wait_until_entered(1);
    h.service.cancel();
    h.runner.release();
    h.drain();

    CK_CHECK(h.runner.runs() == 1);  // the second kernel never started
    CK_CHECK(h.results.empty());     // and the interrupted one is not a measurement
    CK_CHECK(h.finished == 1);
    CK_CHECK(h.cancelled);
}

CK_TEST(a_second_run_is_refused_while_one_is_measuring) {
    Harness h;
    h.runner.hold();
    CK_CHECK(h.start({BenchmarkId::IntegerMix, BenchmarkId::FloatingPoint}));
    h.runner.wait_until_entered(1);

    // Two runs at once would measure each other.
    CK_CHECK(!h.start({BenchmarkId::MemoryBandwidth}));
    CK_CHECK(h.service.running());

    h.runner.release();
    h.drain();
    CK_CHECK(h.results.size() == 2);
}

CK_TEST(an_empty_plan_is_not_a_run) {
    Harness h;
    CK_CHECK(!h.start({}));
    CK_CHECK(!h.service.running());
    CK_CHECK(h.finished == 0);
}

CK_TEST(a_subscriber_that_died_mid_run_receives_nothing) {
    Harness h;
    h.runner.hold();
    CK_CHECK(h.start({BenchmarkId::IntegerMix, BenchmarkId::FloatingPoint}));
    h.runner.wait_until_entered(1);

    // The window closed while the machine was measuring for it.
    h.token.reset();
    h.runner.release();
    h.drain();

    CK_CHECK(h.results.empty());
    CK_CHECK(h.progress.empty());
    CK_CHECK(h.finished == 0);
    // The run still ended cleanly; nobody was told, because nobody was
    // there to tell.
    CK_CHECK(!h.service.running());
}

// The kernels are handed what they cannot ask for themselves. A test that
// did not check this would let the scaling run silently fall back to one
// thread, and the disk kernel to a directory nobody chose.
CK_TEST(the_run_options_reach_the_kernel_that_needs_them) {
    Harness h;
    h.options.maximum_threads = 6;
    h.options.scratch_directory = "/scratch";
    CK_CHECK(h.start({BenchmarkId::ThreadScaling}));
    h.drain();

    CK_CHECK(h.runner.last_options().maximum_threads == 6);
    CK_CHECK(h.runner.last_options().scratch_directory == "/scratch");
}

CK_TEST(the_disk_kernel_without_a_directory_measures_nothing_rather_than_choosing_one) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::sysinfo::MeasuredBenchmarkRunner runner(clock);
    std::atomic<bool> cancelled{false};

    ckv::sysinfo::RunOptions options;  // no scratch directory
    const BenchmarkResult result = runner.run(BenchmarkId::DiskThroughput, options, cancelled);
    CK_CHECK(result.rate == 0.0);
    CK_CHECK(result.rate_text == "no directory chosen");
    // And it is the only kernel that could have written anything, so this
    // suite writes nothing to the machine running it.
}

// Every catalogue entry carries both a one-line synopsis and the longer
// explanation, and they are not the same string. This exists because they
// were: adding `synopsis` ahead of `explanation` shifted one entry's fields
// by one, so the memory kernel wore its explanation as its synopsis and had
// no explanation at all -- an empty help topic and an empty paragraph in
// every exported report, from an initializer that still compiled.
CK_TEST(every_benchmark_says_what_it_is_briefly_and_at_length) {
    for (const ckv::sysinfo::BenchmarkDescriptor& descriptor : ckv::sysinfo::benchmark_catalogue()) {
        CK_CHECK(!descriptor.key.empty());
        CK_CHECK(!descriptor.title.empty());
        CK_CHECK(!descriptor.rate_unit.empty());
        CK_CHECK(!descriptor.synopsis.empty());
        CK_CHECK(!descriptor.explanation.empty());
        // A synopsis is one line; an explanation is what a reader gets when
        // they ask about this one in particular.
        CK_CHECK(descriptor.synopsis != descriptor.explanation);
        CK_CHECK(descriptor.synopsis.size() < descriptor.explanation.size());
        CK_CHECK(descriptor.synopsis.size() <= 62);
        CK_CHECK(descriptor.synopsis.find('\n') == std::string_view::npos);
    }
}
