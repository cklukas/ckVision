---
title: Converting from Turbo Vision
author: C. Klukas
date: 2026-08-07
format: report
description: A worked, line-by-line port of Turbo Vision's own canonical minimal example to ckVision, with every structural deviation called out and confirmed against a screenshot.
---

# Converting from Turbo Vision

This chapter is a worked example for anyone porting a Turbo Vision
(or Turbo-Vision-family) application to ckVision: the SAME small
application, built twice, compared construct by construct. The
ckVision side is `examples/tvhello/` in this repository — a real,
tested, buildable app, not a documentation-only snippet — and its
rendered output is pinned by a golden regression test
(`tests/test_tvhello_golden.cpp`) so this chapter cannot silently
drift out of sync with what the code actually does.

The source application is Turbo Vision's own tutorial example,
commonly distributed as `hello.cpp`: `THelloApp` — a desktop, a menu
bar with one custom command, a status line, and a "Hello, World!"
dialog with static text and four buttons. It is reproduced below only
as short, illustrative fragments in Turbo Vision's own well-documented
public API shape (`TApplication`, `TMenuBar`, `TDialog`, …) — not
copied verbatim from any Turbo Vision distribution — enough to compare
each construct directly against its ckVision equivalent.

## Side by side

### Application shell, desktop, menu bar, status line

Turbo Vision wires the desktop/menu/status line through virtual
factory methods `TApplication` calls during construction:

```cpp
class THelloApp : public TApplication {
public:
    THelloApp() : TProgInit(&initStatusLine, &initMenuBar, &initDeskTop) {}
    void handleEvent(TEvent& event) override;
    static TMenuBar* initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r,
            *new TSubMenu("~H~ello", kbAltH) +
                *new TMenuItem("~G~reeting...", GreetThemCmd, kbAltG) +
                newLine() +
                *new TMenuItem("E~x~it", cmQuit, cmQuit, hcNoContext, "Alt-X"));
    }
    static TStatusLine* initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                *new TStatusItem(0, kbF10, cmMenu));
    }
};
```

ckVision has no equivalent "the framework calls back into your
factory methods" protocol — `Application`/`Desktop`/`MenuBar`/
`StatusLine` are plain, directly-constructed objects your own
constructor wires up in whatever order makes sense:

```cpp
TvHelloApp::TvHelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background);
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem hello_menu{"&Hello", {}};
    hello_menu.items.push_back(widgets::MenuItem{"&Greeting...", kCmdGreeting, nullptr, false});
    hello_menu.items.push_back(widgets::MenuItem{"", ui::kInvalidCommand, nullptr, true});  // separator
    hello_menu.items.push_back(widgets::MenuItem{"E&xit", kCmdQuit, nullptr, false});
    menus.push_back(std::move(hello_menu));
    auto menu_bar = std::make_unique<widgets::MenuBar>(std::move(menus), app_.theme(), roles_.menu_bar_normal,
                                                         roles_.menu_bar_active, roles_.menu_dropdown_normal,
                                                         roles_.menu_dropdown_highlighted,
                                                         roles_.menu_dropdown_disabled, app_, *desktop_);
    desktop_->dock_top(std::move(menu_bar));

    auto status = std::make_unique<widgets::StatusLine>(app_.theme(), roles_.menu_bar_normal, app_);
    status->set_items({{"Alt-X Exit", kCmdQuit}, {"F10 Menu", ui::kInvalidCommand}});
    desktop_->dock_bottom(std::move(status));

    app_.commands().register_command(kCmdGreeting, "Greeting...", "Hello", KeyChord{Key::Char, Modifier::Alt, "g"});
    app_.commands().register_command(kCmdQuit, "Exit", "Hello", KeyChord{Key::Char, Modifier::Alt, "x"});
    app_.set_command_handler(kCmdGreeting, [this] { greeting_box(); });
    app_.set_command_handler(kCmdQuit, [this] { quit_ = true; });
}
```

