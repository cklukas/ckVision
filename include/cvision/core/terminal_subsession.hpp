// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Core contract for an embedded terminal child session (D-042).  The
// contract contains only deterministic values and operations; process and
// native-readiness details belong to the term layer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/cursor.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/process_launch.hpp"
#include "cvision/core/process_resources.hpp"

namespace ckv::core {

// Whether the terminal answers a child's status and capability queries (device attributes,
// cursor-position and mode reports, window and graphics geometry, colour and keyboard-protocol
// queries). NoResponse leaves every query unanswered, as a terminal without the feature would.
// DeclaredProfile answers from the TerminalCapabilityProfile, never from the parent terminal the
// host itself runs in, so the answers are deterministic.
enum class TerminalQueryPolicy : std::uint8_t { NoResponse, DeclaredProfile };
// Whether a child may set the window title (OSC 0 and 2, and the XTWINOPS 22/23 title stack).
// Deny ignores those requests; StoreMetadata keeps the title, sanitized and bounded by
// TerminalSubsessionOptions::max_title_bytes, for the host to show.
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

// What happens when a child asks the terminal to print (Media Copy).
//
// Printing was on this library's never-list, and the reason was right about
// the danger it named: a child must not be able to write to a printer. But
// that danger is the SINK, not the sequence — xterm implements MC by piping
// the captured stream to a shell command, and ships that command empty
// precisely because `cat`-ing a hostile file must not reach a device.
//
// `Capture` replaces the sink and the hazard goes with it: the stream is
// collected into a bounded in-memory spool and handed to the host, which
// shows it to a person. No device is opened, no command is run, and nothing
// reaches a disk without whatever the host asks its user first.
//
// The default is `Deny`, so no existing application changes behaviour by
// upgrading: a child that asks to print is told there is no printer, which is
// the truth for a host that has not opted in.
enum class TerminalPrinterPolicy : std::uint8_t { Deny, Capture };

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

// Bitwise union and intersection of two flag sets, and whether `set` contains any bit of `flag`
// (for a single flag: whether it is on).
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
// What `close()` — and therefore the destructor — may do to a child that has
// not exited. There is deliberately NO default: `Unspecified` is the value a
// caller gets by forgetting, and a launch carrying it fails immediately rather
// than picking one on the caller's behalf.
//
// The two policies differ in whether they are BOUNDED, which is the property a
// caller must choose knowingly:
//   * `WaitForExit` requests platform-native graceful termination (SIGHUP and
//     SIGTERM on POSIX, Control-C through ConPTY on Windows) and waits for the
//     child to exit without escalating. A child that ignores that request can
//     block the destructor indefinitely.
//   * `TerminateAfterGrace` escalates after the grace period (SIGKILL on
//     POSIX, termination of the child process job on Windows).
//
// The unbounded one is a legitimate choice for a child whose output must never
// be lost. It is not a reasonable thing to acquire by omission, which is what
// this enumeration's shape is for: the accident is a diagnosable state rather
// than a silent hang.
enum class TerminalExitPolicy : std::uint8_t { Unspecified, WaitForExit, TerminateAfterGrace };
// Where a session is in its life. Ready: created (and, for a process-backed session, launched)
// but no child output has arrived yet. Running: output has been received. Exited: the child ended
// and its exit code is known. Failed: the launch failed or the host reported a failure; a
// diagnostic says why. Closed: the host closed the session. Once Exited, Failed or Closed, further
// output and input are ignored.
enum class TerminalSubsessionState : std::uint8_t { Ready, Running, Exited, Failed, Closed };
// How the child asked for pointer reports to be encoded. None while it is not tracking the
// pointer at all; X10 for the legacy byte encoding (the default once tracking is on); Sgr once it
// has also set DEC mode 1006.
enum class TerminalMouseEncoding : std::uint8_t { None, X10, Sgr };
// How much of what the pointer does a child asked to hear about. The three DEC
// modes are three levels of one facility rather than three independent
// switches: 1000 reports presses and releases, 1002 adds motion while a button
// is held, and 1003 adds motion with no button held at all. A host that
// collapses them into "reporting is on" sends a program written for 1000 a
// stream of motion reports it never asked for — and a program that parses only
// what it asked for reads the surplus as something else entirely.
enum class TerminalMouseTracking : std::uint8_t { None, Buttons, ButtonMotion, AnyMotion };

// The terminal a child is told it is running in: its size, its colours, what it can do and what
// the host allows. It is a declaration the host chooses, not a probe of any real terminal, so the
// same profile gives the same behaviour everywhere. Apart from the printer policy (which
// TerminalSubsession::set_printer_policy changes at run time) and the sizes (which follow
// resize), it is fixed for the session's life.
struct TerminalCapabilityProfile {
    // The initial grid in cells, clamped by the session to between 1x1 and
    // TerminalSubsessionOptions::max_cells; and the pixel size of one cell, which Sixel placement
    // and the pixel geometry reported to the child are derived from.
    ::ckv::Size cells{80, 24};
    ::ckv::PixelSize cell_pixels{9, 18};
    // Ordinary text sits below the brightest white the palette can reach, so
    // that the bold a program asks for has somewhere to go. Setting this to
    // the palette's own light grey leaves emphasis and body text a shade
    // apart, which is too little to read as emphasis at all.
    ::ckv::Style default_style{::ckv::Color::rgb(187, 187, 187), ::ckv::Color::rgb(0, 0, 0)};
    // Whether Sixel graphics are offered: advertised in the device attributes and decoded when
    // sent. Off, a Sixel sequence is ignored with a diagnostic.
    bool sixel = false;
    // Whether the child may turn on bracketed paste (DEC mode 2004) and pointer reporting (DEC
    // modes 1000, 1002, 1003 and 1006). Off, the request is accepted but has no effect.
    bool bracketed_paste = true;
    bool mouse_reporting = true;
    // Whether this terminal can bracket a child's frame update as one
    // atomic change (DEC mode 2026) instead of exposing every write in
    // between. Unlike Sixel this costs nothing to support — the emulator
    // already holds whatever a child wrote until something reads it — so it
    // defaults on rather than needing a host to opt in.
    bool synchronized_output = true;
    // The initial state of alternate scroll (DEC mode 1007), which the child
    // may turn off or back on. On, because a wheel over a full-screen program
    // that has not asked for mouse reporting is a request to scroll, and
    // `less` and `man` never ask: with it off they simply do not move, which
    // reads as a broken wheel rather than as a mode nobody enabled.
    bool alternate_scroll = true;
    // What the child is permitted: answering its queries, keeping the title it sets, letting it
    // write the clipboard, and capturing what it prints. See each policy type; every one but
    // query answering defaults to the refusing choice.
    TerminalQueryPolicy query_policy = TerminalQueryPolicy::DeclaredProfile;
    TerminalOscPolicy osc_policy = TerminalOscPolicy::Deny;
    TerminalClipboardPolicy clipboard_policy = TerminalClipboardPolicy::Deny;
    TerminalPrinterPolicy printer_policy = TerminalPrinterPolicy::Deny;

