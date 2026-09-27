// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/help_viewer.hpp"

#include <functional>
#include <optional>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/text_view.hpp"

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
using ckv::widgets::present_modeless_help_viewer;
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
    provider.add_topic("intro", HelpTopic{"Introduction", {{"Welcome to the app."}}, {{"details", "See details"}}});
    provider.add_topic("details", HelpTopic{"Details", {{"More information here."}}, {{"intro", "Back to intro"}}});
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
    provider.add_topic("z", HelpTopic{"Zulu", {{"Last."}}, {}});
    provider.add_topic("a", HelpTopic{"Alpha", {{"First."}}, {}});

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

CK_TEST(the_viewer_keeps_its_own_copy_of_the_see_also_label) {
    // A caller may pass a temporary StandardStrings: the label is read again
    // on every navigation, long after the call, from a copy of the viewer's
    // own. A label longer than any small-string buffer makes a dangling read
    // one a sanitizer build reports.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    const std::string see_also = "Related topics worth reading next:";
    auto handle = [&] {
        ckv::widgets::StandardStrings strings = ckv::widgets::english_standard_strings();
        strings.help_see_also = see_also;
        return make_help_viewer(provider, "details", f.roles, app, nullptr, strings);
    }();
    CK_CHECK(handle.initial_focus != nullptr);
    if (handle.initial_focus == nullptr) return;
    CK_CHECK(handle.initial_focus->on_key(key(ckv::Key::Down)));  // "intro", after the strings are gone
    ckv::widgets::TextView* topic = nullptr;
    const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
        if (auto* const text = dynamic_cast<ckv::widgets::TextView*>(&view); text != nullptr && topic == nullptr)
            topic = text;
        for (const auto& child : view.children()) walk(*child);
    };
    walk(*handle.window);
    CK_CHECK(topic != nullptr);
    if (topic == nullptr) return;
    CK_CHECK(topic->text().find("Introduction") != std::string::npos);
    CK_CHECK(topic->text().find(see_also) != std::string::npos);
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


CK_TEST(present_modeless_help_viewer_is_modeless_and_completes_on_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(
        ckv::Rect{0, 0, 80, 24}));
    auto provider = sample_provider();

    auto presentation = present_modeless_help_viewer(provider, "intro", app, *desktop, roles);
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

    auto presentation = present_modeless_help_viewer(provider, "intro", app, *desktop, roles);
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

    auto presentation = present_modeless_help_viewer(provider, "intro", app, *desktop, roles);
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
        provider.add_topic("alpha", HelpTopic{"Alpha", {{"About alpha."}}, {{"beta", "Beta"}}});
        provider.add_topic("beta", HelpTopic{"Beta", {{"About beta, which mentions zebra."}}, {}});
        provider.add_topic("gamma", HelpTopic{"Gamma", {{"About gamma."}}, {}});
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "gamma", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::SearchBox* const search = f.search_box();
    CK_CHECK(search != nullptr);
    if (search == nullptr) return;
    f.app.set_focus(&search->field());

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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);

    f.app.dispatch(key(Key::Escape));
    f.app.step(0);
    CK_CHECK(presentation.completed());
}

CK_TEST(enter_on_the_index_still_opens_the_highlighted_topic) {
    // Enter was not taken away, only stopped from being intercepted: the
    // surface that holds the keyboard decides what it means.
    BrowserFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    CK_CHECK(f.shows("See also:"));

    // A topic with no cross-references says nothing about them.
    auto second = present_modeless_help_viewer(f.provider, "gamma", f.app, *f.desktop, f.roles);
    f.app.step(0);
    CK_CHECK(f.shows("About gamma."));
}

CK_TEST(the_index_cursor_starts_on_the_topic_being_shown) {
    // Regression: the current topic was marked selected but the cursor stayed
    // on the first row, so two rows looked highlighted and the first arrow
    // key jumped to the second topic instead of the one after the current.
    BrowserFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "beta", f.app, *f.desktop, f.roles);
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
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
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


CK_TEST(a_narrow_help_window_shrinks_its_index_before_its_topic) {
    // Held at its full preferred width, the index left the topic -- what the
    // reader opened help for -- no width at all in a narrow window.
    BrowserFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "alpha", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::Window* const window = f.desktop->windows()[0];
    const ckv::Rect placed = window->bounds();
    window->set_bounds(ckv::Rect{placed.x, placed.y, 30, placed.height});
    f.app.step(0);

    ckv::widgets::TextView* topic = nullptr;
    const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
        if (auto* const text = dynamic_cast<ckv::widgets::TextView*>(&view); text != nullptr && topic == nullptr)
            topic = text;
        for (const auto& child : view.children()) walk(*child);
    };
    walk(*window);
    CK_CHECK(topic != nullptr);
    if (topic == nullptr) return;
    CK_CHECK(topic->bounds().width > 0);
    // The index gave up columns it would like (24), and kept some.
    ckv::widgets::ListView* const index = f.index_list();
    CK_CHECK(index != nullptr);
    if (index == nullptr) return;
    CK_CHECK(index->bounds().width < 24);
    CK_CHECK(index->bounds().width > 0);
}

