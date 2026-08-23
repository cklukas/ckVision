// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <memory>

#include "cvision/term/sixel_decoder.hpp"
#include "cvision/term/terminal_subsession.hpp"

namespace ckv::term {

// Defensive, off-screen xterm subset emulator.  It is intentionally an input
// model, not a Terminal implementation: it cannot emit bytes to the parent.
class TerminalEmulator final : public TerminalSubsession {
public:
    explicit TerminalEmulator(TerminalCapabilityProfile profile = embedded_xterm_sixel_profile(),
                              TerminalSubsessionOptions options = {});

    TerminalSnapshot snapshot() const override;
    // The scalar half, at a few dozen bytes rather than a grid. What a host
    // reads every tick while reading cells only where damage says to (U0-b).
    TerminalStatus status() const override;
    // The parts of a snapshot a caller actually wants. The whole reason this
    // overload exists is that `snapshot()` copies the entire bounded history
    // every time it is called, and a host diffing at tick rate wants the grid:
    // paying for what the terminal remembers on every frame is the difference
    // between a cost proportional to what changed and one proportional to how
    // long the terminal has been alive.
    TerminalSnapshot snapshot(TerminalSnapshotOptions options) const;

    // The active grid and the history, borrowed rather than copied. Valid
    // until the next call that changes this emulator — which for the server is
    // its own drain, so a diff engine reads these between drains and copies
    // only the cells it is going to send.
    std::span<const Cell> cells() const noexcept override;
    std::span<const Cell> scrollback() const noexcept override;
    std::span<const TerminalRaster> rasters() const noexcept override;
    std::span<const TerminalDiagnostic> diagnostics() const noexcept override;

    // What has changed since `clear_damage()`. A host that has this does not
    // have to keep the previous screen and compare: the emulator already knew
    // what it wrote, and throwing that away only to reconstruct it by
    // comparison is the copy this exists to remove.
    const TerminalDamage& damage() const noexcept override { return damage_; }
    // Said by the host when it has sent everything: "I am caught up". Not done
    // by `snapshot()` or by `damage()`, because reading state must not change
    // it — a second reader would otherwise find a terminal that had nothing to
    // report, and there are three of them (a diff engine, a title poll, a bell
    // badge).
    void clear_damage() noexcept override;
    bool synchronized_output_active() const noexcept override { return synchronized_output_active_; }
    const TerminalCapabilityProfile& profile() const noexcept override { return profile_; }

    // The printer policy, changed on a terminal that is already running.
    //
    // Every other capability in the profile is a statement about the terminal
    // a child was launched into and is answered to that child for its whole
    // life: changing what it has already been told it can draw would make a
    // liar of the advertisement it read at startup. The printer is not like
    // that, and the difference is consent rather than capability. Whether a
    // host keeps what a child prints is the READER's decision, they make it in
    // a settings dialog while the child runs, and there is nothing to unsay —
    // a child asks `CSI ? 15 n` afresh each time it wants to know, and gets an
    // honest answer for the policy in force at that moment.
    //
    // Without this a host could only offer the choice by relaunching the
    // child, which for a terminal multiplexer means killing the reader's shell
    // to change a preference about printing.
    //
    // Turning capture OFF does not discard what is already spooled: the jobs a
    // child finished under a policy that was in force belong to the reader who
    // had it in force, and are still handed over by take_printer_jobs(). What
    // stops is new capture — and an open controller job stops with it, sunk
    // rather than resumed to the screen, for the reason print_bytes() gives.
    void set_printer_policy(TerminalPrinterPolicy policy) override;
    // How much one job may collect before it is abandoned and the terminal
    // sinks. Zero is refused — see the seam.
    void set_printer_spool_limit(std::size_t bytes) override;
    void feed_output(std::string_view bytes) override;
    void resize(Size cells, Size cell_pixels) override;
    void send_input(std::string_view bytes) override;
    std::string take_pending_input() override;
    TerminalSubsessionState state() const noexcept override { return state_; }
    void set_raster_identity(int identity) noexcept override { raster_identity_ = identity; }
    void close() noexcept override {
        if (state_ == TerminalSubsessionState::Closed) return;
        state_ = TerminalSubsessionState::Closed;
        damage_.lifecycle = true;
    }

    // The print jobs the child has completed, handed over exactly once. A
    // snapshot carries only how many are waiting, because a snapshot is a
    // value that may be read as often as anyone likes and a job is a document
    // that must be delivered once.
    //
    // This is the seam's drain (core::TerminalSubsession::take_printer_jobs),
    // which it had promised in prose before it declared it: the base's
    // TerminalStatus comment already told hosts that jobs came out this way,
    // and the method existed only here — so a host holding the terminal
    // through the seam, which is how every host holds one, could not reach it.
    std::vector<TerminalPrinterJob> take_printer_jobs() override;