    // Memberwise equality over every field above.
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
// Everything needed to start a child program in an embedded terminal. Build one with program()
// or windows_command_processor(), then name exit_policy: a spec left Unspecified produces a
// Failed session rather than a child.
struct TerminalLaunchSpec : ProcessLaunchSpec {
    // Invocation fields are inherited from the shared ProcessLaunchSpec.
    // The terminal the child is told it runs in, including its initial size.
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    // Must be named. See `TerminalExitPolicy` — a launch left `Unspecified`
    // fails rather than choosing for you.
    TerminalExitPolicy exit_policy = TerminalExitPolicy::Unspecified;

    // A spec for `executable` with `arguments` and every other field at its default. The exit
    // policy is still Unspecified, so the caller must set it before launching.
    static TerminalLaunchSpec program(std::string image, std::vector<std::string> values = {}) {
        TerminalLaunchSpec spec;
        static_cast<ProcessLaunchSpec&>(spec) = ProcessLaunchSpec::program(std::move(image), std::move(values));
        return spec;
    }

    // A Windows command-processor launch. The caller explicitly names
    // cmd.exe and optionally supplies trusted command syntax, not untrusted
    // data to be escaped. No command means interactive. AutoRun is disabled.
    // Exit policy remains Unspecified.
    static TerminalLaunchSpec windows_command_processor(std::string image,
                                                        std::optional<std::string> command = std::nullopt) {
        TerminalLaunchSpec spec;
        static_cast<ProcessLaunchSpec&>(spec) = ProcessLaunchSpec::windows_command_processor(std::move(image), std::move(command));
        return spec;
    }
};

// The resource bounds that keep a hostile or runaway child from exhausting the host, in bytes
// unless a field says otherwise. Exceeding a bound never fails the session: the excess is dropped
// or trimmed as each field describes, and most overflows are also recorded as a LimitExceeded
// diagnostic.
struct TerminalSubsessionOptions {
    // Bytes queued toward the child and not yet delivered: the host's input plus the terminal's
    // own query replies. Bytes past the bound are dropped.
    std::size_t max_input_bytes = 64 * 1024;
    // Child output accepted but not yet parsed. Output that arrives while the queue is full is
    // dropped.
    std::size_t max_output_bytes = 64 * 1024;
    // The longest control sequence or string (CSI, OSC, non-Sixel DCS) the parser collects. One
    // that grows past it is dropped, and the parser skips output up to where that sequence ends:
    // a CSI's final byte, an OSC's BEL or ST (ESC \), a DCS's ST.
    std::size_t max_control_bytes = 16 * 1024;
    // Sixel payloads are graphics data, not ordinary CSI/OSC control
    // strings. Keep a separate bound so real images are not rejected by the
    // much smaller control-sequence limit.
    std::size_t max_graphics_payload_bytes = 4 * 1024 * 1024;
    // The longest run of printable bytes buffered before it is written to the grid. A longer run
    // is not lost: it is written in pieces, with a diagnostic at each split.
    std::size_t max_printable_run_bytes = 16 * 1024;
    // How much text a child may put on the clipboard in one OSC 52. Large
    // enough for the paragraph or the file listing somebody actually meant to
    // copy, small enough that a hostile child cannot use the clipboard as
    // storage for a megabyte the reader will never see.
    std::size_t max_clipboard_bytes = 64 * 1024;
    // How long a title a child may set. A window caption is a line of text on
    // a frame, so anything past a sentence is already unreadable — but the
    // real reason for a bound is that a snapshot carries the title by value:
    // a host reading one per terminal per frame would copy whatever a child
    // felt like sending, at frame rate, forever. Cut on a character boundary,
    // never mid-sequence.
    std::size_t max_title_bytes = 1'024;
    // How much captured print output one job may hold. A print job is held in
    // memory and shown to a person, so this is a page count in disguise: a
    // megabyte is several hundred pages of text, and past it what a child is
    // doing is not printing. Overflow frees the buffer rather than truncating
    // it — half a document that looks whole is worse than a job that says it
    // was too big (see TerminalPrinterJob::overflowed).
    std::size_t max_printer_spool_bytes = 1024 * 1024;
    // How many titles a child may push with XTWINOPS 22. A program that
    // pushes without ever popping — one that died between the two — must not
    // grow this without limit, and nothing legitimately nests captions deeper
    // than an editor inside a shell inside a build script.
    std::size_t max_title_stack_depth = 8;
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
    // How many queued output bytes one feed_output call parses; the rest stays queued for the
    // next call, so a flood of output cannot stall the host's frame. Zero is treated as one.
    std::size_t max_parser_work_per_step = 32 * 1024;
    // The largest grid, in columns and rows, a profile or resize may ask for; larger requests are
    // clamped to it (and smaller than 1x1 to 1x1).
    ::ckv::Size max_cells{500, 300};
};

// One complaint a terminal recorded about its child: something it refused, could not parse, or
// cut short. Diagnostics are observations for a host to log or show; none of them stops the
// session. A terminal keeps only the most recent ones (see TerminalStatus::diagnostics_serial).
struct TerminalDiagnostic {
    // LimitExceeded: a TerminalSubsessionOptions bound was hit. UnsupportedSequence: a sequence
    // this terminal does not implement, or one it refused by policy. MalformedSequence: bytes
    // that do not form a valid sequence or payload. ChildExited: the launch or the child failed
    // (the message says how).
    enum class Kind : std::uint8_t { LimitExceeded, UnsupportedSequence, MalformedSequence, ChildExited };
    // The category, and a human-readable description meant for a log, not for parsing.
    Kind kind = Kind::MalformedSequence;
    std::string message;