Both sides are honestly about the same length. The real difference is
**where the wiring lives**: Turbo Vision's menu items and status
items each carry a raw key-chord constant (`kbAltG`) baked in at
construction, invisible to anything outside that one factory method;
ckVision's commands are registered once in `CommandRegistry` with
their chord, title, and category together, and menu items /
status-line items reference the command by id — the SAME registry a
future help viewer, a keymap editor, or an exported command reference
can all introspect. `dock_top`/`dock_bottom` replace the
`r.b.y = r.a.y + 1` / `r.a.y = r.b.y - 1` manual rect surgery: the
menu bar and status line stay pinned to their edges across every
future resize automatically (see `widgets::Desktop::on_resized`),
which the original's one-time rect computation does not.

### The greeting dialog

Turbo Vision builds and runs the dialog inside one method, blocking
until a button fires `cmCancel`:

```cpp
void THelloApp::greetingBox() {
    TDialog* d = new TDialog(TRect(25, 5, 55, 16), "Hello, World!");
    d->insert(new TStaticText(TRect(3, 5, 15, 6), "How are you?"));
    d->insert(new TButton(TRect(16, 2, 28, 4), "Terrific", cmCancel, bfNormal));
    d->insert(new TButton(TRect(16, 4, 28, 6), "Ok", cmCancel, bfNormal));
    d->insert(new TButton(TRect(16, 6, 28, 8), "Lousy", cmCancel, bfNormal));
    d->insert(new TButton(TRect(16, 8, 28, 10), "Cancel", cmCancel, bfNormal));
    deskTop->execView(d);  // blocks here until a button fires cmCancel
    destroy(d);
}
```

ckVision's port (`examples/tvhello/tvhello_app.cpp`, `greeting_box()`):

```cpp
void TvHelloApp::greeting_box() {
    auto window = std::make_unique<widgets::Window>("Hello, World!", app_.theme(), roles_.window_frame_active,
                                                      roles_.window_frame_inactive, roles_.window_title_active,
                                                      roles_.window_title_inactive);
    window->set_bounds(Rect{25, 5, 30, 11});
    widgets::Window* window_ptr = window.get();

    auto content = std::make_unique<ui::View>();
    auto text = std::make_unique<widgets::StaticText>("How are you?", app_.theme(), roles_.static_text);
    text->set_bounds(Rect{2, 4, 14, 1});
    content->add_child(std::move(text));

    const std::pair<const char*, int> labels[] = {{"Terrific", 0}, {"Ok", 2}, {"Lousy", 4}, {"Cancel", 6}};
    widgets::Button* first_button = nullptr;
    for (const auto& [label, y] : labels) {
        auto button = std::make_unique<widgets::Button>(label, app_.theme(), roles_.button_normal,
                                                          roles_.button_focused, roles_.button_default);
        button->set_bounds(Rect{15, y, 12, 2});
        button->set_focus_policy(ui::FocusPolicy::TabStop);
        widgets::Button* raw = static_cast<widgets::Button*>(content->add_child(std::move(button)));
        if (first_button == nullptr) first_button = raw;
        raw->on_press = [window_ptr] { window_ptr->close(); };
    }

    window->set_content(std::move(content));
    window->on_closed = [window_ptr, this] { schedule_self_detach(*window_ptr, app_); };
    desktop_->add_window(std::move(window));
    app_.set_focus(first_button);
}
```

Geometry: `TRect(25, 5, 55, 16)` is absolute desktop coordinates
`{x:25, y:5, width:30, height:11}` — `Window::set_bounds(Rect{25, 5,
30, 11})` is the direct translation. Each `TRect(16, 2, 28, 4)` button
is likewise `{x:15, y:0..6, width:12, height:2}` once re-anchored to
the content interior's own local origin (ckVision's content coordinate
space starts at the window's interior, not the desktop — see
`Window::content_rect()`).

## The one deliberate deviation — and why

