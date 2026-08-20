// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/help_viewer.hpp"

#include <optional>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/list_view.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::HelpTopic;
using ckv::widgets::HelpIndexEntry;
using ckv::widgets::make_help_viewer;
using ckv::widgets::present_help_viewer;
using ckv::widgets::MemoryHelpProvider;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

MemoryHelpProvider sample_provider() {
    MemoryHelpProvider provider;
    provider.add_topic("intro", HelpTopic{"Introduction", "Welcome to the app.", {{"details", "See details"}}});
    provider.add_topic("details", HelpTopic{"Details", "More information here.", {{"intro", "Back to intro"}}});
    return provider;
}

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }
}  // namespace

// --- MemoryHelpProvider ------------------------------------------------

CK_TEST(memory_help_provider_returns_the_added_topic) {
    auto provider = sample_provider();
    const auto t = provider.topic("intro");
    CK_CHECK(t.title == "Introduction");
    CK_CHECK(t.links.size() == 1);
}

CK_TEST(memory_help_provider_returns_a_not_found_topic_for_an_unknown_key) {
    MemoryHelpProvider provider;
    const auto t = provider.topic("nonexistent");
    CK_CHECK(t.title == "Not Found");
    CK_CHECK(t.links.empty());
}

CK_TEST(memory_help_provider_index_is_sorted_and_stable) {
    MemoryHelpProvider provider;
    provider.add_topic("z", HelpTopic{"Zulu", "Last.", {}});
    provider.add_topic("a", HelpTopic{"Alpha", "First.", {}});

    const std::vector<HelpIndexEntry> expected{{"a", "Alpha"}, {"z", "Zulu"}};
    CK_CHECK(provider.index() == expected);
}

CK_TEST(memory_help_provider_keyword_search_matches_key_title_body_and_link_labels) {
    auto provider = sample_provider();

    const std::vector<HelpIndexEntry> intro{{"intro", "Introduction"}};
    const std::vector<HelpIndexEntry> details{{"details", "Details"}};
    CK_CHECK(provider.search("Welcome") == intro);
    CK_CHECK(provider.search("information") == details);
    CK_CHECK(provider.search("See details") == intro);
    CK_CHECK(provider.search("missing").empty());
}

// --- HelpViewer: construction / navigation --------------------------

CK_TEST(the_viewer_builds_successfully_for_an_existing_topic) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_an_unknown_initial_topic_shows_not_found_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryHelpProvider provider;  // empty
    auto handle = make_help_viewer(provider, "missing", f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
}

CK_TEST(back_with_empty_history_is_a_harmless_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, nullptr);
    // Exercise Back before any cross-link navigation happened; must
    // not crash, and Close must still work afterward, proving the dialog is
    // still in a valid state.
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    bool closed = false;
    window_ptr->on_closed = [&closed, previous = window_ptr->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    window_ptr->cancel_request();
    CK_CHECK(closed);
}


CK_TEST(closing_restores_focus_to_the_view_that_invoked_the_viewer) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, invoker);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(app.focused() == invoker);
}


CK_TEST(present_help_viewer_is_modeless_and_completes_on_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(
        ckv::Rect{0, 0, 80, 24}));
    auto provider = sample_provider();

    auto presentation = present_help_viewer(provider, "intro", app, *desktop, roles);
    std::optional<ckv::widgets::HelpViewerResult> completion;
    presentation.set_completion_handler([&](ckv::widgets::HelpViewerResult result) { completion = result; });

    // Help is a reference, consulted while doing the thing it is about, so it
    // never scopes input to itself: the reader can go back to their work with
    // the answer still on screen.
    CK_CHECK(!app.is_modal());
    app.dispatch(key(Key::Escape));
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == ckv::widgets::HelpViewerResult::Closed);
    CK_CHECK(completion == ckv::widgets::HelpViewerResult::Closed);
    CK_CHECK(!app.is_modal());
}

CK_TEST(a_presented_help_window_is_wide_and_deep_enough_to_show_its_topic) {
    // Regression: the viewer used to state no room for its prose, so the
    // automatic placement sized it from the links list and button row alone.
    // The result was a window around two dozen cells wide with the topic
    // text clipped out of existence — a help viewer showing no help.
    ckv::term::HeadlessTerminal term(ckv::Size{100, 30});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 100, 30}));
    auto provider = sample_provider();

    auto presentation = present_help_viewer(provider, "intro", app, *desktop, roles);
    app.step(0);
    CK_CHECK(desktop->windows().size() == 1U);
    const ckv::Rect bounds = desktop->windows()[0]->bounds();
    // Prose needs a prose column, not a button's width.
    CK_CHECK(bounds.width >= 50);
    CK_CHECK(bounds.height >= 12);
    // ...and it still fits the desktop it was placed on.
    CK_CHECK(bounds.width <= desktop->content_area().width);
    CK_CHECK(bounds.height <= desktop->content_area().height);
}

