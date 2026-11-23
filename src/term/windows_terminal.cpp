// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Windows console modes, VT character transport, and native clipboard are
// confined to the terminal layer. Input records supplement VT only for resize
// (D-020).
#include "cvision/term/windows_terminal.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "cvision/core/utf8.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cvision/term/osc_sequences.hpp"
#include "cvision/term/pointer_shape_names.hpp"
#include "term/terminal_host_summary_internal.hpp"

namespace ckv::term {
namespace {

constexpr std::string_view kRestore =
    "\x1B[?1003l\x1B[?1006l\x1B[?1016l\x1B[?2004l\x1B[?1004l\x1B[?2031l\x1B[?2026l\x1B[?25h\x1B[0m\x1B[?1049l";
// Probe the published terminal replies, including kitty keyboard support.
// A verified kitty reply is followed by a session-owned push and readback.
constexpr std::string_view kProbe =
    "\x1B]10;?\x1B\\\x1B]11;?\x1B\\\x1B[?2031h\x1B[?2031$p\x1B[?2026h\x1B[?2026$p\x1B[?2026l"
    "\x1B_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1B\\"
    "\x1B[c\x1B[?1;4;0S\x1B[?2;4;0S\x1B[?1016$p\x1B[16t\x1B[14t\x1B[?u";
constexpr std::int64_t kProbeNanos = 250'000'000;
constexpr std::int64_t kLatePixelReportGraceNanos = 500'000'000;

std::string make_session_enter_sequence(const Capabilities& caps) {
    std::string seq = "\x1B[?1049h";
    if (caps.keyboard_protocol == KeyboardProtocol::Kitty && caps.kitty_keyboard_flags != 0)
        seq += "\x1B[>" + std::to_string(caps.kitty_keyboard_flags) + "u";
    if (caps.mouse_protocol == MouseProtocol::SGR) {
        seq += "\x1B[?1003h\x1B[?1006h";
        if (caps.pixel_mouse) seq += "\x1B[?1016h";
    }
    if (caps.bracketed_paste) seq += "\x1B[?2004h";
    if (caps.focus_events) seq += "\x1B[?1004h";
    return seq;
}

void write_best_effort(HANDLE handle, std::string_view bytes) noexcept {
    while (!bytes.empty()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 64 * 1024));
        if (!::WriteFile(handle, bytes.data(), count, &written, nullptr) || written == 0) return;
        bytes.remove_prefix(written);
    }
}

struct WinConsoleCtrlSlot {
    std::atomic<bool> active{false};
    HANDLE output = nullptr;
    HANDLE input = nullptr;
    DWORD output_mode = 0;
    DWORD input_mode = 0;
    UINT output_cp = 0;
    UINT input_cp = 0;
};

constexpr std::size_t kMaxWinConsoleSlots = 8;
WinConsoleCtrlSlot g_win_console_slots[kMaxWinConsoleSlots]{};
std::atomic<bool> g_win_ctrl_handler_installed{false};

void restore_all_win_console_slots() noexcept {
    for (WinConsoleCtrlSlot& slot : g_win_console_slots) {
        if (!slot.active.load(std::memory_order_acquire)) continue;
        write_best_effort(slot.output, "\x1B[<u");
        write_best_effort(slot.output, "\x1B]22;\x1B\\");
        write_best_effort(slot.output, kRestore);
        if (slot.input != nullptr) ::SetConsoleMode(slot.input, slot.input_mode);
        if (slot.output != nullptr) ::SetConsoleMode(slot.output, slot.output_mode);
        if (slot.output_cp != 0) ::SetConsoleOutputCP(slot.output_cp);
        if (slot.input_cp != 0) ::SetConsoleCP(slot.input_cp);
    }
}

BOOL WINAPI win_console_ctrl_handler(DWORD ctrl_type) noexcept {
    if (ctrl_type != CTRL_C_EVENT && ctrl_type != CTRL_BREAK_EVENT &&
        ctrl_type != CTRL_CLOSE_EVENT) {
        return FALSE;
    }
    restore_all_win_console_slots();
    return FALSE;  // allow default handler / process exit after restoration
}

LONG WINAPI win_unhandled_exception_filter(EXCEPTION_POINTERS*) noexcept {
    restore_all_win_console_slots();
    return EXCEPTION_CONTINUE_SEARCH;
}

