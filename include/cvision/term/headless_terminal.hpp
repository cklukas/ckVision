// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// A full in-memory Terminal for tests and headless automation
// (ARCHITECTURE.md §4): scripted input, captured output, no real I/O.
#pragma once

#include <string>
#include <vector>

#include "cvision/term/input_decoder.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

class HeadlessTerminal final : public Terminal {
public:
    explicit HeadlessTerminal(Size size, Capabilities caps = baseline_capabilities())
        : size_(size), caps_(caps), decoder_(caps) {}

    Capabilities capabilities() const noexcept override { return caps_; }
    Size size() const noexcept override { return size_; }

    // Headless never blocks on real time — it just drains whatever was
    // queued by inject_bytes()/inject_event() since the last poll().
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;

    void write(std::string_view bytes) override { written_.append(bytes); }
    void set_title(std::string_view title) override { title_ = std::string(title); }
    void bell() override { ++bell_count_; }
    void write_clipboard(std::string_view text) override {
        if (caps_.clipboard_write) clipboard_ = std::string(text);
    }

    // --- Scripting interface (tests / recorded scripts) -------------
    void inject_bytes(std::string_view bytes, std::int64_t now_nanos);
    void inject_timeout_check(std::int64_t now_nanos);  // resolves a pending lone ESC, if due
    void inject_event(TerminalEvent event) { pending_events_.push_back(std::move(event)); }
    void set_capabilities(Capabilities caps) noexcept {
        caps_ = caps;
        decoder_.set_capabilities(caps);
    }
    void resize(Size new_size) {
        size_ = new_size;
        pending_events_.push_back(TerminalEvent{ResizeEvent{new_size}});
    }

    std::string_view written_bytes() const noexcept { return written_; }
    void clear_written() noexcept { written_.clear(); }
    const std::string& title() const noexcept { return title_; }
    int bell_count() const noexcept { return bell_count_; }
    const std::string& clipboard() const noexcept { return clipboard_; }

private:
    Size size_;
    Capabilities caps_;
    InputDecoder decoder_;
    std::string written_;
    std::string title_;
    int bell_count_ = 0;
    std::string clipboard_;
    std::vector<TerminalEvent> pending_events_;
};

}  // namespace ckv::term
