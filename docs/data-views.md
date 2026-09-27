---
title: ckVision Data Views
author: C. Klukas
date: 2026-08-11
format: guide
description: Provider-backed lists, trees, and tables with stable identities and typed editing.
---

# Provider-backed data views

`ListView`, `TreeView`, and `Table` can display compact materialized values,
but their scalable interface is a caller-owned provider. The widget never
copies a large result set and never treats a display offset as persistent
identity.

## List providers

Implement `ListModel` for the application's ordered view. `item_at()` is called
only for visible rows; `index_of()` is the required reverse lookup that lets the
view keep its cursor and selections through sorting, filtering, paging, or a
refresh. `ListItemId` is opaque to ckVision and must be non-zero and stable for
as long as the underlying item exists.

```cpp
class SearchResults final : public widgets::ListModel {
public:
    std::size_t item_count() const override;
    widgets::ListItem item_at(std::size_t display_index) const override;
    std::optional<std::size_t> index_of(widgets::ListItemId id) const override;
    std::optional<std::size_t> find_prefix(std::string_view folded, std::size_t after) const override;
};

SearchResults results;
widgets::ListView list;
list.set_model(results);                 // results outlives list
list.on_activate_id = [](widgets::ListItemId id) { /* open result */ };
```

When the application changes its result ordering or cache, it does so on the
UI thread and then calls `list.model_changed()`. A surviving id remains the
cursor/selection. A removed id is cleared. A background worker updates the
model only through `Application::post()`; providers never call a view directly
from a worker.

`set_items()` remains a compact convenience for small static menus and lists.
It uses deterministic internal ids, but applications that need stable identity
across a refresh should use `ListModel`.

## Tree providers

`TreeModel` represents a stable hierarchy without materializing a second tree
inside the widget. It supplies root, parent, child, and reverse-index lookups;
`TreeView` owns expansion, cursor, and selection by `TreeItemId`. This lets a
model preserve application identity across refreshes while the view resolves
only the expanded path needed for a visible row.

```cpp
class DirectoryTree final : public widgets::TreeModel {
public:
    std::size_t root_count() const override;
    widgets::TreeItemId root_id_at(std::size_t root_index) const override;
    std::optional<std::size_t> root_index_of(widgets::TreeItemId id) const override;
    std::optional<widgets::TreeItemId> parent_id_of(widgets::TreeItemId id) const override;
    std::size_t child_count(widgets::TreeItemId parent) const override;
    widgets::TreeItemId child_id_at(widgets::TreeItemId parent, std::size_t child_index) const override;
    std::optional<std::size_t> child_index_of(widgets::TreeItemId parent,
                                               widgets::TreeItemId child) const override;
    std::optional<widgets::TreeItem> item(widgets::TreeItemId id) const override;
};

DirectoryTree directories;
widgets::TreeView tree;
tree.set_model(directories);            // directories outlives tree
tree.on_selection_changed_id = [](widgets::TreeItemId id) { /* show detail */ };
```

Every id is non-zero, unique, and stable while its item exists. Calling
`tree.reveal_and_select(id)` opens the required ancestors without synthesizing
input. After a model changes, call `tree.model_changed()` on the UI thread: a
surviving selected or expanded id remains in place; a removed id is cleared.
`set_item_expanded()` sets view-owned expansion for a known item. An item with
`children_known == false` exposes an expander and emits
`on_expand_request_id` once; the application performs any work, publishes its
new hierarchy, and calls `model_changed()`.

The model is queried only for visible item content plus the small chain of
root/parent/child-index lookups needed to resolve state. A provider can page or
cache its own backing data, but it must not start work, access services, or
call a view from another thread. `set_roots()` remains the simple API for
small, static `TreeNode` value trees.

## Table providers

`TableModel` separates application data from presentation. Its rows have
stable `TableRowId`s; a `TableCellRef` combines that id with a column index.
Cells carry a portable typed `CellValue`, an optional display override, an
optional style, and an editability flag. A column declares the expected edit
type. The framework performs canonical parsing for text, Boolean, integer, and
real values, while the provider owns business validation and persistence.

