// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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

// One row as a ListModel describes it.
struct ListItem {
    // The item's stable identity; must be non-zero. A row whose item has the
    // invalid id is drawn blank and can be neither selected nor activated.
    ListItemId id = kInvalidListItemId;
    // The row's text, drawn from the list's left edge.
    std::string text;
    // When set, the style the row is drawn in instead of the list's normal
    // one. A cursor or selected row keeps it under the highlight, as ListView
    // describes; a disabled list still mutes it.
    std::optional<Style> style;
};

// A synchronous visible-slice provider. A provider may represent millions of
// items; it must not require ListView to enumerate the model to recover an
// identity after a refresh. Async clients update their own provider and call
// ListView::model_changed() on the UI thread.
class ListModel {
public:
    // Destroying a model a ListView still borrows is the caller's error; see
    // ListView::set_model.
    virtual ~ListModel() = default;

    // The items in display order: how many there are; the item at a display
    // index in [0, item_count()); and the display index of an id, or nullopt
    // when no current item has it (ListView then drops that id from its
    // selection and cursor on model_changed()).
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
// Scrollbar resolves its own roles the same way, independently. A disabled
// list (D-076) draws its rows in "ckv.list.disabled"'s foreground, its
// cursor and selected rows on the inactive selection's background.
//
// A cursor or selected row with a style of its own (ListItem::style) keeps
// it under the highlight as CellGrid's cells do (D-067): a row with a colour
// swaps its two colours, or takes the highlight's where the swapped pair
// would not read; a row that sets no colour wears the highlight's colours
// with its own attributes. The cursor of a multi-select list is underlined
// while the list has the focus, styled or not.
class ListView : public ui::View {
public:
    // An empty tab-stop list with its vertical Scrollbar in the rightmost
    // column. A row's text stops short of that column while the bar shows,
    // clipped a whole grapheme cluster at a time. Single-select by default:
    // the cursor row is the selection and moves with it. Multi-select keeps
    // the cursor and a set of selected rows apart, Space toggling the row
    // under the cursor.
    explicit ListView(bool multi_select = false);
    // Optional alternating neutral surfaces; explicit styles and selection win.
    void set_banded_rows(bool banded) noexcept {
        if (banded_rows_ == banded) return;
        banded_rows_ = banded;
        invalidate();
    }
    bool banded_rows() const noexcept { return banded_rows_; }


    // Replaces the roles of ordinary rows and of cursor and selected rows
    // while the list has focus. A role left kInvalidRole when the view
    // attaches falls back to its standard one. Each of these setters repaints
    // when it changes a role.
    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        if (normal_role_ == normal_role && selected_role_ == selected_role) return;
        normal_role_ = normal_role;
        selected_role_ = selected_role;
        invalidate();
    }
    // The selection's appearance while the keyboard is elsewhere. Separate
    // from the two above so an existing caller that overrides only the
    // focused pair keeps working.
    void set_selected_inactive_role_override(ui::RoleId role) noexcept {
        if (selected_inactive_role_ == role) return;
        selected_inactive_role_ = role;
        invalidate();
    }
    // The role whose foreground every row takes while the list is disabled.
    void set_disabled_role_override(ui::RoleId role) noexcept {
        if (disabled_role_ == role) return;
        disabled_role_ = role;
        invalidate();
    }

    // Restyles the embedded scrollbar. It resolves its own roles, which is
    // right for a list on a document window and wrong for one on a dialog
    // surface: the bar then keeps the document colouring and reads as a
    // strip of some other window showing through the panel. A caller that
    // has already said what surface its list sits on is the only one that
    // can say what the bar should wear.
    void set_scrollbar_role_override(ui::RoleId track_role, ui::RoleId thumb_role) noexcept;

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

    // The id of the cursor row (nullopt while the list is empty), and whether
    // the item at a display index, or with an id, is in the selection. An
    // out-of-range index or the invalid id is never selected. In a
    // single-select list the cursor row is highlighted as selected but is
    // only in the selection once chosen (by a move, Space, a click or
    // set_selected).
    std::optional<ListItemId> cursor_id() const noexcept;
    bool is_selected(std::size_t index) const;
    bool is_selected_id(ListItemId id) const;
    // Single-select: selecting an item deselects every other one.
    // Multi-select: toggles independently.
    void set_selected(std::size_t index, bool selected);
    void set_selected_id(ListItemId id, bool selected);
    std::vector<std::size_t> selected_indices() const;
    const std::vector<ListItemId>& selected_ids() const noexcept { return selected_ids_; }

    // When the scrollbar is on screen. Always by default: in a list large
    // enough to be the point of its window, a permanently visible bar reads
    // as part of the frame and its absence would be the surprise. Auto suits
    // a short list presented as a small group of choices — a help topic's
    // cross-links, say — where a control that cannot do anything is the only
    // thing suggesting the list has more to it than the reader can see.
    void set_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy scrollbar_policy() const noexcept;

    // What this list is worth showing, so that a container asking how big to be
    // gets an answer about the CONTENT rather than about nothing.
    //
    // Without these a dialog built around a list is sized as though the list
    // were empty: the stock window-list dialog came out five rows tall with a
    // Close button and no room for a single entry, and every other list dialog
    // was one content change away from the same. The hints are derived from the
    // model — how many items, and how wide the widest of the ones worth
    // measuring — never from live bounds, which would feed a container's own
    // resize back into the next layout pass.
    // The widest of the first items, plus the scrollbar and a margin — the
    // list's own measure — unless set_preferred_size() named a positive
    // width, which then wins: a container that gives a list a column of a
    // stated width must not see it grow with the longest title. Either
    // way the content's changes reach the container: every model or item
    // change reports a changed hint, so a Dock or Column lays the list
    // out again rather than keeping the width it had while empty.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    // The most rows a list asks for before it would rather scroll. A list of ten
    // thousand must not ask for a window ten thousand rows tall, and a reader
    // deciding between windows wants to see a screenful at most.
    static constexpr std::size_t kPreferredVisibleRows = 10;
    // How many items are measured for the width hint. Measuring a virtual
    // provider's whole model to answer "how wide should this be" would defeat
    // the point of a provider (D-043); the first screenful is what a reader
    // sees when the dialog opens.
    static constexpr std::size_t kMeasuredItemsForWidth = 32;

