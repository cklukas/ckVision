// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/terminal_emulator.hpp"

#include <algorithm>
#include <chrono>
#include <array>
#include <charconv>
#include <initializer_list>
#include <limits>

#include "cvision/core/base64.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/text.hpp"
#include "cvision/term/graphics_log.hpp"
#include "cvision/term/sixel_decoder.hpp"

namespace ckv::term {
namespace {
Size bounded_size(Size size, Size limit) noexcept {
    return {std::clamp(size.width, 1, std::max(1, limit.width)),
            std::clamp(size.height, 1, std::max(1, limit.height))};
}

int ceil_div_positive(int value, int divisor) noexcept {
    if (value <= 0) return 0;
    return 1 + (value - 1) / std::max(1, divisor);
}


// Bold has meant "brighter" for the low eight colours since the hardware
// that had only eight, and programs still write for that: text asking for
// bold on the default foreground expects white, not a heavier grey. The
// eight base colours are the whole of it -- a true-colour foreground is a
// specific colour the program chose, and brightening that would be
// overriding the choice rather than honouring a convention.
Style bold_brightened(Style style, const Style& default_style) noexcept {
    if (!has_attr(style.attrs, Attr::Bold)) return style;
    // Reverse video swaps the two colours, so brightening the foreground
    // here would land on what the reader sees as the background: a program
    // asking for emphasis inside a header bar would get a paler patch behind
    // its text instead of emphasis, which is the opposite of the intent and
    // is not what terminals do. Bold still applies as weight.
    if (has_attr(style.attrs, Attr::Reverse)) return style;
    // The default foreground brightens too, and to white. A terminal's
    // default behaves as the eighth colour does for this purpose, and it is
    // the case that matters most: a program reaching for emphasis without
    // naming a colour is the common one, and ncurses does exactly that.
    if (style.fg == default_style.fg) {
        style.fg = Color::indexed(15);
        return style;
    }
    // Now that a colour keeps its index, this is the whole rule: the low
    // eight are the eight that brighten, and everything else -- an explicit
    // 256-colour index, a chosen RGB -- is left as it was asked for. Before
    // indices survived, a program's own `38;2;205;49;49` was indistinguishable
    // from palette red and got brightened along with it.
    if (style.fg.is_indexed() && style.fg.index() < 8)
        style.fg = Color::indexed(static_cast<std::uint8_t>(style.fg.index() + 8));
    return style;
}

std::size_t complete_utf8_prefix(std::string_view text) noexcept {
    if (text.empty()) return 0;
    std::size_t start = text.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) --start;
    const unsigned char lead = static_cast<unsigned char>(text[start]);
    int expected = 1;
    if ((lead & 0xE0U) == 0xC0U) expected = 2;
    else if ((lead & 0xF0U) == 0xE0U) expected = 3;
    else if ((lead & 0xF8U) == 0xF0U) expected = 4;
    if (expected > 1 && text.size() - start < static_cast<std::size_t>(expected)) return start;
    return text.size();
}


bool is_sixel_control(std::string_view control) noexcept {
    const std::size_t command = control.find('q');
    if (command == std::string_view::npos) return false;
    for (std::size_t i = 0; i < command; ++i)
        if (control[i] != ';' && (control[i] < '0' || control[i] > '9')) return false;
    return true;
}

}  // namespace

TerminalEmulator::TerminalEmulator(TerminalCapabilityProfile profile, TerminalSubsessionOptions options)
    : profile_(std::move(profile)), options_(std::move(options)), cells_(bounded_size(profile_.cells, options_.max_cells)),
      style_(profile_.default_style) {
    profile_.cells = cells_;
    primary_.assign(static_cast<std::size_t>(cells_.width * cells_.height), Cell::from_grapheme(" ", style_));
    alternate_ = primary_;
    scroll_bottom_ = cells_.height;
    reset_tab_stops();
    // A fresh terminal has told nobody anything, so everything is outstanding.
    // Starting clean would mean the first frame after attach sent nothing.
    resize_damage();
    damage_everything();
}

std::vector<Cell>& TerminalEmulator::active_cells() noexcept { return alternate_buffer_ ? alternate_ : primary_; }
const std::vector<Cell>& TerminalEmulator::active_cells() const noexcept { return alternate_buffer_ ? alternate_ : primary_; }

std::span<const Cell> TerminalEmulator::cells() const noexcept {
    const std::vector<Cell>& cells = active_cells();
    return std::span<const Cell>(cells.data(), cells.size());
}

std::span<const Cell> TerminalEmulator::scrollback() const noexcept {
    return std::span<const Cell>(scrollback_.data() + scrollback_begin_,
                                 scrollback_.size() - scrollback_begin_);
}

std::span<const TerminalRaster> TerminalEmulator::rasters() const noexcept {
    return std::span<const TerminalRaster>(rasters_.data(), rasters_.size());
}

std::span<const TerminalDiagnostic> TerminalEmulator::diagnostics() const noexcept {
    return std::span<const TerminalDiagnostic>(diagnostics_.data(), diagnostics_.size());
}

TerminalSnapshot TerminalEmulator::snapshot() const { return snapshot(TerminalSnapshotOptions{}); }

// Written with the field names rather than by position: this is the one place
// every scalar a host reads is assembled, and in a positional list a new field
// in the middle of the struct silently re-aims every value after it.
TerminalStatus TerminalEmulator::status() const {
    const bool tracking = mouse_tracking_ != TerminalMouseTracking::None;
    return TerminalStatus{
        .cells = cells_,
        .cursor = cursor_,
        .alternate_buffer = alternate_buffer_,
        .title = title_,
        .state = state_,
        .bracketed_paste_enabled = bracketed_paste_enabled_,
        .mouse_reporting_enabled = tracking,
        .mouse_encoding = !tracking             ? TerminalMouseEncoding::None
                          : mouse_sgr_enabled_  ? TerminalMouseEncoding::Sgr
                                                : TerminalMouseEncoding::X10,
        .mouse_tracking = mouse_tracking_,
        .application_cursor_keys = application_cursor_keys_,
        .focus_reporting_enabled = focus_reporting_enabled_,
        .alternate_scroll_enabled = alternate_scroll_enabled_,
        .keyboard_flags = (alternate_buffer_ ? alternate_keyboard_ : primary_keyboard_).flags,
        .clipboard_serial = clipboard_serial_,
        .bell_serial = bell_serial_,
        .diagnostics_serial = diagnostics_serial_,
        .printer_controller_active = printer_controller_,
        .printer_pending_bytes = printer_spool_.size(),
        .printer_jobs_ready = printer_jobs_.size(),
        .printer_sunk = printer_overflowed_,
        .exit_code = exit_code_};
}

TerminalSnapshot TerminalEmulator::snapshot(TerminalSnapshotOptions options) const {
    // Built from status() rather than beside it: every field the two share has
    // one source, so the cheap read and the whole-terminal read cannot come to
    // disagree about where the cursor is.
    const TerminalStatus scalars = status();
    // The two expensive members are the ones a caller can decline. Built as
    // empty rather than copied and cleared, because the copy is the cost.
    static const std::vector<Cell> kNoScrollback;
    static const std::vector<TerminalRaster> kNoRasters;
    // Copied from the live part, so a caller never sees the dead prefix the
    // offset is hiding.
    const std::vector<Cell> history =
        options.include_scrollback
            ? std::vector<Cell>(scrollback_.begin() + static_cast<std::ptrdiff_t>(scrollback_begin_),
                                scrollback_.end())
            : std::vector<Cell>{};
    return TerminalSnapshot{.cells = scalars.cells,
                            .cell_buffer = active_cells(),
                            .cursor = scalars.cursor,
                            .alternate_buffer = scalars.alternate_buffer,
                            .rasters = options.include_rasters ? rasters_ : kNoRasters,
                            .diagnostics = diagnostics_,
                            .title = scalars.title,
                            .state = scalars.state,
                            .scrollback = options.include_scrollback ? history : kNoScrollback,
                            .bracketed_paste_enabled = scalars.bracketed_paste_enabled,
                            .mouse_reporting_enabled = scalars.mouse_reporting_enabled,
                            .mouse_encoding = scalars.mouse_encoding,
                            .mouse_tracking = scalars.mouse_tracking,
                            .application_cursor_keys = scalars.application_cursor_keys,
                            .focus_reporting_enabled = scalars.focus_reporting_enabled,
                            .alternate_scroll_enabled = scalars.alternate_scroll_enabled,
                            .clipboard_text = clipboard_text_,
                            .clipboard_serial = scalars.clipboard_serial,
                            .keyboard_flags = scalars.keyboard_flags,
                            .bell_serial = scalars.bell_serial,
                            .diagnostics_serial = scalars.diagnostics_serial,
                            .printer_controller_active = scalars.printer_controller_active,
                            .printer_pending_bytes = scalars.printer_pending_bytes,
                            .printer_jobs_ready = scalars.printer_jobs_ready};
}

void TerminalEmulator::diagnostic(TerminalDiagnostic::Kind kind, std::string message) {
    diagnostics_.push_back(TerminalDiagnostic{kind, std::move(message)});
    if (diagnostics_.size() > 64) diagnostics_.erase(diagnostics_.begin());
    // Counted before it is bounded, so the count says how many were made rather
    // than how many survived — and flagged, because the ring is a payload a
    // host fetches rather than something `status()` carries.
    ++diagnostics_serial_;
    damage_.diagnostics = true;
}

// A colour as a terminal reports one: `rgb:` and four hex digits per channel,
// each byte doubled into the sixteen-bit form the convention uses. A program
// that asked is prepared for this shape and no other.
std::string TerminalEmulator::report_color(Color color, Color fallback) {
    const Color resolved = resolved_color(color, fallback);
    constexpr char digits[] = "0123456789abcdef";
    std::string out = "rgb:";
    bool first = true;
    for (const std::uint8_t channel : {resolved.r(), resolved.g(), resolved.b()}) {
        if (!first) out += '/';
        first = false;
        // Sixteen bits per channel, each eight-bit value doubled — 0xBB
        // becomes 0xBBBB, which is how a terminal widens what it has.
        for (int repeat = 0; repeat < 2; ++repeat) {
            out += digits[channel >> 4];
            out += digits[channel & 0x0F];
        }
    }
    return out;
}

void TerminalEmulator::reply_osc(std::string_view body, bool bel_terminated) {
    send_input("\x1b]" + std::string(body) + (bel_terminated ? "\a" : "\x1b\\"));
}

void TerminalEmulator::handle_osc(bool bel_terminated) {
    const std::string_view control(control_);
    const std::size_t separator = control.find(';');
    const std::string_view command = control.substr(0, separator);
    const std::string_view body =
        separator == std::string_view::npos ? std::string_view{} : control.substr(separator + 1);
    int number = -1;
    if (!command.empty() && command.size() <= 3) {
        int parsed = 0;
        const auto result = std::from_chars(command.data(), command.data() + command.size(), parsed);
        if (result.ec == std::errc{} && result.ptr == command.data() + command.size()) number = parsed;
    }

    switch (number) {
        // OSC 0 sets the icon name and the window title together; OSC 2 sets
        // the title alone. Both are how a program says what it is now working
        // on, and a host that shows a caption wants either. OSC 1 is the icon
        // name only and deliberately does not touch the title.
        case 0:
        case 2:
            if (profile_.osc_policy != TerminalOscPolicy::StoreMetadata) return;
            set_title(body);
            return;
        case 1:
            return;  // icon name: understood, and deliberately without effect
        // OSC 22 asks for a mouse pointer shape. Understood, and
        // deliberately without effect: the pointer belongs to the window
        // this emulator is a view inside, and it is a single, shared thing.
        // A child that could set it could change the pointer while the
        // reader is nowhere near the child — including a child in a
        // background window, or one that is scrolled out of sight. The
        // shape over an embedded terminal is the outer application's to
        // choose, the same way the title bar around it is.
        case 22:
            return;
        case 4:
            handle_palette_query(body, bel_terminated);
            return;
        // The colours a terminal has of its own. vim and nvim ask for the
        // background before they choose a colour scheme, and a terminal that
        // says nothing leaves them guessing — on a dark background they
        // usually guess light, and every syntax colour comes out wrong.
        case 10:
        case 11:
            if (profile_.query_policy != TerminalQueryPolicy::DeclaredProfile) return;
            if (body != "?") {
                // Setting them is a separate question from answering them:
                // the default colours are the profile's, and a child that
                // could rewrite them could recolour the window it sits in.
                diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                           "child attempted to set a terminal default colour");
                return;
            }
            reply_osc(std::string(command) + ";" +
                          report_color(number == 10 ? profile_.default_style.fg
                                                    : profile_.default_style.bg,
                                       Color::rgb(0, 0, 0)),
                      bel_terminated);
            return;
        case 52:
            handle_clipboard(body);
            return;
        default:
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child OSC sequence");
            return;
    }
}

// OSC 4 asks what a palette entry is, one `index ; ?` pair at a time and
// sometimes several in one sequence. Each pair is answered separately, which
// is what the asking program expects to read back.
void TerminalEmulator::handle_palette_query(std::string_view body, bool bel_terminated) {
    if (profile_.query_policy != TerminalQueryPolicy::DeclaredProfile) return;
    std::size_t position = 0;
    while (position < body.size()) {
        const std::size_t index_end = body.find(';', position);
        if (index_end == std::string_view::npos) break;
        const std::string_view index_text = body.substr(position, index_end - position);
        const std::size_t value_end = body.find(';', index_end + 1);
        const std::string_view value = body.substr(
            index_end + 1, value_end == std::string_view::npos ? body.size() - index_end - 1
                                                               : value_end - index_end - 1);
        int index = -1;
        const auto result =
            std::from_chars(index_text.data(), index_text.data() + index_text.size(), index);
        const bool valid_index = result.ec == std::errc{} &&
                                 result.ptr == index_text.data() + index_text.size() && index >= 0 &&
                                 index <= 255;
        if (valid_index && value == "?") {
            reply_osc("4;" + std::to_string(index) + ";" +
                          report_color(Color::indexed(static_cast<std::uint8_t>(index)),
                                       Color::rgb(0, 0, 0)),
                      bel_terminated);
        } else if (valid_index) {
            // Redefining a palette entry is the same question as setting a
            // default colour, and gets the same answer.
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                       "child attempted to redefine a palette colour");
        }
        if (value_end == std::string_view::npos) break;
        position = value_end + 1;
    }
}

