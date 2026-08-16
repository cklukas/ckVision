// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cvision/term/terminal_subsession.hpp"

namespace ckv::term {

// Defensive, off-screen xterm subset emulator.  It is intentionally an input
// model, not a Terminal implementation: it cannot emit bytes to the parent.
class TerminalEmulator final : public TerminalSubsession {
public:
    explicit TerminalEmulator(TerminalCapabilityProfile profile = embedded_xterm_sixel_profile(),
                              TerminalSubsessionOptions options = {});

    TerminalSnapshot snapshot() const override;
    const TerminalCapabilityProfile& profile() const noexcept override { return profile_; }
    void feed_output(std::string_view bytes) override;
    void resize(Size cells, Size cell_pixels) override;
    void send_input(std::string_view bytes) override;
    std::string take_pending_input() override;
    TerminalSubsessionState state() const noexcept override { return state_; }
    void set_raster_identity(int identity) noexcept override { raster_identity_ = identity; }
    void close() noexcept override { state_ = TerminalSubsessionState::Closed; }

    void mark_exited(int exit_code);
    void mark_failed(std::string message);
    std::optional<int> exit_code() const noexcept { return exit_code_; }

private:
    enum class ParseState : unsigned char { Ground, Escape, Csi, Osc, OscEscape, Dcs, DcsEscape, Discard, DiscardEscape, Scs };

    // Which repertoire the printable bytes currently stand for. A program
    // drawing a frame with ncurses does not send box-drawing characters: it
    // designates the DEC line-drawing set and then sends the letters l, q,
    // k, x, m and j. Read as ASCII those spell nonsense across the frame,
    // which is what an emulator that ignores the designation shows.
    enum class Charset : unsigned char { Ascii, DecGraphics };

    // The byte a DEC line-drawing designation gives, or nullopt when the
    // byte stands for itself.
    static std::string_view dec_graphic_for(char byte) noexcept;

    std::vector<Cell>& active_cells() noexcept;
    const std::vector<Cell>& active_cells() const noexcept;
    void put_text(bool flush_incomplete = false);
    void put_grapheme(std::string_view grapheme);
    void handle_csi(char final_byte);
    void erase_cells(int left, int top, int right, int bottom) noexcept;
    void insert_cells(int count) noexcept;
    void delete_cells(int count) noexcept;
    void scroll_up(int rows) noexcept;
    void scroll_up_region(int top, int bottom, int rows) noexcept;
    void scroll_down_region(int top, int bottom, int rows) noexcept;
    void index() noexcept;
    void reverse_index() noexcept;
    void reset_active_buffer();
    void diagnostic(TerminalDiagnostic::Kind kind, std::string message);
    bool append_control(char byte);
    bool append_dcs_byte(char byte);
    static int parameter(std::string_view text, std::size_t index, int default_value);

    TerminalCapabilityProfile profile_;
    TerminalSubsessionOptions options_;
    Size cells_;
    Style style_;
    CursorState cursor_{true, {}, CursorShape::Block};
    CursorState saved_cursor_{true, {}, CursorShape::Block};
    bool saved_cursor_valid_ = false;
    std::vector<Cell> primary_;
    std::vector<Cell> alternate_;
    std::vector<Cell> scrollback_;
    std::vector<TerminalRaster> rasters_;
    int raster_identity_ = 0;
    bool alternate_buffer_ = false;
    int scroll_top_ = 0;
    int scroll_bottom_ = 0;
    bool origin_mode_ = false;
    ParseState parse_state_ = ParseState::Ground;
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
    std::string title_;
    TerminalSubsessionState state_ = TerminalSubsessionState::Ready;
    std::optional<int> exit_code_;
    bool bracketed_paste_enabled_ = false;
    bool mouse_reporting_enabled_ = false;
    bool mouse_sgr_enabled_ = false;
    bool application_cursor_keys_ = false;
    bool focus_reporting_enabled_ = false;
};

}  // namespace ckv::term
