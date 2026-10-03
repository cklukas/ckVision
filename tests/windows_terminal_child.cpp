// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// A real console child for the private ConPTY transport contract.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"

namespace {
BOOL WINAPI ignore_control_c(DWORD event) { return event == CTRL_C_EVENT ? TRUE : FALSE; }

void write(std::string_view bytes) {
    (void)std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    (void)std::fflush(stdout);
}

bool read_size(COORD& cells) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE), &info)) return false;
    cells.X = static_cast<SHORT>(info.srWindow.Right - info.srWindow.Left + 1);
    cells.Y = static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top + 1);
    return true;
}

void print_size() {
    COORD cells{};
    if (!read_size(cells)) {
        write("SIZE-ERROR\n");
        return;
    }
    write("SIZE:" + std::to_string(cells.X) + "x" + std::to_string(cells.Y) + "\n");
}

int parse_positive_extent(const char* text) {
    int value = 0;
    const char* const end = text + std::strlen(text);
    const auto [next, error] = std::from_chars(text, end, value);
    return error == std::errc{} && next == end && value > 0 ? value : 0;
}

bool wait_for_size(int columns, int lines) {
    // Input may become readable before ConPTY applies the final resize.
    const ULONGLONG deadline = ::GetTickCount64() + 8'000;
    do {
        COORD cells{};
        if (read_size(cells) && cells.X == columns && cells.Y == lines) return true;
        ::Sleep(5);
    } while (::GetTickCount64() < deadline);
    return false;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 90;
    if (std::strcmp(argv[1], "outer-probe") == 0 ||
        std::strcmp(argv[1], "outer-no-replies") == 0) {
        const bool expect_kitty = std::strcmp(argv[1], "outer-probe") == 0;
        const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
        const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD input_mode = 0;
        DWORD output_mode = 0;
        const bool original_modes = ::GetConsoleMode(input, &input_mode) &&
                                    ::GetConsoleMode(output, &output_mode);
        const UINT input_cp = ::GetConsoleCP();
        const UINT output_cp = ::GetConsoleOutputCP();
        ckv::term::WindowsClock clock;
        bool negotiated = false;
        bool legacy_fallback = false;
        {
            ckv::term::WindowsTerminal terminal(clock);
            const auto deadline = clock.now_nanos() + 2'000'000'000LL;
            do {
                (void)terminal.poll(std::numeric_limits<std::int64_t>::max());
                const auto caps = terminal.capabilities();
                negotiated = caps.keyboard_protocol == ckv::term::KeyboardProtocol::Kitty &&
                             caps.kitty_keyboard_flags == ckv::term::kKittyRequestedFlags;
                legacy_fallback = caps.keyboard_protocol == ckv::term::KeyboardProtocol::Legacy &&
                                  !caps.pixel_mouse;
            } while (clock.now_nanos() < deadline && expect_kitty && !negotiated);
            terminal.restore();
        }
        DWORD restored_input = 0;
        DWORD restored_output = 0;
        const bool restored = original_modes && ::GetConsoleMode(input, &restored_input) &&
                              ::GetConsoleMode(output, &restored_output) &&
                              restored_input == input_mode && restored_output == output_mode &&
                              ::GetConsoleCP() == input_cp && ::GetConsoleOutputCP() == output_cp;
        const bool passed = restored && (expect_kitty ? negotiated : legacy_fallback);
        write(passed ? "OUTER-PROBE-OK\n" : "OUTER-PROBE-FAIL\n");
        return passed ? 0 : 97;
    }
    if (std::strcmp(argv[1], "outer-live-mouse") == 0 ||
        std::strcmp(argv[1], "outer-live-drag") == 0) {
        if (argc < 3) return 101;
        const bool require_drag = std::strcmp(argv[1], "outer-live-drag") == 0;
        std::ofstream record(argv[2], std::ios::binary | std::ios::trunc);
        if (!record) return 102;
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        const std::int64_t start = clock.now_nanos();
        const std::int64_t deadline = start + (require_drag ? 60'000'000'000LL : 180'000'000'000LL);
        terminal.write(require_drag ? "Drag inside this window to record live mouse motion.\r\n"
                                    : "Click once in this window to record a complete left-click gesture.\r\n");
        bool recorded_capabilities = false;
        bool saw_down = false;
        bool saw_held_move = false;
        bool saw_up = false;
        int mouse_events = 0;
        while (clock.now_nanos() < deadline && !saw_up) {
            for (const auto& event : terminal.poll(clock.now_nanos() + 250'000'000LL)) {
                if (const auto* mouse = std::get_if<ckv::MouseEvent>(&event)) {
                    record << "MOUSE action=" << static_cast<int>(mouse->action)
                           << " button=" << static_cast<int>(mouse->button)
                           << " cell=" << mouse->cell.x << ',' << mouse->cell.y;
                    if (mouse->pixel) record << " pixel=" << mouse->pixel->x << ',' << mouse->pixel->y;
                    else record << " pixel=none";
                    record << '\n' << std::flush;
                    if (mouse->button == ckv::MouseButton::Left) {
                        if (mouse->action == ckv::MouseAction::Down) {
                            saw_down = true;
                            saw_held_move = false;
                        } else if (mouse->action == ckv::MouseAction::Move && saw_down) {
                            saw_held_move = true;
                        } else if (mouse->action == ckv::MouseAction::Up) {
                            if (saw_down && (!require_drag || saw_held_move)) saw_up = true;
                            else {
                                saw_down = false;
                                saw_held_move = false;
                            }
                        }
                    }
                    ++mouse_events;
                }
            }
            if (!recorded_capabilities && clock.now_nanos() >= start + 750'000'000LL) {
                const auto caps = terminal.capabilities();
                record << "CAP mouse_protocol=" << static_cast<int>(caps.mouse_protocol)
                       << " pixel_mouse=" << caps.pixel_mouse
                       << " cell_pixels=" << caps.cell_pixels.width << 'x' << caps.cell_pixels.height
                       << " sixel=" << caps.sixel_graphics << '\n' << std::flush;
                recorded_capabilities = true;
            }
        }
        terminal.restore();
        const bool passed = saw_up;
        record << "END mouse_events=" << mouse_events << " down=" << saw_down
               << " held_move=" << saw_held_move << " up=" << saw_up << '\n' << std::flush;
        return passed ? 0 : (require_drag ? 107 : 103);
    }
    if (std::strcmp(argv[1], "outer-live-clipboard") == 0) {
        if (argc < 3) return 104;
        std::ofstream record(argv[2], std::ios::binary | std::ios::trunc);
        if (!record) return 105;
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        const std::string expected = "CKV-LIVE-CLIPBOARD-\xE2\x9C\x93-\xF0\x9F\x98\x80";
        const auto exported = terminal.write_clipboard(expected);
        record << "EXPORT status=" << static_cast<int>(exported.status)
               << " native_error=" << exported.native_error << " bytes=" << expected.size() << '\n' << std::flush;
        if (exported.status != ckv::ClipboardWriteStatus::Ok) return 108;
        terminal.write("Press Ctrl+V to round-trip the native clipboard.\r\n");
        const std::int64_t deadline = clock.now_nanos() + 180'000'000'000LL;
        bool matched = false;
        while (clock.now_nanos() < deadline && !matched) {
            for (const auto& event : terminal.poll(clock.now_nanos() + 250'000'000LL)) {
                if (const auto* text = std::get_if<ckv::TextEvent>(&event); text && text->from_paste) {
                    matched = text->text == expected;
                    record << "PASTE bytes=" << text->text.size() << " matched=" << matched
                           << " hex=";
                    constexpr char digits[] = "0123456789ABCDEF";
                    for (const unsigned char byte : text->text) {
                        record << digits[byte >> 4] << digits[byte & 0x0F];
                    }
                    record << '\n' << std::flush;
                }
            }
        }
        terminal.restore();
        record << "END matched=" << matched << '\n' << std::flush;
        return matched ? 0 : 106;
    }
    if (std::strcmp(argv[1], "outer-paste") == 0) {
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        terminal.write("OUTER-PASTE-READY");
        const auto deadline = clock.now_nanos() + 2'000'000'000LL;
        bool pasted = false;
        while (clock.now_nanos() < deadline && !pasted) {
            for (const auto& event : terminal.poll(deadline)) {
                const auto* text = std::get_if<ckv::TextEvent>(&event);
                if (text != nullptr && text->from_paste &&
                    text->text == "clipboard text \xCE\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80")
                    pasted = true;
            }
        }
        terminal.restore();
        write(pasted ? "OUTER-PASTE-OK\n" : "OUTER-PASTE-FAIL\n");
        return pasted ? 0 : 98;
    }
    if (std::strcmp(argv[1], "outer-pixel-mouse") == 0) {
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        terminal.write("PIXEL-MOUSE-READY");
        const std::int64_t deadline = clock.now_nanos() + 5'000'000'000LL;
        bool verified = false;
        while (clock.now_nanos() < deadline && !verified) {
            (void)terminal.poll(clock.now_nanos() + 100'000'000LL);
            const auto caps = terminal.capabilities();
            verified = caps.mouse_protocol == ckv::term::MouseProtocol::SGR &&
                       caps.pixel_mouse && caps.cell_pixels == ckv::PixelSize{10, 20};
        }
        if (!verified) {
            terminal.restore();
            write("PIXEL-MOUSE-NO-VERIFIED-METRIC\n");
            return 109;
        }
        terminal.write("PIXEL-MOUSE-ARMED");
        bool down = false;
        bool moved = false;
        bool up = false;
        while (clock.now_nanos() < deadline && !up) {
            for (const auto& event : terminal.poll(clock.now_nanos() + 100'000'000LL)) {
                const auto* mouse = std::get_if<ckv::MouseEvent>(&event);
                if (mouse == nullptr || mouse->button != ckv::MouseButton::Left ||
                    mouse->cell != ckv::Point{3, 0} || !mouse->pixel.has_value())
                    continue;
                if (mouse->action == ckv::MouseAction::Down &&
                    *mouse->pixel == ckv::PixelPoint{30, 4}) down = true;
                if (mouse->action == ckv::MouseAction::Move && down &&
                    *mouse->pixel == ckv::PixelPoint{31, 4}) moved = true;
                if (mouse->action == ckv::MouseAction::Up && down && moved &&
                    *mouse->pixel == ckv::PixelPoint{31, 4}) up = true;
            }
        }
        terminal.restore();
        const bool passed = verified && down && moved && up;
        write(passed ? "PIXEL-MOUSE-OK\n" : "PIXEL-MOUSE-FAIL\n");
        return passed ? 0 : 110;
    }
    if (std::strcmp(argv[1], "outer-plain-vt-mouse") == 0) {
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        const std::int64_t probe_end = clock.now_nanos() + 350'000'000LL;
        while (clock.now_nanos() < probe_end)
            (void)terminal.poll(clock.now_nanos() + 50'000'000LL);
        const auto caps = terminal.capabilities();
        const bool plain = caps.keyboard_protocol == ckv::term::KeyboardProtocol::Legacy &&
                           caps.mouse_protocol == ckv::term::MouseProtocol::SGR &&
                           !caps.pixel_mouse && !caps.sixel_graphics &&
                           caps.cell_pixels == ckv::PixelSize{};
        terminal.write("PLAIN-VT-MOUSE-READY");
        bool down = false;
        bool up = false;
        const std::int64_t deadline = clock.now_nanos() + 5'000'000'000LL;
        while (clock.now_nanos() < deadline && !up) {
            for (const auto& event : terminal.poll(clock.now_nanos() + 100'000'000LL)) {
                const auto* mouse = std::get_if<ckv::MouseEvent>(&event);
                if (mouse == nullptr || mouse->button != ckv::MouseButton::Left ||
                    mouse->cell != ckv::Point{9, 4} || mouse->pixel.has_value())
                    continue;
                if (mouse->action == ckv::MouseAction::Down) down = true;
                if (mouse->action == ckv::MouseAction::Up && down) up = true;
            }
        }
        terminal.restore();
        const bool passed = plain && down && up;
        write(passed ? "PLAIN-VT-MOUSE-OK\n" : "PLAIN-VT-MOUSE-FAIL\n");
        return passed ? 0 : 108;
    }
    if (std::strcmp(argv[1], "outer-resize") == 0) {
        ckv::term::WindowsClock clock;
        ckv::term::WindowsTerminal terminal(clock);
        terminal.write("OUTER-RESIZE-READY");
        const auto deadline = clock.now_nanos() + 3'000'000'000LL;
        while (clock.now_nanos() < deadline && !terminal.capabilities().sixel_graphics)
            (void)terminal.poll(deadline);
        if (!terminal.capabilities().sixel_graphics) {
            terminal.restore();
            write("OUTER-RESIZE-NO-INITIAL-GRAPHICS\n");
            return 99;
        }
        terminal.write("OUTER-RESIZE-ARMED");
        bool saw_resize = false;
        bool withheld = false;
        bool restored = false;
        while (clock.now_nanos() < deadline && !restored) {
            for (const auto& event : terminal.poll(deadline)) {
                if (const auto* resize = std::get_if<ckv::ResizeEvent>(&event))
                    saw_resize = resize->cells == ckv::Size{57, 10};
                if (const auto* changed = std::get_if<ckv::term::CapabilityChangedEvent>(&event)) {
                    if (saw_resize && !changed->capabilities.sixel_graphics) withheld = true;
                    if (saw_resize && withheld && changed->capabilities.sixel_graphics)
                        restored = true;
                }
            }
        }
        const bool passed = saw_resize && withheld && restored &&
                            terminal.capabilities().sixel_graphics &&
                            terminal.capabilities().sixel_max_geometry == ckv::PixelSize{};
        terminal.restore();
        write(passed ? "OUTER-RESIZE-OK\n" : "OUTER-RESIZE-FAIL\n");
        return passed ? 0 : 100;
    }
    if (std::strcmp(argv[1], "output") == 0) {
        write("PRIVATE-HELLO\n");
        return 7;
    }
    if (std::strcmp(argv[1], "echo") == 0) {
        write("READY> ");
        std::string line;
        if (!std::getline(std::cin, line)) return 91;
        write("ECHO:" + line + "\n");
        return 0;
    }
    if (std::strcmp(argv[1], "alternate") == 0) {
        write("\x1b[?1049h\x1b[2J\x1b[HALT-SCREEN");
        std::string line;
        if (!std::getline(std::cin, line)) return 92;
        write("\x1b[?1049lPRIMARY-DONE\n");
        return 0;
    }
    if (std::strcmp(argv[1], "resize") == 0) {
        print_size();
        std::string line;
        if (!std::getline(std::cin, line)) return 93;
        if (argc == 4) {
            const int expected_columns = parse_positive_extent(argv[2]);
            const int expected_lines = parse_positive_extent(argv[3]);
            if (expected_columns == 0 || expected_lines == 0) return 94;
            const bool matched = wait_for_size(expected_columns, expected_lines);
            print_size();
            return matched ? 0 : 95;
        }
        print_size();
        return 0;
    }
    if (std::strcmp(argv[1], "sixel") == 0) {
        DWORD mode = 0;
        const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (!::GetConsoleMode(output, &mode) ||
            !::SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            return 96;
        write("\x1bPq#0;2;100;0;0~\x1b\\");
        write("\x1b[2;1HGRAPHIC-DONE\n");
        return 0;
    }
    if (std::strcmp(argv[1], "sixel-hold") == 0 ||
        std::strcmp(argv[1], "sixel-abrupt-exit") == 0) {
        const bool abrupt_exit = std::strcmp(argv[1], "sixel-abrupt-exit") == 0;
        if (argc < 3 || (std::strcmp(argv[2], "red") != 0 &&
                         std::strcmp(argv[2], "green") != 0)) return 109;
        DWORD mode = 0;
        const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (!::GetConsoleMode(output, &mode) ||
            !::SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            return 110;
        const bool red = std::strcmp(argv[2], "red") == 0;
        write(red ? "\x1bPq#0;2;100;0;0~\x1b\\" : "\x1bPq#0;2;0;100;0~\x1b\\");
        write(red ? "\x1b[2;1HRED-READY\n" : "\x1b[2;1HGREEN-READY\n");
        std::string line;
        if (!std::getline(std::cin, line)) return 111;
        if (abrupt_exit) {
            // Terminate without C++ destructors or terminal cleanup.
            (void)::TerminateProcess(::GetCurrentProcess(), 37);
            return 112;
        }
        if (argc == 5 && !abrupt_exit) {
            const int expected_columns = parse_positive_extent(argv[3]);
            const int expected_lines = parse_positive_extent(argv[4]);
            if (expected_columns == 0 || expected_lines == 0) return 115;
            const bool matched = wait_for_size(expected_columns, expected_lines);
            print_size();
            if (!matched) return 116;
        }
        write(std::string(red ? "RED-ECHO:" : "GREEN-ECHO:") + line + "\n");
        return 0;
    }
    if (std::strcmp(argv[1], "sixel-limit-recover") == 0) {
        DWORD mode = 0;
        const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (!::GetConsoleMode(output, &mode) ||
            !::SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            return 113;
        write(std::string("\x1bPq#0;2;100;0;0") + std::string(512, '~') + "\x1b\\");
        write("\x1b[2;1HREJECTED-READY\n");
        std::string line;
        if (!std::getline(std::cin, line)) return 114;
        write("\x1b[3;1H\x1bPq#0;2;100;0;0~\x1b\\");
        write("\x1b[4;1HRECOVERED:" + line + "\n");
        return 0;
    }
    if (std::strcmp(argv[1], "flood") == 0) {
        const std::string run(100'000, 'x');
        write(run);
        write("\nFLOOD-TAIL\n");
        return 0;
    }
    if (std::strcmp(argv[1], "linger-tree") == 0) {
        // CreateProcess children inherit their parent's job unless breakaway
        // is enabled (Microsoft's Job Objects contract). No breakaway here.
        wchar_t executable[32'768]{};
        const DWORD length = ::GetModuleFileNameW(nullptr, executable, 32'768);
        if (length == 0 || length >= 32'768) return 115;
        std::wstring command = L"\"" + std::wstring(executable, length) + L"\" linger";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!::CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return 116;
        (void)::CloseHandle(process.hThread);
        (void)::CloseHandle(process.hProcess);
        (void)::SetConsoleCtrlHandler(ignore_control_c, TRUE);
        write("DESCENDANT:" + std::to_string(process.dwProcessId) + ":END\nLINGER-READY\n");
        for (;;) ::Sleep(1000);
    }
    if (std::strcmp(argv[1], "linger") == 0) {
        (void)::SetConsoleCtrlHandler(ignore_control_c, TRUE);
        write("LINGER-READY\n");
        for (;;) ::Sleep(1000);
    }
    if (std::strcmp(argv[1], "arguments") == 0) {
        char environment[128]{};
        const DWORD count = ::GetEnvironmentVariableA("CKV_CHILD_TEST", environment,
                                                       static_cast<DWORD>(sizeof(environment)));
        if (count == 0 || count >= sizeof(environment)) return 94;
        write("ARG:" + std::string(argc > 2 ? argv[2] : "<missing>") + "\n");
        write("ENV:" + std::string(environment) + "\n");
        return 0;
    }
    return 95;
}
