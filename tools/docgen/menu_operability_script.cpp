// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "menu_operability_script.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "cvision/scene/painter.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::docgen {

// The document the menus act on. It takes the focus, shows what the View
// menu has set, and asks for its context menu the ways a reader asks: the
// Menu key or Shift-F10 at the focus, and a right click where the pointer is. It draws
// its own lines rather than holding a child view, because a pointer press
// is delivered to the view under it and to nothing above.
class DocumentPane final : public ui::View {
public:
    DocumentPane(ui::RoleId text_role, std::function<void(std::optional<Point>)> open_context_menu)
        : View(Rect{}, ui::FocusPolicy::TabStop),
          text_role_(text_role),
          open_context_menu_(std::move(open_context_menu)) {}

    void set_lines(std::vector<std::string> lines) {
        lines_ = std::move(lines);
        invalidate();
    }

    void draw(scene::Painter& painter) override {
        const Style style = context().theme->resolve(text_role_);
        painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
        for (std::size_t row = 0; row < lines_.size(); ++row)
            painter.draw_text(Point{0, static_cast<int>(row)}, lines_[row], style);
    }

    bool on_key(const KeyEvent& event) override {
        if (!widgets::is_keyboard_context_menu_request(event)) return false;
        open_context_menu_(std::nullopt);
        return true;
    }

    bool on_mouse(const MouseEvent& event) override {
        if (event.action != MouseAction::Down || event.button != MouseButton::Right) return false;
        open_context_menu_(event.cell);
        return true;
    }

private:
    ui::RoleId text_role_;
    std::function<void(std::optional<Point>)> open_context_menu_;
    std::vector<std::string> lines_;
};

namespace {

widgets::MenuMark mark(bool checked) {
    return checked ? widgets::MenuMark::Checked : widgets::MenuMark::Unchecked;
}

}  // namespace

MenuStage::MenuStage(std::vector<ScriptBeat> beats)
    : player(terminal, app, std::move(beats)), roles_(ui::intern_standard_roles(app.roles())) {
    app.theme() = ui::make_classic_theme(app.roles(), roles_);
    auto desktop = std::make_unique<widgets::Desktop>(app.root().bounds());
    desktop_ = desktop.get();
    app.root().add_child(std::move(desktop));

    const auto row = [this](const char* label, const char* what) {
        return widgets::MenuItem::action(label, [this, what] { choose(what); });
    };
    // A checkable row: choosing it flips `state`, and its mark reads `state`
    // whenever the menu draws.
    const auto toggle = [this](const char* label, bool& state, const char* what) {
        return widgets::MenuItem::action(label,
                                         [this, &state, what] {
                                             state = !state;
                                             choose(what);
                                         })
            .with_mark_provider([&state] { return mark(state); });
    };
    widgets::MenuBarItem file{
        "&File",
        {row("&New", "new"),
         widgets::MenuItem::submenu("Open &recent", {row("&Alpha", "alpha"), row("&Beta", "beta")}),
         widgets::MenuItem::separator(),
         widgets::MenuItem::command(app.commands().standard().quit)}};
    widgets::MenuBarItem view{"&View",
                              {toggle("&Wrap lines", wrap_lines_, "wrap"),
                               toggle("&Line numbers", line_numbers_, "numbers"),
                               widgets::MenuItem::separator(),
                               row("&Print preview", "preview")
                                   .with_enabled(false)
                                   .with_disabled_reason("No printer is configured.")}};
    bar_ = desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file), std::move(view)}));
    // The status line explains the row the reader stands on when it cannot
    // be used (D-083): the application's own wiring of the menus' highlight.
    status_ = desktop_->dock_bottom(std::make_unique<widgets::StatusLine>());
    bar_->on_highlight_changed = [this](const widgets::MenuHighlight& highlight) { explain(highlight); };

    auto window = std::make_unique<widgets::Window>("Document");
    window->set_bounds(Rect{4, 3, 48, 12});
    auto pane = std::make_unique<DocumentPane>(roles_.static_text, [this](std::optional<Point> at) {
        widgets::DropdownMenu* menu = at ? widgets::show_context_menu(context_items(), *at, app, *desktop_)
                                         : widgets::show_context_menu_for_focus(context_items(), app, *desktop_);
        menu->on_highlight_changed = [this](const widgets::MenuHighlight& highlight) { explain(highlight); };
        explain(menu->highlight());
    });
    document_ = pane.get();
    window->set_content(std::move(pane));
    desktop_->add_window(std::move(window));
    refresh_document();
    app.set_focus(document_);
}

ui::View* MenuStage::document() const noexcept { return document_; }