// OSC 52, the one thing a child can do that reaches outside its own window.
//
// Writing is opt-in per profile and bounded; reading is refused outright and
// under every policy (D-022), because a program that can read the clipboard
// can read whatever its reader last copied — a password, most likely, since
// that is what people copy. The refusal is silent as far as the child is
// concerned: it gets no reply at all, which is what a terminal without the
// feature looks like.
void TerminalEmulator::handle_clipboard(std::string_view body) {
    const std::size_t separator = body.find(';');
    const std::string_view payload =
        separator == std::string_view::npos ? body : body.substr(separator + 1);
    if (payload == "?") {
        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                   "child clipboard read refused");
        return;
    }
    if (profile_.clipboard_policy != TerminalClipboardPolicy::AllowWrite) {
        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                   "child clipboard write denied by policy");
        return;
    }
    // Bound the encoded form before decoding it: four base64 characters are
    // three bytes, less one for each padding character, so the decoded size
    // is known exactly and an oversized payload is refused without building
    // it. A malformed length is left for the decoder to reject.
    std::size_t padding = 0;
    while (padding < 2 && payload.size() > padding && payload[payload.size() - 1 - padding] == '=')
        ++padding;
    if (payload.size() / 4 * 3 - std::min(payload.size() / 4 * 3, padding) >
        options_.max_clipboard_bytes) {
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded,
                   "child clipboard payload exceeded configured limit");
        return;
    }
    std::string decoded;
    if (!base64::decode(payload, decoded)) {
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "malformed child clipboard payload");
        return;
    }
    if (decoded.size() > options_.max_clipboard_bytes) {
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded,
                   "child clipboard payload exceeded configured limit");
        return;
    }
    // ckVision has one clipboard, so which selection the child named makes no
    // difference to where the text goes; it is recorded and the host decides
    // what to do with it.
    clipboard_text_ = text::sanitize_clipboard_text(decoded);
    ++clipboard_serial_;
    // The serial says WHAT to send and the flag says WHEN to look. A host that
    // reads this terminal every tick would find the serial on its own; one that
    // reads it only when there is damage — which is what a delta transport is —
    // would never look, and the text a child asked to put on somebody's
    // clipboard would sit here until the next keystroke happened to move a cell.
    damage_.clipboard = true;
}

// The title, as the child asked for it and as a window frame can survive.
//
// Two things happen to it. Control bytes are replaced, because this is
// child-supplied text on its way onto a frame and a control byte there is
// drawn as frame content rather than obeyed. And it is cut to
// `max_title_bytes`: a caption is one line, `max_control_bytes` is sixteen
// kilobytes, and the snapshot that carries this is copied per terminal per
// frame — a child that sends a novel for a title would otherwise have every
// host copying that novel forever.
//
// The cut lands on a character boundary. Cutting mid-sequence would leave a
// trailing byte that sanitisation then renders as a replacement character,
// so a title too long by one byte would end in a visible mark the child never
// sent.
void TerminalEmulator::set_title(std::string_view text) {
    const std::size_t limit = std::max<std::size_t>(1, options_.max_title_bytes);
    if (text.size() > limit) {
        std::size_t cut = limit;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) --cut;
        text = text.substr(0, cut);
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child title exceeded configured limit");
    }
    title_ = text::sanitize_display_text(text);
    damage_.title = true;
}

// XTWINOPS 22 and 23. A program that is about to rename the window saves the
// caption first and puts it back on the way out, which is how a shell gets
// its own name back when an editor exits — and, without this, does not: the
// caption keeps whatever the last program to set one called itself.
//
// Ps selects which of the two names a terminal keeps: 0 both, 1 the icon name,
// 2 the window title. ckVision keeps a title and no icon name (OSC 1 is
// deliberately without effect), so 0 and 2 act and 1 is understood and does
// nothing — including on the way back, so a program that pushed only its icon
// name does not pop somebody else's title.
void TerminalEmulator::handle_title_stack(bool push) {
    if (profile_.osc_policy != TerminalOscPolicy::StoreMetadata) return;
    const int which = parameter(control_, 1, 0);
    if (which == 1) return;
    if (which != 0 && which != 2) {
        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                   "unsupported child window-title stack operation");
        return;
    }
    if (push) {
        // A stack has to have a bottom. Dropping the oldest entry bounds a
        // program that pushes without ever popping; it can only lose a caption
        // it had already stopped tracking.
        const std::size_t depth = std::max<std::size_t>(1, options_.max_title_stack_depth);
        if (title_stack_.size() >= depth) title_stack_.erase(title_stack_.begin());
        title_stack_.push_back(title_);
        return;
    }
    // Popping what was never pushed is not an error to report: a program that
    // pops one too many is asking for the caption it started with, and the
    // honest answer is to leave the current one alone rather than to blank it.
    if (title_stack_.empty()) return;
    title_ = title_stack_.back();
    title_stack_.pop_back();
    damage_.title = true;
}

bool TerminalEmulator::printer_available() const noexcept {
    return profile_.printer_policy == TerminalPrinterPolicy::Capture;
}

void TerminalEmulator::set_printer_spool_limit(std::size_t bytes) {
    // Refused rather than honoured: an unbounded spool is the hazard the whole
    // capture design exists to remove, so a host asking for one has made a
    // mistake this terminal should not carry out.
    if (bytes == 0) return;
    options_.max_printer_spool_bytes = bytes;
    // A job already over the new, smaller bound is NOT retro-actively
    // abandoned. It was collected under a limit that permitted it, and
    // discarding a reader's capture because they later lowered a preference
    // would be deciding something they did not ask for. The new bound governs
    // what is collected from here.
}

void TerminalEmulator::set_printer_policy(TerminalPrinterPolicy policy) {
    if (profile_.printer_policy == policy) return;
    profile_.printer_policy = policy;
    // Turning capture off mid-document. The controller stays ON — the three
    // ways out of it are `CSI 4 i`, RIS and the child exiting, and a policy
    // change is none of them — but nothing is stored from here, and the
    // reader's screen is deliberately NOT resumed: the child is still sending
    // a document, and painting the middle of one across the terminal is the
    // failure print_bytes() already refuses on overflow. So an in-flight job's
    // buffer goes and the terminal sinks, which is the state a probing child
    // is told about honestly (`CSI ? 15 n` answers ?13n with no printer at
    // all, so it learns the policy changed rather than that a job failed).
    //
    // Completed jobs are NOT touched. They were finished while the policy said
    // to keep them, they belong to the reader who had it in force, and
    // take_printer_jobs() still hands them over. Discarding a reader's own
    // captures because they later turned capture off would be deciding
    // something they did not ask for.
    if (policy == TerminalPrinterPolicy::Deny) {
        printer_spool_.clear();
        printer_spool_.shrink_to_fit();
        printer_job_open_ = false;
        printer_overflowed_ = false;
    }
    // The button, the badge and the probe answer all change with this, and a
    // host that sends what damage names would otherwise show the old policy
    // until the child happened to print again.
    damage_.printer = true;
}

// Media Copy. The forms a real program uses, and honest silence for the ones
// this terminal does not have.
void TerminalEmulator::handle_media_copy() {
    const bool private_form = !control_.empty() && control_.front() == '?';
    const int what = parameter(control_, 0, 0);
    if (!printer_available()) {
        // Refused, and said so once per attempt rather than silently: a
        // program that prints into a terminal with no printer should leave a
        // trace a host can show its reader, since the reader will otherwise
        // simply find that nothing happened.
        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                   "child asked to print; this terminal has no printer");
        return;
    }
    if (private_form) {
        switch (what) {
            case 1:
                // Print the cursor's line.
                print_bytes(screen_text(cursor_.position.y, cursor_.position.y + 1));
                finish_printer_job(TerminalPrinterJob::Origin::Line);
                return;
            case 4:
                // Autoprint off — and the lines collected so far become one
                // job, rather than one job per line, which is what makes a
                // printed session readable.
                if (autoprint_) {
                    autoprint_ = false;
                    finish_printer_job(TerminalPrinterJob::Origin::Autoprint);
                }
                return;
            case 5:
                autoprint_ = true;
                return;
            default:
                diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                           "unsupported child print form");
                return;
        }
    }
    switch (what) {
        case 0: {
            // Print screen. DECPEX decides how much of it: the scrolling
            // region by default, the whole screen when the program says so.
            const int top = printer_print_extent_ ? 0 : scroll_top_;
            const int bottom = printer_print_extent_ ? cells_.height : scroll_bottom_;
            print_bytes(screen_text(top, bottom));
            finish_printer_job(TerminalPrinterJob::Origin::Screen);
            return;
        }
        case 4:
            // The controller is already off; ending it twice is not an error.
            return;
        case 5:
            printer_controller_ = true;
            // While this is on the child's output goes to the printer and NOT
            // to the screen, which is the one printer fact a host must show its
            // reader — otherwise they are watching a terminal that has
            // apparently stopped responding.
            damage_.printer = true;
            printer_terminator_.clear();
            return;
        default:
            // CSI 10 i and CSI 11 i are xterm's HTML and SVG screen dumps.
            // They are not implemented, and saying so beats writing an empty
            // file that claims to be a screen.
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child print form");
            return;
    }
}

void TerminalEmulator::print_bytes(std::string_view bytes) {
    if (bytes.empty()) return;
    printer_job_open_ = true;
    if (printer_overflowed_) return;  // sinking: the buffer is already gone
    const std::size_t limit = std::max<std::size_t>(1, options_.max_printer_spool_bytes);
    if (printer_spool_.size() + bytes.size() > limit) {
        // Freed rather than truncated. Half a document that looks whole is
        // worse than a job that says it was too big — and the screen is
        // deliberately NOT resumed: the child is still sending a document,
        // and painting the middle of it across the reader's terminal would be
        // the worst of both.
        printer_spool_.clear();
        printer_spool_.shrink_to_fit();
        printer_overflowed_ = true;
        damage_.printer = true;
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded,
                   "child print job exceeded configured limit");
        return;
    }
    printer_spool_.append(bytes);
    // `printer_pending_bytes` is what a host puts on the badge it shows while a
    // document is collecting, so the number moving is news.
    damage_.printer = true;
}

void TerminalEmulator::finish_printer_job(TerminalPrinterJob::Origin origin) {
    if (!printer_job_open_ && !printer_overflowed_) return;
    if (printer_form_feed_ && !printer_overflowed_) printer_spool_.push_back('\f');  // DECPFF
    // A job queue has to have a bottom. A host that never drains is a host
    // that has stopped looking; keeping the newest is the useful half.
    constexpr std::size_t kMaxQueuedJobs = 32;
    if (printer_jobs_.size() >= kMaxQueuedJobs) printer_jobs_.erase(printer_jobs_.begin());
    printer_jobs_.push_back(
        TerminalPrinterJob{origin, std::move(printer_spool_), printer_overflowed_});
    printer_spool_.clear();
    printer_overflowed_ = false;
    printer_job_open_ = false;
    // A job is a payload waiting to be taken, so what a host has to hear is
    // that the count moved — not that the screen is worthless. Reporting this
    // as a full repaint (which it was) made every print screen cost the whole
    // grid to a host that sends what damage names.
    damage_.printer = true;
}

std::vector<TerminalPrinterJob> TerminalEmulator::take_printer_jobs() {
    std::vector<TerminalPrinterJob> jobs = std::move(printer_jobs_);
    printer_jobs_.clear();
    // The count a host reads has gone to zero, and the host that drained is not
    // necessarily the only one reading this terminal (see `clear_damage`).
    if (!jobs.empty()) damage_.printer = true;
    return jobs;
}

// The screen as a printer would have received it: one line per row, with the
// blanks a grid pads a row with removed. Those spaces are how an empty cell is
// stored, not something anybody printed.
std::string TerminalEmulator::screen_text(int top, int bottom) const {
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    const std::vector<Cell>& cells = active_cells();
    std::string out;
    for (int row = top; row < bottom; ++row) {
        std::string line;
        for (int column = 0; column < cells_.width; ++column) {
            const Cell& cell = cells[static_cast<std::size_t>(row * cells_.width + column)];
            if (!cell.is_continuation()) line += cell.grapheme();
        }
        while (!line.empty() && line.back() == ' ') line.pop_back();
        out += line;
        out += '\n';
    }
    return out;
}

bool TerminalEmulator::append_control(char byte) {
    if (control_.size() < options_.max_control_bytes) {
        control_.push_back(byte);
        return true;
    }
    diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child control string exceeded configured limit");
    control_.clear();
    parse_state_ = ParseState::Discard;
    return false;
}

