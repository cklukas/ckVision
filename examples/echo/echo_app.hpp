// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Echo: the classic terminal input echo (the roadmap M3 exit). The whole
// screen is one view that writes a decoded line for every key, key release,
// mouse, paste, focus and resize event it receives, newest last, and a line
// whenever the host's capabilities change. It is the quickest way to see
// what a host actually sends and what ckVision made of it: which chord a key
// decodes to, whether the host reports releases, pixels or focus, and
// whether a paste arrives bracketed. See docs/example-apps.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>

#include "cvision/core/event.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::echo {

// The decoded line for each kind of event, without the sequence number the
// view puts in front of it. Key text and paste text are quoted with their
// control characters escaped, and a character key also names its
// codepoints, so what the host sent is readable whatever it was.
std::string describe(const KeyEvent& event);
std::string describe(const MouseEvent& event);
std::string describe(const TextEvent& event);
std::string describe(const FocusEvent& event);
std::string describe(const ResizeEvent& event);
// The input-relevant part of a capability set: keyboard protocol and its
// verified kitty flags, mouse protocol and pixel reports, focus reports and
// bracketed paste.
std::string describe(const term::Capabilities& capabilities);

// The full-screen echo surface. It takes the focus and every pointer report
// over its area, records each event as a numbered line, and draws a header,
// then as many of the newest lines as fit. Keys are observed rather than
// consumed, so a key bound to a command (the standard quit, Tab) still runs
// it after its line is written; text, mouse and release events are consumed.
class EchoView final : public ui::View {
public:
    // How many lines the view keeps; older ones are dropped first. The
    // observer below still receives every line.
    static constexpr std::size_t kKeptLines = 1000;

    // `roles` resolves the header and body styles; `observer`, when set,
    // receives every recorded line (sequence number included) as it is made.
    EchoView(ui::StandardRoles roles, std::function<void(std::string_view)> observer);

    // Numbers `line`, keeps it, hands it to the observer and repaints.
    void record(std::string_view line);

    // The kept lines, oldest first, each with its sequence number.
    const std::deque<std::string>& lines() const noexcept { return lines_; }
    // How many lines have been recorded since the view was made.
    std::uint64_t recorded() const noexcept { return recorded_; }

    void draw(scene::Painter& painter) override;
    // Records the new size: the view fills the root, so it is the terminal's.
    void on_resized() override;
    // Records the key and returns false, leaving it to the command keymap.
    bool on_key(const KeyEvent& event) override;
    // Records a reported release.
    bool on_key_release(const KeyEvent& event) override;
    // Receives the modifier, Super and Menu keys a kitty host reports alone.
    bool accepts_standalone_keys() const noexcept override { return true; }
    // Records a paste (or other inserted text).
    bool on_text(const TextEvent& event) override;
    // Records every pointer report that reaches the view.
    bool on_mouse(const MouseEvent& event) override;
    // Records the view gaining or losing focus, and host focus reports.
    void on_focus(const FocusEvent& event) override;

private:
    ui::StandardRoles roles_;
    std::function<void(std::string_view)> observer_;
    std::deque<std::string> lines_;
    std::uint64_t recorded_ = 0;
};

// The application: the classic theme, the echo view as the root's only
// child, the standard quit command wired to Application::request_quit, and
// capability changes recorded as they arrive.
class EchoApp {
public:
    // `observer` is handed to the view; see EchoView's constructor.
    explicit EchoApp(ui::Application& app, std::function<void(std::string_view)> observer = {});

    // The echo surface, owned by the Application's root.
    EchoView& view() noexcept { return *view_; }
    const EchoView& view() const noexcept { return *view_; }

private:
    ui::Application& app_;
    ui::StandardRoles roles_;
    EchoView* view_ = nullptr;
};

}  // namespace ckv::echo
