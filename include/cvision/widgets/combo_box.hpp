// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

enum class ComboBoxMode { PickOnly, Editable };

// Editable or pick-only combo box with deterministic in-application history.
// The dropdown is drawn inside this widget's own bounds; callers that want an
// overlaid popup give the control enough height while it is open.
class ComboBox : public ui::View {
public:
    explicit ComboBox(ComboBoxMode mode = ComboBoxMode::PickOnly);

    void set_items(std::vector<std::string> items);
    const std::vector<std::string>& items() const noexcept { return items_; }

    void set_mode(ComboBoxMode mode);
    ComboBoxMode mode() const noexcept { return mode_; }
    bool editable() const noexcept { return mode_ == ComboBoxMode::Editable; }

    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }

    void set_selected_index(std::optional<std::size_t> index);
    std::optional<std::size_t> selected_index() const noexcept { return selected_index_; }

    void set_history(ui::HistoryRegistry* registry, std::string key);
    void commit_to_history();

    void open_dropdown();
    void close_dropdown();
    bool dropdown_open() const noexcept { return dropdown_open_; }

    std::function<void(std::size_t)> on_select;
    std::function<void(const std::string&)> on_text_changed;

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void select_index(std::size_t index, bool notify);
    void move_selection(int delta);
    void recall_history(int index);

    ComboBoxMode mode_;
    std::vector<std::string> items_;
    std::string text_;
    std::optional<std::size_t> selected_index_;
    bool dropdown_open_ = false;
    bool has_focus_ = false;

    ui::HistoryRegistry* history_registry_ = nullptr;
    std::string history_key_;
    int history_index_ = -1;
    std::string history_saved_text_;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId shadow_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