bool TerminalEmulator::append_dcs_byte(char byte) {
    const std::size_t limit = dcs_sixel_payload_ ? options_.max_graphics_payload_bytes : options_.max_control_bytes;
    if (control_.size() < std::max<std::size_t>(1, limit)) {
        control_.push_back(byte);
        if (!dcs_sixel_payload_ && byte == 'q' && is_sixel_control(control_)) dcs_sixel_payload_ = true;
        return true;
    }
    diagnostic(TerminalDiagnostic::Kind::LimitExceeded,
               dcs_sixel_payload_ ? "child Sixel payload exceeded configured limit"
                                   : "child control string exceeded configured limit");
    control_.clear();
    dcs_sixel_payload_ = false;
    parse_state_ = ParseState::Discard;
    return false;
}

void TerminalEmulator::feed_output(std::string_view bytes) {
    if (state_ == TerminalSubsessionState::Closed || state_ == TerminalSubsessionState::Failed ||
        state_ == TerminalSubsessionState::Exited)
        return;
    // The cursor is compared once here rather than marked at each of the three
    // dozen places that move it. Those places are cursor addressing, tab stops,
    // wrapping, scrolling, save/restore and every erase that homes it — a mark
    // at each would be thirty-odd chances to forget one, and forgetting one
    // leaves a cursor that stops moving on a reader's screen. What a host
    // needs to know is whether it ended up somewhere else, and that is one
    // comparison of four small fields.
    const CursorState cursor_before = cursor_;
    // The first byte a child sends is also the moment it stops being Ready, and
    // that is a change a host reports rather than discovers.
    if (state_ != TerminalSubsessionState::Running) {
        state_ = TerminalSubsessionState::Running;
        damage_.lifecycle = true;
    }
    const std::size_t output_limit = std::max<std::size_t>(1, options_.max_output_bytes);
    const std::size_t available = output_limit > pending_output_.size() ? output_limit - pending_output_.size() : 0;
    const std::size_t accepted = std::min(bytes.size(), available);
    pending_output_.append(bytes.substr(0, accepted));
    if (accepted < bytes.size())
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child output queue exceeded configured limit");

    const std::size_t parser_budget = std::max<std::size_t>(1, options_.max_parser_work_per_step);
    const std::size_t budget = std::min(pending_output_.size(), parser_budget);
    if (pending_output_.size() > budget)
        diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child output step exceeded parser work budget");
    for (std::size_t i = 0; i < budget; ++i) {
        const char byte = pending_output_[i];
        // While the printer controller is on, every byte belongs to the
        // printer and none of it reaches the screen — that is what `CSI 5 i`
        // means. The only thing being looked for is the sequence that turns it
        // off again, and that sequence may be split across two reads, so a
        // partial match is held rather than printed and then regretted.
        if (printer_controller_) {
            printer_terminator_.push_back(byte);
            const std::string_view candidate(printer_terminator_);
            const bool could_be_terminator =
                candidate == "\x1b" || candidate == "\x1b[" || candidate == "\x1b[?" ||
                candidate == "\x1b[4" || candidate == "\x1b[?4";
            if (could_be_terminator) continue;
            if (candidate == "\x1b[4i" || candidate == "\x1b[?4i") {
                printer_controller_ = false;
                damage_.printer = true;  // the screen is the child's again
                printer_terminator_.clear();
                finish_printer_job(TerminalPrinterJob::Origin::Controller);
                continue;
            }
            // Not the terminator after all: everything held goes to the
            // printer, exactly as the child sent it.
            print_bytes(printer_terminator_);
            printer_terminator_.clear();
            continue;
        }
        switch (parse_state_) {
            case ParseState::Ground:
                if (byte == '\x1b') { put_text(true); parse_state_ = ParseState::Escape; }
                else if (byte == '\r') { put_text(true); cursor_.position.x = 0; }
                else if (byte == '\n') { put_text(true); cursor_.position.x = 0; index(); }
                else if (byte == '\t') { put_text(true); cursor_.position.x = next_tab_stop(cursor_.position.x); }
                else if (byte == '\b') { put_text(true); cursor_.position.x = std::max(0, cursor_.position.x - 1); }
                // BEL. It writes nothing and moves nothing, so the printable
                // run is left alone — flushing here would split a grapheme
                // that has not finished arriving. What it does is ask for the
                // reader's attention, which is the host's to give: this counts
                // the request and says so in the snapshot.
                else if (byte == '\a') { ++bell_serial_; damage_.bell = true; }
                else if (byte == '\x0f') { put_text(true); active_charset_ = 0; }  // SI
                else if (byte == '\x0e') { put_text(true); active_charset_ = 1; }  // SO
                else if (static_cast<unsigned char>(byte) >= 0x20) {
                    if (printable_.size() == options_.max_printable_run_bytes) { diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child printable run exceeded configured limit"); put_text(); }
                    // A designated line-drawing set turns letters into frame
                    // pieces. Substituting here keeps the rest of the parser,
                    // the grapheme splitter and the cell store working in one
                    // encoding rather than carrying the repertoire downstream.
                    const std::string_view graphic =
                        charsets_[active_charset_] == Charset::DecGraphics ? dec_graphic_for(byte)
                                                                           : std::string_view{};
                    if (graphic.empty()) printable_.push_back(byte);
                    else printable_.append(graphic);
                }
                break;
            case ParseState::Escape:
                if (byte == '[') { control_.clear(); parse_state_ = ParseState::Csi; }
                else if (byte == ']') { control_.clear(); parse_state_ = ParseState::Osc; }
                else if (byte == 'P') { control_.clear(); dcs_sixel_payload_ = false; parse_state_ = ParseState::Dcs; }
                else if (byte == 'D') { index(); parse_state_ = ParseState::Ground; }
                else if (byte == 'E') { cursor_.position.x = 0; index(); parse_state_ = ParseState::Ground; }
                else if (byte == 'M') { reverse_index(); parse_state_ = ParseState::Ground; }
                // HTS: a stop where the cursor is. This is how a program that
                // lays out columns says where they are, once, instead of
                // padding every row with spaces.
                else if (byte == 'H') {
                    put_text(true);
                    if (cursor_.position.x >= 0 && cursor_.position.x < static_cast<int>(tab_stops_.size()))
                        tab_stops_[static_cast<std::size_t>(cursor_.position.x)] = true;
                    parse_state_ = ParseState::Ground;
                }
                else if (byte == '(' || byte == ')' || byte == '*' || byte == '+') {
                    pending_charset_slot_ = static_cast<unsigned char>(byte == '(' ? 0 : byte == ')' ? 1 : byte == '*' ? 2 : 3);
                    parse_state_ = ParseState::Scs;
                }
                else if (byte == '7') { saved_cursor_ = cursor_; saved_cursor_valid_ = true; parse_state_ = ParseState::Ground; }
                else if (byte == '8') { if (saved_cursor_valid_) cursor_ = saved_cursor_; parse_state_ = ParseState::Ground; }
                // RIS. A program that has lost track of what it did to the
                // terminal — an editor killed mid-screen, a `cat` of a binary
                // — is put back by `reset(1)`, which sends this and nothing
                // else. An emulator that ignores it leaves the reader with the
                // wedged screen they were trying to escape.
                else if (byte == 'c') { put_text(true); reset_terminal(); parse_state_ = ParseState::Ground; }
                else if (byte == '#') { put_text(true); parse_state_ = ParseState::Hash; }
                // DECPAM and DECPNM, the application and numeric keypad. Known,
                // consumed, and deliberately without effect — D-053. Every
                // curses program sends `ESC =` on the way in and `ESC >` on the
                // way out (terminfo's `smkx`/`rmkx`), so reporting them as
                // unsupported would fill a bounded diagnostic ring with the one
                // thing every child does, crowding out the complaints a reader
                // needs; and there is nothing to record, because ckVision's key
                // model has no keypad key to send differently.
                else if (byte == '=' || byte == '>') { parse_state_ = ParseState::Ground; }
                else { diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child escape sequence"); parse_state_ = ParseState::Ground; }
                break;
            case ParseState::Scs:
                // The final byte names the repertoire. Everything that is not
                // the line-drawing set is treated as the ASCII one: the sets
                // that differ from it do so in accented letters, and showing
                // an unaccented letter beats showing the designation itself
                // as text across the middle of the screen.
                put_text(true);
                charsets_[pending_charset_slot_] = byte == '0' ? Charset::DecGraphics : Charset::Ascii;
                parse_state_ = ParseState::Ground;
                break;
            case ParseState::Hash:
                // The DEC private sequences with a '#' intermediate. Only the
                // alignment pattern is implemented; the others (double-width
                // and double-height lines) change how a row is drawn rather
                // than what it says, and are reported instead of silently
                // dropping the row's text.
                if (byte == '8') screen_alignment_pattern();
                else diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child DEC line-attribute sequence");
                parse_state_ = ParseState::Ground;
                break;
            case ParseState::Csi:
                if (byte >= '@' && byte <= '~') { handle_csi(byte); control_.clear(); parse_state_ = ParseState::Ground; }
                else append_control(byte);
                break;
            case ParseState::Osc:
                if (byte == '\a') { handle_osc(true); control_.clear(); parse_state_ = ParseState::Ground; }
                else if (byte == '\x1b') parse_state_ = ParseState::OscEscape;
                else append_control(byte);
                break;
            case ParseState::OscEscape:
                if (byte == '\\') { handle_osc(false); control_.clear(); parse_state_ = ParseState::Ground; }
                else {
                    const bool retained_escape = append_control('\x1b');
                    const bool retained_byte = retained_escape && append_control(byte);
                    if (retained_byte) parse_state_ = ParseState::Osc;
                }
                break;
            case ParseState::Dcs:
                if (byte == '\x1b') parse_state_ = ParseState::DcsEscape;
                else append_dcs_byte(byte);
                break;
            case ParseState::DcsEscape:
                if (byte == '\\') {
                    // A picture is recognised first and refused second. Asking
                    // whether this terminal has graphics before asking whether
                    // the child sent a picture put the specific answer behind a
                    // condition that had already excluded it — the refusal
                    // below could never fire, and a child drawing into a
                    // terminal with no graphics was told only that some DCS
                    // sequence had been ignored. Which sequence, and why, is
                    // exactly what the reader of a diagnostic needs.
                    if (is_sixel_control(control_)) {
                        if (!graphics_available()) {
                            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                                       "child Sixel ignored: this terminal declares no graphics");
                        } else {
                            const int cell_width = std::max(1, profile_.cell_pixels.width);
                            const int cell_height = std::max(1, profile_.cell_pixels.height);
                            // A picture starts at the cursor and stops at the
                            // edge of the screen, which is all of it anyone
                            // can ever see.
                            const auto room_span = [](int cells, int position, int pixels) {
                                const std::int64_t span = static_cast<std::int64_t>(std::max(0, cells - position)) *
                                                          static_cast<std::int64_t>(pixels);
                                return static_cast<int>(std::min<std::int64_t>(span, std::numeric_limits<int>::max()));
                            };
                            const Size room{room_span(cells_.width, cursor_.position.x, cell_width),
                                            room_span(cells_.height, cursor_.position.y, cell_height)};
                            // The same bytes into the same room are the same
                            // picture. Reusing it also lets the presenter
                            // recognise it as one it has already encoded and
                            // already put on the screen — and lets a host
                            // diffing by object identity see "unchanged"
                            // without touching a pixel. Sized for real
                            // pictures at real cell sizes: at 22x48px HiDPI
                            // cells a window-filling plot's payload passes
                            // two megabytes easily, and a cap under the
                            // ordinary case meant the ordinary case re-paid
                            // the whole decode on every redraw of an
                            // unchanged picture.
                            constexpr std::size_t kMaxCachedPayload = 16U * 1024U * 1024U;
                            if (last_sixel_image_ != nullptr && room == last_sixel_room_ &&
                                control_ == last_sixel_payload_) {
                                if (graphics_log_enabled())
                                    graphics_log("emulator: child re-sent the same Sixel (" +
                                                 std::to_string(control_.size()) + " bytes); reused the decode");
                                const Size extent{
                                    std::max(1, ceil_div_positive(last_sixel_image_->width(), cell_width)),
                                    std::max(1, ceil_div_positive(last_sixel_image_->height(), cell_height))};
                                place_raster(last_sixel_image_, cursor_.position, extent);
                                control_.clear();
                                dcs_sixel_payload_ = false;
                                parse_state_ = ParseState::Ground;
                                break;
                            }
                            std::string error;
                            const auto decode_started = graphics_log_enabled()
                                                            ? std::chrono::steady_clock::now()
                                                            : std::chrono::steady_clock::time_point{};
                            std::optional<DecodedSixel> decoded =
                                decode_sixel(control_, room, options_.max_image_pixels, sixel_palette_, error);
                            if (graphics_log_enabled()) {
                                const double ms = std::chrono::duration<double, std::milli>(
                                                      std::chrono::steady_clock::now() - decode_started)
                                                      .count();
                                graphics_log("emulator: decoded child Sixel of " + std::to_string(control_.size()) +
                                             " bytes in " + std::to_string(ms) + " ms -> " +
                                             (decoded ? std::to_string(decoded->image.width()) + "x" +
                                                            std::to_string(decoded->image.height()) + " px"
                                                      : "REJECTED: " + error));
                            }
                            if (decoded && !decoded->image.empty()) {
                                const Size extent{
                                    std::max(1, ceil_div_positive(decoded->image.width(), cell_width)),
                                    std::max(1, ceil_div_positive(decoded->image.height(), cell_height))};
                                auto picture = std::make_shared<Image>(std::move(decoded->image));
                                if (control_.size() <= kMaxCachedPayload) {
                                    last_sixel_payload_ = control_;
                                    last_sixel_image_ = picture;
                                    last_sixel_room_ = room;
                                }
                                place_raster(std::move(picture), cursor_.position, extent);
                            } else {
                                diagnostic(TerminalDiagnostic::Kind::MalformedSequence,
                                           "malformed child Sixel sequence: " + error);
                            }
                        }
                    } else {
                        diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "child DCS sequence ignored");
                    }
                    control_.clear();
                    dcs_sixel_payload_ = false;
                    parse_state_ = ParseState::Ground;
                }
                else {
                    const bool retained_escape = append_dcs_byte('\x1b');
                    const bool retained_byte = retained_escape && append_dcs_byte(byte);
                    if (retained_byte) parse_state_ = ParseState::Dcs;
                }
                break;
            case ParseState::Discard:
                if (byte == '\x1b') parse_state_ = ParseState::DiscardEscape;
                break;
            case ParseState::DiscardEscape:
                parse_state_ = byte == '\\' ? ParseState::Ground : ParseState::Discard;
                break;
        }
    }
    pending_output_.erase(0, budget);
    put_text();
    if (!(cursor_ == cursor_before)) damage_.cursor = true;
}

