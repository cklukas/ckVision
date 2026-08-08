// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Record/replay backend (ARCHITECTURE.md §4): wraps any Terminal,
// recording the event stream (including capability-probe responses,
// since CapabilityChangedEvent flows through poll() like any other
// event) and presented bytes for deterministic replay and debugging.
#pragma once

#include <string>
#include <variant>
#include <vector>

#include "cvision/term/terminal.hpp"

namespace ckv::term {

struct RecordedEvents {
    std::vector<TerminalEvent> events;
    friend bool operator==(const RecordedEvents&, const RecordedEvents&) = default;
};
struct RecordedWrite {
    std::string bytes;
    friend bool operator==(const RecordedWrite&, const RecordedWrite&) = default;
};
struct RecordedTitle {
    std::string title;
    friend bool operator==(const RecordedTitle&, const RecordedTitle&) = default;
};
struct RecordedBell {
    friend bool operator==(const RecordedBell&, const RecordedBell&) = default;
};
struct RecordedClipboard {
    std::string text;
    friend bool operator==(const RecordedClipboard&, const RecordedClipboard&) = default;
};
using RecordedEntry = std::variant<RecordedEvents, RecordedWrite, RecordedTitle, RecordedBell, RecordedClipboard>;

// Wraps `inner`, forwarding every call to it while appending a
// RecordedEntry for each poll() batch (even empty ones, so replay can
// reconstruct timing-relative structure) and every observable terminal
// output operation. The initial capability/size snapshot is retained so a
// replay never has to borrow mutable state from the original terminal.
class RecordingTerminal final : public Terminal {
public:
    explicit RecordingTerminal(Terminal& inner)
        : inner_(inner), initial_capabilities_(inner.capabilities()), initial_size_(inner.size()) {}

    Capabilities capabilities() const noexcept override { return inner_.capabilities(); }
    Size size() const noexcept override { return inner_.size(); }

    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override {
        std::vector<TerminalEvent> events = inner_.poll(deadline_nanos);
        log_.push_back(RecordedEntry{RecordedEvents{events}});
        return events;
    }
    void wake() noexcept override { inner_.wake(); }

    void write(std::string_view bytes) override {
        inner_.write(bytes);
        log_.push_back(RecordedEntry{RecordedWrite{std::string(bytes)}});
    }

    void set_title(std::string_view title) override {
        inner_.set_title(title);
        log_.push_back(RecordedEntry{RecordedTitle{std::string(title)}});
    }
    void bell() override {
        inner_.bell();
        log_.push_back(RecordedEntry{RecordedBell{}});
    }
    void write_clipboard(std::string_view text) override {
        const bool supported = inner_.capabilities().clipboard_write;
        inner_.write_clipboard(text);
        if (supported) log_.push_back(RecordedEntry{RecordedClipboard{std::string(text)}});
    }

    const std::vector<RecordedEntry>& recording() const noexcept { return log_; }
    Capabilities initial_capabilities() const noexcept { return initial_capabilities_; }
    Size initial_size() const noexcept { return initial_size_; }

private:
    Terminal& inner_;
    Capabilities initial_capabilities_;
    Size initial_size_;
    std::vector<RecordedEntry> log_;
};

// Replays a previously captured recording: poll() returns each
// RecordedEvents entry in order (recorded output entries are skipped —
// they were this session's OUTPUT, not input to replay), ignoring the
// real deadline entirely (deterministic replay has no real time).
// Every replayed poll and output is itself logged, allowing a complete
// operation-by-operation comparison with the original recording.
class ReplayTerminal final : public Terminal {
public:
    ReplayTerminal(std::vector<RecordedEntry> recording, Capabilities caps, Size size)
        : recording_(std::move(recording)), caps_(caps), size_(size) {}

    Capabilities capabilities() const noexcept override { return caps_; }
    Size size() const noexcept override { return size_; }

    std::vector<TerminalEvent> poll(std::int64_t /*deadline_nanos*/) override {
        while (cursor_ < recording_.size()) {
            const RecordedEntry& entry = recording_[cursor_++];
            if (const auto* events = std::get_if<RecordedEvents>(&entry)) {
                for (const TerminalEvent& ev : events->events)
                    if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev))
                        caps_ = changed->capabilities;
                replayed_.push_back(RecordedEntry{*events});
                return events->events;
            }
        }
        replayed_.push_back(RecordedEntry{RecordedEvents{}});
        return {};
    }
    void wake() noexcept override {}

    void write(std::string_view bytes) override {
        written_.append(bytes);
        replayed_.push_back(RecordedEntry{RecordedWrite{std::string(bytes)}});
    }
    void set_title(std::string_view title) override {
        title_ = std::string(title);
        replayed_.push_back(RecordedEntry{RecordedTitle{title_}});
    }
    void bell() override {
        ++bell_count_;
        replayed_.push_back(RecordedEntry{RecordedBell{}});
    }
    void write_clipboard(std::string_view text) override {
        if (!caps_.clipboard_write) return;
        clipboard_ = std::string(text);
        replayed_.push_back(RecordedEntry{RecordedClipboard{clipboard_}});
    }

    std::string_view written_bytes() const noexcept { return written_; }
    void clear_written() noexcept { written_.clear(); }
    const std::string& title() const noexcept { return title_; }
    int bell_count() const noexcept { return bell_count_; }
    const std::string& clipboard() const noexcept { return clipboard_; }
    bool exhausted() const noexcept { return cursor_ >= recording_.size(); }
    const std::vector<RecordedEntry>& replayed() const noexcept { return replayed_; }
    bool matches_recording() const noexcept { return replayed_ == recording_; }

private:
    std::vector<RecordedEntry> recording_;
    std::size_t cursor_ = 0;
    Capabilities caps_;
    Size size_;
    std::string written_;
    std::string title_;
    int bell_count_ = 0;
    std::string clipboard_;
    std::vector<RecordedEntry> replayed_;
};

}  // namespace ckv::term