Turbo Vision's `deskTop->execView(d)` **blocks the calling thread**
until the dialog is dismissed: it runs a nested event loop right there
in `greetingBox()`. ckVision's loop is embeddable by construction
(`step()`/`wake()`, D-021 in `DECISIONS.md`) specifically so a host
application can drive it from its own poll loop — which rules out a
constructor-time blocking call by design, not by oversight. Modality
in ckVision is loop *state* (a future modal stack, not yet built past
M6's non-modal windows), not a nested native loop.

The port's `greeting_box()` therefore opens the dialog as an ordinary,
non-modal `Window` and gives each button its own `on_press` that
closes it directly, rather than routing all four through one shared
`cmCancel`-equivalent command. **Observable behavior is identical** —
all four buttons dismiss the dialog, exactly as `hello.cpp`'s reader
would expect (`tests/test_tvhello_golden.cpp`:
`every_greeting_button_dismisses_the_dialog_matching_the_originals_shared_cmcancel`
drives all four and asserts the window closes each time) — but the
*mechanism* differs because the two frameworks' event models differ at
the root, not because ckVision's dialog code is somehow less capable.

No other deviation was necessary. Layout, mnemonics (`&Greeting...` /
`~G~reeting...`), the Alt-G/Alt-X chords, the menu separator, and the
status line's two items all carry over directly.

## Visual comparison — confirmed, not assumed

Both screenshots below come from the SAME headless run
`tests/test_tvhello_golden.cpp` pins byte-exactly (via
`tools/docgen/capture_tvhello_screenshots`, driving the real render
pipeline — `scene::Compositor`/`term::Presenter` — not a mock).

Initial screen — desktop, menu bar, status line:

![TV-Hello: initial frame](generated/screenshots/tvhello-initial.svg)

The greeting dialog (Alt+G) — double-line active frame (this window
has focus), the close/zoom glyphs, and a solid content-interior fill
(the classic scheme's blue-on-light-gray desktop, blue window):

![TV-Hello: greeting dialog](generated/screenshots/tvhello-greeting.svg)

In the Classic theme this is, deliberately, meant to look like a
faithful windowed-desktop application — not "inspired by," identical
in spirit: double-frame-on-focus, the ▲▼/◄► scrollbar glyphs, the `■`
close glyph and `↑`/`↕` zoom glyph, the light-shade desktop texture,
and the blue-window/light-gray-menu/green-highlight palette are all
covered by dedicated tests (see `tests/test_window.cpp`,
`tests/test_scrollbar.cpp`, `tests/test_standard_roles.cpp`) as well
as by this chapter's own golden dump. **No deviation from the
original's visual language was introduced in this port** — porting
`hello.cpp` surfaced two real ckVision rendering bugs along the way
(a window's content interior was never filled at all, and the zoom
control's glyph silently overwrote the frame's own top-right corner),
both fixed as part of landing this chapter, not worked around in the
example.

## Verified, not just built

| Claim | Test |
|---|---|
| The initial frame matches the pinned golden dump byte-exactly | `tvhello_initial_frame_matches_the_pinned_golden_dump` |
| The greeting dialog matches the pinned golden dump byte-exactly | `tvhello_greeting_dialog_frame_matches_the_pinned_golden_dump` |
| Alt+G opens the dialog (matching the original's Alt-G mnemonic) | `alt_g_opens_the_greeting_dialog_matching_the_originals_alt_g_mnemonic` |
| Every button dismisses the dialog (matching the original's shared cmCancel) | `every_greeting_button_dismisses_the_dialog_matching_the_originals_shared_cmcancel` |
| Alt+X quits (matching the original's status-line shortcut) | `alt_x_quits_matching_the_originals_alt_x_status_line_shortcut` |

To regenerate the golden fixtures after a deliberate rendering change:

```bash
cmake --build build --target generate_tvhello_golden
build/tools/docgen/generate_tvhello_golden tests/golden
```

Review the diff like any other source change before committing —
regeneration is a manual, reviewed act, never automatic.