    // Memberwise equality.
    friend bool operator==(const TerminalDiagnostic&, const TerminalDiagnostic&) = default;
};

// One completed print job, as the child produced it.
//
// The text is verbatim: this is a document on its way to a person, and the
// escape sequences in it are what a program formatting a page for a printer
// meant to send. A host that renders it decides what to do with them — which
// is why `[printer] save-format` in an application can offer both plain text
// and the original stream.
struct TerminalPrinterJob {
    // The Media Copy request that produced a job.
    enum class Origin : std::uint8_t {
        // Everything the child sent between `CSI 5 i` and `CSI 4 i`.
        Controller,
        // `CSI 0 i` — the screen as it stands.
        Screen,
        // `CSI ? 1 i` — the line the cursor is on.
        Line,
        // `CSI ? 5 i` … `CSI ? 4 i` — each line as the cursor left it,
        // coalesced into one job rather than one job per line.
        Autoprint,
    };
    // Which kind of print request produced the job, and the job's bytes (empty when overflowed).
    Origin origin = Origin::Controller;
    std::string text;
    // The job exceeded `max_printer_spool_bytes` and its buffer was freed. The
    // text is empty rather than partial: a host that showed the first megabyte
    // of a longer document as though it were the document would be lying about
    // what was printed.
    bool overflowed = false;

