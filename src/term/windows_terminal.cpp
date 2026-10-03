// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Windows console modes, VT character transport, and native clipboard are
// confined to the terminal layer. Input records supplement VT only for resize
// (D-020).
#include "cvision/term/windows_terminal.hpp"

#include "cvision/core/utf8.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cvision/term/osc_sequences.hpp"
#include "cvision/term/pointer_shape_names.hpp"

namespace ckv::term {
namespace {

constexpr std::string_view kEnter = "\x1B[?1049h\x1B[?1003h\x1B[?1006h\x1B[?2004h\x1B[?1004h";
constexpr std::string_view kRestore =
    "\x1B[?1003l\x1B[?1006l\x1B[?1016l\x1B[?2004l\x1B[?1004l\x1B[?2026l\x1B[?25h\x1B[0m\x1B[?1049l";
// Probe the published terminal replies, including kitty keyboard support.
// A verified kitty reply is followed by a session-owned push and readback.
constexpr std::string_view kProbe =
    "\x1B]10;?\x1B\\\x1B]11;?\x1B\\\x1B[?2026h\x1B[?2026$p\x1B[?2026l\x1B[c"
    "\x1B[?1;4;0S\x1B[?2;4;0S\x1B[?1016$p\x1B[16t\x1B[14t\x1B[?u";
constexpr std::int64_t kProbeNanos = 250'000'000;

DWORD wait_milliseconds(std::int64_t now, std::int64_t deadline) noexcept {
    if (deadline == std::numeric_limits<std::int64_t>::max()) return INFINITE;
    if (deadline <= now) return 0;
    const auto nanos = static_cast<std::uint64_t>(deadline) - static_cast<std::uint64_t>(now);
    const auto millis = 1U + (nanos - 1U) / 1'000'000U;
    return static_cast<DWORD>(std::min<std::uint64_t>(millis, INFINITE - 1U));
}

std::runtime_error win32_error(const char* operation) {
    return std::runtime_error(std::string("Windows terminal: ") + operation +
                              " failed (Win32 " + std::to_string(::GetLastError()) + ")");
}

void write_best_effort(HANDLE handle, std::string_view bytes) noexcept {
    while (!bytes.empty()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 64 * 1024));
        if (!::WriteFile(handle, bytes.data(), count, &written, nullptr) || written == 0) return;
        bytes.remove_prefix(written);
    }
}

}  // namespace

WindowsTerminal::WindowsTerminal(const Clock& clock, HANDLE output, HANDLE input)
    : clock_(clock), output_(output), input_(input), caps_(baseline_capabilities()), decoder_(caps_) {
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
    caps_.clipboard_write = true;  // Native CF_UNICODETEXT, independent of OSC 52.
    decoder_.set_capabilities(caps_);
    last_size_ = size();
    decoder_.set_cell_grid(last_size_);
    wait_handles_[0] = WaitHandle{WaitHandleKind::WindowsHandle,
                                  reinterpret_cast<std::uintptr_t>(input_)};
    wait_handles_[1] = WaitHandle{WaitHandleKind::WindowsHandle,
                                  reinterpret_cast<std::uintptr_t>(wake_event_)};
    active_ = true;
    try {
        write_all(kEnter);
        begin_probes();
    } catch (...) {
        restore();
        throw;
    }
}

WindowsTerminal::~WindowsTerminal() { restore(); }

Size WindowsTerminal::size() const noexcept {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!::GetConsoleScreenBufferInfo(output_, &info)) return Size{80, 24};
    return Size{static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1),
                static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1)};
}

void WindowsTerminal::begin_probes() {
    decoder_.begin_capability_probe_window(caps_);
    probe_deadline_nanos_ = clock_.now_nanos() + kProbeNanos;
    // Until the cell metric and mode reply establish pixel coordinates, an
    // SGR report cannot safely be interpreted as a cell click.
    decoder_.set_sgr_mouse_input_suppressed(caps_.mouse_protocol == MouseProtocol::SGR &&
                                            !caps_.pixel_mouse);
    write_all("\x1B[?1016h");
    write_all(kProbe);
}