int register_win_console_slot(HANDLE output, HANDLE input, DWORD out_mode, DWORD in_mode,
                              UINT out_cp, UINT in_cp) noexcept {
    if (!g_win_ctrl_handler_installed.exchange(true, std::memory_order_acq_rel)) {
        ::SetConsoleCtrlHandler(win_console_ctrl_handler, TRUE);
        ::SetUnhandledExceptionFilter(win_unhandled_exception_filter);
    }
    for (std::size_t i = 0; i < kMaxWinConsoleSlots; ++i) {
        bool expected = false;
        if (g_win_console_slots[i].active.compare_exchange_strong(expected, true,
                                                                  std::memory_order_acq_rel)) {
            g_win_console_slots[i].output = output;
            g_win_console_slots[i].input = input;
            g_win_console_slots[i].output_mode = out_mode;
            g_win_console_slots[i].input_mode = in_mode;
            g_win_console_slots[i].output_cp = out_cp;
            g_win_console_slots[i].input_cp = in_cp;
            return static_cast<int>(i);
        }
    }
    return -1;
}

void unregister_win_console_slot(int slot) noexcept {
    if (slot >= 0 && static_cast<std::size_t>(slot) < kMaxWinConsoleSlots) {
        g_win_console_slots[static_cast<std::size_t>(slot)].active.store(false,
                                                                         std::memory_order_release);
    }
}

std::runtime_error win32_error(const char* operation) {
    return std::runtime_error(std::string("Windows terminal: ") + operation +
                              " failed (Win32 " + std::to_string(::GetLastError()) + ")");
}

// The console the session runs on can no longer serve it (D-078, D-138).
TerminalLost console_lost(const char* operation) {
    const auto error = static_cast<int>(::GetLastError());  // before anything can overwrite it
    return TerminalLost(error, std::system_category(), std::string("Windows terminal: ") + operation + " failed");
}

}  // namespace

WindowsTerminal::WindowsTerminal(const Clock& clock)
    : WindowsTerminal(clock, ::GetStdHandle(STD_OUTPUT_HANDLE), ::GetStdHandle(STD_INPUT_HANDLE),
                      baseline_capabilities(), true) {}

WindowsTerminal::WindowsTerminal(const Clock& clock, Capabilities initial_capabilities,
                                 bool enable_capability_probes)
    : WindowsTerminal(clock, ::GetStdHandle(STD_OUTPUT_HANDLE), ::GetStdHandle(STD_INPUT_HANDLE),
                      initial_capabilities, enable_capability_probes) {}