    // Memberwise equality.
    friend bool operator==(const TerminalPrinterJob&, const TerminalPrinterJob&) = default;
};

// One picture a child has drawn (a decoded Sixel), placed on the terminal's grid.
struct TerminalRaster {
    // The scene raster id to draw it under: the host-assigned identity of its terminal plus a
    // slot from allocate_local_raster_slot. Zero while the terminal has no identity, and a view
    // does not draw a raster with id zero.
    int id = 0;
    // The top-left cell the picture starts at (0-based, in the terminal's grid) and how many
    // columns and rows it covers: its pixel size divided by the cell size, rounded up, at least
    // 1x1. The anchor moves with the text when the screen scrolls.
    ::ckv::Point anchor;
    ::ckv::Size cell_extent;
    // The decoded pixels, read-only to the host. Cells of the picture that child text overwrites
    // are erased from it, but a copy is made first whenever anyone else holds this pointer, so a
    // snapshot or a retained copy keeps the pixels as they were when it was taken.
    std::shared_ptr<const ::ckv::Image> image;
    // Text drawn at the anchor, over blanked cells, where the host cannot show images.
    std::string fallback;
    // Null means every cell of the anchor still belongs to the picture.
    // Otherwise one byte per cell, row-major: 1 is live, 0 was overwritten
    // by child text. Shared ownership keeps an earlier snapshot stable.
    std::shared_ptr<const std::vector<std::uint8_t>> live_cells = {};
};

// The smallest non-negative offset from `base` not already used as an id by
// any raster in `existing`. A terminal's own identity (`base`) is one value
// for its whole lifetime, but a program that draws a second picture without
// clearing the first — ordinary enough that a terminal keeps up to 64 at
// once — leaves more than one raster alive together. Surface::add_raster_region
// requires a distinct id per drawn region, not per terminal, so a caller
// building a TerminalRaster's id combines `base` with this: stable for as
// long as that raster lives (assigned once, at placement), and distinct from
// every other raster currently alive on the same terminal. The span checked
// is bounded (kMaxRasters in terminal_emulator.cpp), so 4096 candidate slots
// is headroom no real terminal reaches.
inline int allocate_local_raster_slot(std::span<const TerminalRaster> existing, int base) noexcept {
    for (int slot = 0; slot < 4096; ++slot) {
        bool in_use = false;
        for (const TerminalRaster& raster : existing) {
            if (raster.id == base + slot) { in_use = true; break; }
        }
        if (!in_use) return slot;
    }
    return 0;
}

// What a host asks a snapshot to contain.
//
// It exists because the expensive parts are the ones a caller most often does
// not want. `snapshot()` copies the grid, the whole bounded scrollback and the
// raster list every time it is called; a multiplexer diffing N terminals at
// its flush tick wants the grid and nothing else, and paying for the history
// on every frame is the difference between a cost proportional to what changed
// and one proportional to what the terminal remembers.
//
// Defaults are everything, so a caller that has not thought about it gets what
// it always got.
struct TerminalSnapshotOptions {
    // Whether the snapshot copies the scrollback and the raster list; when false, that member of
    // the snapshot is left empty.
    bool include_scrollback = true;
    bool include_rasters = true;

    // Memberwise equality.
    friend bool operator==(const TerminalSnapshotOptions&, const TerminalSnapshotOptions&) = default;
};

// What has changed since a host last said it had caught up.
//
// A diff engine that has this does not have to compare two grids to find out;
// without it, the only way to know what a child touched is to keep the
// previous screen and walk both, which costs a full copy per terminal per
// frame before any comparison begins.
//
// Spans rather than a bitset per cell: a child writes runs, and a row's
// touched columns are almost always one stretch. Rows rather than one
// rectangle: a program that repaints its status line and its cursor row has
// damaged two rows, and a bounding box over them claims everything between.
struct TerminalDamage {
    // A half-open span of columns. `first >= last` means the row is clean,
    // which is also what a default-constructed span says.
    struct RowSpan {
        // The first damaged column and one past the last, both 0-based.
        int first = 0;
        int last = 0;

        // Whether the row is clean, and memberwise equality.
        bool empty() const noexcept { return first >= last; }
        friend bool operator==(const RowSpan&, const RowSpan&) = default;
    };

