// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <string>
#include <vector>
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"

namespace ckv::widgets {

// ckv.input.accessory.hovered. The field resolves the usual input roles.
class CVISION_TYPE_API SearchBox : public ui::View {
public:
    // An empty query in a field that is a Tab stop.
    CVISION_MEMBER_API SearchBox();
    // Owned display-safe prompt, including any desired trailing separator.
    // Defaults to "Search "; changing it relayouts and fires no query callback.
    CVISION_MEMBER_API void set_prompt(std::string prompt);
    const std::string& prompt() const noexcept { return prompt_; }
    // Color-led query chrome, shared with InputLine and ComboBox. No brackets.
    CVISION_MEMBER_API void set_presentation(InputPresentation presentation);
    InputPresentation presentation() const noexcept { return field_->presentation(); }
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    CVISION_MEMBER_API void on_hover_changed(bool hovered) override;
    // Replaces the query, caret at its end; fires on_change if it differs,
    // programmatic or not.
    CVISION_MEMBER_API void set_query(std::string query);
    // The query as the field holds it.
    std::string query() const { return field_->text(); }
    // Empties the query, firing on_change if it was not empty, then fires
    // on_clear in any case. Escape and the "x" control come here.
    CVISION_MEMBER_API void clear();
    // Fires with the whole new query after every change: each typed
    // character, an erase, a cut, paste or undo, a recalled history entry,
    // clearing, and set_query.
    std::function<void(const std::string&)> on_change;
    // Fires after every clear(), including one of an already empty query.
    std::function<void()> on_clear;
    // The status text drawn at the right, empty (the default) for none.
    // Drawn verbatim; setting it repaints and fires nothing.
    CVISION_MEMBER_API void set_status(std::string status);
    const std::string& status() const noexcept { return status_; }
    // The columns the query field keeps however long the status is.
    static constexpr int kMinimumQueryColumns = 6;
    // Names the history list the field shares (InputLine::set_history_key):
    // the list under `key` in the Application::history() of the Application
    // the box is attached to, which every input line, combo box, search box
    // and dialog field naming the same key shares too. While a key is set and
    // the box is attached, Up recalls older queries and Down newer ones, back
    // to the query that was there before recall began, each recall firing
    // on_change as typing does; and Enter records the query, leaving the key
    // unhandled for whatever else answers it. Empty (the default) turns
    // history off.
    void set_history_key(std::string key) { field_->set_history_key(std::move(key)); }
    const std::string& history_key() const noexcept { return field_->history_key(); }
    // Records the query as the newest entry of the history list. An empty
    // query is not recorded, and nothing is without a key or an Application.
    CVISION_MEMBER_API void commit_to_history();

    // The query field: the box's focus stop, and what an owner focuses
    // (Application::set_focus(&box.field())), gives another focus policy, or
    // hands keys to when the typing happens elsewhere -- a list whose
    // type-ahead filters it, say. Edits made through it report on_change like
    // any other. Its bounds belong to the box, which lays it out.
    InputLine& field() noexcept { return *field_; }
    const InputLine& field() const noexcept { return *field_; }

    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Escape and Enter as they bubble up from the field (see above).
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    // Presses outside the field: the clear control, and the rest of the box.
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    CVISION_MEMBER_API void on_resized() override;
    CVISION_MEMBER_API void on_attached() override;
    CVISION_MEMBER_API void on_strings_changed() override;

private:
    // Where the parts fall across the box's width, left to right: the
    // prompt, the query field, the status (with its leading blank) and the
    // clear control. Drawing, the field's placement and hit-testing all read
    // this one layout, which keeps what the reader sees and what answers a
    // click the same thing.
    struct Layout {
        int prompt_width = 0;
        int field_x = 0;
        int field_width = 0;
        // The status's columns, its leading blank included; they end where
        // the clear control begins.
        int status_width = 0;
        int clear_x = 0;
        int clear_width = 0;
    };
    Layout layout() const;
    // Puts the field where layout() says it goes. The layout changes with
    // the width, the status and whether there is a query to clear.
    void place_field();
    // What every change of query does: the field is placed again (the clear
    // control comes and goes with the query) and on_change hears the query.
    void query_changed();
    // Columns the "x" clear control occupies at the right edge. Drawing and
    // hit-testing both derive from this, which is what keeps the control the
    // reader can see and the region that answers a click the same thing.
    static constexpr int kClearControlWidth = 3;

    InputLine* field_ = nullptr;
    std::string prompt_ = "Search ";
    int prompt_columns_ = 7;
    std::optional<Point> hover_position_;
    std::string status_;
    std::string shown_status_;
    int status_x_ = 0;
    ui::RoleId accessory_role_ = ui::kInvalidRole;
    ui::RoleId accessory_hovered_role_ = ui::kInvalidRole;
    ui::RoleId label_role_ = ui::kInvalidRole;
    ui::RoleId label_disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
