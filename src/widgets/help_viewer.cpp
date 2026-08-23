// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/help_viewer.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/text_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

std::string lowercase(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) out.push_back(static_cast<char>(std::tolower(ch)));
    return out;
}

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    return lowercase(haystack).find(lowercase(needle)) != std::string::npos;
}
}  // namespace

void MemoryHelpProvider::add_topic(std::string key, HelpTopic topic) { topics_[std::move(key)] = std::move(topic); }

HelpTopic MemoryHelpProvider::topic(const std::string& key) const {
    auto it = topics_.find(key);
    if (it != topics_.end()) return it->second;
    return HelpTopic{"Not Found", "No help is available for this topic.", {}};
}

std::vector<HelpIndexEntry> MemoryHelpProvider::index() const {
    std::vector<HelpIndexEntry> entries;
    entries.reserve(topics_.size());
    for (const auto& [key, topic] : topics_) entries.push_back(HelpIndexEntry{key, topic.title});
    std::sort(entries.begin(), entries.end(), [](const HelpIndexEntry& a, const HelpIndexEntry& b) {
        if (a.title != b.title) return a.title < b.title;
        return a.key < b.key;
    });
    return entries;
}

std::vector<HelpIndexEntry> MemoryHelpProvider::search(std::string_view keyword) const {
    std::vector<HelpIndexEntry> entries;
    for (const auto& [key, topic] : topics_) {
        bool match = contains_case_insensitive(key, keyword) || contains_case_insensitive(topic.title, keyword) ||
                     contains_case_insensitive(topic.body, keyword);
        for (const auto& [link_key, label] : topic.links)
            match = match || contains_case_insensitive(link_key, keyword) || contains_case_insensitive(label, keyword);
        if (match) entries.push_back(HelpIndexEntry{key, topic.title});
    }
    std::sort(entries.begin(), entries.end(), [](const HelpIndexEntry& a, const HelpIndexEntry& b) {
        if (a.title != b.title) return a.title < b.title;
        return a.key < b.key;
    });
    return entries;
}