    // Nothing about the previous screen can be relied on: a resize, a reset,
    // or a switch between the primary and alternate buffers. A host that sees
    // this resends everything rather than trying to reconcile.
    bool full = false;
    // One entry per row of the CURRENT grid, in row order.
    std::vector<RowSpan> rows;
    // The cursor's position, visibility or shape differs from when damage was last cleared.
    bool cursor = false;
    // Everything the child has switched on and can switch off again: the DEC
    // private modes, the one ANSI mode, the mouse tracking level and encoding,
    // and the kitty keyboard enhancements — which are modes in all but their
    // spelling, being switches a program turns on for as long as it needs them
    // and puts back on the way out.
    bool modes = false;
    // The child set its title (or the title stack changed it).
    bool title = false;
    // The picture list (rasters()) changed: a picture was placed, or one was written over in
    // part or whole, moved or removed by scrolling, or cleared away (also set with `full`). The
    // rows of any cells involved are damaged as well.
    bool rasters = false;
    // The four below are things that HAPPENED rather than things that are set,
    // and a host does something different with each: put text on the system
    // clipboard, ring or badge, offer a print job, show a complaint. They are
    // separate flags for the same reason `title` is not folded into `modes` —
    // and because three of them gate a read that is not free. The clipboard
    // text, the print spool and the diagnostic ring are deliberately not in
    // `TerminalStatus` (only a serial or a count is), so the flag is what tells
    // a host that fetching one is worth doing now.
    bool clipboard = false;
    bool bell = false;
    bool printer = false;
    bool diagnostics = false;
    // The child's own life: Ready → Running → Exited, Failed or Closed, and the
    // code it left behind. Not a mode, because the child did not ask for it,
    // and the last damage a terminal ever reports.
    bool lifecycle = false;
    // How many lines have entered the scrollback since the last clear. A host
    // sends exactly those rather than the whole history — which is the other
    // half of not copying it.
    std::size_t scrollback_pushed = 0;

    // Whether anything at all needs sending. Asked once per terminal per tick,
    // so it answers without walking the rows where it can.
    bool any() const noexcept {
        if (full || cursor || modes || title || rasters || clipboard || bell || printer ||
            diagnostics || lifecycle || scrollback_pushed > 0)
            return true;
        for (const RowSpan& row : rows)
            if (!row.empty()) return true;
        return false;
    }

    // Memberwise equality, row spans included.
    friend bool operator==(const TerminalDamage&, const TerminalDamage&) = default;
};

// The half of a terminal that is not cells: where the cursor is, what the
// child has turned on, what it called its window, and how it ended.
//
// A host at its flush tick needs all of this EVERY frame — `TerminalDamage`
// flags the cursor, the modes and the title precisely because they change
// independently of any cell — and needs cells only for the rows damage names.
// Before this, the only way to read them was `snapshot()`, so a host that
// wanted to know the cursor had moved copied the entire grid to find out: the
// copy U0-b exists to remove, reintroduced by the fields around it. This is
// the same answers in a few dozen bytes.
//
// What is deliberately NOT here is everything that carries a payload — the
// clipboard text, the rasters, the diagnostics, the print spool. Each has a
// serial or a count here (and a flag in `TerminalDamage`) instead, so a host
// learns that there is something new to fetch without the fetch happening on
// every tick whether or not anything changed.
struct TerminalStatus {
    // The grid size in columns and rows, and the child's cursor within it (0-based cells).
    ::ckv::Size cells;
    ::ckv::CursorState cursor;
    // Whether the alternate screen (DEC modes 47, 1047, 1049) is showing; it has no scrollback.
    bool alternate_buffer = false;
    // The title the child set, sanitized for display; empty until it sets one, and always empty
    // under TerminalOscPolicy::Deny.
    std::string title;
    // Where the session is in its life.
    TerminalSubsessionState state = TerminalSubsessionState::Ready;
    // Whether the child has bracketed paste (DEC mode 2004) on, so pasted text should be wrapped
    // in the paste markers before send_input.
    bool bracketed_paste_enabled = false;
    // Whether the child is tracking the pointer at all — `mouse_tracking !=
    // None`, kept because that is the question most hosts have, and answered
    // beside the level rather than instead of it. A session implementing this
    // seam that fills in only this one is read as the coarsest level, which is
    // what such a host was always sent.
    bool mouse_reporting_enabled = false;
    // How pointer reports must be encoded; None exactly when the child is not tracking.
    TerminalMouseEncoding mouse_encoding = TerminalMouseEncoding::None;
    // How much of the pointer's behaviour the child asked for. A host that
    // delivers pointer events decides from this whether a motion is reported at
    // all, and one that only forwards bytes carries it so that the far end can.
    TerminalMouseTracking mouse_tracking = TerminalMouseTracking::None;
    // Input modes the host must honour when encoding what it sends: application cursor keys
    // (DECCKM, DEC mode 1: arrows as SS3 rather than CSI sequences), focus in/out reports (DEC
    // mode 1004), alternate scroll (DEC mode 1007, see TerminalSnapshot), and the kitty keyboard
    // enhancements in force on the screen currently showing.
    bool application_cursor_keys = false;
    bool focus_reporting_enabled = false;
    bool alternate_scroll_enabled = false;
    TerminalKeyboardFlags keyboard_flags = TerminalKeyboardFlags::None;
    // How many clipboard writes the child has been granted, and how many times it has rung the
    // bell, since the session began; a host acts when a value differs from the last it saw.
    // Counts, not flags, and for the reason the snapshot's own are: a value a
    // host may read as often as it likes cannot carry a flag that reading
    // clears, and a flag that reading does not clear cannot say a second one
    // arrived.
    std::uint64_t clipboard_serial = 0;
    std::uint64_t bell_serial = 0;
    // How many complaints this terminal has made, ever — not how many the ring
    // is holding. The ring is bounded and drops its oldest entry, so a count of
    // what it contains would go back down and a host would read that as "there
    // is nothing new" while entries it has never seen were arriving.
    std::uint64_t diagnostics_serial = 0;
    // The printer scalars, as in TerminalSnapshot: whether the controller is on (the child's
    // output is going to the spool, not the screen), how many bytes the job in progress holds,
    // and how many completed jobs take_printer_jobs() would hand over.
    bool printer_controller_active = false;
    std::size_t printer_pending_bytes = 0;
    std::size_t printer_jobs_ready = 0;
    // The job in progress went over the limit, its buffer was freed, and the
    // terminal is SINKING — still in printer-controller mode, still scanning
    // for `CSI 4 i`, still keeping the child's bytes off the screen, and
    // storing none of them.
    //
    // Beside `printer_controller_active` rather than folded into it, because
    // the two are different things to tell a reader and only one of them is
    // reassuring. "Capturing" and "capturing nothing" look identical from
    // outside, and a host whose badge said the first while the second was true
    // would be promising a document it is not keeping. A probing child is
    // already told the difference (`CSI ? 15 n` answers not-ready while sunk);
    // this is the same honesty offered to the host.
    bool printer_sunk = false;
    // What the child exited with, once it has. Nothing until then: "still
    // running" and "exited 0" are different answers.
    // The POSIX session reports -1 for a child ended by a signal; a failed launch has none.
    std::optional<int> exit_code;

