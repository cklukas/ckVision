<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Provider data views and flow content architecture

## Purpose

This plan promotes three reusable client-library capabilities:

1. Provider-backed `ListView` and `Table` for data sets that must not be
   materialized in a widget.
2. Typed, in-place table editing with identity-stable cursor and selection.
3. `FlowView`, a wrapped, styled, read-only flow surface with inline raster
   content and a mandatory cell fallback.

The widgets are deliberately not spreadsheet, database, Markdown, chat, or
PDF widgets. Those applications retain their semantic models and compose the
controls below.

## Provider contract

Providers are caller-owned objects and are synchronously queried only for the
visible slice. Item and row identities are opaque values supplied by the
provider; no view stores a display index as durable state. Providers offer a
reverse lookup for an identity after their ordering or membership changes.

The framework does not prescribe caching, paging, storage, or worker use. A
client updates its provider on the UI thread and explicitly calls
`model_changed()`. A background worker reaches that thread through
`Application::post()`. This gives async clients a clear ownership boundary and
keeps headless testing deterministic.

The `Table` asks its provider to order rows and to commit an edit. It never
sorts a provider by formatted display text and never assumes that a successful
edit is an in-memory assignment. The provider may reject an edit with a
diagnostic; the edit remains active until the user cancels or supplies an
acceptable value.

## Stable interaction state

List and table cursor/selection values use item/row identities. On
`model_changed()`, a view asks the provider whether each identity still exists
and resolves its current display position. A surviving identity remains
selected even when sort, filter, page, or refresh moves it. A removed identity
is discarded. The resulting callback sequence is deterministic and is emitted
only for a real identity-state change.

## Flow layout

`FlowDocument` is a value sequence of blocks. A block contains styled text,
links, explicit line breaks, and inline image atoms. The layout consumes
graphemes at the current content width, never splits a wide grapheme, and uses
stable logical display rows for vertical scrolling. Images carry their cell
extent; text wraps around neither side of an image, so a raster atom occupies a
complete run of layout rows. Its fallback is always painted before its raster
region is registered through `Painter::draw_image`.

The widget owns focusable link navigation and ordinary keyboard/mouse
scrolling. Clients own semantic parsing, streaming update policy, image
creation, link actions, and any message/document chrome. A client may replace
one numbered block when its semantic document changes in place; out-of-range
replacement is rejected without changing the document. The view preserves
block order and ordinary scroll position (subject to its normal range clamp),
then rebuilds derived layout and link navigation from the new value. Replacing
the final block after a layout has been realized reflows only that block; a
replacement before following blocks correctly rebuilds their dependent layout.

## Required evidence

- Provider calls are limited to visible data and identity resolution; a
  million-item test double proves no materialization path.
- Sort/filter/refresh preserve surviving cursor and multi-selection identities
  and clear removed identities.
- Typed edits cover valid conversion, provider rejection, cancellation, and
  commit without a stale selection.
- Flow layout covers wrapping, Unicode width, links, scroll, clipping, inline
  raster fallback, decoded raster output, and a no-graphics frame.
- Public reference documentation and a runnable example demonstrate the
  intended application composition without application-specific models.
