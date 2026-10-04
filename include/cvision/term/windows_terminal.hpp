// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// VT-first outer terminal session for Windows console/ConPTY hosts (D-020).
#pragma once

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <functional>

#include "cvision/core/diagnostics.hpp"

#include "cvision/core/clock.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/term/windows_clipboard.hpp"
#include "cvision/term/windows_wait_set.hpp"

namespace ckv::term {

// Standard handles are borrowed; their modes and UTF-8 code pages are
// restored when the session ends. One instance owns one console session.
class WindowsTerminal final : public Terminal {
public:
    // Switches both console handles to VT processing and UTF-8, enters the session and starts
    // a bounded capability-probe window. Capabilities start from baseline_capabilities() with
    // clipboard_write on, since the clipboard is written natively (CF_UNICODETEXT) rather
    // than through OSC 52. Throws std::runtime_error, with the console modes and code pages
    // put back, when either handle is not a console or the host will not keep VT mode (a
    // legacy console). `clock` must outlive the terminal; it times polls and the probe window.
    explicit WindowsTerminal(const Clock& clock, HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE),
                             HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE));
    // Restores the session (see restore()).
    ~WindowsTerminal() override;

    // Not copyable: one instance owns one console session and its wake event.
    WindowsTerminal(const WindowsTerminal&) = delete;
    WindowsTerminal& operator=(const WindowsTerminal&) = delete;

    Capabilities capabilities() const noexcept override { return caps_; }
    Size size() const noexcept override;
    // Cumulative SGR mouse reports decoded by this session, not guessed from
    // delivered events. Counter semantics match PosixTerminal's observation.
    std::size_t mouse_reports_seen() const noexcept { return decoder_.mouse_reports_seen(); }
    std::size_t frame_acknowledgements() const noexcept override {
        return decoder_.frame_acknowledgements();
    }
    std::span<const WaitHandle> wait_handles() const noexcept override {
        return active_ ? std::span<const WaitHandle>(wait_handles_) : std::span<const WaitHandle>();
    }
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos,
                                    std::span<const WaitHandle> additional_wait_handles) override;
    void wake() noexcept override;
    void restore() noexcept override;
    void write_diagnostic_after_restore(std::string_view message) noexcept override;
    [[noreturn]] void terminate_after_callback_failure() noexcept override;
    // Throws std::runtime_error when the console output handle refuses a write.
    void write(std::string_view bytes) override;
    // Observes ordered byte attempts from installation onward, before native
    // writes, including probes/title/bell and normal restoration. Constructor
    // bytes are not included. Empty disables capture; sink exceptions never
    // suppress output. Native clipboard publication emits no terminal bytes.
    void set_output_capture(std::function<void(std::string_view)> capture);
    // Borrowed trace sink/clock for host capability summaries when probes
    // settle. Off by default; both borrowed objects must outlive the session.
    void set_graphics_trace(GraphicsTrace trace) noexcept { trace_ = trace; }
    void set_title(std::string_view title) override;
    void bell() override;
    // Native, instance-owned CF_UNICODETEXT export (WindowsClipboardWriter).
    // Invalid UTF-8, embedded NUL or a locked clipboard drops the write.
    ClipboardWriteResult write_clipboard(std::string_view text) override;

private:
    void write_all(std::string_view bytes) const;
    void capture_output(std::string_view bytes) const noexcept;
    void write_restoration(std::string_view bytes) const noexcept;
    void observe_resize(std::vector<TerminalEvent>& events);
    void append_decoded(std::vector<TerminalEvent> decoded, std::vector<TerminalEvent>& events);
    void begin_probes();
    void finish_probes(std::vector<TerminalEvent>& events);
    void negotiate_kitty_enhancements();
    void maybe_demote_kitty_keyboard();

    const Clock& clock_;
    WindowsWaitSet wait_set_;
    WindowsClipboardWriter clipboard_;
    HANDLE output_;
    HANDLE input_;
    HANDLE wake_event_ = nullptr;
    DWORD original_output_mode_ = 0;
    DWORD original_input_mode_ = 0;
    UINT original_output_cp_ = 0;
    UINT original_input_cp_ = 0;
    Capabilities caps_;
    InputDecoder decoder_;
    Size last_size_{};
    std::array<WaitHandle, 2> wait_handles_{};
    std::int64_t probe_deadline_nanos_ = -1;
    bool withheld_sixel_graphics_ = false;
    bool kitty_flags_negotiated_ = false;
    bool kitty_push_active_ = false;
    bool kitty_demoted_ = false;
    wchar_t pending_high_surrogate_ = 0;
    bool active_ = false;
    std::function<void(std::string_view)> capture_;
    GraphicsTrace trace_{};
};

}  // namespace ckv::term

#endif
