---
title: ckVision Flow View
author: C. Klukas
date: 2026-08-11
format: guide
description: Wrapped styled flow content with links and inline raster atoms.
---
{% raw %}

# Flow content

`FlowView` is the read-only rich-content surface for document previews,
transcripts, reports, and other application-defined content. It is not a
Markdown parser or a document model. Clients build a `FlowDocument` value from
their own semantic model and give it to the view.

Each `FlowBlock` contains `FlowText`, `FlowLineBreak`, and `FlowImage` atoms.
Text wraps by grapheme width at the view's current width; styled runs and links
remain intact. `Tab` and `Shift+Tab` cycle links, `Enter` activates the current
link, and mouse clicks activate the linked run. Arrow, page, home, end, and
wheel input scroll ordinary display rows.

```cpp
widgets::FlowDocument document{
    .blocks = {{.content = {
        widgets::FlowText{"Build ", Attr::Bold, std::nullopt},
        widgets::FlowText{"details", Attr{}, std::string("build://42")},
        widgets::FlowLineBreak{},
        widgets::FlowText{"The image below remains useful without graphics.", Attr{}, std::nullopt},
    }}}
};

widgets::FlowView preview;
preview.set_document(std::move(document));
preview.on_link_activate = [](const std::string& target) { /* client action */ };
```

## Inline raster atoms

`FlowImage` reserves its explicit cell extent in the document flow. The image
is emitted through `Painter::draw_image`, just like `ImageView` and `Canvas`:
it is clipped and occluded by the normal scene compositor, has a bounded cell
fallback, and appears as no raster pixels when graphics are unavailable.

```cpp
auto chart = std::make_shared<Image>(640, 240);
// The application draws chart pixels into chart.
document.blocks.push_back({{widgets::FlowImage{chart, Size{48, 12}, "[chart]"}}});
preview.set_document(std::move(document));
```

An image occupies complete flow rows rather than allowing text to wrap around
its sides. That makes resize, scrolling, fallback, and raster clipping
deterministic on every terminal size. Applications choose their own image
generation, refresh, loading placeholder, and semantic layout policy.
{% endraw %}
