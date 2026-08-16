// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ListView: provider-backed scrolling, identity-stable single/multi selection,
// keyboard search, and double-click activation (D-043). It owns its vertical
// Scrollbar directly: scrolling is "first visible model item", not an arbitrary
// child View offset. The provider is caller-owned and queried only for the
// visible slice. `set_items` remains the compact value convenience; it selects
// the widget's internal materialized model.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

using ui::SizeHint;

// Stable identity is supplied by the application model, never derived from a
// volatile display index. Zero is reserved as the invalid/no-item value.
using ListItemId = std::uint64_t;
inline constexpr ListItemId kInvalidListItemId = 0;

struct ListItem {
    ListItemId id = kInvalidListItemId;
    std::string text;
    std::optional<Style> style;
};

// A synchronous visible-slice provider. A provider may represent millions of
// items; it must not require ListView to enumerate the model to recover an
// identity after a refresh. Async clients update their own provider and call
// ListView::model_changed() on the UI thread.
class ListModel {
public:
    virtual ~ListModel() = default;

    virtual std::size_t item_count() const = 0;
    virtual ListItem item_at(std::size_t index) const = 0;
    virtual std::optional<std::size_t> index_of(ListItemId id) const = 0;

    // Optional provider-side type-ahead. `after` is the current display index;
    // a returned index must be in [0, item_count()). No default linear scan is
    // supplied, because that would silently defeat a virtual provider.
    virtual std::optional<std::size_t> find_prefix(std::string_view folded_prefix,
                                                    std::size_t after) const {
        (void)folded_prefix;
        (void)after;
        return std::nullopt;
    }
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.list.normal"/"ckv.list.selected"; its embedded
// Scrollbar resolves its own roles the same way, independently.
class ListView : public ui::View {
public:
    explicit ListView(bool multi_select = false);

    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        normal_role_ = normal_role;
        selected_role_ = selected_role;
    }

    // Borrows `model`; it must outlive this ListView or be replaced/cleared
    // before destruction. Changing models clears identities; model_changed()
    // preserves identities that survive a reorder/filter/refresh.
    void set_model(ListModel& model);
    void clear_model();
    ListModel* model() const noexcept { return model_; }
    void model_changed();

    // Convenience materialized model for small static lists. Calling this
    // clears any external provider and assigns deterministic non-zero ids.
    void set_items(std::vector<std::string> items);
    const std::vector<std::string>& items() const noexcept { return items_; }

    std::optional<ListItemId> cursor_id() const noexcept;
    bool is_selected(std::size_t index) const;
    bool is_selected_id(ListItemId id) const;
    // Single-select: selecting an item deselects every other one.
    // Multi-select: toggles independently.
    void set_selected(std::size_t index, bool selected);
    void set_selected_id(ListItemId id, bool selected);
    std::vector<std::size_t> selected_indices() const;
    const std::vector<ListItemId>& selected_ids() const noexcept { return selected_ids_; }

    // Whether the scrollbar disappears while every item already fits. Off by
    // default: in a list large enough to be the point of its window, a
    // permanently visible bar reads as part of the frame and its absence
    // would be the surprise. It is the wrong default for a short list
    // presented as a small group of choices — a help topic's cross-links,
    // say — where a control that cannot do anything is the only thing
    // suggesting the list has more to it than the reader can see.
    void set_scrollbar_auto_hide(bool enabled);
    bool scrollbar_auto_hide() const noexcept;

    int cursor() const noexcept { return cursor_; }  // display index, -1 if empty

    // Index callbacks remain useful for compact materialized clients. Provider
    // clients should use the identity callbacks, which survive reordering.
    std::function<void(std::size_t)> on_activate;
    std::function<void(ListItemId)> on_activate_id;
    std::function<void(std::size_t)> on_selection_changed;
    std::function<void(ListItemId)> on_selection_changed_id;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    static constexpr std::int64_t kDoubleClickIntervalNanos = 500'000'000;

    std::size_t item_count() const;
    ListItem item_at(std::size_t index) const;
    std::optional<std::size_t> index_of(ListItemId id) const;
    ListItemId id_at(std::size_t index) const;
    void move_cursor(int new_cursor, bool select_on_move);
    void ensure_cursor_visible();
    void select_only(std::size_t index);
    void toggle_selected(ListItemId id);
    bool contains_selected(ListItemId id) const noexcept;
    void notify_selection(ListItemId id);
    void resolve_model_identities();

    std::vector<std::string> items_;
    ListModel* model_ = nullptr;
    std::vector<ListItemId> selected_ids_;
    ListItemId cursor_id_ = kInvalidListItemId;
    int cursor_ = -1;
    bool multi_select_;
    int last_click_index_ = -1;
    std::int64_t last_click_nanos_ = -1;

    Scrollbar* scrollbar_ = nullptr;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
