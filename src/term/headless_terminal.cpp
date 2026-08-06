// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/headless_terminal.hpp"

namespace ckv::term {

std::vector<TerminalEvent> HeadlessTerminal::poll(std::int64_t /*deadline_nanos*/) {
    std::vector<TerminalEvent> out = std::move(pending_events_);
    pending_events_.clear();
    return out;
}

void HeadlessTerminal::inject_bytes(std::string_view bytes, std::int64_t now_nanos) {
    std::vector<TerminalEvent> decoded = decoder_.feed(bytes, now_nanos);
    for (auto& ev : decoded) {
        if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev)) caps_ = changed->capabilities;
        pending_events_.push_back(std::move(ev));
    }
}

void HeadlessTerminal::inject_timeout_check(std::int64_t now_nanos) {
    std::vector<TerminalEvent> decoded = decoder_.poll_timeout(now_nanos);
    for (auto& ev : decoded) {
        if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev)) caps_ = changed->capabilities;
        pending_events_.push_back(std::move(ev));
    }
}

}  // namespace ckv::term
