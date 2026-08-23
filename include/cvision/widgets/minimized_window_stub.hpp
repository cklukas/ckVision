// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// MinimizedWindowStub (D-064): the one row a put-away window leaves behind —
// its own top frame, rolled up, parked along the bottom of the desktop.
//
//     ┌[■]── config.yaml ──[↑]┐
//
// Nothing here is a new vocabulary. The corners, the rule, the bracketed
// controls and the centred caption are the ones `Window::draw` puts on a
// frame's top border, and the two controls are the two verbs that still
// apply to a window nobody can see: `[■]` closes it, `[↑]` brings it back.
// The minimize control is the one that is gone, because it has already
// happened. A reader who can read a window frame can read this without
// being taught a second alphabet — which is the whole argument for drawing
// the frame rather than inventing an icon.
//
// It is NOT a Window. The window it stands for is untouched behind it —
// hidden, its bounds, zoom state and z-order exactly as they were (D-056) —
// and this view owns no part of it. That separation is what lets restoring
// replay nothing: the stub is deleted, the window is made visible again, and
// it comes back the size and shape it left.
//
// A Desktop whose `minimized_window_placement()` is `Parked` creates, places
// and destroys these itself; an application never constructs one. It is
// public because it is what a reader SEES, so a test naming what is on the
// screen — and a host restyling it through the window roles — both need it
// by name.
#pragma once

#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class Window;

class MinimizedWindowStub : public ui::View {
public:
    // The cells the frame itself costs: two corners, the rule cell inside
    // each, the two three-cell bracketed controls, and one space of padding
    // on each side of the caption. A stub is `kChromeWidth + title` wide
    // when it can be, and elides the title when it cannot.
    static constexpr int kChromeWidth = 12;

    explicit MinimizedWindowStub(Window& window);

    // The window this stands for. Never null while the stub is on a desktop:
    // a Desktop drops the stub as part of removing the window, before either
    // can be observed apart.
    Window* window() const noexcept { return window_; }

    // What this stub would like to be, caption included. The parking layout
    // asks, then gives what it can — never less than kChromeWidth + 1, so
    // there is always a cell of name.
    int natural_width() const;

    // Re-reads the window's title. The Desktop calls this on a rename; a
    // stub does not subscribe on its own, because the Desktop is already
    // subscribed and a second observer would only race the first.
    void refresh();

    // What the two controls do. The Desktop installs both — restoring is a
    // question about a desktop (it activates, and activation is a desktop's
    // word), and closing has to go through the window's own close_request so
    // that an editor still gets to ask about unsaved changes.
    std::function<void()> on_restore;
    std::function<void()> on_close;

    // Where the controls are, for hit-testing and for tests that press them
    // by name rather than by column arithmetic.
    bool point_in_close_control(Point local) const noexcept;
    bool point_in_restore_control(Point local) const noexcept;

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    bool on_key_release(const KeyEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    // The restore control sits where the frame's zoom control sits, counted
    // back from the right edge, so the two line up when a stub is parked
    // under the window it came from.
    int restore_control_x() const noexcept;

    Window* window_ = nullptr;
    std::string title_;
    ui::RoleId frame_role_ = ui::kInvalidRole;
    ui::RoleId title_role_ = ui::kInvalidRole;
    ui::RoleId control_role_ = ui::kInvalidRole;
    ui::RoleId control_pressed_role_ = ui::kInvalidRole;

    // A press on this row is ARMED on the way down and DECIDED on the way up,
    // exactly as the window frame's own controls are (Window::on_mouse's
    // held_control_): held while the button is down, drawn pressed only while
    // the pointer is still over what it went down on, fired only if released
    // there, and taken back by releasing anywhere else. Firing on the press
    // itself — what this row did first — gave a reader no way to change
    // their mind, and closed a window from a press they were still deciding
    // about. Two controls: Close, and Restore — which the caption and the
    // arrow both are.
    enum class Held : unsigned char { None, Close, Restore };
    Held held_ = Held::None;
    bool held_inside_ = true;
    // The keyboard half of the same grammar, Button's way (D-055): Enter or
    // Space on a focused stub arms the restore control; on a session whose
    // verified key enhancements report releases, the key coming back up is
    // what restores, and Escape or focus moving away in between takes the
    // press back; on a session that cannot report a release the press acts
    // at once, held visibly down for a moment so an accepted keystroke does
    // not look exactly like an ignored one.
    bool key_armed_ = false;
    bool key_flash_ = false;
};

}  // namespace ckv::widgets
