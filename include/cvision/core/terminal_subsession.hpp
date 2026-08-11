// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Core contract for an embedded terminal child session (D-042).  The
// contract contains only deterministic values and operations; process and
// native-readiness details belong to the term layer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/cursor.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv::core {

enum class TerminalQueryPolicy : std::uint8_t { NoResponse, DeclaredProfile };
enum class TerminalOscPolicy : std::uint8_t { Deny, StoreMetadata };
enum class TerminalExitPolicy : std::uint8_t { WaitForExit, TerminateAfterGrace };
enum class TerminalSubsessionState : std::uint8_t { Ready, Running, Exited, Failed, Closed };
enum class TerminalMouseEncoding : std::uint8_t { None, X10, Sgr };

struct TerminalCapabilityProfile {
    ::ckv::Size cells{80, 24};
    ::ckv::Size cell_pixels{9, 18};
    ::ckv::Style default_style{::ckv::Color::rgb(229, 229, 229), ::ckv::Color::rgb(0, 0, 0)};
    bool sixel = false;
    bool bracketed_paste = true;
    bool mouse_reporting = true;
    TerminalQueryPolicy query_policy = TerminalQueryPolicy::DeclaredProfile;
    TerminalOscPolicy osc_policy = TerminalOscPolicy::Deny;

    friend bool operator==(const TerminalCapabilityProfile&, const TerminalCapabilityProfile&) = default;
};

// The conservative, deterministic profile used by the embedded-terminal
// example and all emulator tests. It is not derived from TERM or the parent
// terminal's capabilities.
constexpr TerminalCapabilityProfile embedded_xterm_sixel_profile() noexcept {
    TerminalCapabilityProfile profile;
    profile.sixel = true;
    return profile;
}

struct TerminalLaunchSpec {
    std::string executable;
    std::vector<std::string> arguments;
    std::string working_directory = "/";
    std::vector<std::pair<std::string, std::string>> environment;
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    TerminalExitPolicy exit_policy = TerminalExitPolicy::WaitForExit;

    static TerminalLaunchSpec program(std::string executable, std::vector<std::string> arguments = {}) {
        TerminalLaunchSpec spec;
        spec.executable = std::move(executable);
        spec.arguments = std::move(arguments);
        return spec;
    }
};

struct TerminalSubsessionOptions {
    std::size_t max_input_bytes = 64 * 1024;
    std::size_t max_output_bytes = 64 * 1024;
    std::size_t max_control_bytes = 16 * 1024;
    // Sixel payloads are graphics data, not ordinary CSI/OSC control
    // strings. Keep a separate bound so real images are not rejected by the
    // much smaller control-sequence limit.
    std::size_t max_graphics_payload_bytes = 4 * 1024 * 1024;
    std::size_t max_printable_run_bytes = 16 * 1024;
    std::size_t max_scrollback_lines = 2'000;
    std::size_t max_image_pixels = 4 * 1024 * 1024;
    std::size_t max_parser_work_per_step = 32 * 1024;
    ::ckv::Size max_cells{500, 300};
};

struct TerminalDiagnostic {
    enum class Kind : std::uint8_t { LimitExceeded, UnsupportedSequence, MalformedSequence, ChildExited };
    Kind kind = Kind::MalformedSequence;
    std::string message;

    friend bool operator==(const TerminalDiagnostic&, const TerminalDiagnostic&) = default;
};

struct TerminalRaster {
    int id = 0;
    ::ckv::Point anchor;
    ::ckv::Size cell_extent;
    std::shared_ptr<const ::ckv::Image> image;
    std::string fallback;
};

struct TerminalSnapshot {
    ::ckv::Size cells;
    std::vector<::ckv::Cell> cell_buffer;
    ::ckv::CursorState cursor;
    bool alternate_buffer = false;
    std::vector<TerminalRaster> rasters;
    std::vector<TerminalDiagnostic> diagnostics;
    std::string title;
    TerminalSubsessionState state = TerminalSubsessionState::Ready;
    std::vector<::ckv::Cell> scrollback;
    bool bracketed_paste_enabled = false;
    bool mouse_reporting_enabled = false;
    TerminalMouseEncoding mouse_encoding = TerminalMouseEncoding::None;
    bool application_cursor_keys = false;
    bool focus_reporting_enabled = false;
};

// UI-facing seam. Platform adapters may add readiness and child-process
// operations, but widgets consume only this deterministic core contract.
class TerminalSubsession {
public:
    virtual ~TerminalSubsession() = default;

    virtual TerminalSnapshot snapshot() const = 0;
    virtual const TerminalCapabilityProfile& profile() const noexcept = 0;
    virtual void feed_output(std::string_view bytes) = 0;
    virtual void resize(::ckv::Size cells, ::ckv::Size cell_pixels) = 0;
    virtual void send_input(std::string_view bytes) = 0;
    virtual std::string take_pending_input() = 0;
    virtual TerminalSubsessionState state() const noexcept = 0;
};

}  // namespace ckv::core