int TerminalEmulator::parameter(std::string_view text, std::size_t index, int default_value) {
    if (!text.empty() && text.front() == '?') text.remove_prefix(1);
    std::size_t first = 0;
    for (std::size_t current = 0; current < index; ++current) {
        const std::size_t next = text.find(';', first);
        if (next == std::string_view::npos) return default_value;
        first = next + 1;
    }
    const std::size_t last = text.find(';', first);
    const std::string_view value = text.substr(first, last == std::string_view::npos ? text.size() - first : last - first);
    if (value.empty()) return default_value;
    int parsed = default_value;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? parsed : default_value;
}

void TerminalEmulator::handle_csi(char final_byte) {
    // DECSCUSR (CSI Ps SP q) is emitted by interactive shells to select a
    // steady/blinking cursor style. The core records the declaration; the
    // outer presenter emits the host terminal's corresponding mode.
    if (final_byte == 'q' && !control_.empty() && control_.back() == ' ') {
        const int cursor_style = parameter(std::string_view(control_).substr(0, control_.size() - 1), 0, 0);
        cursor_.blink = (cursor_style == 1 || cursor_style == 3 || cursor_style == 5);
        if (cursor_style == 3 || cursor_style == 4)
            cursor_.shape = CursorShape::Underline;
        else if (cursor_style == 5 || cursor_style == 6)
            cursor_.shape = CursorShape::Bar;
        else
            cursor_.shape = CursorShape::Block;
        return;
    }
    // A nested child's own probe for synchronized output (DEC 2026) is
    // answered for real: whether to hold a child's damage until the
    // matching reset is this embedded terminal's own decision, not the
    // outer terminal's probe state, so — unlike the two below — this is not
    // something to withhold. ckVision's own probe sequence (posix_terminal.
    // cpp) sets the mode immediately before asking, so a profile that
    // supports it answers "set" here precisely because the dispatch above
    // already ran for the same input.
    if (final_byte == 'p' && control_.ends_with('$') && control_ == "?2026$") {
        send_input(std::string("\x1b[?2026;") +
                   (!profile_.synchronized_output ? "0" : synchronized_output_active_ ? "1" : "2") + "$y");
        return;
    }
    // Capability probes emitted by a nested ckVision child are private to
    // that child endpoint.  The profile deliberately does not expose the
    // outer terminal's probe state, so these queries receive no reply but
    // are still consumed as well-formed input rather than diagnostics.
    if (final_byte == 'p' && control_.ends_with('$') &&
        (control_ == "?2031$" || control_ == "?1016$"))
        return;
    // DA is `CSI Ps c` with `Ps` defaulting to 0 (ECMA-48), and the VT100
    // manual gives the request as `CSI c` OR `CSI 0 c` — the same question
    // written two ways. Matching only the empty form left `CSI 0 c` consumed
    // as an unrecognised CSI: no reply, and no diagnostic either, so a child
    // that spelled it that way waited forever. vttest spells it that way.
    //
    // Compared against the literal "0" rather than through `parameter()`,
    // which strips a leading private marker and would therefore answer DA2
    // (`CSI > c`) with a DA1 response.
    if (final_byte == 'c' && (control_.empty() || control_ == "0")) {
        // DA1 is where a program asks what this terminal is, and parameter 4
        // is the one answer that decides whether it draws a picture at all:
        // img2sixel, chafa, gnuplot and ckVision's own capability probe all
        // read the Sixel advertisement out of this list. Answering a fixed
        // `?1;2c` while the profile declares graphics tells every one of them
        // to fall back to text on a terminal that would have drawn the image.
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile) {
            send_input(graphics_available() ? "\x1b[?1;2;4c" : "\x1b[?1;2c");
            if (graphics_log_enabled())
                graphics_log(std::string("emulator: child asked DA1, answered ") +
                             (graphics_available() ? "?1;2;4c (Sixel)" : "?1;2c (no Sixel)"));
        }
        return;
    }
    // XTSMGRAPHICS asks for the graphics limits behind that advertisement.
    // It shares its final byte with SU (scroll up) and only the private
    // marker separates them: read as SU, a probe scrolls the child's own
    // screen away instead of answering it.
    if (final_byte == 'S' && !control_.empty() && control_.front() == '?') {
        handle_graphics_attributes();
        return;
    }
    if (final_byte == 't' && control_ == "16") {
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile)
            send_input("\x1b[6;" + std::to_string(std::max(1, profile_.cell_pixels.height)) + ";" +
                       std::to_string(std::max(1, profile_.cell_pixels.width)) + "t");
        return;
    }
    // XTWINOPS 22/23: push and pop the window title. They share their final
    // byte with the geometry reports above and are told apart by their first
    // parameter, which is what `CSI 22 ; 0 t` means.
    if (final_byte == 't' && (parameter(control_, 0, 0) == 22 || parameter(control_, 0, 0) == 23)) {
        handle_title_stack(parameter(control_, 0, 0) == 22);
        return;
    }
    if (final_byte == 't' && control_ == "14") {
        // XTWINOPS 14: the text area's total pixel size — cell metric times
        // the grid, exactly as a real terminal derives it.
        if (profile_.query_policy == TerminalQueryPolicy::DeclaredProfile)
            send_input("\x1b[4;" + std::to_string(std::max(1, profile_.cell_pixels.height) * cells_.height) +
                       ";" + std::to_string(std::max(1, profile_.cell_pixels.width) * cells_.width) + "t");
        return;
    }
    if (final_byte == 's') {
        saved_cursor_ = cursor_;
        saved_cursor_valid_ = true;
        return;
    }
    if (final_byte == 'u') {
        // Bare `CSI u` is the cursor restore it has always been. The kitty
        // keyboard protocol's forms all carry a private prefix, which is what
        // keeps the two apart.
        if (control_.empty()) {
            if (saved_cursor_valid_) cursor_ = saved_cursor_;
            return;
        }
        handle_keyboard_protocol();
        return;
    }
    if ((final_byte == 'h' || final_byte == 'l') && !control_.empty() && control_.front() == '?') {
        // One mark for the whole loop: which of the modes changed is the
        // host's business to read out of the snapshot, and this only has to
        // say that the set is no longer what it last sent.
        damage_.modes = true;
        const bool enabled = final_byte == 'h';
        std::string_view modes(control_);
        modes.remove_prefix(1);
        std::size_t begin = 0;
        while (begin <= modes.size()) {
            const std::size_t end = modes.find(';', begin);
            const std::string_view value = modes.substr(begin, end == std::string_view::npos ? modes.size() - begin : end - begin);
            const int mode = parameter(value, 0, -1);
            if (mode == 1049) {
                // A different screen entirely: nothing a host knew about the
                // one it was shown carries over.
                if (alternate_buffer_ != enabled) damage_everything();
                if (enabled) {
                    saved_cursor_ = cursor_;
                    saved_cursor_valid_ = true;
                    alternate_buffer_ = true;
                    reset_active_buffer();
                    clear_rasters();
                    cursor_.position = {};
                } else {
                    alternate_buffer_ = false;
                    clear_rasters();
                    if (saved_cursor_valid_) cursor_ = saved_cursor_;
                }
            } else if (mode == 47 || mode == 1047) {
                if (alternate_buffer_ != enabled) damage_everything();
                alternate_buffer_ = enabled;
                if (enabled) {
                    reset_active_buffer();
                    clear_rasters();
                    cursor_.position = {};
                } else {
                    clear_rasters();
                }
            } else if (mode == 1048) {
                if (enabled) {
                    saved_cursor_ = cursor_;
                    saved_cursor_valid_ = true;
                } else if (saved_cursor_valid_) {
                    cursor_ = saved_cursor_;
                }
            } else if (mode == 2004) {
                bracketed_paste_enabled_ = enabled && profile_.bracketed_paste;
            } else if (mode == 7) {
                autowrap_ = enabled;
            } else if (mode == 6) {
                origin_mode_ = enabled;
                cursor_.position = Point{0, origin_mode_ ? scroll_top_ : 0};
            } else if (mode == 1) {
                application_cursor_keys_ = enabled;
            } else if (mode == 1000 || mode == 1002 || mode == 1003) {
                // Three levels of one facility, not three switches. Setting one
                // selects that level — a program that wants drag motion sends
                // 1002 and means "instead of", not "as well as" — and resetting
                // any of them ends tracking, which is how a program that turned
                // on 1002 and puts back 1000 and 1002 on its way out leaves the
                // pointer to the terminal rather than half-tracked.
                const TerminalMouseTracking level = mode == 1000 ? TerminalMouseTracking::Buttons
                                                    : mode == 1002
                                                        ? TerminalMouseTracking::ButtonMotion
                                                        : TerminalMouseTracking::AnyMotion;
                mouse_tracking_ = enabled && profile_.mouse_reporting ? level
                                                                      : TerminalMouseTracking::None;
                if (mouse_tracking_ == TerminalMouseTracking::None) mouse_sgr_enabled_ = false;
            } else if (mode == 1006) {
                mouse_sgr_enabled_ = enabled && profile_.mouse_reporting;
            } else if (mode == 1004) {
                focus_reporting_enabled_ = enabled;
            } else if (mode == 1007) {
                // Alternate scroll: with the alternate screen up and no mouse
                // tracking of its own, the child wants a wheel notch as cursor
                // keys. The emulator only records the request — turning a
                // wheel into keys needs a wheel, and that is the view's end.
                alternate_scroll_enabled_ = enabled;
            } else if (mode == 18) {
                printer_form_feed_ = enabled;  // DECPFF
            } else if (mode == 19) {
                printer_print_extent_ = enabled;  // DECPEX
            } else if (mode == 25) {
                cursor_.visible = enabled;
            } else if (mode == 2026) {
                synchronized_output_active_ = enabled && profile_.synchronized_output;
            }
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        return;
    }
    // The ANSI modes, which carry no '?' and are a separate space from the
    // DEC private ones above: the same number means different things in each.
    if (final_byte == 'h' || final_byte == 'l') {
        damage_.modes = true;
        const bool enabled = final_byte == 'h';
        std::string_view modes(control_);
        std::size_t begin = 0;
        while (begin <= modes.size()) {
            const std::size_t end = modes.find(';', begin);
            const std::string_view value =
                modes.substr(begin, end == std::string_view::npos ? modes.size() - begin : end - begin);
            if (parameter(value, 0, -1) == 4) insert_mode_ = enabled;
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
        return;
    }
    if (final_byte == 'H' || final_byte == 'f') {
        const int origin_top = origin_mode_ ? scroll_top_ : 0;
        const int origin_bottom = origin_mode_ ? scroll_bottom_ : cells_.height;
        cursor_.position.y = std::clamp(origin_top + parameter(control_, 0, 1) - 1, origin_top, origin_bottom - 1);
        cursor_.position.x = std::clamp(parameter(control_, 1, 1) - 1, 0, cells_.width - 1);
        return;
    }
    if (final_byte == 'G' || final_byte == '`') {
        cursor_.position.x = std::clamp(parameter(control_, 0, 1) - 1, 0, cells_.width - 1);
        return;
    }
    if (final_byte == 'd') {
        const int origin_top = origin_mode_ ? scroll_top_ : 0;
        const int origin_bottom = origin_mode_ ? scroll_bottom_ : cells_.height;
        cursor_.position.y = std::clamp(origin_top + parameter(control_, 0, 1) - 1,
                                         origin_top, origin_bottom - 1);
        return;
    }
    if (final_byte == 'E') {
        cursor_.position.y = std::min((origin_mode_ ? scroll_bottom_ : cells_.height) - 1,
                                      cursor_.position.y + parameter(control_, 0, 1));
        cursor_.position.x = 0;
        return;
    }
    if (final_byte == 'F') {
        cursor_.position.y = std::max(origin_mode_ ? scroll_top_ : 0,
                                      cursor_.position.y - parameter(control_, 0, 1));
        cursor_.position.x = 0;
        return;
    }
    // CHT and CBT: forward and backward by whole tab stops. A program drawing
    // a table moves between its columns with these rather than by counting
    // spaces, so an emulator that ignores them lands every field in the wrong
    // place.
    if (final_byte == 'I') {
        const int count = std::clamp(parameter(control_, 0, 1), 0, cells_.width);
        for (int step = 0; step < count; ++step) cursor_.position.x = next_tab_stop(cursor_.position.x);
        return;
    }
    if (final_byte == 'Z') {
        const int count = std::clamp(parameter(control_, 0, 1), 0, cells_.width);
        for (int step = 0; step < count; ++step) cursor_.position.x = previous_tab_stop(cursor_.position.x);
        return;
    }
    // TBC: clear the stop under the cursor (0) or every stop there is (3).
    // Anything else is a form this terminal does not have — the stops are
    // per screen, not per line — and is reported rather than guessed at.
    if (final_byte == 'g') {
        const int mode = parameter(control_, 0, 0);
        if (mode == 0) {
            if (cursor_.position.x >= 0 && cursor_.position.x < static_cast<int>(tab_stops_.size()))
                tab_stops_[static_cast<std::size_t>(cursor_.position.x)] = false;
        } else if (mode == 3) {
            std::fill(tab_stops_.begin(), tab_stops_.end(), false);
        } else {
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child tab-clear form");
        }
        return;
    }
    if (final_byte == 'W') {
        // DECST8C (CSI ? 5 W) puts the default stops back, which is how a
        // program that found the terminal in an unknown state starts from a
        // known one. CTC (no private marker) is the same three operations
        // under their ECMA-48 names.
        if (!control_.empty() && control_.front() == '?') {
            if (parameter(control_, 0, 0) == 5) reset_tab_stops();
            else diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child tab-set form");
            return;
        }
        const int mode = parameter(control_, 0, 0);
        const std::size_t column = static_cast<std::size_t>(std::max(0, cursor_.position.x));
        if (mode == 0 && column < tab_stops_.size()) tab_stops_[column] = true;
        else if (mode == 2 && column < tab_stops_.size()) tab_stops_[column] = false;
        else if (mode == 5) std::fill(tab_stops_.begin(), tab_stops_.end(), false);
        else diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child tab-control form");
        return;
    }
    if (final_byte == 'A') { cursor_.position.y = std::max(0, cursor_.position.y - parameter(control_, 0, 1)); return; }
    if (final_byte == 'B') { cursor_.position.y = std::min(cells_.height - 1, cursor_.position.y + parameter(control_, 0, 1)); return; }
    if (final_byte == 'C') { cursor_.position.x = std::min(cells_.width - 1, cursor_.position.x + parameter(control_, 0, 1)); return; }
    if (final_byte == 'D') { cursor_.position.x = std::max(0, cursor_.position.x - parameter(control_, 0, 1)); return; }
    if (final_byte == 'b') {
        // REP: repeat the last printed character. This is how a curses
        // program draws a long run cheaply -- one character and a count
        // instead of the whole run -- so an emulator without it shows the
        // first cell of every rule and nothing after it, which is a frame
        // with its edges missing rather than an obviously broken screen.
        if (last_graphic_.empty()) return;
        // Bounded by the screen: a count larger than that cannot describe
        // anything visible, and an unbounded loop here is reachable from
        // child output.
        const int limit = std::max(1, cells_.width * cells_.height);
        const int count = std::clamp(parameter(control_, 0, 1), 0, limit);
        const std::string repeated = last_graphic_;  // put_grapheme rewrites the member
        for (int i = 0; i < count; ++i) put_grapheme(repeated);
        return;
    }
    if (final_byte == '@') { insert_cells(parameter(control_, 0, 1)); return; }
    if (final_byte == 'P') { delete_cells(parameter(control_, 0, 1)); return; }
    if (final_byte == 'L') {
        scroll_down_region(cursor_.position.y, scroll_bottom_, parameter(control_, 0, 1));
        return;
    }
    if (final_byte == 'M') {
        scroll_up_region(cursor_.position.y, scroll_bottom_, parameter(control_, 0, 1));
        return;
    }
    if (final_byte == 'X') {
        const int count = std::clamp(parameter(control_, 0, 1), 0, cells_.width - cursor_.position.x);
        erase_cells(cursor_.position.x, cursor_.position.y, cursor_.position.x + count, cursor_.position.y + 1);
        return;
    }
    if (final_byte == 'J') {
        const int mode = parameter(control_, 0, 0);
        if (mode == 0) erase_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        if (mode == 0 && cursor_.position.y + 1 < cells_.height)
            erase_cells(0, cursor_.position.y + 1, cells_.width, cells_.height);
        else if (mode == 1) {
            if (cursor_.position.y > 0) erase_cells(0, 0, cells_.width, cursor_.position.y);
            erase_cells(0, cursor_.position.y, cursor_.position.x + 1, cursor_.position.y + 1);
        } else if (mode == 2) {
            erase_cells(0, 0, cells_.width, cells_.height);
            clear_rasters();
        } else if (mode == 3) {
            clear_scrollback();
        }
        return;
    }
    if (final_byte == 'K') {
        const int mode = parameter(control_, 0, 0);
        if (mode == 0) erase_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        else if (mode == 1) erase_cells(0, cursor_.position.y, cursor_.position.x + 1, cursor_.position.y + 1);
        else if (mode == 2) erase_cells(0, cursor_.position.y, cells_.width, cursor_.position.y + 1);
        return;
    }
    if (final_byte == 'r') {
        const int top = std::clamp(parameter(control_, 0, 1) - 1, 0, cells_.height - 1);
        const int bottom = std::clamp(parameter(control_, 1, cells_.height), 1, cells_.height);
        if (top >= bottom) return;
        scroll_top_ = top;
        scroll_bottom_ = bottom;
        cursor_.position = Point{0, origin_mode_ ? scroll_top_ : 0};
        return;
    }
    if (final_byte == 'S') { scroll_up_region(scroll_top_, scroll_bottom_, parameter(control_, 0, 1)); return; }
    if (final_byte == 'T') { scroll_down_region(scroll_top_, scroll_bottom_, parameter(control_, 0, 1)); return; }
    if (final_byte == 'm') {
        handle_sgr();
        return;
    }
    if (final_byte == 'i') {
        handle_media_copy();
        return;
    }
    if (final_byte == 'n' && profile_.query_policy == TerminalQueryPolicy::DeclaredProfile) {
        const int query = parameter(control_, 0, 0);
        // DSR 15: is there a printer? Answered honestly, because a program
        // that is told there is one and then prints into silence has no way
        // to find out otherwise. Ready, not-ready (a job overflowed and is
        // being sunk), or no-printer at all.
        if (!control_.empty() && control_.front() == '?' && query == 15) {
            if (!printer_available()) send_input("\x1b[?13n");
            else if (printer_overflowed_) send_input("\x1b[?11n");
            else send_input("\x1b[?10n");
            return;
        }
        if (query == 5) send_input("\x1b[0n");
        else if (query == 6)
            send_input("\x1b[" + std::to_string(cursor_.position.y + 1) + ";" +
                       std::to_string(cursor_.position.x + 1) + "R");
        return;
    }
    diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child CSI sequence");
}

// Whether a child's picture can be shown at all, which is what every graphics
// answer below is derived from. This is the profile's declaration and nothing
// else: a picture is decoded at its own size, so how large the reader has made
// their terminal has no bearing on whether one can be drawn in it. What the
// pixel budget bounds is the picture, and a picture past it is refused when it
// arrives — with its own measurements — rather than by pretending in advance
// that this terminal has no graphics.
bool TerminalEmulator::graphics_available() const noexcept { return profile_.sixel; }

// XTSMGRAPHICS (CSI ? Pi ; Pa ; Pv S), the graphics counterpart of the
// XTWINOPS metrics above. Pi names the item — 1 the Sixel colour registers,
// 2 the Sixel geometry, 3 ReGIS geometry — and Pa the action: 1 read the
// current value, 2 reset it to the default, 3 set it, 4 read the maximum.
// The reply repeats Pi, then a status: 0 success, 1 an item this terminal
// does not know, 2 an action it does not know, 3 a request it will not
// grant. Something is always sent back, because the program on the other end
// is waiting for an answer and silence costs it a timeout before it gives up.
void TerminalEmulator::handle_graphics_attributes() {
    if (profile_.query_policy != TerminalQueryPolicy::DeclaredProfile) return;
    const int item = parameter(control_, 0, 0);
    const int action = parameter(control_, 1, 0);
    const std::string reply = "\x1b[?" + std::to_string(item) + ";";
    if (item < 1 || item > 3) {
        send_input(reply + "1;0S");
        return;
    }
    if (action < 1 || action > 4) {
        send_input(reply + "2;0S");
        return;
    }
    // Reading, resetting and reading the maximum all land on the same number
    // here: these limits are the decoder's palette and the child's own
    // window, neither of which a child may move. A set request is therefore
    // refused rather than acknowledged — an acknowledgement would be a
    // promise the next image breaks.
    if (action == 3 || item == 3 || !graphics_available()) {
        send_input(reply + "3;0S");
        return;
    }
    if (item == 1) {
        send_input(reply + "0;" + std::to_string(kSixelColorRegisters) + "S");
        if (graphics_log_enabled())
            graphics_log("emulator: child asked XTSMGRAPHICS registers, answered " +
                         std::to_string(kSixelColorRegisters));
        return;
    }
    // The pixel budget, as the largest square it admits — NOT the window
    // size. The window was the first answer here, and it stranded a child
    // whose picture later outgrew it: this maximum is probed once, there is
    // no unsolicited re-advertisement in the protocol, and a well-behaved
    // emitter (ckVision's own presenter among them) refuses outright to send
    // a picture past the maximum it was told — so a child whose window then
    // grew showed nothing at all, while a reader's real terminal advertises a
    // generous fixed maximum (16384x16384 observed) precisely so no resize
    // can invalidate it. What is decoded is the picture, bounded by the same
    // budget this derives from and cropped to the room it lands in, so this
    // is the one answer that stays true at every size the terminal ever has.
    std::size_t side = 1;
    while (side < 16384U && (side + 1U) * (side + 1U) <= options_.max_image_pixels) ++side;
    const std::string geometry = std::to_string(side) + ";" + std::to_string(side);
    send_input(reply + "0;" + geometry + "S");
    if (graphics_log_enabled())
        graphics_log("emulator: child asked XTSMGRAPHICS geometry, answered " + geometry);
}

// The kitty keyboard protocol, from the terminal's side. A program pushes the
// enhancements it wants, uses them, and pops them back off on its way out —
// the stack is what makes that safe when the program is killed, nested, or
// interrupted by another that wants something different.
//
// The stack is per screen, so a full-screen editor's settings do not follow
// it out onto the shell's screen. A program that dies without popping leaves
// its flags on the alternate screen it died in, and the shell underneath is
// unaffected.
void TerminalEmulator::handle_keyboard_protocol() {
    KeyboardState& state = alternate_buffer_ ? alternate_keyboard_ : primary_keyboard_;
    const char introducer = control_.front();
    const std::string_view parameters(std::string_view(control_).substr(1));
    const auto requested = [&parameters](std::size_t index) {
        return static_cast<TerminalKeyboardFlags>(static_cast<std::uint8_t>(
            std::clamp(parameter(parameters, index, 0), 0, 0xFF)));
    };
    // Only what this terminal can really deliver takes effect, so what a
    // program reads back is what it will really receive.
    const auto honoured = [](TerminalKeyboardFlags flags) {
        return flags & supported_terminal_keyboard_flags();
    };

    // Every path below that moves the flags says so, and the query does not:
    // these are modes in all but their spelling — a set of switches a program
    // turns on for as long as it needs them — and a host that forwards this
    // terminal has to send the new set before it sends the next key, or the
    // child is answered in an encoding it has stopped expecting. Asking what
    // they are moves nothing.
    const TerminalKeyboardFlags before = state.flags;
    const auto moved = [this, &state, before] {
        if (state.flags != before) damage_.modes = true;
    };

    switch (introducer) {
        case '?':
            if (profile_.query_policy != TerminalQueryPolicy::DeclaredProfile) return;
            send_input("\x1b[?" + std::to_string(static_cast<int>(state.flags)) + "u");
            return;
        case '>': {
            // A stack has to have a bottom. Dropping the oldest entry keeps a
            // program that pushes without popping from growing this without
            // limit; it can only ever lose a setting it had already stopped
            // tracking.
            constexpr std::size_t kMaxDepth = 16;
            if (state.stack.size() >= kMaxDepth) state.stack.erase(state.stack.begin());
            state.stack.push_back(state.flags);
            state.flags = honoured(requested(0));
            moved();
            return;
        }
        case '<': {
            const int count = std::max(0, parameter(parameters, 0, 1));
            for (int popped = 0; popped < count; ++popped) {
                if (state.stack.empty()) {
                    // Popping more than was ever pushed returns to no
                    // enhancements at all: that is the state a terminal
                    // starts in, and the only safe answer to "put it back the
                    // way it was" when nothing was saved.
                    state.flags = TerminalKeyboardFlags::None;
                    break;
                }
                state.flags = state.stack.back();
                state.stack.pop_back();
            }
            moved();
            return;
        }
        case '=': {
            const TerminalKeyboardFlags flags = honoured(requested(0));
            const int mode = parameter(parameters, 1, 1);
            if (mode == 2) state.flags = state.flags | flags;
            else if (mode == 3)
                state.flags = static_cast<TerminalKeyboardFlags>(
                    static_cast<std::uint8_t>(state.flags) & ~static_cast<std::uint8_t>(flags));
            else state.flags = flags;
            moved();
            return;
        }
        default:
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child CSI sequence");
            return;
    }
}

// SGR is the one control a child writes with sub-parameters. `4:3` is a curly
// underline rather than parameters 4 and 3, and `58:2::R:G:B` is one colour
// rather than five numbers, so the parameter list is read as values each with
// their own colon-separated tail. Everything else in this parser takes plain
// semicolon-separated numbers, which is why this reading is here and not in
// `parameter()`.
//
// The reading is deliberately not shared with the virtual display's, which
// decodes what the Presenter writes: that decoder is an independent oracle
// (D-035), and an oracle that agreed with the thing it checks by construction
// would not be one.
std::vector<TerminalEmulator::SgrParameter> TerminalEmulator::parse_sgr_parameters(
    std::string_view text, bool& valid) {
    std::vector<SgrParameter> parameters;
    valid = true;
    std::size_t position = 0;
    for (;;) {
        SgrParameter parameter;
        for (bool first = true;; first = false) {
            const std::size_t start = position;
            while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
            int value = 0;
            if (position > start) {
                const auto parsed =
                    std::from_chars(text.data() + start, text.data() + position, value);
                // A run of digits longer than an int is not a colour anybody
                // meant; treat it as the malformed sequence it is.
                if (parsed.ec != std::errc{}) {
                    valid = false;
                    return {};
                }
            }
            if (first) parameter.value = value;
            else parameter.subs.push_back(value);
            if (position == text.size() || text[position] != ':') break;
            ++position;
        }
        parameters.push_back(std::move(parameter));
        if (position == text.size()) break;
        if (text[position] != ';') {
            valid = false;
            return {};
        }
        ++position;
        if (position == text.size()) {
            parameters.push_back(SgrParameter{});
            break;
        }
    }
    return parameters;
}

void TerminalEmulator::handle_sgr() {
    bool valid = false;
    const std::vector<SgrParameter> parameters = parse_sgr_parameters(control_, valid);
    if (!valid || parameters.empty()) {
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "malformed child SGR sequence");
        return;
    }
    const auto clear_attrs = [this](Attr attrs) {
        style_.attrs = static_cast<Attr>(static_cast<std::uint8_t>(style_.attrs) &
                                         static_cast<std::uint8_t>(~static_cast<std::uint8_t>(attrs)));
    };
    // An underline that is not being drawn has no shape and no colour of its
    // own. Keeping that canonical is what lets two cells that look the same
    // compare the same — and the presenter redraw only what changed.
    const auto clear_underline = [this, &clear_attrs] {
        clear_attrs(Attr::Underline);
        style_.underline = UnderlineShape::Straight;
        style_.underline_color = Color::default_color();
    };

    for (std::size_t index = 0; index < parameters.size(); ++index) {
        const SgrParameter& parameter = parameters[index];
        const int value = parameter.value;
        if (value == 0) style_ = profile_.default_style;
        else if (value == 1) style_.attrs |= Attr::Bold;
        else if (value == 2) style_.attrs |= Attr::Dim;
        else if (value == 3) style_.attrs |= Attr::Italic;
        else if (value == 4) {
            // Bare `4` is the rule underlines have always been; `4:0` turns
            // it off, and the remaining shapes are what a compiler's
            // diagnostics use to tell a spelling mistake from a type error.
            const int shape = parameter.subs.empty() ? 1 : parameter.subs.front();
            if (parameter.subs.size() > 1) {
                diagnostic(TerminalDiagnostic::Kind::MalformedSequence,
                           "malformed child underline style");
                return;
            }
            if (shape == 0) {
                clear_underline();
            } else if (shape >= 1 && shape <= 5) {
                style_.attrs |= Attr::Underline;
                style_.underline = shape == 2   ? UnderlineShape::Double
                                   : shape == 3 ? UnderlineShape::Curly
                                   : shape == 4 ? UnderlineShape::Dotted
                                   : shape == 5 ? UnderlineShape::Dashed
                                                : UnderlineShape::Straight;
            } else {
                // A shape nobody has defined is not a reason to lose the
                // underline the program plainly wants.
                style_.attrs |= Attr::Underline;
                style_.underline = UnderlineShape::Straight;
                diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence,
                           "unsupported child underline style");
            }
        }
        else if (value == 7) style_.attrs |= Attr::Reverse;
        else if (value == 9) style_.attrs |= Attr::Strike;
        // SGR 21 is a double underline. It was once "bold off" on hardware
        // that could not do either, and terminals that answer it at all now
        // answer it this way; 22 is the reset every program actually uses to
        // end bold.
        else if (value == 21) {
            style_.attrs |= Attr::Underline;
            style_.underline = UnderlineShape::Double;
        }
        else if (value == 22) clear_attrs(Attr::Bold | Attr::Dim);
        else if (value == 23) clear_attrs(Attr::Italic);
        else if (value == 24) clear_underline();
        else if (value == 27) clear_attrs(Attr::Reverse);
        else if (value == 29) clear_attrs(Attr::Strike);
        else if (value == 39) style_.fg = profile_.default_style.fg;
        else if (value == 49) style_.bg = profile_.default_style.bg;
        else if (value == 59) style_.underline_color = Color::default_color();
        else if ((value >= 30 && value <= 37) || (value >= 90 && value <= 97))
            style_.fg = Color::indexed(static_cast<std::uint8_t>(value >= 90 ? value - 90 + 8 : value - 30));
        else if ((value >= 40 && value <= 47) || (value >= 100 && value <= 107))
            style_.bg = Color::indexed(static_cast<std::uint8_t>(value >= 100 ? value - 100 + 8 : value - 40));
        else if (value == 38 || value == 48 || value == 58) {
            Color color;
            if (!extended_color(parameters, index, parameter, color)) return;
            if (value == 48) style_.bg = color;
            else if (value == 58) style_.underline_color = color;
            else style_.fg = color;
        } else if (value != 5 && value != 6) {
            diagnostic(TerminalDiagnostic::Kind::UnsupportedSequence, "unsupported child SGR parameter");
        }
    }
}

