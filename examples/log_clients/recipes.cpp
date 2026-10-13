// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cvision/widgets/log_controller.hpp>
#include <cvision/term/headless_terminal.hpp>
#include <cvision/ui/application.hpp>
#include <cvision/ui/standard_roles.hpp>
#include <iostream>
#include <atomic>
#include <thread>
namespace {
bool append_and_metadata() {
    ckv::widgets::LogModel model;
    if (!model.begin_source(1, 1) || !model.append_bytes("one\npar")) return false;
    const auto partial = model.at(1)->id;
    if (!model.append_bytes("tial\n") || model.at(1)->id != partial || model.at(1)->text != "partial") return false;
    const std::array fields{ckv::LogField{"unit", "sample"}};
    if (!model.append_record("warning\n detail", {ckv::LogSeverity::Warning, 1234, fields})) return false;
    const auto record = model.at(2);
    return record && record->severity == ckv::LogSeverity::Warning && record->timestamp_nanos == 1234 && record->metadata().fields[0].value == "sample";
}
bool custom_bounded_publication() {
    using namespace ckv;
    auto activity = std::make_shared<LogActivity>();
    LogMailbox mailbox(7, 1, {2, 1024, LogOverflowPolicy::DropOldest}, activity);
    widgets::LogModel model; widgets::LogFilter filter(model); widgets::LogStreamReceiver receiver(model, mailbox);
    ManualClock clock; term::HeadlessTerminal terminal({80, 25}); ui::Application app(terminal, clock);
    const auto roles = ui::intern_standard_roles(app.roles()); app.set_theme(ui::make_classic_theme(app.roles(), roles));
    auto* panel = app.root().make<widgets::LogPanel>(filter);
    widgets::LogController controller({}, activity.get());
    const auto lane = controller.attach({&model, &filter, nullptr, panel, nullptr, &receiver}); if (!lane) return false;
    // Custom producer maps its schema only. The shipped receiver/controller
    // drain it; no app-owned queue pump or filter implementation is needed.
    std::atomic<bool> published{true};
    std::thread producer([&] {
        LogMessage message; message.source = 7; message.epoch = 1; message.arrival_nanos = 0;
        const std::array fields{LogField{"component", "custom"}};
        for (auto text : {"first", "second", "third", "fourth"}) {
            if (!message.set_record(text, {LogSeverity::Info, {}, fields}) || mailbox.offer(message) != LogOfferResult::Accepted) published.store(false);
        }
        if (!mailbox.close(LogSourceState::Ended, 0, 0)) published.store(false);
    });
    // This finite recipe fixes publication order for its executable assertion.
    // Join is outside dispatch; a live app drives input while its producer runs.
    producer.join(); if (!published.load()) return false;
    for (int turn = 0; turn < 40; ++turn) { clock.advance(1'000'000); controller.step(clock.now_nanos()); app.step(clock.now_nanos()); }
    const auto status = receiver.status();
    const bool valid = status.mailbox.dropped.records == 2 && status.gap_markers == 1 && status.state == LogSourceState::Ended && filter.size() == 3;
    controller.detach(lane); return valid;
}
}
int main() {
    if (!append_and_metadata() || !custom_bounded_publication()) return 1;
    std::cout << "Installed public append, metadata and bounded custom-source recipes passed.\n"; return 0;
}