WindowHandle make_help_viewer(const HelpProvider& provider, std::string initial_topic_key,
                               const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, const StandardStrings& strings) {
    auto window = std::make_unique<Window>(strings.help_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    // Resizable: a help window is a document, and how much of a document a
    // reader wants on screen is theirs to decide. The panes below are laid
    // out to grow with it.
    window->set_resizable(true);
    window->set_min_size(Size{40, 12});
    // Prose is held off the frame; the window works out for itself that the
    // button row below it needs no margin, its shadow being the gap already.
    window->set_content_margin(1, 1);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    // Two panes: every topic on the left, permanently, and the current one's
    // prose on the right. The navigation surface therefore never moves under
    // the reader — the whole difficulty with a viewer that showed only the
    // current topic's outgoing links was that clicking one replaced both the
    // page and the list of ways off it, so nothing on screen stayed still
    // long enough to be understood as navigation.
    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto panes = std::make_unique<Row>();
    panes->set_spacing(1);

    // Left pane: search over the index, then the index itself. Spacing zero —
    // a filter belongs against the list it filters, and a blank row between
    // them would read as a separation that isn't there.
    auto index_pane = std::make_unique<Column>();
    index_pane->set_spacing(0);

    auto search_box = std::make_unique<SearchBox>();
    auto* search_ptr =
        static_cast<SearchBox*>(index_pane->add_item(std::move(search_box), LayoutSpec{SizePolicy::Fixed, 1}));

    auto index_list = std::make_unique<ListView>(/*multi_select=*/false);
    index_list->set_scrollbar_policy(ScrollbarPolicy::Auto);
    index_list->set_preferred_size(Size{24, 12});
    auto* index_list_ptr =
        static_cast<ListView*>(index_pane->add_item(std::move(index_list), LayoutSpec{SizePolicy::Expanding, 1}));
    panes->add_item(std::move(index_pane), LayoutSpec{SizePolicy::Fixed, 1});

    auto text_view = std::make_unique<TextView>();
    // Help is prose. Prose that runs off the right edge is prose the reader
    // cannot finish, and a help window is resizable precisely so they can
    // choose how wide a line should be.
    text_view->set_wrap_mode(WrapMode::Word);  // prose
    // A viewer that is not told how much room its prose wants gets sized by
    // whatever children do ask for room, and comes out too narrow to read.
    // Nothing else can supply this: a TextView holds whatever text it is
    // later given, and placement runs before any topic has been loaded.
    // Placement clamps to the desktop and the view scrolls, so a small
    // terminal loses no content.
    text_view->set_preferred_size(Size{54, 12});
    auto* text_view_ptr =
        static_cast<TextView*>(panes->add_item(std::move(text_view), LayoutSpec{SizePolicy::Expanding, 1}));
    column->add_item(std::move(panes), LayoutSpec{SizePolicy::Expanding, 1});

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto back_button = std::make_unique<Button>(strings.back);
    auto* back_ptr =
        static_cast<Button*>(button_row->add_item(std::move(back_button), LayoutSpec{SizePolicy::Fixed, 1}));
    // Not a default button. A default button is the affirmative answer to a
    // question a dialog is asking, and a help viewer asks nothing: there is
    // no input to commit and no choice to confirm. Marking Close as the
    // default made Enter — pressed anywhere, including part-way through
    // typing a search — dismiss the window the reader was reading, which is
    // the one thing they did not ask for. Escape still closes it, which is
    // what "no affirmative action" leaves.
    auto close_button = std::make_unique<Button>(strings.close);
    auto* close_ptr =
        static_cast<Button*>(button_row->add_item(std::move(close_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    auto current_key = std::make_shared<std::string>(std::move(initial_topic_key));
    auto history = std::make_shared<std::vector<std::string>>();
    // The keys behind the rows currently listed, which is the filtered set
    // rather than the whole index once a search narrows it.
    auto listed_keys = std::make_shared<std::vector<std::string>>();
    // Set while a callback is itself rewriting the list, so the selection
    // changes that causes cannot be mistaken for the reader choosing a topic.
    auto updating = std::make_shared<bool>(false);

    auto show_topic = std::make_shared<std::function<void()>>();
    *show_topic = [&provider, current_key, text_view_ptr, &strings]() {
        const HelpTopic t = provider.topic(*current_key);
        std::string page = t.title + "\n\n" + t.body;
        // Curated cross-references still carry meaning the index cannot: they
        // say which topics the author thought related. They are prose here
        // rather than a second navigable list, because every topic they name
        // is already one click away in the pane on the left.
        if (!t.links.empty()) {
            page += "\n\n" + strings.help_see_also;
            for (std::size_t i = 0; i < t.links.size(); ++i)
                page += (i == 0 ? " " : ", ") + t.links[i].second;
        }
        text_view_ptr->set_text(std::move(page));
    };

    // Rebuilds the left pane from the index, or from a search when the box
    // holds a query, and keeps the current topic highlighted if it survived
    // the filter.
    auto refresh_index = std::make_shared<std::function<void()>>();
    *refresh_index = [&provider, current_key, listed_keys, index_list_ptr, search_ptr, updating]() {
        const std::string query = search_ptr->query();
        const std::vector<HelpIndexEntry> entries =
            query.empty() ? provider.index() : provider.search(query);
        std::vector<std::string> labels;
        listed_keys->clear();
        for (const HelpIndexEntry& entry : entries) {
            listed_keys->push_back(entry.key);
            labels.push_back(entry.title);
        }
        *updating = true;
        index_list_ptr->set_items(std::move(labels));
        // Put the cursor where the reader actually is, so the highlight marks
        // the topic on show and the next arrow key moves from there. A filter
        // that hides the current topic simply leaves the cursor at the top
        // rather than moving the reader somewhere they did not ask to go.
        for (std::size_t i = 0; i < listed_keys->size(); ++i) {
            if ((*listed_keys)[i] != *current_key) continue;
            index_list_ptr->set_cursor(i);
            break;
        }
        *updating = false;
    };

    // One entry point for "go to this topic", so the pane, the highlight and
    // the history can never disagree about which topic is current.
    auto navigate_to = std::make_shared<std::function<void(std::string, bool)>>();
    *navigate_to = [current_key, history, show_topic, refresh_index](std::string key, bool record) {
        if (key.empty() || key == *current_key) return;
        if (record) history->push_back(*current_key);
        *current_key = std::move(key);
        (*show_topic)();
        (*refresh_index)();
    };

    (*show_topic)();
    (*refresh_index)();

    // Selecting in the index navigates: with the list permanently on screen,
    // moving the highlight IS the request, and demanding a separate Enter
    // would leave the highlight pointing at a topic the pane is not showing.
    index_list_ptr->on_selection_changed = [listed_keys, navigate_to, updating](std::size_t index) {
        if (*updating || index >= listed_keys->size()) return;
        (*navigate_to)((*listed_keys)[index], /*record=*/true);
    };
    index_list_ptr->on_activate = [listed_keys, navigate_to, updating](std::size_t index) {
        if (*updating || index >= listed_keys->size()) return;
        (*navigate_to)((*listed_keys)[index], /*record=*/true);
    };
    search_ptr->on_change = [refresh_index](const std::string&) { (*refresh_index)(); };
    search_ptr->on_clear = [refresh_index]() { (*refresh_index)(); };

    back_ptr->on_press = [current_key, history, show_topic, refresh_index]() {
        if (history->empty()) return;
        *current_key = history->back();
        history->pop_back();
        (*show_topic)();
        (*refresh_index)();
    };
    close_ptr->on_press = [window_ptr]() { window_ptr->close(); };

    // accept_request is deliberately left unset, so Window::on_key does not
    // claim Enter for the window. Enter therefore belongs to whatever holds
    // the keyboard: the index list opens the highlighted topic with it, a
    // focused button presses itself, and the search box — which does not use
    // Enter, its filtering being live — does nothing at all with it. That
    // last case is the point. Typing a query and reaching for Enter is a
    // habit, and the habit must not throw the answer away.
    window_ptr->cancel_request = [close_ptr]() {
        if (close_ptr->on_press) close_ptr->on_press();
    };
    window_ptr->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    // The index takes the keyboard first. It is what the reader came to use —
    // arrow keys move between topics straight away — and it is the surface
    // whose highlight tells them where they are. Tab reaches the prose to
    // scroll it, and the search box above.
    return WindowHandle{std::move(window), index_list_ptr};
}

HelpViewerPresentation present_help_viewer(const HelpProvider& provider, std::string initial_topic_key,
                                           ui::Application& app, Desktop& desktop,
                                           const ui::StandardRoles& roles,
                                           const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<HelpViewerResult>;
    auto parts = Access::make();
    auto handle = make_help_viewer(provider, std::move(initial_topic_key), roles, app, app.focused(), strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, HelpViewerResult::Closed);
    };
    // Modeless: help is a reference, and a reader consults it while doing the
    // thing it is about. Scoping input to it would mean answering a question
    // requires dismissing the answer first.
    desktop.present_modeless(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