// The colour that follows `38`, `48` or `58`, in either of the two spellings
// programs use: the older one where the parts are ordinary parameters
// (`38;5;196`), and the ISO 8613-6 one where they are sub-parameters of a
// single parameter (`38:2::12:34:56`, with the colour space left unnamed).
// The second is the form that made colon sub-parameters necessary at all, and
// nvim writes underline colours in it.
//
// `index` advances past whatever this colour consumed. A malformed colour
// abandons the rest of the sequence: the parameters after it can no longer be
// located, and applying them by guesswork is how a stray colour ends up on
// the screen.
bool TerminalEmulator::extended_color(const std::vector<SgrParameter>& parameters,
                                      std::size_t& index, const SgrParameter& parameter,
                                      Color& color) {
    const auto in_byte_range = [](int value) { return value >= 0 && value <= 255; };
    if (!parameter.subs.empty()) {
        const std::vector<int>& subs = parameter.subs;
        const int mode = subs.front();
        if (mode == 5 && subs.size() >= 2 && in_byte_range(subs[1])) {
            color = Color::indexed(static_cast<std::uint8_t>(subs[1]));
            return true;
        }
        // Five sub-parameters carry the colour space that the three-component
        // form omits; both spellings are in circulation.
        const std::size_t first = subs.size() >= 5 ? 2 : 1;
        if (mode == 2 && subs.size() >= first + 3 && in_byte_range(subs[first]) &&
            in_byte_range(subs[first + 1]) && in_byte_range(subs[first + 2])) {
            color = Color::rgb(static_cast<std::uint8_t>(subs[first]),
                               static_cast<std::uint8_t>(subs[first + 1]),
                               static_cast<std::uint8_t>(subs[first + 2]));
            return true;
        }
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "invalid child extended color");
        return false;
    }
    if (index + 1 >= parameters.size()) {
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "truncated child extended color");
        return false;
    }
    const int mode = parameters[++index].value;
    if (mode == 5 && index + 1 < parameters.size() && in_byte_range(parameters[index + 1].value)) {
        color = Color::indexed(static_cast<std::uint8_t>(parameters[++index].value));
        return true;
    }
    if (mode == 2 && index + 3 < parameters.size() && in_byte_range(parameters[index + 1].value) &&
        in_byte_range(parameters[index + 2].value) && in_byte_range(parameters[index + 3].value)) {
        color = Color::rgb(static_cast<std::uint8_t>(parameters[index + 1].value),
                           static_cast<std::uint8_t>(parameters[index + 2].value),
                           static_cast<std::uint8_t>(parameters[index + 3].value));
        index += 3;
        return true;
    }
    diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "invalid child extended color");
    return false;
}