WindowsTerminal::WindowsTerminal(const Clock& clock, void* output, void* input,
                                 Capabilities initial_capabilities, bool enable_capability_probes)
    : clock_(clock), wait_set_(clock), output_(output), input_(input),
      observed_caps_(initial_capabilities), caps_(observed_caps_), decoder_(observed_caps_),
      enable_probes_(enable_capability_probes) {
    if (output_ == nullptr || output_ == INVALID_HANDLE_VALUE ||
        input_ == nullptr || input_ == INVALID_HANDLE_VALUE ||
        !::GetConsoleMode(output_, &original_output_mode_) ||
        !::GetConsoleMode(input_, &original_input_mode_)) {
        throw std::runtime_error("Windows terminal: stdin and stdout must be attached to a VT-capable console");
    }
    wake_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (wake_event_ == nullptr) throw win32_error("CreateEventW");
    original_output_cp_ = ::GetConsoleOutputCP();
    original_input_cp_ = ::GetConsoleCP();
    const DWORD output_mode = original_output_mode_ | ENABLE_PROCESSED_OUTPUT |
                              ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    const DWORD input_mode = (original_input_mode_ &
                              ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT |
                                ENABLE_QUICK_EDIT_MODE)) |
                             ENABLE_EXTENDED_FLAGS | ENABLE_VIRTUAL_TERMINAL_INPUT |
                             ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    const auto fail_setup = [this](std::string reason) {
        ::SetConsoleMode(input_, original_input_mode_);
        ::SetConsoleMode(output_, original_output_mode_);
        ::SetConsoleOutputCP(original_output_cp_);
        ::SetConsoleCP(original_input_cp_);
        ::CloseHandle(wake_event_);
        wake_event_ = nullptr;
        throw std::runtime_error("Windows terminal: " + std::move(reason));
    };
    constexpr std::string_view vt_host_advice =
        "Use Windows Terminal or Windows Console Host with 'Use legacy console' disabled.";
    if (!::SetConsoleMode(output_, output_mode))
        fail_setup("VT output cannot be enabled (Win32 " + std::to_string(::GetLastError()) + "). " +
                   std::string(vt_host_advice));
    DWORD verified_output_mode = 0;
    if (!::GetConsoleMode(output_, &verified_output_mode))
        fail_setup("VT output mode cannot be verified (Win32 " +
                   std::to_string(::GetLastError()) + "). " + std::string(vt_host_advice));
    if ((verified_output_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        fail_setup("the console host did not retain VT output mode. " + std::string(vt_host_advice));
    }
    if (!::SetConsoleMode(input_, input_mode))
        fail_setup("VT input cannot be enabled (Win32 " + std::to_string(::GetLastError()) + "). " +
                   std::string(vt_host_advice));
    DWORD verified_input_mode = 0;
    if (!::GetConsoleMode(input_, &verified_input_mode))
        fail_setup("VT input mode cannot be verified (Win32 " +
                   std::to_string(::GetLastError()) + "). " + std::string(vt_host_advice));
    if ((verified_input_mode & ENABLE_VIRTUAL_TERMINAL_INPUT) == 0) {
        fail_setup("the console host did not retain VT input mode. " + std::string(vt_host_advice));
    }
    if (!::SetConsoleOutputCP(CP_UTF8))
        fail_setup("UTF-8 output code page cannot be enabled (Win32 " +
                   std::to_string(::GetLastError()) + ").");
    if (!::SetConsoleCP(CP_UTF8))
        fail_setup("UTF-8 input code page cannot be enabled (Win32 " +
                   std::to_string(::GetLastError()) + ").");
    observed_caps_.clipboard_write = true;  // Native CF_UNICODETEXT, independent of OSC 52.
    (void)update_effective_capabilities();
    decoder_.set_capabilities(observed_caps_);
    last_size_ = size();
    decoder_.set_cell_grid(last_size_);
    wait_handles_[0] = WaitHandle{WaitHandleKind::WindowsHandle,
                                  reinterpret_cast<std::uintptr_t>(input_)};
    wait_handles_[1] = WaitHandle{WaitHandleKind::WindowsHandle,
                                  reinterpret_cast<std::uintptr_t>(wake_event_)};
    active_ = true;
    ctrl_slot_ = register_win_console_slot(output_, input_, original_output_mode_,
                                           original_input_mode_, original_output_cp_,
                                           original_input_cp_);
    try {
        write_all(make_session_enter_sequence(observed_caps_));
        if (enable_probes_) {
            begin_probes();
        } else {
            decoder_.set_capability_update_policy(CapabilityUpdatePolicy::Reject);
        }
    } catch (...) {
        restore();
        throw;
    }
}

WindowsTerminal::~WindowsTerminal() {
    restore();
    unregister_win_console_slot(ctrl_slot_);
    ctrl_slot_ = -1;
}

Size WindowsTerminal::size() const noexcept {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!::GetConsoleScreenBufferInfo(output_, &info)) return Size{80, 24};
    return Size{static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1),
                static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1)};
}

void WindowsTerminal::begin_probes() {
    if (!enable_probes_) {
        decoder_.set_capability_update_policy(CapabilityUpdatePolicy::Reject);
        return;
    }
    decoder_.set_pixel_reports_possible(true);
    late_pixel_deadline_nanos_ = -1;
    decoder_.begin_capability_probe_window(observed_caps_);
    probe_deadline_nanos_ = clock_.now_nanos() + kProbeNanos;
    // Until the cell metric and mode reply establish pixel coordinates, an
    // SGR report cannot safely be interpreted as a cell click.
    decoder_.set_sgr_mouse_input_suppressed(observed_caps_.mouse_protocol == MouseProtocol::SGR &&
                                            !observed_caps_.pixel_mouse);
    write_all("\x1B[?1016h");
    write_all(kProbe);
    write_all(kPointerShapeQuerySequence);
}