    // The cursor row's display index; -1 while the list is empty. Filling an
    // empty list puts the cursor on the first row.
    int cursor() const noexcept { return cursor_; }  // display index, -1 if empty

    // Puts the cursor on `index` and scrolls it into view, selecting it too
    // in a single-select list — where "the cursor row" and "the selected row"
    // are the same idea. set_selected() alone cannot do this: it marks a row
    // as chosen but leaves the cursor where it was, so the list paints two
    // highlighted rows and the next arrow key moves from the wrong place.
    // A caller restoring a list to a known position — which topic a help
    // viewer is showing, which file a dialog reopened on — needs the cursor
    // to move, not just the selection. Out-of-range is a harmless no-op.
    void set_cursor(std::size_t index);

    // Index callbacks remain useful for compact materialized clients. Provider
    // clients should use the identity callbacks, which survive reordering.
    // The cursor moved to another row — browsing, not choosing.
    //
    // A list is very often one half of a master/detail pair, and the
    // other half has to know which row the reader is looking at before
    // they commit to it: a footer naming the current item, a status
    // hint, a pane that previews it. Activation (below) is the other
    // half of that grammar and deliberately separate — an application
    // that wants an expensive detail view to follow only a deliberate
    // choice listens to on_activate and ignores this, and one that
    // wants a cheap caption to track the highlight does the reverse.
    // MenuBar::on_highlight_changed is the same distinction for menus.
    //
    // Fires for every route that moves the cursor — keys, mouse,
    // set_cursor() — from the single assignment point, so no caller can
    // move it silently.
    std::function<void(std::size_t)> on_cursor_changed;

    // The reader chose a row: Enter on the cursor row, or a double click on
    // it (MouseEvent::click_count, which Application counts on its clock). The
    // id form runs first, then the index form. Programmatic changes never
    // activate.
    std::function<void(std::size_t)> on_activate;
    std::function<void(ListItemId)> on_activate_id;
    // The selection changed, reported with the item that changed: in a
    // single-select list the newly selected item; in a multi-select list the
    // item toggled, or the item a click made the only selected one. Fires for
    // programmatic set_selected/set_cursor as well as for the reader, but not
    // when a new model, a new item list or model_changed() drops selected
    // items. The id form runs first; the index form is skipped when the
    // id no longer resolves to an index.
    std::function<void(std::size_t)> on_selection_changed;
    std::function<void(ListItemId)> on_selection_changed_id;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    // A press on a row moves the cursor there (selecting it in a multi-select list); the
    // second press of a double click (MouseEvent::click_count) activates the row instead. The
    // vertical wheel scrolls ui::kWheelRows rows per notch and leaves the cursor.
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;
    void on_focus(const FocusEvent& event) override;

private:
    std::size_t item_count() const;
    ListItem item_at(std::size_t index) const;
    std::optional<std::size_t> index_of(ListItemId id) const;
    ListItemId id_at(std::size_t index) const;
    void move_cursor(int new_cursor, bool select_on_move);
    void ensure_cursor_visible();
    // Scrolls so the cursor shows, counting from the top: the first row when
    // the cursor fits, else just far enough to show it.
    void reveal_cursor_from_top();
    void select_only(std::size_t index);
    void toggle_selected(ListItemId id);
    bool contains_selected(ListItemId id) const noexcept;
    void notify_selection(ListItemId id);
    void resolve_model_identities();
    // The columns a row's text may use: the whole width, less the column the
    // scrollbar covers while it shows. The bar paints over that column after
    // the rows, so text drawn into it would lose whatever the bar covers —
    // the second half of a wide glyph, say.
    int text_columns() const noexcept;

    std::vector<std::string> items_;
    ListModel* model_ = nullptr;
    std::vector<ListItemId> selected_ids_;
    ListItemId cursor_id_ = kInvalidListItemId;
    // The row's text, from the model or the materialized items.
    std::string text_at(std::size_t index) const;

    int cursor_ = -1;
    // Keyboard search (D-070): letters typed within kTypeaheadWindowNanos
    // of each other form one prefix, so "sa" reaches "sample" past "parts";
    // a letter typed alone, or the same letter again, steps to the next row
    // beginning with it. The application's clock times the window, never
    // a wall clock.
    std::string typeahead_;
    std::int64_t typeahead_at_ = 0;
    bool multi_select_;

    Scrollbar* scrollbar_ = nullptr;
    // The cursor was placed before the list was first drawn. Until then every
    // size the list is given may be a container's interim pass — a Column
    // lays each item out as it is added, at its minimum — so the reveal is
    // held and made again from the top at each size, and the first frame
    // shows it against the geometry that frame actually has.
    bool reveal_pending_ = false;
    // Whether the list has been drawn: from then on a reveal scrolls the least
    // that shows the cursor, from wherever the reader left the list.
    bool drawn_ = false;

    bool banded_rows_ = false;
    ui::RoleId banded_role_ = ui::kInvalidRole;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
