// Copyright (c) 2026 C. Klukas. All rights reserved.
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
    std::function<void(std::string)> on_selection_copy;

    void set_cell_metrics(Size cell_pixels);
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_resized() override;
    void on_attached() override;
    void on_focus(const FocusEvent& event) override;
    std::optional<CursorState> cursor_state() const override;

protected:
    void on_terminal_subsession_changed(const core::TerminalSubsession& session) override;

private:
    static std::string encode_key(const KeyEvent& event, bool application_cursor_keys);
    std::string encode_mouse(const MouseEvent& event, core::TerminalMouseEncoding encoding) const;
    std::optional<Point> local_cell(Point absolute) const;
    std::string selected_text() const;

    core::TerminalSubsession* session_;
    KeyChord parent_escape_{Key::Char, Modifier::Ctrl | Modifier::Alt, " "};
    Size cell_pixels_{};
    int scrollback_offset_ = 0;
    std::optional<Point> selection_start_;
    std::optional<Point> selection_end_;
    ui::RoleId text_role_ = ui::kInvalidRole;
    bool has_explicit_cell_metrics_ = false;
    bool focused_ = false;
};

}  // namespace ckv::widgets