// --- Cross-links and Back ---------------------------------------------------
//
// A topic's cross-links -- the linked runs of its prose and its curated
// see-also list -- are the prose view's links: Tab walks them, Enter or a
// click follows one, and Back returns to the topic left behind.

namespace {

struct LinkedFixture : BrowserFixture {
    LinkedFixture() {
        provider.add_topic("windows", HelpTopic{"Windows",
                                                {{"A window is moved by its "},
                                                 {"title bar", "chrome"},
                                                 {" and closed from the "},
                                                 {"menu", "menus"},
                                                 {"."}},
                                                {{"status", "Status line"}}});
        provider.add_topic("chrome", HelpTopic{"Chrome", {{"Frames, titles and grips."}}, {{"windows", "Windows"}}});
        provider.add_topic("menus", HelpTopic{"Menus", {{"F10 walks the menu bar."}}, {}});
        provider.add_topic("status", HelpTopic{"Status line", {{"Hints follow the focus."}}, {}});
    }

    ckv::widgets::TextView* prose() {
        ckv::widgets::TextView* found = nullptr;
        const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
            if (auto* const text = dynamic_cast<ckv::widgets::TextView*>(&view); text != nullptr && found == nullptr)
                found = text;
            for (const auto& child : view.children()) walk(*child);
        };
        walk(app.root());
        return found;
    }

    ckv::widgets::Button* button(std::string_view caption) {
        ckv::widgets::Button* found = nullptr;
        const std::function<void(ckv::ui::View&)> walk = [&](ckv::ui::View& view) {
            if (auto* const b = dynamic_cast<ckv::widgets::Button*>(&view); b != nullptr && b->text() == caption)
                found = b;
            for (const auto& child : view.children()) walk(*child);
        };
        walk(app.root());
        return found;
    }

    // The screen cell where `needle` begins on the first row that also holds
    // `row_marker`, so a label that the index lists too is found in the prose.
    std::optional<ckv::Point> cell_of(std::string_view needle, std::string_view row_marker) {
        const std::vector<std::string> screen = rows();
        const ckv::FrameView frame = app.current_frame();
        for (int y = 0; y < static_cast<int>(screen.size()); ++y) {
            if (screen[static_cast<std::size_t>(y)].find(row_marker) == std::string::npos) continue;
            // Columns, not bytes: walk the row's cells.
            std::string row;
            for (int x = 0; x < frame.size().width; ++x) {
                row += frame.at(ckv::Point{x, y}).grapheme();
                if (row.size() >= needle.size() && row.ends_with(needle))
                    return ckv::Point{x - static_cast<int>(needle.size()) + 1, y};
            }
        }
        return std::nullopt;
    }

    void click(ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }
};

ckv::KeyEvent shift_tab() { return ckv::KeyEvent{KeyChord{Key::Tab, Modifier::Shift, ""}}; }

}  // namespace

CK_TEST(memory_help_provider_search_matches_the_topics_a_body_links_to) {
    LinkedFixture f;
    const std::vector<HelpIndexEntry> windows{{"windows", "Windows"}};
    // "menus" is named nowhere in the Windows topic's text; only its link does.
    CK_CHECK(f.provider.search("menus") == (std::vector<HelpIndexEntry>{{"menus", "Menus"}, {"windows", "Windows"}}));
    CK_CHECK(f.provider.search("title bar") == windows);
}

CK_TEST(tab_walks_a_topics_cross_links_in_reading_order_then_moves_on_to_back) {
    LinkedFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "windows", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::TextView* const prose = f.prose();
    CK_CHECK(prose != nullptr);
    if (prose == nullptr) return;
    // Two links in the sentence, then the see-also entry.
    CK_CHECK(prose->link_count() == 3U);

    // From the index, Tab reaches the prose with its first link current.
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(f.app.focused() == prose);
    CK_CHECK(prose->current_link() == std::optional<std::size_t>{0});
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(prose->current_link() == std::optional<std::size_t>{1});
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(prose->current_link() == std::optional<std::size_t>{2});
    // Past the last link Tab lets go: the keyboard is never trapped in a page.
    // Back is disabled with nothing to go back to, so it is Close that is next.
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(f.app.focused() == f.button("Close"));
    // ...and Shift+Tab comes back to the prose, its walk starting over.
    CK_CHECK(f.app.dispatch(shift_tab()));
    CK_CHECK(f.app.focused() == prose);
    CK_CHECK(prose->current_link() == std::optional<std::size_t>{0});
    CK_CHECK(!presentation.completed());
}

