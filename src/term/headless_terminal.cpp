// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/headless_terminal.hpp"

#include <cstdio>
#include <stdexcept>
#include <utility>

#include "cvision/core/assert.hpp"

namespace ckv::term {

void HeadlessTerminal::set_capability_overrides(CapabilityOverrides overrides) {
    if ((overrides.cell_pixels && (overrides.cell_pixels->width <= 0 || overrides.cell_pixels->height <= 0)) ||
        (overrides.sixel_color_registers && *overrides.sixel_color_registers <= 0)) {
        throw std::invalid_argument("terminal capability overrides require positive cell metrics and color caps");
    }
    if (overrides == overrides_) return;
    overrides_ = std::move(overrides);
    const Capabilities effective = apply_capability_overrides(observed_caps_, overrides_);
    if (effective == caps_) return;
    caps_ = effective;
    display_.set_cell_pixels(effective_cell_pixels(caps_));
    pending_events_.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
}

std::vector<TerminalEvent> HeadlessTerminal::poll(std::int64_t /*deadline_nanos*/) {
    std::vector<TerminalEvent> out = std::move(pending_events_);
    pending_events_.clear();
    return out;
}

void HeadlessTerminal::write(std::string_view bytes) {
    written_.append(bytes);
    // The display knows precisely what it refused and why, and `error()` says
    // so in a sentence. Asserting on the bare call discarded that and left the
    // reader a false — which cost a session an afternoon on a clipping bug the
    // display had been describing the whole time. An assertion can only carry
    // an expression, so the sentence goes out just before it fires.
    if (!display_.feed(bytes))
        std::fprintf(stderr, "ckVision headless display refused output: %s\n",
                     display_.error().c_str());
    CKV_ASSERT(display_.valid());
}

void HeadlessTerminal::inject_event(TerminalEvent event) {
    if (const auto* changed = std::get_if<CapabilityChangedEvent>(&event)) {
        set_capabilities(changed->capabilities);
        event = TerminalEvent{CapabilityChangedEvent{caps_}};
    }
    pending_events_.push_back(std::move(event));
}

void HeadlessTerminal::inject_capability_change(Capabilities caps) {
    set_capabilities(caps);
    pending_events_.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
}

void HeadlessTerminal::set_capabilities(Capabilities caps) noexcept {
    observed_caps_ = caps;
    caps_ = apply_capability_overrides(observed_caps_, overrides_);
    decoder_.set_capabilities(observed_caps_);
    display_.set_cell_pixels(effective_cell_pixels(caps_));
}

void HeadlessTerminal::enqueue_decoded(std::vector<TerminalEvent> decoded) {
    for (auto& ev : decoded) {
        if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev)) {
            set_capabilities(changed->capabilities);
            ev = TerminalEvent{CapabilityChangedEvent{caps_}};
        }
        pending_events_.push_back(std::move(ev));
    }
}

void HeadlessTerminal::inject_bytes(std::string_view bytes, std::int64_t now_nanos) {
    enqueue_decoded(decoder_.feed(bytes, now_nanos));
}

void HeadlessTerminal::inject_timeout_check(std::int64_t now_nanos) {
    enqueue_decoded(decoder_.poll_timeout(now_nanos));
}

void HeadlessTerminal::inject_input_disconnect() { enqueue_decoded(decoder_.abort_paste()); }

}  // namespace ckv::term
