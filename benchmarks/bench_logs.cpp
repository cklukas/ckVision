// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "ckbench.hpp"
#include "cvision/widgets/log_controller.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/standard_roles.hpp"

#include <cstdio>

// Deterministic work gates and per-stage timing. These are CPU-stage figures,
// not a fabricated terminal-delivery baseline. End-to-end pinned-host timing
// and 1x/10x/100x terminal-rate trials are the final WP84 audit, after the demo.
bool run_log_benchmarks(const ckbench::Runner& bench) {
    bool holds = true;
    for (const auto history : {1024U, 16384U}) {
        ckv::LogLimits limits; limits.records = history;
        ckv::widgets::LogModel model(limits); model.begin_source(1, 1);
        for (unsigned i = 0; i < history; ++i) model.append_record("ordinary message with Unicode 中 é 🙂");
        ckv::widgets::LogFilter filter(model);
        filter.set_include("Unicode"); while (filter.step()) {}
        const auto allocated = model.allocated_bytes() + filter.allocated_bytes();
        char label[80]{}; std::snprintf(label, sizeof(label), "log_append_filter_%u", history);
        bench.run(label, 1000, [&] {
            const auto evaluated = filter.status().evaluated;
            holds = model.append_record("latest Unicode message") && holds;
            filter.step();
            holds = holds && filter.status().evaluated - evaluated <= 1 && !filter.status().live_pending &&
                model.allocated_bytes() + filter.allocated_bytes() == allocated;
        });
        ckv::ui::RoleRegistry registry; const auto roles = ckv::ui::intern_standard_roles(registry);
        const auto theme = ckv::ui::make_classic_theme(registry, roles);
        ckv::widgets::LogView view(filter); view.set_context({&theme, &registry, nullptr}); view.set_bounds({0, 0, 120, 40});
        ckv::scene::Surface surface({120, 40}, ckv::Cell::from_grapheme(" ", {}));
        ckv::scene::Painter painter(surface, {0, 0, 120, 40});
        view.refresh(); view.draw(painter);
        std::snprintf(label, sizeof(label), "log_visible_paint_%u", history);
        bench.run(label, 500, [&] { view.draw(painter); });
        std::printf("  log history %u: model/filter/view storage %zu B, retained %zu, matches %zu\n",
                    history, allocated + view.allocated_bytes(), model.size(), filter.size());
    }
    ckv::widgets::LogModel model;
    model.begin_source(1, 1);
    const std::string long_line(32 * 1024, 'x');
    model.append_record(long_line); model.append_record("rare match 中 é\nmultiline context");
    ckv::widgets::LogFilter filter(model, {8, 4096});
    bool alternate = false;
    bench.run("log_long_query_cancel_quantum", 200, [&] {
        filter.set_include(alternate ? "missing" : "rare"); alternate = !alternate;
        filter.step();
        holds = holds && filter.status().last_step_comparisons <= 4096 && filter.status().last_step_records <= 8;
    });
    ckv::LogMailbox mailbox(1, 1, {4, 1024, ckv::LogOverflowPolicy::DropOldest});
    ckv::LogMessage message, transfer; message.source = 1; message.epoch = 1; message.set_bytes("sample\n");
    const auto storage = mailbox.allocated_bytes();
    bench.run("log_mailbox_burst_overload", 1000, [&] {
        for (int offer = 0; offer < 100; ++offer) holds = mailbox.offer(message) == ckv::LogOfferResult::Accepted && holds;
        for (int take = 0; take < 4; ++take) holds = mailbox.take(transfer) == ckv::LogTakeResult::Taken && holds;
        mailbox.acknowledge();
        ckv::LogMailboxStatus status;
        holds = mailbox.status(status) && status.high_water_batches <= 4 && status.high_water_bytes <= 4096 &&
            mailbox.allocated_bytes() == storage && holds;
    });
    // Admission stress relative to a one-publication drain quantum, not a
    // measured terminal rate. Real-rate trials remain the pinned-host gate.
    for (const auto burst : {1, 10, 100}) {
        ckv::LogLimits limits; limits.records = 256; limits.record_bytes = 1024;
        limits.retained_bytes = 128 * 1024;
        ckv::widgets::LogModel first(limits), second(limits);
        ckv::widgets::LogFilter first_filter(first), second_filter(second);
        ckv::LogMailbox first_queue(1, 1, {4, 1024, ckv::LogOverflowPolicy::DropOldest});
        ckv::LogMailbox second_queue(2, 1, {4, 1024, ckv::LogOverflowPolicy::DropOldest});
        ckv::widgets::LogStreamReceiver first_receiver(first, first_queue), second_receiver(second, second_queue);
        ckv::widgets::LogController controller({2, 1, 1, 33});
        controller.attach({&first, &first_filter, nullptr, nullptr, nullptr, &first_receiver});
        controller.attach({&second, &second_filter, nullptr, nullptr, nullptr, &second_receiver});
        ckv::LogMessage entry; entry.source = 1; entry.epoch = 1; entry.set_record("steady 中 é 🙂");
        const auto memory = first.allocated_bytes() + second.allocated_bytes() + first_filter.allocated_bytes() +
            second_filter.allocated_bytes() + first_queue.allocated_bytes() + second_queue.allocated_bytes() +
            first_receiver.allocated_bytes() + second_receiver.allocated_bytes() + controller.allocated_bytes();
        std::int64_t now = 0;
        char label[80]{}; std::snprintf(label, sizeof(label), "log_two_sources_burst_%d", burst);
        bench.run(label, 100, [&] {
            for (int turn = 0; turn < 20; ++turn, ++now) {
                entry.arrival_nanos = now;
                for (int offer = 0; offer < burst; ++offer) {
                    entry.source = 1; holds = first_queue.offer(entry) == ckv::LogOfferResult::Accepted && holds;
                    entry.source = 2; holds = second_queue.offer(entry) == ckv::LogOfferResult::Accepted && holds;
                }
                controller.step(now);
                holds = holds && controller.status().last_turn_lanes == 1;
            }
            ckv::LogMailboxStatus a, b;
            holds = first_queue.status(a) && second_queue.status(b) && a.high_water_batches <= 4 &&
                b.high_water_batches <= 4 && first.size() <= 256 && second.size() <= 256 && holds;
            holds = holds && first.allocated_bytes() + second.allocated_bytes() + first_filter.allocated_bytes() +
                second_filter.allocated_bytes() + first_queue.allocated_bytes() + second_queue.allocated_bytes() +
                first_receiver.allocated_bytes() + second_receiver.allocated_bytes() + controller.allocated_bytes() == memory;
        });
        std::printf("  two sources, burst %d: fixed storage %zu B, quanta %llu, source counts %zu/%zu\n", burst, memory,
            static_cast<unsigned long long>(controller.status().lane_quanta), first.size(), second.size());
        holds = holds && first_receiver.status().records_started > 0 && second_receiver.status().records_started > 0;
    }
    return holds;
}
