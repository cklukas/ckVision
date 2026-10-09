// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The appearance matrix (WP-38): every drawn element of the public catalog,
// in each state it can be seen in, under every built-in scheme. Each entry
// here is a specimen — a small screen built through public API only — and
// generate_appearance_matrix renders every specimen × state × scheme to a
// pinned golden. tests/check_appearance_matrix.py then proves the set is
// complete: every public view type is an element or an explained exemption,
// every element has the states its traits require, and states and schemes
// that ought to look different really do.
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::docgen::appearance {

enum class Scheme { Classic, Dark, Light, Mono };
inline constexpr std::array kSchemes{Scheme::Classic, Scheme::Dark, Scheme::Light, Scheme::Mono};
std::string_view scheme_name(Scheme scheme) noexcept;

// Which graphics profile the specimen's terminal declares. A raster-bearing
// element is captured under both; everything else under None.
enum class Graphics { None, Sixel };

// One screen: a headless terminal, an Application in the requested scheme,
// and a Desktop to put windows on.
class Stage {
public:
    Stage(Size screen, Scheme scheme, Graphics graphics);

    ui::Application& app() noexcept { return app_; }
    widgets::Desktop& desktop() noexcept { return *desktop_; }
    term::HeadlessTerminal& terminal() noexcept { return terminal_; }
    const ui::StandardRoles& roles() const noexcept { return roles_; }

    // A window dressed in the dialog roles — where form controls belong —
    // and its content view.
    ui::View& dialog(Rect bounds, std::string title = {});
    // A plain document window and its content view.
    ui::View& document(Rect bounds, std::string title = {});
    // A window whose content is `content`, added through add_window().
    widgets::Window& window(Rect bounds, std::string title, std::unique_ptr<ui::View> content,
                            bool dialog_roles);
    // Adds `window` to the desktop, which makes it the active one. It opens
    // with nothing focused, whatever the activation carried into it: only
    // focus() gives a specimen the keyboard.
    widgets::Window& add_window(std::unique_ptr<widgets::Window> window);

    // Adds `view` to `parent` at `bounds` and returns it.
    template <class T>
    T& place(ui::View& parent, Rect bounds, std::unique_ptr<T> view) {
        view->set_bounds(bounds);
        T& placed = *view;
        parent.add_child(std::move(view));
        return placed;
    }

    // Gives `view` the keyboard. A specimen that asks to be shown focused
    // and cannot be is a broken specimen, so this refuses loudly.
    void focus(ui::View& view);

    // Composes the frame to capture.
    void step();

private:
    term::HeadlessTerminal terminal_;
    ManualClock clock_;
    ui::Application app_;
    ui::StandardRoles roles_{};
    widgets::Desktop* desktop_ = nullptr;
};

// What an element is decides which states it must be seen in (the checker
// derives the list; see the internal plans).
struct Traits {
    bool focusable = false;  // takes the keyboard: `focused` is required and must show
    bool control = false;    // sets a value of the reader's: also `disabled`, which must show
    bool text = false;     // shows caller text: `wide` and `narrow` are required
    bool window = false;   // window chrome: `active` and `inactive` are required
    bool raster = false;   // draws pictures: `sixel` and `no-graphics` are required
};

struct State {
    std::string name;
    Size screen;
    Graphics graphics = Graphics::None;
    std::function<void(Stage&)> build;
    // Non-empty for a state drawn the same under every scheme by design, and
    // why. The checker then requires the four schemes to be identical rather
    // than different.
    std::string fixed_reason;
};

struct Element {
    std::string name;    // the public type, or a standard dialog's name
    std::string header;  // the installed header that declares it
    Traits traits;
    std::vector<State> states;
};

// A public view type that is deliberately not a specimen of its own, and why.
struct Exemption {
    std::string name;
    std::string reason;
};

struct Catalog {
    std::vector<Element> elements;
    std::vector<Exemption> exemptions;

    Element& element(std::string name, std::string header, Traits traits) {
        elements.push_back(Element{std::move(name), std::move(header), traits, {}});
        return elements.back();
    }
};

// Adds one state to `element`.
inline void state(Element& element, std::string name, Size screen, std::function<void(Stage&)> build,
                  Graphics graphics = Graphics::None) {
    element.states.push_back(State{std::move(name), screen, graphics, std::move(build), {}});
}

// Adds a state that no scheme may change, with the reason it is fixed.
inline void fixed_state(Element& element, std::string name, Size screen, std::function<void(Stage&)> build,
                        std::string reason) {
    element.states.push_back(State{std::move(name), screen, Graphics::None, std::move(build), std::move(reason)});
}

// Text used by every `wide` state: CJK, an emoji presentation sequence, and
// a combining mark, so double-width clusters and zero-width marks are both
// exercised at once.
inline constexpr std::string_view kWideText = "表示 🙂 été";

void add_progress_task_specimens(Catalog& catalog);
void add_control_specimens(Catalog& catalog);
void add_data_specimens(Catalog& catalog);
void add_chrome_specimens(Catalog& catalog);
void add_component_specimens(Catalog& catalog);
void add_media_specimens(Catalog& catalog);

}  // namespace ckv::docgen::appearance