    // The child's life, as the host observing the process reports it. Both set
    // the lifecycle damage flag: a terminal that has stopped is news, and a
    // host gated on damage would otherwise never hear that its child had died.
    void mark_exited(int exit_code);
    void mark_failed(std::string message);
    std::optional<int> exit_code() const noexcept { return exit_code_; }

private:
    enum class ParseState : unsigned char { Ground, Escape, Csi, Osc, OscEscape, Dcs, DcsEscape, Discard, DiscardEscape, Scs, Hash };

    // Which repertoire the printable bytes currently stand for. A program
    // drawing a frame with ncurses does not send box-drawing characters: it
    // designates the DEC line-drawing set and then sends the letters l, q,
    // k, x, m and j. Read as ASCII those spell nonsense across the frame,
    // which is what an emulator that ignores the designation shows.
    enum class Charset : unsigned char { Ascii, DecGraphics };

    // One SGR parameter with its colon-separated sub-parameters — the shape
    // of an underline, or a whole colour written as one parameter.
    struct SgrParameter {
        int value = 0;
        std::vector<int> subs;
    };

    // The kitty keyboard enhancements in force, and the stack a program
    // pushes them onto so it can put them back. There is one of these per
    // screen: a full-screen program's settings must not follow it out onto
    // the shell's screen, and must not survive its own death either.
    struct KeyboardState {
        TerminalKeyboardFlags flags = TerminalKeyboardFlags::None;
        std::vector<TerminalKeyboardFlags> stack;
    };

    // The byte a DEC line-drawing designation gives, or nullopt when the
    // byte stands for itself.
    static std::string_view dec_graphic_for(char byte) noexcept;

    std::vector<Cell>& active_cells() noexcept;
    const std::vector<Cell>& active_cells() const noexcept;
    void put_text(bool flush_incomplete = false);
    void put_grapheme(std::string_view grapheme);
    void handle_csi(char final_byte);
    void handle_private_csi(char final_byte);
    void erase_in_display(int mode);
    void erase_in_line(int mode);
    static std::vector<SgrParameter> parse_sgr_parameters(std::string_view text, bool& valid);
    void handle_sgr();
    bool graphics_available() const noexcept;
    void handle_graphics_attributes();
    void handle_keyboard_protocol();
    void handle_key_modifier_options();
    bool extended_color(const std::vector<SgrParameter>& parameters, std::size_t& index,
                        const SgrParameter& parameter, Color& color);
    // The cell an erase leaves behind. Not the profile default: erasing
    // fills with the colour the program has currently selected, which is
    // what "background colour erase" means and what xterm-family terminfo
    // advertises as `bce`. A curses program paints a coloured bar by setting
    // the colour and erasing the line rather than by writing spaces across
    // it, so a terminal that erases to its own default shows that bar only
    // under the characters that happened to be written.
    Cell erase_cell() const noexcept;
    void erase_cells(int left, int top, int right, int bottom) noexcept;
    void insert_cells(int count) noexcept;
    void delete_cells(int count) noexcept;
    void scroll_up(int rows) noexcept;
    void scroll_up_region(int top, int bottom, int rows) noexcept;
    void scroll_down_region(int top, int bottom, int rows) noexcept;
    void index() noexcept;
    void reverse_index() noexcept;
    void reset_active_buffer();
    // The one place the history is emptied, so the start offset cannot be
    // left pointing into a vector that no longer has anything before it.
    void clear_scrollback() noexcept;
    // Every path that puts different cells in a region says so here, so a
    // host learns what changed without keeping the previous screen to compare
    // against.
    void changed_cells(int left, int top, int right, int bottom) noexcept;
    // ...and every path that WRITES cells says so here instead, which is the
    // same thing plus the erasure a write means for a picture underneath.
    //
    // The two are separate because scrolling is the case that tells them
    // apart: it puts different cells in every row of its region, so the host
    // must hear about it, but it MOVES those cells rather than writing over
    // them — a picture in the region rides along (scroll_rasters) instead of
    // being erased. Routing a scroll through the write path destroys the
    // picture it should have carried.
    void wrote_cells(int left, int top, int right, int bottom);
    // Nothing about the previous screen can be relied on — a resize, a reset,
    // a switch of buffers.
    void damage_everything() noexcept;
    void resize_damage() noexcept;
    // The tab stops, as a terminal has always had them: a column is a stop or
    // it is not, and `\t` goes to the next one. Every eight columns is only
    // the default — a program that lays out a table sets its own with HTS and
    // then relies on them.
    void reset_tab_stops();
    int next_tab_stop(int from) const noexcept;
    int previous_tab_stop(int from) const noexcept;
    // RIS (ESC c). Everything a child can change goes back to what this
    // emulator was constructed with — which is what `reset(1)` is for, and
    // what a program that has wedged the terminal is asking for when it sends
    // this and nothing else.
    void reset_terminal();
    // DECALN (ESC # 8): the screen filled with E, which is how a terminal is
    // checked for alignment and how vttest starts most of its chapters.
    void screen_alignment_pattern();
    void diagnostic(TerminalDiagnostic::Kind kind, std::string message);
    bool append_control(char byte);
    // Applies a completed OSC string. A reply goes back with the same
    // terminator the child used to ask, which is what a program that accepts
    // only one of the two will be reading for.
    void handle_osc(bool bel_terminated);
    // OSC 0/2, under the profile's title policy: the child says what it is
    // working on, and a host that shows a caption follows it.
    void set_title(std::string_view text);
    // XTWINOPS 22/23 — the title stack a program pushes its caption onto
    // before replacing it, and pops on the way out. Without it a shell keeps
    // the name of the editor that exited an hour ago.
    void handle_title_stack(bool push);
    // Media Copy (CSI Ps i and CSI ? Ps i): the printer controller, print
    // screen, print line, and autoprint.
    void handle_media_copy();
    // Whether the child may print at all, which every printer answer below is
    // derived from.
    bool printer_available() const noexcept;
    // Adds to the job in progress, or frees it and starts sinking when it
    // would pass the bound.
    void print_bytes(std::string_view bytes);
    // Ends the job in progress, if there is one, and queues it.
    void finish_printer_job(TerminalPrinterJob::Origin origin);
    // The screen, or one line of it, as text a printer would have received.
    std::string screen_text(int top, int bottom) const;
    void handle_palette_query(std::string_view body, bool bel_terminated);
    void handle_clipboard(std::string_view body);
    void reply_osc(std::string_view body, bool bel_terminated);
    static std::string report_color(Color color, Color fallback);
    bool append_dcs_byte(char byte);
    static int parameter(std::string_view text, std::size_t index, int default_value);