CK_TEST(a_help_window_on_a_small_terminal_is_clamped_rather_than_overflowing) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 12});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 40, 12}));
    auto provider = sample_provider();

    auto presentation = present_help_viewer(provider, "intro", app, *desktop, roles);
    app.step(0);
    const ckv::Rect bounds = desktop->windows()[0]->bounds();
    CK_CHECK(bounds.width <= 40);
    CK_CHECK(bounds.height <= 12);
    CK_CHECK(bounds.width > 0 && bounds.height > 0);
}





// --- The two-pane browser -------------------------------------------------
//
// The viewer shows every topic on the left and the current one's prose on the
// right. That is the whole point of the arrangement: the navigation surface
// stays put, so moving between topics never rearranges the thing the reader
// is navigating with.

namespace {

struct BrowserFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{110, 32}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    ckv::widgets::Desktop* desktop = nullptr;
    MemoryHelpProvider provider;

    BrowserFixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 110, 32}));
        provider.add_topic("alpha", HelpTopic{"Alpha", "About alpha.", {{"beta", "Beta"}}});
        provider.add_topic("beta", HelpTopic{"Beta", "About beta, which mentions zebra.", {}});
        provider.add_topic("gamma", HelpTopic{"Gamma", "About gamma.", {}});
    }

    std::vector<std::string> rows() {
        std::vector<std::string> out;
        const ckv::FrameView frame = app.current_frame();
        for (int y = 0; y < frame.size().height; ++y) {
            std::string row;
            for (int x = 0; x < frame.size().width; ++x) row += frame.at(ckv::Point{x, y}).grapheme();
            out.push_back(row);
        }
        return out;
    }

    bool shows(std::string_view needle) {
        for (const std::string& row : rows())
            if (row.find(needle) != std::string::npos) return true;
        return false;
    }

    // The viewer's own topic list, found by shape rather than by reaching
    // into the factory's internals.
    ckv::widgets::ListView* index_list() {
        ckv::widgets::ListView* found = nullptr;
        const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
            if (auto* const list = dynamic_cast<ckv::widgets::ListView*>(&view); list != nullptr && found == nullptr)
                found = list;
            for (const auto& child : view.children()) walk(*child);
        };
        walk(app.root());
        return found;
    }

    ckv::widgets::SearchBox* search_box() {
        ckv::widgets::SearchBox* found = nullptr;
        const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
            if (auto* const box = dynamic_cast<ckv::widgets::SearchBox*>(&view); box != nullptr && found == nullptr)
                found = box;
            for (const auto& child : view.children()) walk(*child);
        };
        walk(app.root());
        return found;
    }
};

}  // namespace

CK_TEST(the_viewer_lists_every_topic_beside_the_one_it_is_showing) {
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);

    // The prose of the current topic...
    CK_CHECK(f.shows("About alpha."));
    // ...and every topic there is, including the ones not being shown.
    CK_CHECK(f.shows("Alpha"));
    CK_CHECK(f.shows("Beta"));
    CK_CHECK(f.shows("Gamma"));
}

CK_TEST(the_current_topic_is_the_highlighted_row_in_the_index) {
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "gamma", f.app, *f.desktop, f.roles);
    f.app.step(0);

    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    // index() sorts by title: Alpha, Beta, Gamma.
    const std::vector<std::size_t> selected = list->selected_indices();
    CK_CHECK(selected.size() == 1U);
    CK_CHECK(selected[0] == 2U);
}

CK_TEST(choosing_another_topic_changes_the_prose_but_not_the_index) {
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    CK_CHECK(f.shows("About alpha."));

    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    f.app.set_focus(list);
    f.app.dispatch(key(Key::Down));  // Alpha -> Beta
    f.app.step(0);

    CK_CHECK(f.shows("About beta"));
    CK_CHECK(!f.shows("About alpha."));
    // The list itself is untouched: every topic is still offered, which is
    // what makes the arrangement navigable rather than surprising.
    CK_CHECK(f.shows("Alpha"));
    CK_CHECK(f.shows("Beta"));
    CK_CHECK(f.shows("Gamma"));
}

CK_TEST(searching_narrows_the_index_to_the_matching_topics) {
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::SearchBox* const search = f.search_box();
    CK_CHECK(search != nullptr);

    // "zebra" appears only in Beta's body, so a body match counts too.
    search->set_query("zebra");
    if (search->on_change) search->on_change("zebra");
    f.app.step(0);
    CK_CHECK(f.shows("Beta"));
    CK_CHECK(!f.shows("Gamma"));

    // Clearing restores the whole index.
    search->set_query("");
    if (search->on_change) search->on_change("");
    f.app.step(0);
    CK_CHECK(f.shows("Alpha"));
    CK_CHECK(f.shows("Beta"));
    CK_CHECK(f.shows("Gamma"));
}