void WindowsTerminal::finish_probes(std::vector<TerminalEvent>& events) {
    if (probe_deadline_nanos_ < 0) return;
    probe_deadline_nanos_ = -1;
    decoder_.finish_capability_probe_window(observed_caps_);
    if (observed_caps_.mouse_protocol == MouseProtocol::SGR && !observed_caps_.pixel_mouse) {
        write_all("\x1B[?1016l\x1B[?1003h\x1B[?1006h");
        late_pixel_deadline_nanos_ = clock_.now_nanos() + kLatePixelReportGraceNanos;
    } else {
        decoder_.set_pixel_reports_possible(false);
        late_pixel_deadline_nanos_ = -1;
    }
    decoder_.set_sgr_mouse_input_suppressed(false);
    decoder_.require_verified_sixel_geometry(false);
    if (withheld_sixel_graphics_) {
        withheld_sixel_graphics_ = false;
        if (!observed_caps_.sixel_graphics) {
            // A host that already advertised Sixel may omit a later geometry
            // reply. End the resize fence using that established host proof.
            observed_caps_.sixel_graphics = true;
            decoder_.finish_capability_probe_window(observed_caps_);
        }
    }
    if (update_effective_capabilities()) events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    decoder_.set_capability_update_policy(CapabilityUpdatePolicy::AcceptVerifiedLiveRefinements);
    negotiate_kitty_enhancements();
    if (trace_) trace_.line(terminal_host_summary(caps_, last_size_, overrides_));
}