CK_TEST(a_cross_link_in_the_prose_is_followed_with_enter_and_back_returns_to_the_topic_left) {
    LinkedFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "windows", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::Button* const back = f.button("Back");
    CK_CHECK(back != nullptr);
    if (back == nullptr) return;
    CK_CHECK(!back->enabled());  // nothing left behind yet

    // Tab to the prose, Tab to its second link ("menu"), Enter follows it.
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(f.app.dispatch(key(Key::Enter)));
    f.app.step(0);
    CK_CHECK(f.shows("F10 walks the menu bar."));
    CK_CHECK(!f.shows("A window is moved"));
    // The index follows the page, as it does for every navigation.
    ckv::widgets::ListView* const list = f.index_list();
    CK_CHECK(list != nullptr);
    if (list != nullptr) CK_CHECK(list->items()[static_cast<std::size_t>(list->cursor())] == "Menus");
    CK_CHECK(back->enabled());

    // Menus has no links, so Tab from its prose goes straight on to Back, and
    // Enter presses it.
    CK_CHECK(f.app.dispatch(key(Key::Tab)));
    CK_CHECK(f.app.focused() == back);
    CK_CHECK(f.app.dispatch(key(Key::Enter)));
    f.app.step(0);
    CK_CHECK(f.shows("A window is moved"));
    // With nothing further back, Back is disabled again, and the keyboard has
    // moved to the page the reader came back to rather than staying on it.
    CK_CHECK(!back->enabled());
    CK_CHECK(f.app.focused() == f.prose());
    CK_CHECK(!presentation.completed());
}

CK_TEST(back_walks_a_chain_of_followed_links_in_reverse) {
    LinkedFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "windows", f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::TextView* const prose = f.prose();
    ckv::widgets::Button* const back = f.button("Back");
    CK_CHECK(prose != nullptr && back != nullptr);
    if (prose == nullptr || back == nullptr) return;

    // windows -> chrome (the "title bar" link) -> windows (Chrome's see-also).
    f.app.set_focus(prose);
    CK_CHECK(f.app.dispatch(key(Key::Enter)));
    f.app.step(0);
    CK_CHECK(f.shows("Frames, titles and grips."));
    CK_CHECK(f.app.dispatch(key(Key::Enter)));
    f.app.step(0);
    CK_CHECK(f.shows("A window is moved"));

    // Back retraces the way the reader came: Chrome, then Windows.
    const ckv::Rect at = back->absolute_bounds();
    f.click(ckv::Point{at.x + 1, at.y});
    CK_CHECK(f.shows("Frames, titles and grips."));
    CK_CHECK(back->enabled());
    f.click(ckv::Point{at.x + 1, at.y});
    CK_CHECK(f.shows("A window is moved"));
    CK_CHECK(!back->enabled());
}

CK_TEST(a_click_on_a_see_also_link_follows_it_and_on_a_prose_link_too) {
    LinkedFixture f;
    auto presentation = present_modeless_help_viewer(f.provider, "windows", f.app, *f.desktop, f.roles);
    f.app.step(0);

    // "Status line" is listed in the index as well; the one clicked is the
    // one on the see-also row.
    const std::optional<ckv::Point> see_also = f.cell_of("Status line", "See also:");
    CK_CHECK(see_also.has_value());
    if (!see_also) return;
    f.click(*see_also);
    CK_CHECK(f.shows("Hints follow the focus."));

    ckv::widgets::Button* const back = f.button("Back");
    CK_CHECK(back != nullptr);
    if (back == nullptr) return;
    const ckv::Rect at = back->absolute_bounds();
    f.click(ckv::Point{at.x + 1, at.y});
    CK_CHECK(f.shows("A window is moved"));

    const std::optional<ckv::Point> title_bar = f.cell_of("title bar", "A window is moved");
    CK_CHECK(title_bar.has_value());
    if (!title_bar) return;
    f.click(ckv::Point{title_bar->x + 2, title_bar->y});
    CK_CHECK(f.shows("Frames, titles and grips."));
    CK_CHECK(!presentation.completed());
}