    // Memberwise equality.
    friend bool operator==(const TerminalStatus&, const TerminalStatus&) = default;
};

// A self-contained copy of a terminal's whole state at one moment: a value that stays valid and
// unchanged however the terminal moves on. It holds everything TerminalStatus does except
// printer_sunk and exit_code, plus the payloads status leaves out. Copying it costs the grid, the
// scrollback and the raster list, so a host reading at frame rate uses status() and the borrowed
// spans instead.
struct TerminalSnapshot {
    // The grid size, and its cells row-major (cells.width * cells.height entries) for the screen
    // currently showing, primary or alternate.
    ::ckv::Size cells;
    std::vector<::ckv::Cell> cell_buffer;
    // The cursor and alternate-screen state, as in TerminalStatus.
    ::ckv::CursorState cursor;
    bool alternate_buffer = false;
    // The pictures on the screen (empty when TerminalSnapshotOptions::include_rasters is false)
    // and the most recent diagnostics, oldest first.
    std::vector<TerminalRaster> rasters;
    std::vector<TerminalDiagnostic> diagnostics;
    // The title and lifecycle state, as in TerminalStatus.
    std::string title;
    TerminalSubsessionState state = TerminalSubsessionState::Ready;
    // Lines that scrolled off the top of the primary screen, oldest first, each cells.width
    // cells wide; empty when TerminalSnapshotOptions::include_scrollback is false.
    std::vector<::ckv::Cell> scrollback;
    // Input and pointer modes, as in TerminalStatus.
    bool bracketed_paste_enabled = false;
    bool mouse_reporting_enabled = false;
    TerminalMouseEncoding mouse_encoding = TerminalMouseEncoding::None;
    // Which of DEC 1000, 1002 and 1003 the child is in — see TerminalStatus.
    TerminalMouseTracking mouse_tracking = TerminalMouseTracking::None;
    // Application cursor keys (DEC mode 1) and focus reporting (DEC mode 1004), as in
    // TerminalStatus.
    bool application_cursor_keys = false;
    bool focus_reporting_enabled = false;
    // DEC mode 1007: with the alternate screen up and the child not tracking
    // the mouse itself, a wheel notch is to be delivered as cursor keys.
    bool alternate_scroll_enabled = false;
    // The last text the child asked to put on the clipboard, and a count of
    // how many such requests have been granted. A consumer forwards the text
    // when the count differs from the one it last acted on; that keeps the
    // snapshot a value that can be read as often as anyone likes, rather than
    // a queue that reading empties. The text is decoded from base64 and
    // sanitized before it is stored.
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
    // How many complaints this terminal has made, ever — see TerminalStatus.
    // The `diagnostics` above are the bounded ring; this is what says how much
    // of it a host has already seen.
    std::uint64_t diagnostics_serial = 0;
    // The printer, in three scalars — everything a host needs to show a badge
    // and a button without the snapshot carrying the spool itself. The jobs
    // are drained out of band with `take_printer_jobs()`, because a snapshot
    // is a value that may be read as often as anyone likes and a job is a
    // thing that must be delivered exactly once.
    //
    // While the controller is on, the child's output is going to the printer
    // and NOT to the screen: a host that does not say so leaves a reader
    // watching a terminal that has apparently stopped responding.
    bool printer_controller_active = false;
    // How much the job in progress has collected so far, for the "PRINT? ·
    // 12.4 KB" a host can put on the window it belongs to.
    std::size_t printer_pending_bytes = 0;
    // Completed jobs waiting to be taken. A count rather than a flag, for the
    // same reason `bell_serial` is one.
    std::size_t printer_jobs_ready = 0;
};

// UI-facing seam. Platform adapters may add readiness and child-process
// operations, but widgets consume only this deterministic core contract.
class TerminalSubsession {
public:
    // Destroys the session. A process-backed implementation closes it first, which ends or waits
    // for the child as its launch spec's TerminalExitPolicy says.
    virtual ~TerminalSubsession() = default;

