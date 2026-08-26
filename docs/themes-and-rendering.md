---
title: ckVision Themes and Rendering
author: C. Klukas
date: 2026-08-09
format: report
description: Use roles and themes while relying on ckVision's deterministic paint pipeline.
---

# Themes and rendering

Themes are role-based. Widgets resolve named roles from their attached
context, and applications can begin with `ui::make_classic_theme` before
supplying a different `Theme`. A subtree or window can override the roles it
uses without changing process-wide state.

The Gallery shows both document and dialog frame families in one real frame:

![Gallery with themed windows](generated/screenshots/gallery-initial.svg)

```text
widget state changes
  -> affected View invalidates
  -> Application composes the View tree
  -> Presenter emits changed terminal cells and raster overlays
```

You normally set a theme once during application construction, as every
example does:

<!-- ckvision-snippet source="examples/gallery/gallery_app.cpp" lines="23-28" -->
```cpp
GalleryApp::GalleryApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));
```
<!-- /ckvision-snippet -->

The result is deterministic: clients supply services rather than having the UI
read wall-clock, locale, environment, or filesystem state behind their backs.
Paint calls are coordinated by `Application::step()`/`run()`; a client should
change widget state, not issue terminal escape sequences or manual repaint
loops. [Graphics](graphics.md) explains the one capability-sensitive surface.

The built-in themes distinguish two kinds of keyboard cue automatically.
`ckv.hotkey` accents a command chord in StatusLine and an `&` mnemonic in a
menu (red in Classic). `ckv.label.mnemonic` accents focus-changing labels,
buttons, checkboxes, and radio options inside dialogs (yellow in Classic).
Both preserve the receiving surface's background, so a highlighted menu row or
focused dialog control stays visually coherent.

Classic keeps action buttons and choice groups deliberately distinct: buttons
use green faces, while radio and check-group selection surfaces use cyan. This
prevents a persistent selection from reading as an immediate action.
Editable one-line fields use the dark-blue input surface, with a light-cyan
focused value, so form entry stays distinct from both static dialog text and
the cyan choice surface.

`ckv.help.text` is the separate reference-document role: Classic uses yellow
text on a blue window surface. This keeps preformatted help legible without
changing ordinary static dialog text.

`TextEditor` uses the standard `ckv.editor.*` family: text, gutter,
selection, search, and `ckv.editor.syntax.{plain,keyword,type,property,string,
number,comment,command,operator,escape,error}`. Classic, Dark, Light, Mono,
and High Contrast all define these roles explicitly. The paint precedence is selection first,
then search, then syntax/base text; a selected match therefore remains legible
under every built-in scheme. Applications may override any of these semantic
roles in an ordinary `Theme` without changing editor behavior or using a
global palette.

## What a colour is, and what an underline looks like

A `Color` is one of three things, and the difference is kept rather than
collapsed: the terminal's own foreground or background, an entry in the
palette named by its index, or a specific 24-bit colour. Themes name concrete
colours; an embedded terminal's child usually names palette entries. Keeping
the index is what lets a palette be re-themed later — "the palette's red" is a
different fact from "this particular red" — and it is what lets the index
reach the outer terminal unchanged, so a reader's own theme applies to it.

`core/palette.hpp` is the one place that turns an index into channels, for the
things that genuinely need pixels: dimming a shadow, writing an SVG, or
quantising for a host with fewer colours. `resolved_color(colour, fallback)`
is the call; everything in between carries the index.

`Style::underline` refines an underline that is being drawn — straight,
double, curly, dotted or dashed — and `Style::underline_color` gives the rule a
colour of its own, defaulting to "follow the text". Both are meaningful only
while `Attr::Underline` is set, and clearing the underline restores them, so
two cells that look alike compare alike. A shape reaches the screen only where
the host declares `underline_styles`; without it every shape degrades to the
plain rule, which still reads as emphasis.
