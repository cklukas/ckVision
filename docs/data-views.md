---
title: ckVision Data Views
author: C. Klukas
date: 2026-08-11
format: guide
description: Provider-backed lists and tables with stable identities and typed editing.
---
{% raw %}

# Provider-backed data views

`ListView` and `Table` can display a small materialized value list, but their
primary interface is now a caller-owned provider. The widget never copies a
large result set and never treats a display offset as persistent identity.

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

## Ownership and threading

Both widgets borrow their provider. The provider must outlive the view or be
replaced with `clear_model()` first. Calls occur on the owning UI thread.
This leaves cache size, cancellation, query scheduling, transaction policy,
and domain types to applications while retaining deterministic interaction and
testability in ckVision.
{% endraw %}