    // A full copy of the terminal's state, scrollback and rasters included. Costs a copy of
    // everything the terminal holds; prefer status() and the borrowed spans below for per-frame
    // reads.
    virtual TerminalSnapshot snapshot() const = 0;
    // Everything a snapshot carries except the cells, the history and the
    // payloads. Pure rather than defaulted in terms of `snapshot()`, because a
    // default that copied the grid to answer "where is the cursor" is the
    // defect this exists to remove, and it would be invisible at every call
    // site.
    virtual TerminalStatus status() const = 0;
    // U0-b at the seam a host actually holds.
    //
    // The point of a damage report and borrowed cells is that a host reads a
    // terminal without copying it. A host that has to know which concrete class
    // it is holding in order to do that has the copy back — it reaches for
    // `snapshot()`, which is the only thing the seam offered. So these are part
    // of the seam: what changed, the cells to read it out of, the history
    // beside them, and the one call that says "sent".
    //
    // `damage()` never clears, and `clear_damage()` is the host's alone: three
    // consumers read one terminal (a diff engine, a title poll, a bell badge),
    // and a read that cleared would give the news to whichever looked first.
    virtual const TerminalDamage& damage() const noexcept = 0;
    virtual void clear_damage() noexcept = 0;
    // Whether a child currently has an atomic frame update open (DEC mode
    // 2026): it asked to hold what it draws until it says the frame is
    // whole. A host deciding whether now is a good moment to read this
    // terminal's damage — a diff engine's flush tick chief among them —
    // treats true as "not yet, ask again": the alternative is showing
    // exactly the half-drawn frame this mode exists to hide.
    virtual bool synchronized_output_active() const noexcept = 0;
    // The grid of the screen currently showing, row-major at status().cells.width columns, and
    // the primary screen's scrollback, oldest line first, in rows of the same width. Borrowed:
    // valid only until the next call that changes this session, such as feeding or draining
    // child output or a resize.
    virtual std::span<const ::ckv::Cell> cells() const noexcept = 0;
    virtual std::span<const ::ckv::Cell> scrollback() const noexcept = 0;
    // The pictures a child has drawn, and what the terminal has had to
    // complain about. Borrows for the same reason the cells are: a view repaints
    // at frame rate and reads both every time, so the alternative is a snapshot
    // per frame — and a snapshot carries the history, which is the copy U0-b
    // exists to remove.
    virtual std::span<const TerminalRaster> rasters() const noexcept = 0;
    virtual std::span<const TerminalDiagnostic> diagnostics() const noexcept = 0;
    // The finished print jobs, handed over ONCE and forgotten by the terminal
    // that made them.
    //
    // A drain rather than a borrow, which is the whole difference between this
    // and every read above it: `cells()`, `rasters()` and `diagnostics()` are
    // values a host may read as often as it likes, and a job is a thing that
    // must be delivered exactly once. A host that took the same job twice
    // would show a reader the same capture twice; one that never took it would
    // lose the oldest jobs once the emulator's bounded queue (32 jobs) filled.
    //
    // Not on the snapshot, deliberately. `TerminalStatus` carries three
    // printer scalars so a host can draw a badge and a button every frame
    // without paying for the payload — and the payload can be a megabyte, so a
    // snapshot that carried it would undo exactly what U0-b was for.
    //
    // Defaulted to nothing rather than pure, because printing is a capability
    // a terminal may simply not have: a mirror of a remote terminal, a
    // recording, a test double. An implementation that captures overrides it;
    // one that does not is honest by saying so with an empty answer, and its
    // `printer_jobs_ready` stays zero to match.
    virtual std::vector<TerminalPrinterJob> take_printer_jobs() { return {}; }