std::vector<widgets::MenuItem> MenuStage::context_items() {
    return {widgets::MenuItem::action("Cu&t", [this] { choose("cut"); }),
            widgets::MenuItem::action("&Copy", [this] { choose("copy"); }),
            widgets::MenuItem::action("&Paste", [this] { choose("paste"); })
                .with_enabled(false)
                .with_disabled_reason("The clipboard is empty."),
            widgets::MenuItem::separator(),
            widgets::MenuItem::action("Select &all", [this] { choose("select-all"); })};
}

widgets::StatusLine& MenuStage::status() noexcept { return *status_; }

void MenuStage::explain(const widgets::MenuHighlight& highlight) {
    status_->set_transient_hint(!highlight.none && !highlight.enabled ? highlight.disabled_reason : std::string{});
}

void MenuStage::choose(std::string what) {
    chosen_.push_back(std::move(what));
    refresh_document();
}

void MenuStage::refresh_document() {
    const std::string last = chosen_.empty() ? "nothing" : chosen_.back();
    document_->set_lines({std::string("Wrap lines: ") + (wrap_lines_ ? "on" : "off"),
                          std::string("Line numbers: ") + (line_numbers_ ? "on" : "off"),
                          "Last chosen: " + last});
}

std::vector<ScriptBeat> menu_keyboard_script() {
    return {
        {"initial", {}, "menu_initial.dump"},
        {"f10_bar", {key(Key::F10)}, ""},
        {"file_opened", {key(Key::Down)}, ""},
        {"recent_highlighted", {key(Key::Down)}, ""},
        {"submenu_opened", {key(Key::Right)}, "menu_submenu.dump"},
        {"submenu_down", {key(Key::Down)}, ""},
        {"submenu_left", {key(Key::Left)}, ""},
        {"submenu_reentered", {key(Key::Right)}, ""},
        // Esc closes one level at a time: the submenu alone, then the
        // top-level menu, leaving the walk on its title, then the walk; and
        // Esc on the bar walk (F10, nothing open) leaves it.
        {"escape_submenu", {key(Key::Escape)}, ""},
        {"escape_menu", {key(Key::Escape)}, "menu_keyboard_escaped.dump"},
        {"escape_walk", {key(Key::Escape)}, ""},
        {"f10_again", {key(Key::F10)}, ""},
        {"escape_bar", {key(Key::Escape)}, ""},
        {"alt_v_opened", {alt("v")}, ""},
        {"wrap_chosen", {key(Key::Enter)}, ""},
        {"view_reopened", {alt("v")}, ""},
        {"disabled_highlighted", {key(Key::Down), key(Key::Down)}, "menu_keyboard_disabled.dump"},
        {"disabled_enter", {key(Key::Enter)}, ""},
        {"escape_view", {key(Key::Escape)}, ""},
        {"escape_view_walk", {key(Key::Escape)}, ""},
        {"context_opened", {key(Key::F10, Modifier::Shift)}, "menu_keyboard_context.dump"},
        {"context_copy_chosen", {key(Key::Down), key(Key::Enter)}, ""},
        // The Menu key asks for the same menu at the same place.
        {"context_reopened", {key(Key::Menu)}, ""},
        {"context_disabled_highlighted", {key(Key::Down), key(Key::Down)}, ""},
        {"context_escaped", {key(Key::Escape)}, ""},
    };
}

std::vector<ScriptBeat> menu_mouse_script() {
    return {
        {"initial", {}, "menu_initial.dump"},
        {"bar_pressed", {press(MenuStage::kFileTitle)}, ""},
        {"dragged_to_view", {move(MenuStage::kViewTitle, MouseButton::Left)}, ""},
        {"dragged_to_wrap", {move(MenuStage::kWrapRow, MouseButton::Left)}, "menu_mouse_drag.dump"},
        {"released_on_wrap", {release(MenuStage::kWrapRow)}, ""},
        {"file_clicked", {press(MenuStage::kFileTitle), release(MenuStage::kFileTitle)}, ""},
        {"recent_hovered", {move(MenuStage::kRecentRow)}, "menu_submenu.dump"},
        {"alpha_hovered", {move(MenuStage::kAlphaRow)}, ""},
        {"alpha_clicked", {press(MenuStage::kAlphaRow), release(MenuStage::kAlphaRow)}, ""},
        {"context_opened",
         {press(MenuStage::kDocumentCell, MouseButton::Right),
          release(MenuStage::kDocumentCell, MouseButton::Right)},
         "menu_mouse_context.dump"},
        {"outside_dismissed",
         {press(MenuStage::kDesktopCell), release(MenuStage::kDesktopCell)},
         "menu_mouse_dismissed.dump"},
    };
}

}  // namespace ckv::docgen