CK_TEST(enter_in_the_search_box_does_not_dismiss_the_help_window) {
    // Regression: Close was marked the default button, so Window::on_key
    // claimed Enter for the whole window. Typing a query and pressing Enter —
    // the habit every search field trains — threw away the answer the reader
    // had just asked for. A viewer asks no question, so it has no affirmative
    // key.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::SearchBox* const search = f.search_box();
    CK_CHECK(search != nullptr);
    f.app.set_focus(search);

    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "z"}});
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "e"}});
    f.app.step(0);
    CK_CHECK(search->query() == "ze");

    f.app.dispatch(key(Key::Enter));
    f.app.step(0);

    CK_CHECK(!presentation.completed());
    CK_CHECK(f.desktop->windows().size() == 1U);
    // ...and the query the reader typed is still there to edit. Re-found
    // rather than reused: a closed window takes its widgets with it, and the
    // whole point is that this one did not.
    ckv::widgets::SearchBox* const surviving = f.search_box();
    CK_CHECK(surviving != nullptr);
    if (surviving != nullptr) CK_CHECK(surviving->query() == "ze");
}

CK_TEST(escape_still_closes_the_help_window) {
    // The counterpart to the above: dropping the affirmative key must not
    // cost the reader the ordinary way out. Escape belongs to the window
    // because the search box only claims it while there is a query to clear.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);

    f.app.dispatch(key(Key::Escape));
    f.app.step(0);
    CK_CHECK(presentation.completed());
}

CK_TEST(enter_on_the_index_still_opens_the_highlighted_topic) {
    // Enter was not taken away, only stopped from being intercepted: the
    // surface that holds the keyboard decides what it means.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    f.app.set_focus(list);
    list->set_cursor(1);  // index() sorts by title: Alpha, Beta, Gamma.
    f.app.dispatch(key(Key::Enter));
    f.app.step(0);

    CK_CHECK(f.shows("About beta"));
    CK_CHECK(!presentation.completed());
}

CK_TEST(the_search_box_and_the_index_it_filters_end_on_the_same_column) {
    // Regression: ListView stopped its row fill one column short of its own
    // width to "make room" for a scrollbar that paints itself and, under an
    // Auto policy with nothing to scroll, is not there at all. The dialog
    // behind showed through that column, so the search box above the index
    // ran one character further right than the list below it.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);

    ckv::widgets::SearchBox* const search = f.search_box();
    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(search != nullptr);
    CK_CHECK(list != nullptr);

    const ckv::Rect search_bounds = search->absolute_bounds();
    const ckv::Rect list_bounds = list->absolute_bounds();
    CK_CHECK(search_bounds.x == list_bounds.x);
    CK_CHECK(search_bounds.width == list_bounds.width);

    // Laid out alike is not drawn alike; the painted cells are what a reader
    // sees. Every column of a list row carries the list's own background.
    const ckv::FrameView frame = f.app.current_frame();
    const ckv::Style row_style = frame.at(ckv::Point{list_bounds.x, list_bounds.y}).style();
    for (int x = list_bounds.x; x < list_bounds.x + list_bounds.width; ++x)
        CK_CHECK(frame.at(ckv::Point{x, list_bounds.y}).style().bg == row_style.bg);
}

CK_TEST(a_search_does_not_move_the_reader_off_the_topic_they_are_reading) {
    // Filtering is about finding, not going. The prose pane must stay where
    // it was until the reader actually chooses something.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::SearchBox* const search = f.search_box();
    CK_CHECK(search != nullptr);

    search->set_query("gamma");
    if (search->on_change) search->on_change("gamma");
    f.app.step(0);
    CK_CHECK(f.shows("About alpha."));
}

CK_TEST(a_topics_curated_cross_references_are_named_in_its_prose) {
    // The links still carry the author's judgement about what relates to
    // what; they are prose now because every topic they name is already in
    // the pane on the left.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    CK_CHECK(f.shows("See also:"));

    // A topic with no cross-references says nothing about them.
    auto second = present_help_viewer(f.provider, "gamma", f.app, *f.desktop, f.roles);
    f.app.step(0);
    CK_CHECK(f.shows("About gamma."));
}

CK_TEST(the_index_cursor_starts_on_the_topic_being_shown) {
    // Regression: the current topic was marked selected but the cursor stayed
    // on the first row, so two rows looked highlighted and the first arrow
    // key jumped to the second topic instead of the one after the current.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "beta", f.app, *f.desktop, f.roles);
    f.app.step(0);

    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    CK_CHECK(list->cursor() == 1);  // Alpha, Beta, Gamma
    CK_CHECK(f.shows("About beta"));

    f.app.set_focus(list);
    f.app.dispatch(key(Key::Down));
    f.app.step(0);
    CK_CHECK(f.shows("About gamma."));  // the topic after Beta, not before it
}

CK_TEST(the_help_window_is_resizable_and_its_panes_grow_with_it) {
    // A help window shows a document beside an index of documents; how much
    // of either to have on screen is the reader's decision.
    BrowserFixture f;
    auto presentation = present_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::Window* const window = f.desktop->windows()[0];
    CK_CHECK(window->resizable());

    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    const ckv::Rect before_list = list->bounds();
    const ckv::Rect before_window = window->bounds();

    window->set_bounds(ckv::Rect{before_window.x, before_window.y, before_window.width + 10,
                                 before_window.height + 6});
    f.app.step(0);

    // The topic list takes the extra height; it is what fills the left pane.
    CK_CHECK(list->bounds().height > before_list.height);
    // ...and the prose still fits inside the window rather than overflowing.
    CK_CHECK(f.shows("About alpha."));
}