    // The printer's two runtime settings: whether this terminal captures at
    // all, and how much one job may collect before it is abandoned.
    //
    // Runtime because they are the READER's, not the child's. Every other
    // capability in a profile is a statement about the terminal a child was
    // launched into and must not change under it; these two are a preference
    // a reader edits in a dialog while the child runs, and a child asks
    // `CSI ? 15 n` afresh whenever it wants to know. Without them a host could
    // offer the choice only by relaunching — which for a multiplexer means
    // killing somebody's shell to change a preference about printing.
    //
    // Defaulted to nothing, like the drain above: a terminal that captures
    // nothing has no policy worth setting, and saying so by ignoring the call
    // is more honest than pretending to store it.
    //
    // In the emulator, switching capture off frees a job still being collected
    // (the terminal then sinks until the child ends it) but keeps completed jobs
    // for take_printer_jobs().
    virtual void set_printer_policy(TerminalPrinterPolicy) {}
    // Zero is refused rather than treated as "no limit": an unbounded spool is
    // exactly the hazard the capture design exists to remove, and a host that
    // asked for one has made a mistake this seam should not honour.
    // A new limit governs what is collected from then on; a job already past it
    // is not abandoned retroactively.
    virtual void set_printer_spool_limit(std::size_t) {}
    // The profile in force: the launch profile with cells tracking the current (clamped) size,
    // cell_pixels the last valid metric passed to resize, and the current printer policy.
    virtual const TerminalCapabilityProfile& profile() const noexcept = 0;
    // Hands the terminal bytes the child wrote, to be parsed onto the grid. Process-backed
    // sessions call this themselves as they read their child; a host calls it directly only on a
    // session with no process of its own. At most max_parser_work_per_step bytes are parsed per
    // call and the rest stay queued; ignored once the session is Exited, Failed or Closed.
    virtual void feed_output(std::string_view bytes) = 0;
    // Sets the grid to `cells` (clamped to 1x1 through max_cells) and the cell metric to
    // `cell_pixels` (ignored unless both dimensions are positive). Content keeps its top-left
    // position and is cut or padded, not reflowed; scrollback lines are re-laid to the new width;
    // the scrolling region resets; the cursor is clamped into the grid; everything is damaged.
    // A process-backed session also tells its child the new size.
    virtual void resize(::ckv::Size cells, ::ckv::PixelSize cell_pixels) = 0;
    // Queues bytes for the child: keystrokes, pastes and pointer reports the host has already
    // encoded for the modes in status(). Bounded by max_input_bytes; ignored once the session is
    // Exited, Failed or Closed. A process-backed session forwards them to its child itself,
    // keeping queued only what cannot be written yet.
    virtual void send_input(std::string_view bytes) = 0;
    // Removes and returns everything queued for the child, including the terminal's own replies
    // to queries. A host that owns the transport to the child delivers these; a process-backed
    // session delivers them itself, so a host normally does not call this on one.
    virtual std::string take_pending_input() = 0;
    // The session's lifecycle state; the same value status().state reports.
    virtual TerminalSubsessionState state() const noexcept = 0;

    // The pid of the operating-system process behind this session, for
    // OBSERVATION — a host measuring what the process tree under a terminal
    // costs needs to know where that tree is rooted — and -1 when there is no
    // such process to name: the default, kept by any session that is not
    // itself holding one (a remote mirror, a fake in a test). Virtual with a
    // default rather than abstract, because "no pid" is the honest answer for
    // most implementations and forcing each to say so would repeat it.
    //
    // It is NOT a handle for control: a host ends a child through the owning
    // class's own request/close protocol, never by signalling the pid itself.
    // The POSIX implementation says the same where it overrides this.
    virtual ProcessId process_id() const noexcept { return -1; }

    // Resource observation from the process-owning adapter, without leaking
    // native handles or guessing ownership from a system-wide parent table.
    // Mirrors/emulators default to Unsupported. Counters contain no rates or
    // clock reads; callers compute deltas with their injected clock.
    virtual ProcessResources process_resources() const noexcept { return {}; }
};

}  // namespace ckv::core
