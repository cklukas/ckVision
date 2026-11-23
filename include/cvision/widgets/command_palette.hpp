// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "cvision/ui/command.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"

namespace ckv::widgets {

// attach.
class CVISION_TYPE_API CommandPalette : public ui::View {
public:
    // A Tab stop with an empty query, preferring 40 x 8 cells.
    CVISION_MEMBER_API CommandPalette();
    // Replaces the query and moves the highlight back to the first enabled
    // result. The match is a case-insensitive (ASCII) substring of the title
    // as displayed, mnemonic marker removed; an empty query matches every
    // command. Fires nothing.
    CVISION_MEMBER_API void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    // Palette-visible, title-matching and in scope where the palette answers
    // (ui::CommandRegistry::in_scope), enabled or not — in the order the
    // commands were declared.
    CVISION_MEMBER_API std::vector<ui::CommandInfo> filtered_commands() const;
    // The command contexts the palette answers for (ui::command_context_path
    // of the view the reader was on when it opened), instead of its own
    // focus path. A palette presented in a window of its own takes the focus
    // from the place whose commands it lists, exactly as a menu walk does,
    // and without this it would offer what the palette's window allows —
    // no context-bound command at all.
    CVISION_MEMBER_API void set_invocation_contexts(std::vector<std::string> contexts);
    // The command Enter would execute: the highlighted entry of
    // filtered_commands(), or std::nullopt when no enabled command matches.
    CVISION_MEMBER_API std::optional<ui::CommandId> highlighted_command() const;
    // Whether a single-line frame is drawn in the outermost ring of cells,
    // inside the inset, and the palette casts the standard shadow: how it
    // stands as a popup over other content. Off by default.
    CVISION_MEMBER_API void set_framed(bool framed);
    bool framed() const noexcept { return framed_; }
    // When set, the palette dismisses itself as a popup does, calling this
    // once: on Escape, on a left press outside it (which reaches it while it
    // holds the input capture), and before it executes the command Enter or a
    // click chose — so the command runs where the reader was rather than in
    // the palette. show_command_palette sets it to tear the popup down; the
    // palette may be destroyed by the call. Unset, Escape is left to the host.
    std::function<void()> on_dismiss;

    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // The caret at the end of the query: the palette is typed into, and the
    // caret is what says so.
    CVISION_MEMBER_API std::optional<CursorState> cursor_state() const override;
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    CVISION_MEMBER_API bool on_text(const TextEvent& event) override;
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    CVISION_MEMBER_API void on_attached() override;
    // True while framed.
    bool casts_shadow() const noexcept override { return framed_; }

private:
    // The contexts the palette answers for: the invocation contexts when set,
    // otherwise its own place.
    std::vector<std::string> answering_contexts() const;
    bool enabled_command(ui::CommandId id) const;
    // The row the highlight is on once disabled rows are passed over: the
    // chosen row if enabled, else the next enabled one, else the previous;
    // std::nullopt when no row is enabled.
    std::optional<std::size_t> resolved_highlight(const std::vector<ui::CommandInfo>& commands) const;
    // The first result row shown, so the highlighted row is in view.
    std::size_t first_visible(std::size_t highlighted) const;
    void move_highlight(int delta);
    // Executes `id` (dismissing first when on_dismiss is set) and returns
    // whether its handler ran.
    bool run(ui::CommandId id);
    void dismiss();

    std::string query_;
    std::size_t highlighted_ = 0;
    std::optional<std::vector<std::string>> invocation_contexts_;
    std::vector<ui::CommandId> mru_order_;
    bool framed_ = false;
    bool dismissed_ = false;
    bool scrollbar_dragging_ = false;
    ui::RoleId input_role_ = ui::kInvalidRole;
    ui::RoleId result_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// Opens a framed CommandPalette as a popup over `desktop`, the default handler
// of the standard command_palette command (Ctrl+Shift+P). It is centred near
// the top of the desktop's content area, up to 56 x 14 cells, and lists what
// the view focused when it opened allows (set_invocation_contexts with that
// view's context path). It is modal, holds the input capture and takes the
// focus; Escape, a press outside it, or running a command dismisses it, which
// releases the capture and the modal scope, gives the focus back to the view
// that had it, and destroys the palette. Returns the palette, valid until
// then.
CVISION_API CommandPalette* show_command_palette(ui::Application& app, Desktop& desktop);

}  // namespace ckv::widgets