    // What a terminal does with a picture, which is the part an emulator that
    // only ever adds one gets wrong. A Sixel paints pixels onto the screen
    // and nothing takes them off again except writing over the cells it
    // covers — so a picture is not a decoration held until something clears
    // the screen, it is pixels that go away one cell at a time as the program
    // repaints. Every cell-writing path therefore reports what it wrote.
    void damage_rasters(int left, int top, int right, int bottom);
    // Adds a decoded picture at `anchor`, erasing whatever earlier picture it
    // covers — two pictures side by side both stay, which is what a program
    // drawing an image in pieces (or a nested ckVision slicing one around a
    // window) depends on.
    void place_raster(std::shared_ptr<Image> image, Point anchor, Size cell_extent);
    void clear_rasters() noexcept;
    // Pictures ride with the text they were drawn beside; one that would land
    // outside the scrolled region leaves, since a cell-anchored picture
    // cannot be shown half-way off.
    void scroll_rasters(int top, int bottom, int rows) noexcept;

    // A picture's own bookkeeping, kept beside `rasters_` rather than in the
    // public snapshot type: the emulator's writable handle to the pixels,
    // which cells of it still have any, and whether a snapshot is holding the
    // same pixels — the one copy that has to be made before erasing into it.
    struct RasterCoverage {
        std::shared_ptr<Image> image;
        std::vector<bool> live_cells;
        std::size_t live_count = 0;
        mutable bool handed_out = false;
    };