// The DEC Special Graphics repertoire, which occupies the ASCII letter
// range while designated. Only the box-drawing half is what programs
// actually reach for, but the whole published range is here: a partial
// table would render most of a frame and corrupt the rest, which is
// harder to recognise than rendering none of it.
// The cursor, the modes and the title are the parts of a frame that are not
// cells. They are marked rather than compared for the same reason the cells
// are: the emulator knows when it changed them, and a host that had to notice
// by comparison would need last frame's copy of each.
std::string_view TerminalEmulator::dec_graphic_for(char byte) noexcept {
    switch (byte) {
        case '_': return " ";
        case '`': return "\u25c6";  // diamond
        case 'a': return "\u2592";  // checker board
        case 'b': return "\u2409";  // HT
        case 'c': return "\u240c";  // FF
        case 'd': return "\u240d";  // CR
        case 'e': return "\u240a";  // LF
        case 'f': return "\u00b0";  // degree
        case 'g': return "\u00b1";  // plus/minus
        case 'h': return "\u2424";  // NL
        case 'i': return "\u240b";  // VT
        case 'j': return "\u2518";
        case 'k': return "\u2510";
        case 'l': return "\u250c";
        case 'm': return "\u2514";
        case 'n': return "\u253c";
        case 'o': return "\u23ba";  // scan line 1
        case 'p': return "\u23bb";  // scan line 3
        case 'q': return "\u2500";
        case 'r': return "\u23bc";  // scan line 7
        case 's': return "\u23bd";  // scan line 9
        case 't': return "\u251c";
        case 'u': return "\u2524";
        case 'v': return "\u2534";
        case 'w': return "\u252c";
        case 'x': return "\u2502";
        case 'y': return "\u2264";
        case 'z': return "\u2265";
        case '{': return "\u03c0";
        case '|': return "\u2260";
        case '}': return "\u00a3";
        case '~': return "\u00b7";
        default: return {};
    }
}

void TerminalEmulator::put_text(bool flush_incomplete) {
    const std::size_t ready = flush_incomplete ? printable_.size() : complete_utf8_prefix(printable_);
    const std::string_view ready_text(printable_.data(), ready);
    std::size_t position = 0;
    while (position < ready) {
        const std::size_t end = text::grapheme_end(ready_text, position);
        if (end <= position) break;
        put_grapheme(std::string_view(printable_).substr(position, end - position));
        position = end;
    }
    printable_.erase(0, position);
}

void TerminalEmulator::put_grapheme(std::string_view grapheme) {
    if (cursor_.position.x >= cells_.width) {
        // Autowrap off means the last column is written over and over rather
        // than the cursor falling to the next line — which is how a program
        // fills the bottom-right cell without scrolling the screen.
        if (!autowrap_) cursor_.position.x = cells_.width - 1;
        else { cursor_.position.x = 0; index(); }
    }
    last_graphic_.assign(grapheme);
    const Style style = bold_brightened(style_, profile_.default_style);
    Cell cell = Cell::from_grapheme(grapheme, style);
    const int width = std::max(1, cell.width());
    if (insert_mode_) insert_cells(width);

    auto& cells = active_cells();
    const auto at = [this, &cells](int x) -> Cell& {
        return cells[static_cast<std::size_t>(cursor_.position.y * cells_.width + x)];
    };
    // A double-width character occupies two cells that only mean anything
    // together. Writing over either one leaves the other a half of nothing,
    // so it is blanked rather than left to render as a stray fragment of a
    // character that is no longer there.
    const int x = cursor_.position.x;
    if (at(x).is_continuation() && x > 0) at(x - 1) = erase_cell();
    if (at(x).width() == 2 && x + 1 < cells_.width) at(x + 1) = erase_cell();
    if (width == 2 && x + 1 < cells_.width && at(x + 1).width() == 2 && x + 2 < cells_.width)
        at(x + 2) = erase_cell();

    at(x) = cell;
    if (width == 2 && x + 1 < cells_.width) at(x + 1) = Cell::continuation(style_);
    wrote_cells(x, cursor_.position.y, x + width, cursor_.position.y + 1);
    cursor_.position.x += width;
}

void TerminalEmulator::clear_rasters() noexcept {
    rasters_.clear();
    raster_coverage_.clear();
}

void TerminalEmulator::place_raster(std::shared_ptr<Image> image, Point anchor, Size cell_extent) {
    // Whatever was under it is under it no longer, exactly as if the program
    // had written text there — which is what a picture drawn over a picture
    // means on a terminal.
    damage_rasters(anchor.x, anchor.y, anchor.x + cell_extent.width, anchor.y + cell_extent.height);
    damage_.rasters = true;

    // A screen's worth of pieces is the most a picture can ever be split
    // into by anything sensible; past that the oldest goes, so a program
    // that draws without ever clearing cannot grow this without bound.
    constexpr std::size_t kMaxRasters = 64;
    if (rasters_.size() >= kMaxRasters) {
        rasters_.erase(rasters_.begin());
        raster_coverage_.erase(raster_coverage_.begin());
    }

    RasterCoverage coverage;
    // Whether these pixels are somebody else's too. They are when the picture
    // came from the decode cache — the same object is handed back every time
    // a child re-sends identical bytes — and erasing into it there would put
    // holes in the copy the next reuse hands out, so a picture that was
    // partly written over would come back partly missing. Treating a shared
    // image as already handed out makes the first erase copy it, exactly as
    // it does for pixels a snapshot is still holding.
    coverage.handed_out = image.use_count() > 1;
    coverage.live_cells.assign(
        static_cast<std::size_t>(std::max(1, cell_extent.width)) * static_cast<std::size_t>(std::max(1, cell_extent.height)),
        true);
    coverage.live_count = coverage.live_cells.size();
    coverage.image = std::move(image);
    // Sharing raster_identity_ verbatim across every picture this terminal
    // ever places was fine while there was ever only one — but a second
    // picture placed before the first is overwritten (see kMaxRasters above:
    // that is the expected case, not a corner one) then collided with it on
    // whatever Surface came to draw both, and Surface::add_raster_region's
    // own uniqueness contract turned that into a hard abort. An unadopted
    // terminal (raster_identity_ == 0) keeps every raster at id 0 — the
    // view's own "don't draw" signal — rather than manufacturing a nonzero
    // id no adoption ever granted.
    const int id = raster_identity_ == 0
                       ? 0
                       : raster_identity_ + core::allocate_local_raster_slot(rasters_, raster_identity_);
    rasters_.push_back(TerminalRaster{id, anchor, cell_extent, coverage.image, "[sixel]"});
    raster_coverage_.push_back(std::move(coverage));
}

