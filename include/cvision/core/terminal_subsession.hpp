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

// Whether a child may put text on the clipboard with OSC 52.
//
// Opt-in, because it is the one child-initiated action that reaches outside
// the terminal window: a program that writes the clipboard replaces whatever
// the person using the computer had put there, without their having asked.
// That is a feature when it is `yank` in an editor they are driving, and an
// attack when it is a file they happened to `cat`. Reading is not a policy
// choice — see `TerminalSnapshot::clipboard_text`.
enum class TerminalClipboardPolicy : std::uint8_t { Deny, AllowWrite };

// The kitty keyboard protocol's progressive-enhancement flags, as a child
// program turns them on for itself.
//
// The legacy encoding a terminal has always used cannot say some things: it
// spells Ctrl+I and Tab identically, has no way to distinguish a lone Escape
// from the start of a sequence except by waiting, and reports no key release
// at all. A program that needs those asks for them, one capability at a time,
// and reads back what it actually got — which is why the flags a session does
// not implement are dropped at the moment they are set rather than reported
// back as though they were in force.
enum class TerminalKeyboardFlags : std::uint8_t {
    None = 0,
    // Keys whose legacy encoding is ambiguous — Escape, and anything with
    // Ctrl or Alt — are sent as unambiguous escape codes instead.
    DisambiguateEscapeCodes = 1u << 0,
    // Press, repeat and release are distinguished, for the keys that are sent
    // as escape codes.
    ReportEventTypes = 1u << 1,
    // The shifted and base-layout forms of the key travel with it. ckVision's
    // input model carries one key and the text it produced, not the keyboard
    // layout behind them, so this one is not offered.
    ReportAlternateKeys = 1u << 2,
    // Every key becomes an escape code, including those that would otherwise
    // simply be text. This is what a program wants when it is reading keys
    // rather than reading a document.
    ReportAllKeysAsEscapeCodes = 1u << 3,
    // The text a key produced travels with the escape code, so a program can
    // have both the key and the character without decoding a layout itself.
    ReportAssociatedText = 1u << 4,
};

constexpr TerminalKeyboardFlags operator|(TerminalKeyboardFlags a, TerminalKeyboardFlags b) noexcept {
    return static_cast<TerminalKeyboardFlags>(static_cast<std::uint8_t>(a) |
                                              static_cast<std::uint8_t>(b));
}
constexpr TerminalKeyboardFlags operator&(TerminalKeyboardFlags a, TerminalKeyboardFlags b) noexcept {
    return static_cast<TerminalKeyboardFlags>(static_cast<std::uint8_t>(a) &
                                              static_cast<std::uint8_t>(b));
}
constexpr bool has_flag(TerminalKeyboardFlags set, TerminalKeyboardFlags flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0;
}

// The enhancements this terminal can actually deliver. A child's request is
// masked with this before it takes effect, so the flags it reads back are the
// ones that will really change what it receives — the protocol is designed to
// be asked, and answering with a promise we cannot keep is worse than
// answering with less.
constexpr TerminalKeyboardFlags supported_terminal_keyboard_flags() noexcept {
    return TerminalKeyboardFlags::DisambiguateEscapeCodes | TerminalKeyboardFlags::ReportEventTypes |
           TerminalKeyboardFlags::ReportAllKeysAsEscapeCodes |
           TerminalKeyboardFlags::ReportAssociatedText;
}
enum class TerminalExitPolicy : std::uint8_t { WaitForExit, TerminateAfterGrace };
enum class TerminalSubsessionState : std::uint8_t { Ready, Running, Exited, Failed, Closed };
enum class TerminalMouseEncoding : std::uint8_t { None, X10, Sgr };

