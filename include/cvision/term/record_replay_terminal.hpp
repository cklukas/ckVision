// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Record/replay backend (the architecture §4): wraps any Terminal,
// recording the event stream (including capability-probe responses,
// since CapabilityChangedEvent flows through poll() like any other
// event), presented bytes, and post-restore diagnostics for deterministic
// replay and debugging.
#pragma once

#include <cstddef>
#include <cstdlib>  // std::abort
#include <string>
#include <variant>
#include <vector>

#include "cvision/term/terminal.hpp"

namespace ckv::term {

// The batch one poll() call returned, in delivery order. An empty batch (a poll that timed
// out) is recorded too, so the sequence of polls is preserved.
struct RecordedEvents {
    // The events exactly as poll() returned them.
    std::vector<TerminalEvent> events;
    // The recorded terminal's frame_acknowledgements() once that poll returned. Replies to
    // frame-completion markers arrive as input, so they are replayed with the batch that
    // brought them.
    std::size_t frame_acknowledgements = 0;
    // Equal when both batches hold the same events in the same order and left the same
    // acknowledgement count.
    friend bool operator==(const RecordedEvents&, const RecordedEvents&) = default;
};
// One write() call: the raw output bytes, unmodified.
struct RecordedWrite {
    // The bytes passed to that one call; consecutive writes are not merged.
    std::string bytes;
    // Equal when the bytes are identical.
    friend bool operator==(const RecordedWrite&, const RecordedWrite&) = default;
};
// One set_title() call.
struct RecordedTitle {
    // The title as the caller passed it, before any sanitizing a live backend applies.
    std::string title;
    // Equal when the titles are identical.
    friend bool operator==(const RecordedTitle&, const RecordedTitle&) = default;
};
// One bell() call; it carries no data.
struct RecordedBell {
    // Every bell equals every other bell.
    friend bool operator==(const RecordedBell&, const RecordedBell&) = default;
};
// One write_clipboard() attempt and its exact injected outcome, including
// refusals. A failed native export can drive UI behavior and must replay too.
struct RecordedClipboard {
    // The UTF-8 text sent to the clipboard.
    std::string text;
    // Exact backend outcome, including native error and possible partial effect.
    ClipboardWriteResult result;
    // Equal when both text and export outcome are identical.
    friend bool operator==(const RecordedClipboard&, const RecordedClipboard&) = default;
};
// One write_diagnostic_after_restore() call.
struct RecordedDiagnostic {
    // The diagnostic text as passed, including any trailing newline the caller supplied.
    std::string message;
    // Equal when the messages are identical.
    friend bool operator==(const RecordedDiagnostic&, const RecordedDiagnostic&) = default;
};
// One operation in a recording, in the order it happened: an input batch or an output call.
using RecordedEntry =
    std::variant<RecordedEvents, RecordedWrite, RecordedTitle, RecordedBell, RecordedClipboard, RecordedDiagnostic>;

// Wraps `inner`, forwarding every call to it while appending a
// RecordedEntry for each poll() batch (even empty ones, so replay can
// reconstruct timing-relative structure) and every observable terminal
// output operation. The initial capability/size snapshot is retained so a
// replay never has to borrow mutable state from the original terminal.
class RecordingTerminal final : public Terminal {
public:
    // Borrows `inner`, which must outlive this recorder, and snapshots its current
    // capabilities and size for initial_capabilities() and initial_size(). The recording
    // starts empty. Every Terminal member is forwarded to `inner`.
    explicit RecordingTerminal(Terminal& inner)
        : inner_(inner), initial_capabilities_(inner.capabilities()), initial_size_(inner.size()) {}

    Capabilities capabilities() const noexcept override { return inner_.capabilities(); }
    Size size() const noexcept override { return inner_.size(); }
    std::size_t frame_acknowledgements() const noexcept override { return inner_.frame_acknowledgements(); }
    std::span<const WaitHandle> wait_handles() const noexcept override { return inner_.wait_handles(); }

    // Both overloads reach the same overload of `inner` and log the batch it returned together
    // with the acknowledgement count it left.
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override {
        return record(inner_.poll(deadline_nanos));
    }
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos,
                                    std::span<const WaitHandle> additional_wait_handles) override {
        return record(inner_.poll(deadline_nanos, additional_wait_handles));
    }
    void wake() noexcept override { inner_.wake(); }
    void restore() noexcept override { inner_.restore(); }
    void write_diagnostic_after_restore(std::string_view message) noexcept override {
        inner_.write_diagnostic_after_restore(message);
        // Terminal diagnostics are explicitly best-effort and noexcept: an
        // exhausted recorder must not turn a recoverable warning into process
        // termination after the terminal has already been restored.
        try {
            log_.emplace_back(RecordedDiagnostic{std::string(message)});
        } catch (...) {
        }
    }
    [[noreturn]] void terminate_after_callback_failure() noexcept override {
        inner_.terminate_after_callback_failure();
        // inner_'s override is [[noreturn]], but that attribute does not cross
        // the virtual call, so GCC (correctly, by the language rules) sees this
        // function as able to return and -Werror rejects it. abort() states the
        // contract and matches PosixTerminal's own handler; it is unreachable
        // because inner_ has already terminated the process.
        std::abort();
    }

    void write(std::string_view bytes) override {
        inner_.write(bytes);
        log_.emplace_back(RecordedWrite{std::string(bytes)});
    }

    void set_title(std::string_view title) override {
        inner_.set_title(title);
        log_.emplace_back(RecordedTitle{std::string(title)});
    }
    void bell() override {
        inner_.bell();
        log_.emplace_back(RecordedBell{});
    }
    ClipboardWriteResult write_clipboard(std::string_view text) override {
        const auto result = inner_.write_clipboard(text);
        log_.emplace_back(RecordedClipboard{std::string(text), result});
        return result;
    }