void TerminalEmulator::damage_rasters(int left, int top, int right, int bottom) {
    // The ordinary case — a terminal with no picture in it — is one test,
    // which matters because this runs for every character a program writes.
    if (rasters_.empty()) return;
    const int cell_width = std::max(1, profile_.cell_pixels.width);
    const int cell_height = std::max(1, profile_.cell_pixels.height);

    for (std::size_t index = rasters_.size(); index-- > 0;) {
        TerminalRaster& raster = rasters_[index];
        RasterCoverage& coverage = raster_coverage_[index];
        const int raster_right = raster.anchor.x + raster.cell_extent.width;
        const int raster_bottom = raster.anchor.y + raster.cell_extent.height;
        const int x0 = std::max(left, raster.anchor.x);
        const int y0 = std::max(top, raster.anchor.y);
        const int x1 = std::min(right, raster_right);
        const int y1 = std::min(bottom, raster_bottom);
        if (x0 >= x1 || y0 >= y1) continue;

        // Written over completely: drop it rather than blank every pixel of
        // a picture nobody will see again.
        if (x0 == raster.anchor.x && y0 == raster.anchor.y && x1 == raster_right && y1 == raster_bottom) {
            rasters_.erase(rasters_.begin() + static_cast<std::ptrdiff_t>(index));
            raster_coverage_.erase(raster_coverage_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        // A snapshot may still be holding these pixels, so the first erase
        // after one was taken copies. Repainting a whole picture is hundreds
        // of cell writes and one copy, not one copy per cell.
        if (coverage.handed_out) {
            coverage.image = std::make_shared<Image>(*coverage.image);
            coverage.handed_out = false;
            raster.image = coverage.image;
        }
        Image& pixels = *coverage.image;
        for (int cell_y = y0; cell_y < y1; ++cell_y) {
            for (int cell_x = x0; cell_x < x1; ++cell_x) {
                const std::size_t slot =
                    static_cast<std::size_t>(cell_y - raster.anchor.y) * static_cast<std::size_t>(raster.cell_extent.width) +
                    static_cast<std::size_t>(cell_x - raster.anchor.x);
                if (slot >= coverage.live_cells.size() || !coverage.live_cells[slot]) continue;
                coverage.live_cells[slot] = false;
                --coverage.live_count;
                // In pixels, in 64 bits: a terminal that reports an absurd
                // cell size is a report, not a promise, and it must not be
                // able to overflow its way into someone else's memory.
                const std::int64_t px0 = static_cast<std::int64_t>(cell_x - raster.anchor.x) * cell_width;
                const std::int64_t py0 = static_cast<std::int64_t>(cell_y - raster.anchor.y) * cell_height;
                const int px_begin = static_cast<int>(std::min<std::int64_t>(px0, pixels.width()));
                const int py_begin = static_cast<int>(std::min<std::int64_t>(py0, pixels.height()));
                const int px_end = static_cast<int>(std::min<std::int64_t>(px0 + cell_width, pixels.width()));
                const int py_end = static_cast<int>(std::min<std::int64_t>(py0 + cell_height, pixels.height()));
                for (int py = py_begin; py < py_end; ++py)
                    for (int px = px_begin; px < px_end; ++px) pixels.set_pixel(px, py, Image::Rgba{0, 0, 0, 0});
            }
        }
        if (coverage.live_count == 0) {
            if (graphics_log_enabled())
                graphics_log("emulator: a child picture was written over completely and is gone");
            rasters_.erase(rasters_.begin() + static_cast<std::ptrdiff_t>(index));
            raster_coverage_.erase(raster_coverage_.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
}

void TerminalEmulator::scroll_rasters(int top, int bottom, int rows) noexcept {
    if (rasters_.empty() || rows == 0) return;
    for (std::size_t index = rasters_.size(); index-- > 0;) {
        TerminalRaster& raster = rasters_[index];
        const int raster_bottom = raster.anchor.y + raster.cell_extent.height;
        if (raster_bottom <= top || raster.anchor.y >= bottom) continue;  // outside: unmoved
        const int moved_top = raster.anchor.y - rows;
        const int moved_bottom = raster_bottom - rows;
        // A picture only partly inside the scrolled region, or one carried
        // past its edge, would have to be cut in half to stay honest — and a
        // half-picture anchored to a cell it no longer starts at is worse
        // than none. It leaves, the way it would have scrolled off.
        if (raster.anchor.y < top || raster_bottom > bottom || moved_top < top || moved_bottom > bottom) {
            rasters_.erase(rasters_.begin() + static_cast<std::ptrdiff_t>(index));
            raster_coverage_.erase(raster_coverage_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        raster.anchor.y = moved_top;
    }
}

Cell TerminalEmulator::erase_cell() const noexcept {
    // Only the background survives into a blank cell. Weight, italics and the
    // rest describe a glyph that is no longer there, and carrying underline
    // into erased cells would draw a rule across the width of every cleared
    // line. Reverse video is the exception that has to be resolved rather
    // than dropped: while it is set, what the reader sees as the background
    // is the selected foreground.
    const bool reversed = has_attr(style_.attrs, Attr::Reverse);
    // Built from a default Style rather than braced field-by-field: an erased
    // cell keeps the background and nothing else, so every other part of the
    // style — present or added later — should be whatever "unset" means.
    Style blank;
    blank.fg = profile_.default_style.fg;
    blank.bg = reversed ? style_.fg : style_.bg;
    return Cell::from_grapheme(" ", blank);
}

void TerminalEmulator::erase_cells(int left, int top, int right, int bottom) noexcept {
    left = std::clamp(left, 0, cells_.width); right = std::clamp(right, 0, cells_.width);
    top = std::clamp(top, 0, cells_.height); bottom = std::clamp(bottom, 0, cells_.height);
    auto& cells = active_cells();
    const Cell blank = erase_cell();
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            cells[static_cast<std::size_t>(y * cells_.width + x)] = blank;
    wrote_cells(left, top, right, bottom);
}

void TerminalEmulator::insert_cells(int count) noexcept {
    count = std::clamp(count, 0, cells_.width - cursor_.position.x);
    if (count == 0) return;
    std::vector<Cell>& cells = active_cells();
    auto first = cells.begin() + static_cast<std::ptrdiff_t>(cursor_.position.y * cells_.width + cursor_.position.x);
    auto last = cells.begin() + static_cast<std::ptrdiff_t>((cursor_.position.y + 1) * cells_.width);
    std::move_backward(first, last - count, last);
    std::fill(first, first + count, erase_cell());
    wrote_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
}

void TerminalEmulator::delete_cells(int count) noexcept {
    count = std::clamp(count, 0, cells_.width - cursor_.position.x);
    if (count == 0) return;
    std::vector<Cell>& cells = active_cells();
    auto first = cells.begin() + static_cast<std::ptrdiff_t>(cursor_.position.y * cells_.width + cursor_.position.x);
    auto last = cells.begin() + static_cast<std::ptrdiff_t>((cursor_.position.y + 1) * cells_.width);
    std::move(first + count, last, first);
    std::fill(last - count, last, erase_cell());
    wrote_cells(cursor_.position.x, cursor_.position.y, cells_.width, cursor_.position.y + 1);
}

void TerminalEmulator::scroll_up(int rows) noexcept {
    scroll_up_region(0, cells_.height, rows);
}

void TerminalEmulator::scroll_up_region(int top, int bottom, int rows) noexcept {
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    rows = std::clamp(rows, 0, bottom - top);
    if (rows == 0) return;
    auto& cells = active_cells();
    // A capacity of zero is "remember nothing", and it means the line is never
    // copied anywhere — not copied into the history and dropped again on the
    // next statement, which is the same screen at twice the cost.
    if (!alternate_buffer_ && top == 0 && bottom == cells_.height && options_.max_scrollback_lines > 0) {
        for (int row = 0; row < rows; ++row) {
            scrollback_.insert(scrollback_.end(), cells.begin() + static_cast<std::ptrdiff_t>(row * cells_.width),
                               cells.begin() + static_cast<std::ptrdiff_t>((row + 1) * cells_.width));
        }
        const std::size_t maximum = options_.max_scrollback_lines * static_cast<std::size_t>(cells_.width);
        const std::size_t width = static_cast<std::size_t>(std::max(1, cells_.width));
        const std::size_t live = scrollback_.size() - scrollback_begin_;
        // Dropping the oldest lines is moving a number, not moving cells — and
        // it drops whole LINES. Advancing by the raw cell excess would leave
        // the offset mid-row and shear every line in the history by the
        // remainder, which is what the resize test caught: a cap measured in
        // cells against a history stored in rows is only the same number while
        // the width has never changed.
        if (live > maximum) {
            const std::size_t excess_rows = (live - maximum + width - 1) / width;
            scrollback_begin_ = std::min(scrollback_.size(), scrollback_begin_ + excess_rows * width);
        }
        // Reclaim once the dead prefix is as large as what is being kept: the
        // copy costs one history, and it happens once per history's worth of
        // lines rather than once per line.
        if (scrollback_begin_ >= maximum + static_cast<std::size_t>(cells_.width)) {
            scrollback_.erase(scrollback_.begin(),
                              scrollback_.begin() + static_cast<std::ptrdiff_t>(scrollback_begin_));
            scrollback_begin_ = 0;
        }
        // A host sends the lines that entered the history, not the history.
        damage_.scrollback_pushed += static_cast<std::size_t>(rows);
    }
    const auto first = cells.begin() + static_cast<std::ptrdiff_t>(top * cells_.width);
    const auto last = cells.begin() + static_cast<std::ptrdiff_t>(bottom * cells_.width);
    std::move(first + static_cast<std::ptrdiff_t>(rows * cells_.width), last, first);
    std::fill(last - static_cast<std::ptrdiff_t>(rows * cells_.width), last, erase_cell());
    // Every row in the region holds different cells now. A Scroll op will
    // usually carry this far more cheaply than the cells would, but that is the
    // diff engine's judgement to make from honest damage, not the emulator's to
    // pre-empt by under-reporting.
    changed_cells(0, top, cells_.width, bottom);
    scroll_rasters(top, bottom, rows);
}

void TerminalEmulator::scroll_down_region(int top, int bottom, int rows) noexcept {
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    rows = std::clamp(rows, 0, bottom - top);
    if (rows == 0) return;
    auto& cells = active_cells();
    const auto first = cells.begin() + static_cast<std::ptrdiff_t>(top * cells_.width);
    const auto last = cells.begin() + static_cast<std::ptrdiff_t>(bottom * cells_.width);
    std::move_backward(first, last - static_cast<std::ptrdiff_t>(rows * cells_.width), last);
    std::fill(first, first + static_cast<std::ptrdiff_t>(rows * cells_.width), erase_cell());
    changed_cells(0, top, cells_.width, bottom);
    scroll_rasters(top, bottom, -rows);
}

void TerminalEmulator::index() noexcept {
    // Autoprint: the line the cursor is leaving goes to the printer. Done
    // here rather than at the newline, because a line the cursor left by
    // wrapping was just as finished as one it left by newline.
    if (autoprint_ && printer_available())
        print_bytes(screen_text(cursor_.position.y, cursor_.position.y + 1));
    if (cursor_.position.y == scroll_bottom_ - 1) {
        scroll_up_region(scroll_top_, scroll_bottom_, 1);
    } else {
        cursor_.position.y = std::min(cells_.height - 1, cursor_.position.y + 1);
    }
}

void TerminalEmulator::reverse_index() noexcept {
    if (cursor_.position.y == scroll_top_) {
        scroll_down_region(scroll_top_, scroll_bottom_, 1);
    } else {
        cursor_.position.y = std::max(0, cursor_.position.y - 1);
    }
}

// Every eighth column, which is what a terminal starts with and what a
// program that never mentions tab stops is relying on.
void TerminalEmulator::reset_tab_stops() {
    tab_stops_.assign(static_cast<std::size_t>(std::max(1, cells_.width)), false);
    for (std::size_t column = 8; column < tab_stops_.size(); column += 8) tab_stops_[column] = true;
}

// The next stop to the right, or the last column when there is none. Landing
// on the last column rather than one past it is what a terminal does: a tab
// at the right-hand end of a line does not wrap by itself, it stops, and the
// next character written is what decides whether the line ends.
int TerminalEmulator::next_tab_stop(int from) const noexcept {
    const int last = std::max(0, cells_.width - 1);
    for (int column = from + 1; column <= last; ++column)
        if (column < static_cast<int>(tab_stops_.size()) && tab_stops_[static_cast<std::size_t>(column)])
            return column;
    return last;
}

int TerminalEmulator::previous_tab_stop(int from) const noexcept {
    for (int column = std::min(from, cells_.width) - 1; column > 0; --column)
        if (column < static_cast<int>(tab_stops_.size()) && tab_stops_[static_cast<std::size_t>(column)])
            return column;
    return 0;
}

void TerminalEmulator::clear_scrollback() noexcept {
    scrollback_.clear();
    scrollback_begin_ = 0;
}

void TerminalEmulator::reset_active_buffer() {
    std::fill(active_cells().begin(), active_cells().end(), Cell::from_grapheme(" ", profile_.default_style));
    damage_everything();
}

// One row span per row of the current grid, all clean. Sized here rather than
// lazily, because a host indexes it by row and a short vector would be a
// silent "that row did not change".
void TerminalEmulator::resize_damage() noexcept {
    damage_.rows.assign(static_cast<std::size_t>(std::max(0, cells_.height)), TerminalDamage::RowSpan{});
}

void TerminalEmulator::damage_everything() noexcept {
    damage_.full = true;
    damage_.cursor = true;
    damage_.modes = true;
    damage_.title = true;
    damage_.rasters = true;
    for (TerminalDamage::RowSpan& row : damage_.rows) {
        row.first = 0;
        row.last = cells_.width;
    }
}

void TerminalEmulator::wrote_cells(int left, int top, int right, int bottom) {
    changed_cells(left, top, right, bottom);
    // A write puts new content over whatever pixels were there. A move does
    // not — see the header on why these are two functions.
    damage_rasters(std::clamp(left, 0, cells_.width), std::clamp(top, 0, cells_.height),
                   std::clamp(right, 0, cells_.width), std::clamp(bottom, 0, cells_.height));
}

void TerminalEmulator::changed_cells(int left, int top, int right, int bottom) noexcept {
    left = std::clamp(left, 0, cells_.width);
    right = std::clamp(right, left, cells_.width);
    top = std::clamp(top, 0, cells_.height);
    bottom = std::clamp(bottom, top, cells_.height);
    for (int row = top; row < bottom; ++row) {
        if (row >= static_cast<int>(damage_.rows.size())) break;
        TerminalDamage::RowSpan& span = damage_.rows[static_cast<std::size_t>(row)];
        // One span per row, widened rather than split. Two stretches with a
        // clean gap between them would be cheaper to send, but a list per row
        // costs allocation on the hot path to save bytes on a case — a program
        // writing at both ends of one row — that is rare next to the case it
        // would slow down, which is every write.
        if (span.empty()) {
            span.first = left;
            span.last = right;
            continue;
        }
        span.first = std::min(span.first, left);
        span.last = std::max(span.last, right);
    }
}

void TerminalEmulator::clear_damage() noexcept {
    damage_.full = false;
    damage_.cursor = false;
    damage_.modes = false;
    damage_.title = false;
    damage_.rasters = false;
    damage_.clipboard = false;
    damage_.bell = false;
    damage_.printer = false;
    damage_.diagnostics = false;
    damage_.lifecycle = false;
    damage_.scrollback_pushed = 0;
    // Field by field rather than `damage_ = {}`: the row list is sized to the
    // grid and assigning a fresh struct would empty it, which reads as "this
    // terminal has no rows" until the next resize.
    for (TerminalDamage::RowSpan& row : damage_.rows) row = TerminalDamage::RowSpan{};
}

// RIS (ESC c) — the terminal as it was constructed, minus nothing the child
// can reach. Both screens are blanked, the history goes, the cursor comes home
// visible, every mode returns to the profile's declaration, and the parser's
// half-read text is dropped.
//
// Two things deliberately survive. The title is the caption of the host's
// window: a program clearing its own screen has not said the window is now
// nameless, and a caption that empties itself whenever anything calls `reset`
// is worse than one that lags. And the session's state — running, exited,
// failed — describes the child process, not its screen, so a reset cannot
// bring a dead child back to life.
void TerminalEmulator::reset_terminal() {
    printable_.clear();
    last_graphic_.clear();
    control_.clear();
    dcs_sixel_payload_ = false;

    style_ = profile_.default_style;
    cursor_ = CursorState{true, {}, CursorShape::Block};
    saved_cursor_ = cursor_;
    saved_cursor_valid_ = false;

    alternate_buffer_ = false;
    const Cell blank = Cell::from_grapheme(" ", profile_.default_style);
    std::fill(primary_.begin(), primary_.end(), blank);
    std::fill(alternate_.begin(), alternate_.end(), blank);
    clear_scrollback();
    clear_rasters();
    // The child's Sixel colour registers are the child's, and a reset is the
    // child saying it no longer knows what it set.
    sixel_palette_ = SixelPalette{};

    scroll_top_ = 0;
    scroll_bottom_ = cells_.height;
    origin_mode_ = false;
    autowrap_ = true;
    insert_mode_ = false;
    reset_tab_stops();

    for (Charset& charset : charsets_) charset = Charset::Ascii;
    active_charset_ = 0;
    pending_charset_slot_ = 0;

    bracketed_paste_enabled_ = false;
    mouse_tracking_ = TerminalMouseTracking::None;
    // A hard reset is the child disclaiming everything it had set, so an
    // open frame it never closed does not survive it either — the
    // alternative is a terminal a reset was supposed to put back to a known
    // state, still refusing to show anything past whatever it drew last.
    synchronized_output_active_ = false;
    mouse_sgr_enabled_ = false;
    application_cursor_keys_ = false;
    focus_reporting_enabled_ = false;
    alternate_scroll_enabled_ = profile_.alternate_scroll;
    primary_keyboard_ = KeyboardState{};
    alternate_keyboard_ = KeyboardState{};
    // The saved captions go with everything else the child pushed. The title
    // itself stays — see above — but a stack of titles from before a reset is
    // a stack nothing will ever legitimately pop.
    title_stack_.clear();
    // The printer goes back to idle: an unterminated job from before a reset
    // is one nothing will ever close, and a controller left on would swallow
    // the screen for good.
    printer_controller_ = false;
    autoprint_ = false;
    printer_form_feed_ = false;
    printer_print_extent_ = false;
    printer_spool_.clear();
    printer_terminator_.clear();
    printer_overflowed_ = false;
    printer_job_open_ = false;
    // The screen invalidation below says nothing about the printer, which is
    // not part of the screen and has just been put back to idle.
    damage_.printer = true;
    damage_everything();
}

// DECALN (ESC # 8). The screen fills with E and the cursor comes home, which
// is how a terminal's alignment is checked and how most of vttest's chapters
// begin — a chapter run against an emulator that ignores it grades a blank
// screen. The margins go with it: the pattern is the whole screen by
// definition, so a scroll region left over from whatever ran before would put
// the next line the program writes somewhere it did not choose.
void TerminalEmulator::screen_alignment_pattern() {
    // The default style, not the current one: this is a test pattern for the
    // screen itself, and a program that happened to leave reverse video on
    // would otherwise be grading the wrong picture.
    std::fill(active_cells().begin(), active_cells().end(), Cell::from_grapheme("E", profile_.default_style));
    // Every cell was written, so every picture on the screen was written over.
    wrote_cells(0, 0, cells_.width, cells_.height);
    scroll_top_ = 0;
    scroll_bottom_ = cells_.height;
    origin_mode_ = false;
    cursor_.position = {};
}

void TerminalEmulator::resize(Size cells, Size cell_pixels) {
    const Size old_cells = cells_;
    const Size new_cells = bounded_size(cells, options_.max_cells);
    const Style default_style = profile_.default_style;
    auto resized_buffer = [old_cells, new_cells, default_style](const std::vector<Cell>& source) {
        std::vector<Cell> result(static_cast<std::size_t>(new_cells.width * new_cells.height),
                                 Cell::from_grapheme(" ", default_style));
        const int rows = std::min(old_cells.height, new_cells.height);
        const int columns = std::min(old_cells.width, new_cells.width);
        for (int y = 0; y < rows; ++y)
            std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(y * old_cells.width), columns,
                        result.begin() + static_cast<std::ptrdiff_t>(y * new_cells.width));
        return result;
    };

    primary_ = resized_buffer(primary_);
    alternate_ = resized_buffer(alternate_);
    // The history is stored as flat rows of the grid's width, so a width
    // change without this makes every stored line be read at the wrong offset:
    // the whole scrollback shears diagonally, one column further out per line,
    // and what the reader scrolls back to is not what was ever on their
    // screen. Each line is re-laid to the new width instead — truncated, or
    // padded with blanks.
    //
    // This is not reflow: a line that wrapped at 80 columns stays two lines at
    // 100 rather than becoming one. Reflow is a separate feature (it has to
    // know which line breaks the child chose and which the terminal imposed,
    // which nothing here records); what this keeps is the invariant that a
    // line is a line, which is the unit capacity is counted in.
    //
    // Only the LIVE region is re-laid. Lines the capacity trim has already
    // dropped live on in the vector as a dead prefix until it is worth
    // reclaiming (see push_line_to_scrollback), and re-laying that prefix as
    // content would resurrect it: the history would come back over capacity by
    // exactly as much as had been dropped, and the reader would scroll back
    // into lines the terminal had already promised to forget.
    if (scrollback_.size() > scrollback_begin_ && new_cells.width != old_cells.width) {
        const std::size_t old_width = static_cast<std::size_t>(old_cells.width);
        const std::size_t new_width = static_cast<std::size_t>(new_cells.width);
        const std::size_t rows = (scrollback_.size() - scrollback_begin_) / old_width;
        const std::size_t columns = std::min(old_width, new_width);
        std::vector<Cell> relaid(rows * new_width, Cell::from_grapheme(" ", default_style));
        for (std::size_t row = 0; row < rows; ++row)
            std::copy_n(scrollback_.begin() +
                            static_cast<std::ptrdiff_t>(scrollback_begin_ + row * old_width),
                        columns,
                        relaid.begin() + static_cast<std::ptrdiff_t>(row * new_width));
        scrollback_ = std::move(relaid);
        // Re-laying is the one moment the dead prefix cannot survive: what it
        // held were rows of the old width.
        scrollback_begin_ = 0;
    }
    // Tab stops belong to columns, so the ones that still exist keep what the
    // child set; columns that did not exist before start at the default. A
    // program that laid out a table and then had its window widened should
    // not find its columns rearranged, nor the new space stopless.
    if (new_cells.width != old_cells.width) {
        const std::vector<bool> previous = tab_stops_;
        cells_.width = new_cells.width;
        reset_tab_stops();
        const std::size_t shared = std::min(previous.size(), tab_stops_.size());
        for (std::size_t column = 0; column < shared; ++column) tab_stops_[column] = previous[column];
    }
    cells_ = new_cells;
    profile_.cells = cells_;
    // A host that has not yet measured its cells reports {0,0}. Reading that
    // as one pixel per cell is worse than not knowing: every geometry this
    // terminal then quotes to its child — XTWINOPS, XTSMGRAPHICS — says the
    // screen is a few dozen pixels across, and a child that believes it will
    // never send a picture again. The last metric that made sense is kept.
    if (cell_pixels.width > 0 && cell_pixels.height > 0) profile_.cell_pixels = cell_pixels;
    // A resize changes the cell-to-pixel mapping, but it does not erase the
    // child's private image state. Keep each decoded image and rebuild only
    // its cell placement; a later child erase/alternate-buffer transition
    // still removes it explicitly.
    for (TerminalRaster& raster : rasters_) {
        if (raster.image == nullptr) continue;
        raster.cell_extent = Size{
            std::max(1, ceil_div_positive(raster.image->width(), profile_.cell_pixels.width)),
            std::max(1, ceil_div_positive(raster.image->height(), profile_.cell_pixels.height))};
    }
    if (graphics_log_enabled())
        graphics_log("emulator: child resized to " + std::to_string(cells_.width) + "x" +
                     std::to_string(cells_.height) + " cells of " + std::to_string(profile_.cell_pixels.width) +
                     "x" + std::to_string(profile_.cell_pixels.height) + " px");
    scroll_top_ = 0;
    scroll_bottom_ = cells_.height;
    origin_mode_ = false;
    cursor_.position.x = std::clamp(cursor_.position.x, 0, cells_.width - 1);
    cursor_.position.y = std::clamp(cursor_.position.y, 0, cells_.height - 1);
    // A resize invalidates everything a host knew: rows moved, columns
    // appeared or went, and no span from the old geometry means anything in
    // the new one.
    resize_damage();
    damage_everything();
}

void TerminalEmulator::send_input(std::string_view bytes) {
    if (state_ == TerminalSubsessionState::Closed || state_ == TerminalSubsessionState::Failed ||
        state_ == TerminalSubsessionState::Exited)
        return;
    const std::size_t remaining = options_.max_input_bytes > pending_input_.size() ? options_.max_input_bytes - pending_input_.size() : 0;
    pending_input_.append(bytes.substr(0, remaining));
    if (bytes.size() > remaining) diagnostic(TerminalDiagnostic::Kind::LimitExceeded, "child input queue exceeded configured limit");
}

std::string TerminalEmulator::take_pending_input() { std::string result = std::move(pending_input_); pending_input_.clear(); return result; }
void TerminalEmulator::mark_exited(int exit_code) {
    while (!pending_output_.empty()) {
        const std::size_t before = pending_output_.size();
        feed_output({});
        if (pending_output_.size() == before) break;
    }
    put_text(true);
    if (parse_state_ != ParseState::Ground)
        diagnostic(TerminalDiagnostic::Kind::MalformedSequence, "child exited with incomplete terminal control sequence");
    parse_state_ = ParseState::Ground;
    control_.clear();
    state_ = TerminalSubsessionState::Exited;
    exit_code_ = exit_code;
    // The one change a terminal makes after it has stopped making any other. A
    // host gated on damage — a delta transport, a repaint scheduler — looks
    // exactly when this says to, so without it a dead child's window goes on
    // looking alive until something else happens to the terminal, and nothing
    // else is ever going to.
    damage_.lifecycle = true;
}

void TerminalEmulator::mark_failed(std::string message) {
    state_ = TerminalSubsessionState::Failed;
    damage_.lifecycle = true;
    diagnostic(TerminalDiagnostic::Kind::ChildExited, std::move(message));
}

}  // namespace ckv::term