struct TerminalCapabilityProfile {
    ::ckv::Size cells{80, 24};
    ::ckv::Size cell_pixels{9, 18};
    // Ordinary text sits below the brightest white the palette can reach, so
    // that the bold a program asks for has somewhere to go. Setting this to
    // the palette's own light grey leaves emphasis and body text a shade
    // apart, which is too little to read as emphasis at all.
    ::ckv::Style default_style{::ckv::Color::rgb(187, 187, 187), ::ckv::Color::rgb(0, 0, 0)};
    bool sixel = false;
    bool bracketed_paste = true;
    bool mouse_reporting = true;
    // The initial state of alternate scroll (DEC mode 1007), which the child
    // may turn off or back on. On, because a wheel over a full-screen program
    // that has not asked for mouse reporting is a request to scroll, and
    // `less` and `man` never ask: with it off they simply do not move, which
    // reads as a broken wheel rather than as a mode nobody enabled.
    bool alternate_scroll = true;
    TerminalQueryPolicy query_policy = TerminalQueryPolicy::DeclaredProfile;
    TerminalOscPolicy osc_policy = TerminalOscPolicy::Deny;
    TerminalClipboardPolicy clipboard_policy = TerminalClipboardPolicy::Deny;

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

// Where a child's environment comes from. A terminal is a place to run the
// programs one already has, and those programs expect the environment their
// owner configured: HOME, USER, LANG, an editor, a shell's own settings. A
// child given only what a host thought to list gets a machine that is not
// the one its user set up, and fails in ways ("HOME not set") that look
// like the program is broken.
//
// Sandboxed uses want the opposite, so the choice is stated rather than
// assumed -- but the default is the one that makes a terminal a terminal.
enum class TerminalEnvironmentPolicy {
    // Start from the environment this process was given; entries in
    // `environment` replace their namesakes and add the rest.
    InheritAndOverride,
    // Start from nothing: the child sees `environment` and no more.
    ExplicitOnly,
};

struct TerminalLaunchSpec {
    std::string executable;
    std::vector<std::string> arguments;
    // What the child sees as argv[0]. Empty means the executable path, which
    // is what a program expects and what almost every caller wants.
    //
    // It exists because of one convention a terminal cannot do without: a
    // shell is told it is a LOGIN shell by a leading '-' on its own argv[0]
    // ("-zsh"), and by nothing else. There is no flag every shell agrees on,
    // and the dash is how login(1), the terminal emulators, and tmux all say
    // it. A host that cannot set argv[0] cannot open the kind of shell its
    // user gets everywhere else, and their profile files never run.
    std::string argv0;
    std::string working_directory = "/";
    // Applied on top of whatever `environment_policy` starts from.
    std::vector<std::pair<std::string, std::string>> environment;
    TerminalEnvironmentPolicy environment_policy = TerminalEnvironmentPolicy::InheritAndOverride;
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
    // How much text a child may put on the clipboard in one OSC 52. Large
    // enough for the paragraph or the file listing somebody actually meant to
    // copy, small enough that a hostile child cannot use the clipboard as
    // storage for a megabyte the reader will never see.
    std::size_t max_clipboard_bytes = 64 * 1024;
    // How many lines that have scrolled off the top of the primary screen are
    // kept. Zero means none at all — a terminal that remembers nothing, which
    // is a real request (a shell reading a password, a host with a memory
    // budget) and not merely the smallest number: with zero, a line leaving
    // the screen is never copied anywhere, rather than copied and immediately
    // dropped. The alternate screen never contributes, whatever this says,
    // because a full-screen program's transient frames are not history.
    std::size_t max_scrollback_lines = 2'000;
    // The raster plane a child's Sixel is decoded into, in pixels. That plane
    // is the child's own window — its grid times the cell metric — so its
    // size is the host's choice and not something a child can inflate; the
    // quantity a child really controls is bounded by
    // `max_graphics_payload_bytes` above. This limit is therefore a guard
    // against absurd or overflowing geometry reaching the decoder's
    // allocator, and belongs far above any window a person could be looking
    // at: a 4K screen at 2x scaling is already ~33 Mpx. Set anywhere near a
    // real window it protects nothing and instead turns graphics silently off
    // for everyone with a large one — which is what 4 Mpx did.
    std::size_t max_image_pixels = 64 * 1024 * 1024;
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
    // DEC mode 1007: with the alternate screen up and the child not tracking
    // the mouse itself, a wheel notch is to be delivered as cursor keys.
    bool alternate_scroll_enabled = false;
    // The last text the child asked to put on the clipboard, and a count of
    // how many such requests have been granted. A consumer forwards the text
    // when the count differs from the one it last acted on; that keeps the
    // snapshot a value that can be read as often as anyone likes, rather than
    // a queue that reading empties.
    //
    // There is deliberately no way for a child to read the clipboard back
    // (D-022): an OSC 52 read is answered nowhere in ckVision, whatever the
    // policy says about writing, because a program that can read the
    // clipboard can read whatever the person using the computer last copied.
    std::string clipboard_text;
    std::uint64_t clipboard_serial = 0;
    // The kitty keyboard enhancements the child currently has on. Empty is
    // the legacy encoding, which remains correct — only coarser.
    TerminalKeyboardFlags keyboard_flags = TerminalKeyboardFlags::None;
    // How many times the child has rung the bell (BEL, 0x07). A count and not
    // a flag, for the same reason `clipboard_serial` is one: a snapshot is a
    // value that may be read as often as anyone likes, and a flag that reading
    // clears would make the first reader the only one to see the bell — while
    // a flag that reading does not clear cannot say a second bell arrived. A
    // host rings, flashes or badges when the count differs from the one it
    // last acted on, and a host that does nothing with it costs the child
    // nothing.
    std::uint64_t bell_serial = 0;
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