    TerminalCapabilityProfile profile_;
    TerminalSubsessionOptions options_;
    Size cells_;
    Style style_;
    CursorState cursor_{true, {}, CursorShape::Block};
    CursorState saved_cursor_{true, {}, CursorShape::Block};
    bool saved_cursor_valid_ = false;
    std::vector<Cell> primary_;
    std::vector<Cell> alternate_;
    // The history, oldest first, with a start offset rather than a vector
    // whose front is erased.
    //
    // Erasing one line from the front when the history is full costs a move of
    // every cell still in it — O(history) per scrolled line, which at the
    // default ten thousand lines of eighty columns is eight hundred thousand
    // cell moves for one line of output. A program printing steadily therefore
    // spent all of its terminal's time shifting cells that had not changed.
    // The offset makes dropping a line O(1); the dead prefix is reclaimed once
    // it is worth reclaiming, so the amortised cost per line is one row.
    std::vector<Cell> scrollback_;
    std::size_t scrollback_begin_ = 0;
    std::vector<TerminalRaster> rasters_;
    std::vector<RasterCoverage> raster_coverage_;  // one entry per raster, same order
    // Sixel colour registers belong to the terminal, not to one picture: a
    // program may define a palette in one sequence and draw with it in the
    // next, so they outlive both.
    SixelPalette sixel_palette_;
    // The last picture decoded, and the bytes it came from. A program
    // redrawing itself sends the same picture again on every frame — a
    // dialog being dragged sends a third of a megabyte per frame, all of it
    // identical — and decoding it again produces, at some expense, exactly
    // the image already in hand.
    std::string last_sixel_payload_;
    std::shared_ptr<Image> last_sixel_image_;
    Size last_sixel_room_;
    int raster_identity_ = 0;
    // DEC mode 2026: the child asked to hold its damage until it says the
    // frame is whole. Set and reset in handle_csi's mode dispatch; read by
    // synchronized_output_active() and answered back truthfully to the
    // child's own DECRQM probe for it (both below).
    bool synchronized_output_active_ = false;
    bool alternate_buffer_ = false;
    int scroll_top_ = 0;
    int scroll_bottom_ = 0;
    bool origin_mode_ = false;
    // DECAWM. On by default, as a terminal is; a program turns it off to
    // write into the last column without the cursor falling to the next line.
    bool autowrap_ = true;
    // IRM (the ANSI mode, CSI 4 h). While set, a written character pushes the
    // rest of the line right instead of replacing what is under it.
    bool insert_mode_ = false;
    ParseState parse_state_ = ParseState::Ground;
    // The printer controller (CSI 5 i): while it is on, the child's bytes go
    // to the spool and not to the screen, until `CSI 4 i` — which may arrive
    // split across two reads, so a partial terminator is held here rather
    // than printed and then regretted.
    bool printer_controller_ = false;
    bool autoprint_ = false;
    // DECPFF: a form feed at the end of each job. DECPEX: whether print-screen
    // means the whole screen or only the scrolling region.
    bool printer_form_feed_ = false;
    bool printer_print_extent_ = false;
    // The job being collected, and whether it has already overflowed — in
    // which case the rest of it is sunk rather than the screen being resumed
    // half way through a document.
    std::string printer_spool_;
    bool printer_overflowed_ = false;
    bool printer_job_open_ = false;
    std::string printer_terminator_;
    std::vector<TerminalPrinterJob> printer_jobs_;
    // G0..G3 and which of them the printable range currently resolves
    // through. SI and SO move the pointer; ESC ( ) * + change the contents.
    Charset charsets_[4] = {Charset::Ascii, Charset::Ascii, Charset::Ascii, Charset::Ascii};
    unsigned char active_charset_ = 0;
    unsigned char pending_charset_slot_ = 0;
    std::string printable_;
    // The last character actually placed, which REP (CSI Ps b) repeats.
    std::string last_graphic_;
    std::string control_;
    bool dcs_sixel_payload_ = false;
    std::string pending_output_;
    std::string pending_input_;
    std::vector<TerminalDiagnostic> diagnostics_;
    // Complaints made, ever. The ring above drops its oldest entry, so its size
    // is not a number a host can watch — see TerminalStatus::diagnostics_serial.
    std::uint64_t diagnostics_serial_ = 0;
    // One entry per column. Rebuilt on a resize, preserving what the child
    // set for the columns that still exist.
    std::vector<bool> tab_stops_;
    TerminalDamage damage_;
    std::string title_;
    std::vector<std::string> title_stack_;
    TerminalSubsessionState state_ = TerminalSubsessionState::Ready;
    std::optional<int> exit_code_;
    bool bracketed_paste_enabled_ = false;
    // Which of DEC 1000, 1002 and 1003 is in force. One level rather than three
    // booleans: they are alternatives, and a terminal holding three of them at
    // once would have to invent an order of precedence the modes do not have.
    TerminalMouseTracking mouse_tracking_ = TerminalMouseTracking::None;
    bool mouse_sgr_enabled_ = false;
    bool application_cursor_keys_ = false;
    bool focus_reporting_enabled_ = false;
    bool alternate_scroll_enabled_ = profile_.alternate_scroll;
    std::string clipboard_text_;
    std::uint64_t clipboard_serial_ = 0;
    // BEL, counted rather than latched — see TerminalSnapshot::bell_serial.
    std::uint64_t bell_serial_ = 0;
    KeyboardState primary_keyboard_;
    KeyboardState alternate_keyboard_;
};

}  // namespace ckv::term
