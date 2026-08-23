// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TerminalView consumes the deterministic core session seam. It owns neither
// a process nor a PTY and has no access to the outer Terminal.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "cvision/core/terminal_subsession.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class TerminalView final : public ui::View {
public:
    explicit TerminalView(core::TerminalSubsession& session);

    core::TerminalSubsession& session() noexcept { return *session_; }
    const core::TerminalSubsession& session() const noexcept { return *session_; }

    // Called for the reserved parent-focus escape rather than sending it to
    // the child.  The default is Ctrl+Alt+Space as specified by D-042.
    void set_parent_escape(KeyChord chord) { parent_escape_ = std::move(chord); }
    const KeyChord& parent_escape() const noexcept { return parent_escape_; }
    std::function<void()> on_parent_escape;
    // Keys offered to the host once the child is gone.
    //
    // A terminal view writes what it is given to its child. When the
    // subsession has Exited or Failed there is nobody to write to, so every
    // key a reader presses in that window falls into a hole — and a host that
    // wants to offer them something there (restart the command, dismiss the
    // window) has no way to hear it. This is the same idea as
    // `parent_escape`, which reserves one key while the child is alive; this
    // reserves whatever the host claims once it is not.
    //
    // Consulted before the view's own handling and only in those two states,
    // so a live terminal is unaffected: returning false, or leaving this
    // unset, is exactly the behaviour that existed before it.
    std::function<bool(const KeyEvent&)> on_key_after_exit;
    std::function<void(std::string)> on_selection_copy;
    // The child asked to put text on the clipboard with OSC 52, and its
    // profile allows that. The text has already been decoded, bounded and
    // sanitized; what to do with it — export it to the system clipboard, hold
    // it internally, ask first — is the host's decision, not the widget's.
    std::function<void(std::string)> on_clipboard_write;

    // Where the reader stands in this terminal's history, in rows. The view
    // owns the position — the wheel, PageUp/PageDown and set_scrollback_offset
    // all move it — while how much history there is and how much of it fits
    // are the session's, read fresh each time. `offset` counts up from the
    // live edge: 0 is the running screen, the maximum puts the oldest history
    // row at the top. The alternate buffer has no history to stand in, so
    // there `offset` is pinned at 0 and `primary_screen` says why.
    struct ScrollState {
        int total_rows = 0;     // history rows plus the screen's own
        int viewport_rows = 0;  // how many of them the view can show at once
        int offset = 0;         // rows scrolled back from the live edge
        bool primary_screen = true;
        friend bool operator==(const ScrollState&, const ScrollState&) = default;
    };
    ScrollState scroll_state() const;
    int scrollback_offset() const noexcept { return scrollback_offset_; }
    // Clamped to what the history can honour; on the alternate buffer, to 0.
    void set_scrollback_offset(int rows);
    // Fired when scroll_state() would answer differently than it last did —
    // a scroll, history growth, the buffer flipping, the view resizing. One
    // seam for a scrollbar to observe, rather than instrumenting every input
    // path that can move the view.
    std::function<void()> on_scroll_state_changed;

    // Send a key to the child as though the reader had pressed it: the kitty
    // encoding where the child asked for one, the legacy encoding otherwise,
    // and the return to the live edge that any real keystroke makes.
    //
    // What it deliberately does NOT do is what `on_key` does before that — the
    // parent escape chord is not intercepted and the terminal's own paging keys
    // are not consumed locally. This is a caller saying "the child receives
    // this key", which is the whole point for the two things that need it: an
    // application with a reserved chord that offers to send it through anyway
    // (otherwise that chord is unreachable to the child forever), and anything
    // that replays keys — a macro, a recorded session, a paste delivered as
    // keystrokes rather than as text.
    //
    // Returns whether the key produced any bytes. Not every chord does: a
    // release with no event reporting in force, or a key this encoding has no
    // spelling for, is silence rather than a wrong byte — and a caller
    // replaying a script is entitled to know which.
    bool send_key(const KeyEvent& event);

    void set_cell_metrics(Size cell_pixels);
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    // Releases arrive on their own route (they must never look like a
    // second press to an ordinary control), but a hosted child that asked
    // for kitty event types is owed them; on_key's body already reads the
    // action, claims a matching parent-escape or paging release with its
    // press, and encodes or drops the rest per the child's negotiated
    // flags.
    bool on_key_release(const KeyEvent& event) override { return on_key(event); }
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_resized() override;
    void on_attached() override;
    void on_focus(const FocusEvent& event) override;
    std::optional<CursorState> cursor_state() const override;

protected:
    void on_terminal_subsession_changed(const core::TerminalSubsession& session) override;

private:
    // The one path a key takes to the child, shared by the reader's own press
    // and by `send_key`, so the two cannot come to encode a chord differently.
    bool deliver_key(const KeyEvent& event, const core::TerminalStatus& status);
    static std::string encode_key(const KeyEvent& event, bool application_cursor_keys);
    // The kitty keyboard protocol's encoding, for a child that asked for it.
    // Empty when the enhancements in force do not cover this key, which
    // leaves the legacy encoding to say it instead.
    static std::string encode_kitty_key(const KeyEvent& event, core::TerminalKeyboardFlags flags);
    std::string encode_mouse(const MouseEvent& event, core::TerminalMouseEncoding encoding) const;
    // Whether this event is one the tracking level the child selected asks to
    // hear about. Presses, releases and the wheel are reported at every level;
    // motion is the axis the three levels differ on. Takes the whole status
    // because a session that names no level while reporting is on is a
    // statement about the seam, not about the child — see the definition.
    static bool tracking_reports(const MouseEvent& event, const core::TerminalStatus& status);
    // Alternate scroll (DEC mode 1007): a wheel notch over a full-screen
    // program that is not tracking the mouse becomes cursor keys.
    bool forward_alternate_scroll(const MouseEvent& event, const core::TerminalStatus& snapshot);
    // The primary screen's wheel: a notch walks this terminal's own history.
    bool scroll_history(const MouseEvent& event, const core::TerminalStatus& snapshot);
    ScrollState scroll_state_from(const core::TerminalStatus& status) const;
    void apply_scrollback_offset(int rows, const core::TerminalStatus& status);
    // Typing while scrolled back returns the view to the live edge first: the
    // keystroke lands there, and the reader who sent it wants to see it land.
    void return_to_live(const core::TerminalStatus& status);
    void publish_scroll_state(const core::TerminalStatus& status);
    std::optional<Point> local_cell(Point absolute) const;
    std::string selected_text() const;

    core::TerminalSubsession* session_;
    KeyChord parent_escape_{Key::Char, Modifier::Ctrl | Modifier::Alt, " "};
    Size cell_pixels_{};
    int scrollback_offset_ = 0;
    // History length at the last change notification. A reader scrolled back
    // is reading: growth is added to the offset so the same rows stay put,
    // while the live edge (offset 0) keeps following the child.
    int last_history_rows_ = 0;
    ScrollState published_scroll_state_{};
    std::optional<Point> selection_start_;
    std::optional<Point> selection_end_;
    ui::RoleId text_role_ = ui::kInvalidRole;
    // The clipboard request this view has already forwarded. The snapshot
    // carries a count rather than a queue, so comparing is how a value that
    // may be read any number of times drives an action that happens once.
    std::uint64_t forwarded_clipboard_serial_ = 0;
    bool has_explicit_cell_metrics_ = false;
    bool focused_ = false;
};

}  // namespace ckv::widgets