```cpp
class Orders final : public widgets::TableModel {
public:
    std::size_t row_count() const override;
    widgets::TableRowId row_id_at(std::size_t display_index) const override;
    std::optional<std::size_t> index_of(widgets::TableRowId id) const override;
    widgets::TableCell cell(widgets::TableCellRef ref) const override;
    void request_sort(std::optional<std::size_t> column, bool ascending) override;
    widgets::TableEditResult commit(widgets::TableCellRef ref,
                                    const widgets::CellValue& value) override;
};

Orders orders;
widgets::Table table;
table.set_columns({{"Quantity", 10, 4, widgets::TableCellType::Integer, true}});
table.set_model(orders);
```

`F2`, `Enter`, or typing starts an edit for an editable selected cell. `Enter`
commits it and `Esc` cancels it. A rejected provider commit leaves the editor
open and exposes its diagnostic through `edit_diagnostic()`. Header selection
requests sorting from the provider; the table does not sort a large model by
formatted strings. Once the provider publishes the changed order, call
`table.model_changed()`.

The compact `set_rows()` API remains for static string tables. It is not the
right choice for paged or refreshable data.

## Cell grid providers

`CellGrid` shows a two-dimensional surface of cells — a worksheet, a query
result, a matrix — under a column header and beside a row gutter, with a
cursor, a rectangular selection, frozen leading bands and merged spans. Its
`CellGridModel` differs from the three providers above in one respect, and
deliberately (D-067): **it owns the cursor, the selection and the scroll
origin**, and the widget asks it to move them. In a grid of cells those are
what the application's commands act on, and the rules for moving them need the
application's knowledge — where a block of data ends, which rows are hidden,
how a frozen band scrolls — so a second copy in the widget would have to be
reconciled with the first on every key.

```cpp
class Worksheet final : public widgets::CellGridModel {
public:
    void set_viewport(int rows, int width) override;   // the room the grid has
    widgets::GridFrame frame() const override;         // what it shows, frozen bands first
    int column_width(widgets::GridIndex column) const override;
    int row_header_width() const override;
    std::string row_label(widgets::GridIndex row) const override;
    std::string column_label(widgets::GridIndex column) const override;
    widgets::GridCell cell(widgets::GridIndex row, widgets::GridIndex column) const override;
    widgets::GridPosition cursor() const override;
    std::optional<widgets::GridRange> selection() const override;
    void navigate(widgets::GridMove move, bool extend) override;
    void place_cursor(widgets::GridPosition at, bool extend) override;
    void scroll_by(int rows, int columns) override;
};

Worksheet sheet;
widgets::CellGrid grid;
grid.set_model(sheet);                                  // sheet outlives grid
grid.on_type_ahead = [&](const std::string& text) { /* start editing with it */ };
```

Indices are the provider's own ordered integers. The frame lists the visible
rows and columns in display order, each frozen band first; a hidden row is
simply not listed, and a span is resolved over the listed indices, so a region
whose anchor has scrolled away still covers the cells the reader can see. A
cell states only what it changes about the theme's cell style
(`GridCellStyle`: a foreground, a background, the attributes, the underline
shape), and the grid composes the cursor and the selection over it — a cell
with no colour of its own wears the role, a coloured cell keeps both colours
and swaps them, and an unreadable swap falls back to the role.

The grid tells the provider its room through `set_viewport()` before every
frame. After the provider changes anything itself — an edit, a command, a
scroll it performed — the application calls `grid.model_changed()`; after a
change the grid requested, the grid repaints on its own and reports through
`on_changed`. Enter, F2, a printable key and Delete reach the owner through
`on_activate`, `on_type_ahead` and `on_clear_request`, and are consumed only
where a handler is installed.

`MaterializedCellGridModel` holds a table of values with the generic rules —
the cursor clamps to the table, a page is the body's height, a jump reaches
the table's edge — for a grid that shows a result rather than edits a
document, and for tests.

## Ownership and threading

All three widgets borrow their provider. The provider must outlive the view or be
replaced with `clear_model()` first. Calls occur on the owning UI thread.
This leaves cache size, cancellation, query scheduling, transaction policy,
and domain types to applications while retaining deterministic interaction and
testability in ckVision.