void WindowsTerminal::negotiate_kitty_enhancements() {
    if (kitty_flags_negotiated_ || observed_caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    kitty_flags_negotiated_ = true;
    write_all("\x1B[>" + std::to_string(kKittyRequestedFlags) + "u");
    kitty_push_active_ = true;
    write_all("\x1B[?u");
}

void WindowsTerminal::maybe_demote_kitty_keyboard() {
    if (kitty_demoted_ || !kitty_push_active_ ||
        observed_caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    const int flags = observed_caps_.kitty_keyboard_flags;
    if ((flags & kKittyReportAllKeysAsEscapeCodes) == 0 ||
        (flags & kKittyReportAssociatedText) != 0) return;
    kitty_demoted_ = true;
    write_all("\x1B[<u");
    kitty_push_active_ = false;
    write_all("\x1B[>" + std::to_string(kKittyBaselineFlags) + "u");
    kitty_push_active_ = true;
    write_all("\x1B[?u");
}

void WindowsTerminal::observe_resize(std::vector<TerminalEvent>& events) {
    const Size observed = size();
    if (observed == last_size_) return;
    last_size_ = observed;
    decoder_.set_cell_grid(observed);
    events.push_back(TerminalEvent{ResizeEvent{observed}});
    if (!enable_probes_) return;
    if (observed_caps_.sixel_graphics || observed_caps_.pixel_mouse ||
        observed_caps_.cell_pixels.width != 0 || observed_caps_.cell_pixels.height != 0 ||
        observed_caps_.text_area_pixels.width != 0 || observed_caps_.text_area_pixels.height != 0 ||
        observed_caps_.sixel_max_geometry.width != 0 || observed_caps_.sixel_max_geometry.height != 0) {
        withheld_sixel_graphics_ = withheld_sixel_graphics_ || observed_caps_.sixel_graphics;
        observed_caps_.sixel_graphics = false;
        observed_caps_.pixel_mouse = false;
        observed_caps_.cell_pixels = {};
        observed_caps_.text_area_pixels = {};
        observed_caps_.sixel_max_geometry = {};
        decoder_.require_verified_sixel_geometry(true);
        decoder_.set_capabilities(observed_caps_);
        if (update_effective_capabilities()) events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    }
    begin_probes();
}

void WindowsTerminal::append_decoded(std::vector<TerminalEvent> decoded,
                                     std::vector<TerminalEvent>& events) {
    for (auto& event : decoded) {
        if (auto* changed = std::get_if<CapabilityChangedEvent>(&event)) {
            changed->capabilities.clipboard_write = true;
            if (changed->capabilities == observed_caps_) continue;
            const bool late_pixel_proof = probe_deadline_nanos_ < 0 && !observed_caps_.pixel_mouse &&
                                          changed->capabilities.pixel_mouse;
            observed_caps_ = changed->capabilities;
            // The bounded probe may already have reset 1016 before its last
            // pixel report reached ConPTY. Direct report proof re-arms the
            // mode so subsequent input uses the newly established metric.
            if (late_pixel_proof) write_all("\x1B[?1016h");
            decoder_.set_sgr_mouse_input_suppressed(probe_deadline_nanos_ >= 0 &&
                                                    observed_caps_.mouse_protocol == MouseProtocol::SGR &&
                                                    !observed_caps_.pixel_mouse);
            if (probe_deadline_nanos_ < 0) negotiate_kitty_enhancements();
            maybe_demote_kitty_keyboard();
            if (!update_effective_capabilities()) continue;
            changed->capabilities = caps_;
        }
        events.push_back(std::move(event));
    }
}

bool WindowsTerminal::update_effective_capabilities() noexcept {
    const auto effective = apply_capability_overrides(observed_caps_, overrides_);
    if (effective == caps_) return false;
    caps_ = effective;
    return true;
}
void WindowsTerminal::set_capability_overrides(CapabilityOverrides overrides) {
    if ((overrides.cell_pixels && (overrides.cell_pixels->width <= 0 || overrides.cell_pixels->height <= 0)) ||
        (overrides.sixel_color_registers && *overrides.sixel_color_registers <= 0))
        throw std::invalid_argument("terminal capability overrides require positive cell metrics and color caps");
    if (overrides == overrides_) return;
    overrides_ = std::move(overrides);
    if (update_effective_capabilities()) { capability_change_pending_ = true; wake(); }
}

std::vector<TerminalEvent> WindowsTerminal::poll(std::int64_t deadline_nanos) {
    return poll(deadline_nanos, {});
}

std::vector<TerminalEvent> WindowsTerminal::poll(
    std::int64_t deadline_nanos, std::span<const WaitHandle> additional_wait_handles) {
    std::vector<TerminalEvent> events;
    if (!active_) return events;
    if (capability_change_pending_) {
        capability_change_pending_ = false;
        events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    }
    observe_resize(events);
    poll_handles_.assign(wait_handles_.begin(), wait_handles_.end());
    poll_handles_.insert(poll_handles_.end(), additional_wait_handles.begin(), additional_wait_handles.end());
    const auto expire_probes = [this, &events](std::int64_t now) {
        if (probe_deadline_nanos_ >= 0 && now >= probe_deadline_nanos_) finish_probes(events);
        if (late_pixel_deadline_nanos_ >= 0 && now >= late_pixel_deadline_nanos_) {
            late_pixel_deadline_nanos_ = -1;
            decoder_.set_pixel_reports_possible(false);
        }
    };
    const std::int64_t now = clock_.now_nanos();
    expire_probes(now);
    std::int64_t effective_deadline = events.empty() ? deadline_nanos : now;
    // A quiet console must still close the probe window and complete a
    // verified keyboard negotiation, even when the caller waits indefinitely.
    if (probe_deadline_nanos_ >= 0)
        effective_deadline = std::min(effective_deadline, probe_deadline_nanos_);
    if (late_pixel_deadline_nanos_ >= 0)
        effective_deadline = std::min(effective_deadline, late_pixel_deadline_nanos_);
    if (const auto decoder_deadline = decoder_.next_timeout_nanos())
        effective_deadline = std::min(effective_deadline, *decoder_deadline);
    const auto ready = wait_set_.wait(effective_deadline, poll_handles_);
    if (std::find(ready.begin(), ready.end(), wait_handles_[1]) != ready.end()) ::ResetEvent(wake_event_);
    if (std::find(ready.begin(), ready.end(), wait_handles_[0]) != ready.end()) {
        // ReadConsoleW preserves supplementary Unicode in the VT character
        // stream. ReadFile with CP_UTF8 replaces each UTF-16 surrogate half
        // before the shared decoder can see it. Console records are inspected
        // only for non-character events, especially resize notifications.
        constexpr std::size_t kMaxPollBytes = 64 * 1024;
        std::size_t total_bytes = 0;
        while (total_bytes < kMaxPollBytes) {
            INPUT_RECORD record{};
            DWORD pending = 0;
            if (!::PeekConsoleInputW(input_, &record, 1, &pending)) {
                append_decoded(decoder_.abort_paste(), events);
                throw console_lost("PeekConsoleInputW");
            }
            if (pending == 0) break;
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown ||
                record.Event.KeyEvent.uChar.UnicodeChar == L'\0') {
                DWORD consumed = 0;
                if (!::ReadConsoleInputW(input_, &record, 1, &consumed)) {
                    append_decoded(decoder_.abort_paste(), events);
                    throw console_lost("ReadConsoleInputW");
                }
                if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) observe_resize(events);
                continue;
            }
            wchar_t units[4096];
            DWORD count = 0;
            if (!::ReadConsoleW(input_, units, static_cast<DWORD>(std::size(units)), &count, nullptr)) {
                append_decoded(decoder_.abort_paste(), events);
                throw console_lost("ReadConsoleW input");
            }
            if (count == 0) break;
            total_bytes += static_cast<std::size_t>(count) * sizeof(wchar_t);
            poll_bytes_.clear();
            poll_bytes_.reserve(static_cast<std::size_t>(count) * 3);
            for (DWORD i = 0; i < count; ++i) {
                const auto unit = static_cast<char32_t>(units[i]);
                if (pending_high_surrogate_ != 0) {
                    if (unit >= 0xDC00 && unit <= 0xDFFF) {
                        const auto high = static_cast<char32_t>(pending_high_surrogate_);
                        utf8::encode(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00), poll_bytes_);
                        pending_high_surrogate_ = 0;
                        continue;
                    }
                    utf8::encode(utf8::replacement_char, poll_bytes_);
                    pending_high_surrogate_ = 0;
                }
                if (unit >= 0xD800 && unit <= 0xDBFF) {
                    pending_high_surrogate_ = units[i];
                } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                    utf8::encode(utf8::replacement_char, poll_bytes_);
                } else {
                    utf8::encode(unit, poll_bytes_);
                }
            }
            const std::int64_t observed = clock_.now_nanos();
            expire_probes(observed);
            if (!poll_bytes_.empty()) append_decoded(decoder_.feed(poll_bytes_, observed), events);
        }
    }
    const std::int64_t after_wait = clock_.now_nanos();
    expire_probes(after_wait);
    append_decoded(decoder_.poll_timeout(after_wait), events);
    observe_resize(events);
    return events;
}