    // Every entry recorded so far, oldest first. The reference stays valid for the
    // recorder's lifetime; further calls append to the same vector.
    const std::vector<RecordedEntry>& recording() const noexcept { return log_; }
    // The inner terminal's capabilities and size at construction: the values to hand to
    // ReplayTerminal's constructor along with recording().
    Capabilities initial_capabilities() const noexcept { return initial_capabilities_; }
    Size initial_size() const noexcept { return initial_size_; }

private:
    std::vector<TerminalEvent> record(std::vector<TerminalEvent> events) {
        log_.emplace_back(RecordedEvents{events, inner_.frame_acknowledgements()});
        return events;
    }

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
    // Takes ownership of `recording` and starts from `caps` and `size`, normally the
    // recorder's initial_capabilities() and initial_size(). Replayed CapabilityChangedEvent
    // and ResizeEvent entries update what capabilities() and size() report as they are
    // returned from poll(), and frame_acknowledgements() reports the count recorded with the
    // last batch returned (zero before the first). Clipboard attempts return
    // recorded outcomes in attempt order, subject to the replayed capability.
    ReplayTerminal(std::vector<RecordedEntry> recording, Capabilities caps, Size size)
        : recording_(std::move(recording)), caps_(caps), size_(size) {}

    Capabilities capabilities() const noexcept override { return caps_; }
    Size size() const noexcept override { return size_; }
    std::size_t frame_acknowledgements() const noexcept override { return frame_acknowledgements_; }

    std::vector<TerminalEvent> poll(std::int64_t /*deadline_nanos*/) override {
        while (cursor_ < recording_.size()) {
            const RecordedEntry& entry = recording_[cursor_++];
            if (const auto* events = std::get_if<RecordedEvents>(&entry)) {
                for (const TerminalEvent& ev : events->events) {
                    if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev))
                        caps_ = changed->capabilities;
                    else if (const auto* resize = std::get_if<ResizeEvent>(&ev))
                        size_ = resize->cells;
                }
                frame_acknowledgements_ = events->frame_acknowledgements;
                replayed_.emplace_back(*events);
                return events->events;
            }
        }
        replayed_.emplace_back(RecordedEvents{{}, frame_acknowledgements_});
        return {};
    }
    void wake() noexcept override {}

    void write(std::string_view bytes) override {
        written_.append(bytes);
        replayed_.emplace_back(RecordedWrite{std::string(bytes)});
    }
    void set_title(std::string_view title) override {
        title_ = std::string(title);
        replayed_.emplace_back(RecordedTitle{title_});
    }
    void bell() override {
        ++bell_count_;
        replayed_.emplace_back(RecordedBell{});
    }
    ClipboardWriteResult write_clipboard(std::string_view text) override {
        ClipboardWriteResult result{ClipboardWriteStatus::Error};
        while (clipboard_cursor_ < recording_.size()) {
            const auto* attempt = std::get_if<RecordedClipboard>(&recording_[clipboard_cursor_++]);
            if (attempt != nullptr) {
                if (attempt->text == text) result = attempt->result;
                break;
            }
        }
        if (!caps_.clipboard_write) result = {ClipboardWriteStatus::Unsupported};
        if (result.accepted()) clipboard_ = std::string(text);
        replayed_.emplace_back(RecordedClipboard{std::string(text), result});
        return result;
    }
    // Captures the diagnostic in diagnostic_bytes() instead of writing it to stderr.
    void write_diagnostic_after_restore(std::string_view message) noexcept override {
        try {
            diagnostics_.append(message);
            replayed_.emplace_back(RecordedDiagnostic{std::string(message)});
        } catch (...) {
        }
    }

    // Everything write() received since construction or the last clear_written(), as one
    // concatenated byte string. The view is invalidated by the next write or clear.
    // clear_written() empties it without touching replayed().
    std::string_view written_bytes() const noexcept { return written_; }
    void clear_written() noexcept { written_.clear(); }
    // The last title set; empty until set_title() is called.
    const std::string& title() const noexcept { return title_; }
    // How many times bell() has been called.
    int bell_count() const noexcept { return bell_count_; }
    // The last text write_clipboard() accepted; empty until one is accepted.
    const std::string& clipboard() const noexcept { return clipboard_; }
    // Every diagnostic written after restore, concatenated in order.
    std::string_view diagnostic_bytes() const noexcept { return diagnostics_; }
    // True once every recorded entry has been consumed; each further poll() returns an empty
    // batch and logs it as an empty RecordedEvents.
    bool exhausted() const noexcept { return cursor_ >= recording_.size(); }
    // This session's own log, in the same format as RecordingTerminal::recording().
    const std::vector<RecordedEntry>& replayed() const noexcept { return replayed_; }
    // Whether the replay reproduced the recording operation for operation: replayed() equals
    // the recording exactly, including every output entry. A replay that polls past the end
    // of the recording logs extra empty batches and therefore no longer matches.
    bool matches_recording() const noexcept { return replayed_ == recording_; }

private:
    std::vector<RecordedEntry> recording_;
    std::size_t cursor_ = 0;
    std::size_t clipboard_cursor_ = 0;
    Capabilities caps_;
    Size size_;
    std::size_t frame_acknowledgements_ = 0;
    std::string written_;
    std::string title_;
    int bell_count_ = 0;
    std::string clipboard_;
    std::string diagnostics_;
    std::vector<RecordedEntry> replayed_;
};

}  // namespace ckv::term
