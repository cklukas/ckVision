// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "close_veto_script.hpp"

#include <utility>

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::docgen {
namespace {

std::string refusal_text(const char* subject, int refusals) {
    return std::string(subject) + ": close refused " + std::to_string(refusals) +
           (refusals == 1 ? " time." : " times.");
}

}  // namespace

CloseVetoStage::CloseVetoStage(std::vector<ScriptBeat> beats)
    : player(terminal, app, std::move(beats)), roles_(ui::intern_standard_roles(app.roles())) {
    app.theme() = ui::make_classic_theme(app.roles(), roles_);
    auto desktop = std::make_unique<widgets::Desktop>(app.root().bounds());
    desktop_ = desktop.get();
    app.root().add_child(std::move(desktop));

    auto notes = std::make_unique<widgets::Window>("Notes");
    notes->set_bounds(Rect{26, 2, 30, 8});
    notes->set_content(std::make_unique<widgets::StaticText>("Nothing to save here."));
    notes_ = desktop_->add_window(std::move(notes));

    auto draft = std::make_unique<widgets::Window>("Draft");
    draft->set_bounds(Rect{3, 5, 32, 9});
    auto content = std::make_unique<ui::View>();
    auto status = std::make_unique<widgets::StaticText>("Unsaved changes.");
    status->set_bounds(Rect{1, 1, 28, 2});
    draft_status_ = content->add(std::move(status));
    auto save = std::make_unique<widgets::Button>("&Save");
    save->set_bounds(Rect{1, 4, 10, 2});
    save->on_press = [this] {
        saved_ = true;
        draft_status_->set_text("Saved.");
    };
    save_button_ = content->add(std::move(save));
    draft->set_content(std::move(content));
    // The standard unsaved-changes veto: the window stays, and says why.
    draft->close_request = [this] {
        if (saved_) return true;
        ++draft_refusals_;
        draft_status_->set_text(refusal_text("Unsaved changes", draft_refusals_));
        return false;
    };
    // A window with an on_closed handler is detached by its owner; this
    // stage forgets the window and then has it leave the desktop.
    draft->on_closed = [this] {
        widgets::Window* const closed = draft_;
        draft_ = nullptr;
        save_button_ = nullptr;
        draft_status_ = nullptr;
        widgets::schedule_self_detach(*closed, app);
    };
    draft_ = desktop_->add_window(std::move(draft));
    notes_->on_closed = [this] {
        widgets::Window* const closed = notes_;
        notes_ = nullptr;
        widgets::schedule_self_detach(*closed, app);
    };
    app.set_focus(save_button_);

    widgets::FieldDescriptor note{"Review the draft before it may go.", "", nullptr};
    note.kind = widgets::FieldKind::Note;
    review_descriptor_.fields.push_back(std::move(note));
    widgets::FieldDescriptor reviewed{"&Reviewed", "", nullptr};
    reviewed.kind = widgets::FieldKind::Check;
    review_descriptor_.fields.push_back(std::move(reviewed));
    review_descriptor_.buttons.push_back(
        widgets::ButtonDescriptor{"&Close", widgets::ButtonRole::Dismiss, nullptr});

    app.commands().declare({.key = "closeveto.review",
                            .title = "&Review...",
                            .category = "Script",
                            .chord = "F2",
                            .handler = [this] { open_review(); }});
}

void CloseVetoStage::open_review() {
    if (review_ != nullptr) return;
    widgets::MaterializedDialog dialog = widgets::materialize_dialog(review_descriptor_);
    review_note_ = dialog.labels[0];
    reviewed_ = dialog.checks[1];
    ui::View* const initial_focus = dialog.initial_focus;
    auto window = std::make_unique<widgets::Window>("Review");
    window->set_bounds(Rect{10, 6, 42, 10});
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                              roles_.dialog_background);
    window->set_content_margin(1, 1);
    // Installed before wire_dialog_window, which runs it after restoring the
    // focus the dialog took.
    window->on_closed = [this] {
        widgets::Window* const closed = review_;
        review_ = nullptr;
        review_note_ = nullptr;
        reviewed_ = nullptr;
        widgets::schedule_self_detach(*closed, app);
    };
    widgets::wire_dialog_window(*window, std::move(dialog), review_descriptor_, app, app.focused());
    window->close_request = [this] {
        if (reviewed_->checked(0)) return true;
        ++review_refusals_;
        review_note_->set_text(refusal_text("Not reviewed", review_refusals_));
        return false;
    };
    review_ = desktop_->present_modal(widgets::WindowHandle{std::move(window), initial_focus}, app);
}

std::vector<ScriptBeat> close_control_script() {
    const Point control = CloseVetoStage::kDraftCloseControl;
    return {
        {"initial", {}, "close_veto_initial.dump"},
        {"control_refused", {press(control), release(control)}, "close_veto_refused.dump"},
        {"saved", {alt("s")}, ""},
        {"control_closed", {press(control), release(control)}, ""},
    };
}

std::vector<ScriptBeat> close_chord_script() {
    return {
        {"initial", {}, ""},
        {"chord_refused", {key(Key::F3, Modifier::Alt)}, "close_veto_refused.dump"},
        {"saved", {alt("s")}, ""},
        {"chord_closed", {key(Key::F3, Modifier::Alt)}, ""},
    };
}

std::vector<ScriptBeat> dialog_escape_script() {
    return {
        {"initial", {}, ""},
        {"review_opened", {key(Key::F2)}, "close_veto_review_open.dump"},
        {"escape_refused", {key(Key::Escape)}, "close_veto_review_refused.dump"},
        // The Reviewed box holds the focus the dialog opened with.
        {"reviewed_ticked", {character(" ")}, ""},
        {"escape_closed", {key(Key::Escape)}, ""},
    };
}

std::vector<ScriptBeat> quit_sweep_script() {
    return {
        {"initial", {}, ""},
        // F6 brings Notes forward, so the sweep, which asks the front window
        // first, closes Notes before the Draft refuses and stops it.
        {"notes_raised", {key(Key::F6)}, ""},
        {"sweep_refused", {alt("x")}, "close_veto_quit_refused.dump"},
        {"saved", {alt("s")}, ""},
        {"sweep_quit", {alt("x")}, ""},
    };
}

}  // namespace ckv::docgen