void WindowsTerminal::wake() noexcept {
    if (wake_event_ != nullptr) ::SetEvent(wake_event_);
}

void WindowsTerminal::restore() noexcept {
    if (!active_) return;
    active_ = false;
    unregister_win_console_slot(ctrl_slot_);
    ctrl_slot_ = -1;
    wait_set_.clear();
    if (kitty_push_active_) {
        write_restoration("\x1B[<u");
        kitty_push_active_ = false;
    }
    write_restoration(kRestore);
    write_restoration(kPointerShapeResetSequence);
    ::SetConsoleMode(input_, original_input_mode_);
    ::SetConsoleMode(output_, original_output_mode_);
    ::SetConsoleCP(original_input_cp_);
    ::SetConsoleOutputCP(original_output_cp_);
    ::CloseHandle(wake_event_);
    wake_event_ = nullptr;
}

void WindowsTerminal::write_diagnostic_after_restore(std::string_view message) noexcept {
    const HANDLE error = ::GetStdHandle(STD_ERROR_HANDLE);
    if (error == nullptr || error == INVALID_HANDLE_VALUE) return;
    write_best_effort(error, message);
}

[[noreturn]] void WindowsTerminal::terminate_after_callback_failure() noexcept {
    restore();
    write_diagnostic_after_restore("ckVision contract violation: application callback threw\n");
    std::abort();
}

void WindowsTerminal::write_all(std::string_view bytes) const {
    capture_output(bytes);
    while (!bytes.empty()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 64 * 1024));
        if (!::WriteFile(output_, bytes.data(), count, &written, nullptr) || written == 0)
            throw console_lost("WriteFile output");
        bytes.remove_prefix(written);
    }
}

void WindowsTerminal::set_output_capture(std::function<void(std::string_view)> capture) {
    capture_ = std::move(capture);
}

void WindowsTerminal::capture_output(std::string_view bytes) const noexcept {
    if (!capture_) return;
    try { capture_(bytes); } catch (...) {}
}

void WindowsTerminal::write_restoration(std::string_view bytes) const noexcept {
    capture_output(bytes);
    write_best_effort(output_, bytes);
}

void WindowsTerminal::write(std::string_view bytes) { write_all(bytes); }
void WindowsTerminal::set_title(std::string_view title) { write_all(osc_title_sequence(title)); }
void WindowsTerminal::bell() { write_all("\x07"); }

ClipboardWriteResult WindowsTerminal::write_clipboard(std::string_view text) {
    if (!caps_.clipboard_write) return {ClipboardWriteStatus::Unsupported};
    return clipboard_.write_text(text);
}

}  // namespace ckv::term
