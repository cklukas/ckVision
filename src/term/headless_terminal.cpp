// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/headless_terminal.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::term {

std::vector<TerminalEvent> HeadlessTerminal::poll(std::int64_t /*deadline_nanos*/) {
    std::vector<TerminalEvent> out = std::move(pending_events_);
    pending_events_.clear();
    return out;
}

void HeadlessTerminal::write(std::string_view bytes) {
    written_.append(bytes);
    CKV_ASSERT(display_.feed(bytes));
}

void HeadlessTerminal::inject_event(TerminalEvent event) {
    if (const auto* changed = std::get_if<CapabilityChangedEvent>(&event)) set_capabilities(changed->capabilities);
    pending_events_.push_back(std::move(event));
}

void HeadlessTerminal::inject_capability_change(Capabilities caps) {
    set_capabilities(caps);
    pending_events_.push_back(TerminalEvent{CapabilityChangedEvent{caps}});
}

void HeadlessTerminal::set_capabilities(Capabilities caps) noexcept {
    caps_ = caps;
    decoder_.set_capabilities(caps);
    display_.set_cell_pixels(effective_cell_pixels(caps));
}

void HeadlessTerminal::enqueue_decoded(std::vector<TerminalEvent> decoded) {
    for (auto& ev : decoded) {
        if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev)) set_capabilities(changed->capabilities);
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
