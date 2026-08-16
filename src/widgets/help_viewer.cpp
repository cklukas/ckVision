// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/help_viewer.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/label.hpp"
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
    window->set_resizable(false);
    // A help topic is prose; hold it off the frame like every other dialog.
    window->set_content_margin(1, 1);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto text_view = std::make_unique<TextView>();
    // A help window that is not told how much room its prose wants gets sized
    // by the only children that do ask for room — the links list and the
    // button row — and comes out a couple of dozen cells wide, with the topic
    // clipped away entirely. Nothing else can supply this: a TextView holds
    // whatever text it is later given, and the placement pass runs before any
    // topic has been loaded. So the viewer states the shape help wants,
    // rather than the shape its buttons happen to need: a readable prose
    // column, deep enough to be worth scrolling. Placement clamps both to the
    // desktop, and the view scrolls, so a small terminal loses no content.
    text_view->set_preferred_size(Size{60, 12});
    auto* text_view_ptr =
        static_cast<TextView*>(column->add_item(std::move(text_view), LayoutSpec{SizePolicy::Expanding, 1}));

    // The cross-links need saying what they are. On their own they are a box
    // that changes the page when clicked, which reads as anything but
    // navigation — a text field, most likely, since that is what a bordered
    // box holding one line of text usually is.
    auto links_label = std::make_unique<Label>(strings.help_links);
    auto* links_label_ptr =
        static_cast<Label*>(column->add_item(std::move(links_label), LayoutSpec{SizePolicy::Fixed, 1}));

    auto links_list = std::make_unique<ListView>(/*multi_select=*/false);
    // The links almost always fit. A bar that cannot scroll is the only thing
    // suggesting there is more here than the reader can see.
    links_list->set_scrollbar_auto_hide(true);
    auto* links_list_ptr =
        static_cast<ListView*>(column->add_item(std::move(links_list), LayoutSpec{SizePolicy::Fixed, 1}));
    links_label_ptr->set_buddy(links_list_ptr);

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto back_button = std::make_unique<Button>(strings.back);
    auto* back_ptr =
        static_cast<Button*>(button_row->add_item(std::move(back_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto close_button = std::make_unique<Button>(strings.close);
    close_button->set_default(true);
    auto* close_ptr =
        static_cast<Button*>(button_row->add_item(std::move(close_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    auto current_key = std::make_shared<std::string>(std::move(initial_topic_key));
    auto history = std::make_shared<std::vector<std::string>>();
    auto link_keys = std::make_shared<std::vector<std::string>>();
    auto refresh = std::make_shared<std::function<void()>>();
    *refresh = [&provider, current_key, link_keys, text_view_ptr, links_list_ptr, links_label_ptr]() {
        const HelpTopic t = provider.topic(*current_key);
        text_view_ptr->set_text(t.title + "\n\n" + t.body);
        std::vector<std::string> labels;
        link_keys->clear();
        for (const auto& [key, label] : t.links) {
            link_keys->push_back(key);
            labels.push_back(label);
        }
        const int count = static_cast<int>(labels.size());
        links_list_ptr->set_items(std::move(labels));
        // Exactly as many rows as there are links. A fixed height left blank
        // rows under the last one, which is what made the list look like an
        // input field with room to type in. Bounded so a topic that links to
        // a great many others scrolls instead of crowding out the prose.
        links_list_ptr->set_preferred_size(Size{0, std::clamp(count, 1, 6)});
        // A topic with no links shows neither the list nor its heading: an
        // empty box under a heading promising links is worse than no box.
        // Hidden rather than removed, so the pair survives navigation to a
        // topic that does have links.
        links_label_ptr->set_visible(count > 0);
        links_list_ptr->set_visible(count > 0);
    };
    (*refresh)();

    links_list_ptr->on_activate = [current_key, history, link_keys, refresh](std::size_t index) {
        if (index >= link_keys->size()) return;
        history->push_back(*current_key);
        *current_key = (*link_keys)[index];
        (*refresh)();
    };
    back_ptr->on_press = [current_key, history, refresh]() {
        if (history->empty()) return;
        *current_key = history->back();
        history->pop_back();
        (*refresh)();
    };
    close_ptr->on_press = [window_ptr]() { window_ptr->close(); };

    window_ptr->accept_request = [close_ptr]() {
        if (close_ptr->on_press) close_ptr->on_press();
    };
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

    return WindowHandle{std::move(window), text_view_ptr};
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
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