void WindowsTerminal::finish_probes(std::vector<TerminalEvent>& events) {
    if (probe_deadline_nanos_ < 0) return;
    probe_deadline_nanos_ = -1;
    decoder_.finish_capability_probe_window(caps_);
    if (caps_.mouse_protocol == MouseProtocol::SGR && !caps_.pixel_mouse) {
        write_all("\x1B[?1016l\x1B[?1003h\x1B[?1006h");
    }
    decoder_.set_sgr_mouse_input_suppressed(false);
    decoder_.require_verified_sixel_geometry(false);
    if (withheld_sixel_graphics_) {
        withheld_sixel_graphics_ = false;
        if (!caps_.sixel_graphics) {
            // A host that already advertised Sixel may omit a later geometry
            // reply. End the resize fence using that established host proof.
            caps_.sixel_graphics = true;
            decoder_.finish_capability_probe_window(caps_);
            events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
        }
    }
    decoder_.set_capability_update_policy(CapabilityUpdatePolicy::AcceptVerifiedLiveRefinements);
    negotiate_kitty_enhancements();
}

void WindowsTerminal::negotiate_kitty_enhancements() {
    if (kitty_flags_negotiated_ || caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    kitty_flags_negotiated_ = true;
    write_all("\x1B[>" + std::to_string(kKittyRequestedFlags) + "u");
    kitty_push_active_ = true;
    write_all("\x1B[?u");
}

void WindowsTerminal::maybe_demote_kitty_keyboard() {
    if (kitty_demoted_ || !kitty_push_active_ ||
        caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    const int flags = caps_.kitty_keyboard_flags;
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
    if (caps_.sixel_graphics || caps_.pixel_mouse ||
        caps_.cell_pixels.width != 0 || caps_.cell_pixels.height != 0 ||
        caps_.sixel_max_geometry.width != 0 || caps_.sixel_max_geometry.height != 0) {
        withheld_sixel_graphics_ = withheld_sixel_graphics_ || caps_.sixel_graphics;
        caps_.sixel_graphics = false;
        caps_.pixel_mouse = false;
        caps_.cell_pixels = {};
        caps_.text_area_pixels = {};
        caps_.sixel_max_geometry = {};
        decoder_.require_verified_sixel_geometry(true);
        events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    }
    begin_probes();
}

void WindowsTerminal::append_decoded(std::vector<TerminalEvent> decoded,
                                     std::vector<TerminalEvent>& events) {
    for (auto& event : decoded) {
        if (auto* changed = std::get_if<CapabilityChangedEvent>(&event)) {
            changed->capabilities.clipboard_write = true;
            if (changed->capabilities == caps_) continue;
            const bool late_pixel_proof = probe_deadline_nanos_ < 0 && !caps_.pixel_mouse &&
                                          changed->capabilities.pixel_mouse;
            caps_ = changed->capabilities;
            // The bounded probe may already have reset 1016 before its last
            // pixel report reached ConPTY. Direct report proof re-arms the
            // mode so subsequent input uses the newly established metric.
            if (late_pixel_proof) write_all("\x1B[?1016h");
            decoder_.set_sgr_mouse_input_suppressed(probe_deadline_nanos_ >= 0 &&
                                                    caps_.mouse_protocol == MouseProtocol::SGR &&
                                                    !caps_.pixel_mouse);
            if (probe_deadline_nanos_ < 0) negotiate_kitty_enhancements();
            maybe_demote_kitty_keyboard();
        }
        events.push_back(std::move(event));
    }
}

std::vector<TerminalEvent> WindowsTerminal::poll(std::int64_t deadline_nanos) {
    return poll(deadline_nanos, {});
}

std::vector<TerminalEvent> WindowsTerminal::poll(
    std::int64_t deadline_nanos, std::span<const WaitHandle> additional_wait_handles) {
    std::vector<TerminalEvent> events;
    if (!active_) return events;
    observe_resize(events);
    std::vector<HANDLE> handles{input_, wake_event_};
    for (const WaitHandle handle : additional_wait_handles) {
        if (handle.kind == WaitHandleKind::WindowsHandle && handle.value != 0)
            handles.push_back(reinterpret_cast<HANDLE>(handle.value));
    }
    if (handles.size() > MAXIMUM_WAIT_OBJECTS)
        throw std::runtime_error("Windows terminal: too many wait handles");
    const std::int64_t now = clock_.now_nanos();
    if (probe_deadline_nanos_ >= 0 && now >= probe_deadline_nanos_) finish_probes(events);
    std::int64_t effective_deadline = events.empty() ? deadline_nanos : now;
    if (const auto decoder_deadline = decoder_.next_timeout_nanos())
        effective_deadline = std::min(effective_deadline, *decoder_deadline);
    if (probe_deadline_nanos_ >= 0)
        effective_deadline = std::min(effective_deadline, probe_deadline_nanos_);
    const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE,
                                                   wait_milliseconds(now, effective_deadline));
    if (result == WAIT_FAILED) throw win32_error("WaitForMultipleObjects");
    if (result == WAIT_OBJECT_0 + 1) ::ResetEvent(wake_event_);
    if (result == WAIT_OBJECT_0) {
        // ReadConsoleW preserves supplementary Unicode in the VT character
        // stream. ReadFile with CP_UTF8 replaces each UTF-16 surrogate half
        // before the shared decoder can see it. Console records are inspected
        // only for non-character events, especially resize notifications.
        for (int batch = 0; batch < 64; ++batch) {
            INPUT_RECORD record{};
            DWORD pending = 0;
            if (!::PeekConsoleInputW(input_, &record, 1, &pending))
                throw win32_error("PeekConsoleInputW");
            if (pending == 0) break;
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
                DWORD consumed = 0;
                if (!::ReadConsoleInputW(input_, &record, 1, &consumed))
                    throw win32_error("ReadConsoleInputW");
                if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) observe_resize(events);
                continue;
            }
            wchar_t units[4096];
            DWORD count = 0;
            if (!::ReadConsoleW(input_, units, static_cast<DWORD>(std::size(units)), &count, nullptr))
                throw win32_error("ReadConsoleW input");
            if (count == 0) break;
            std::string bytes;
            bytes.reserve(static_cast<std::size_t>(count) * 3);
            for (DWORD i = 0; i < count; ++i) {
                const auto unit = static_cast<char32_t>(units[i]);
                if (pending_high_surrogate_ != 0) {
                    if (unit >= 0xDC00 && unit <= 0xDFFF) {
                        const auto high = static_cast<char32_t>(pending_high_surrogate_);
                        utf8::encode(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00), bytes);
                        pending_high_surrogate_ = 0;
                        continue;
                    }
                    utf8::encode(utf8::replacement_char, bytes);
                    pending_high_surrogate_ = 0;
                }
                if (unit >= 0xD800 && unit <= 0xDBFF) {
                    pending_high_surrogate_ = units[i];
                } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                    utf8::encode(utf8::replacement_char, bytes);
                } else {
                    utf8::encode(unit, bytes);
                }
            }
            const std::int64_t observed = clock_.now_nanos();
            if (probe_deadline_nanos_ >= 0 && observed >= probe_deadline_nanos_) finish_probes(events);
            if (!bytes.empty()) append_decoded(decoder_.feed(bytes, observed), events);
        }
    }
    const std::int64_t after_wait = clock_.now_nanos();
    if (probe_deadline_nanos_ >= 0 && after_wait >= probe_deadline_nanos_) finish_probes(events);
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
    if (kitty_push_active_) {
        write_best_effort(output_, "\x1B[<u");
        kitty_push_active_ = false;
    }
    write_best_effort(output_, kRestore);
    write_best_effort(output_, kPointerShapeResetSequence);
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
    while (!bytes.empty()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 64 * 1024));
        if (!::WriteFile(output_, bytes.data(), count, &written, nullptr) || written == 0)
            throw win32_error("WriteFile output");
        bytes.remove_prefix(written);
    }
}

void WindowsTerminal::write(std::string_view bytes) { write_all(bytes); }
void WindowsTerminal::set_title(std::string_view title) { write_all(osc_title_sequence(title)); }
void WindowsTerminal::bell() { write_all("\x07"); }

ClipboardWriteResult WindowsTerminal::write_clipboard(std::string_view text) {
    if (!caps_.clipboard_write) return {ClipboardWriteStatus::Unsupported};
    return clipboard_.write_text(text);
}

}  // namespace ckv::term
