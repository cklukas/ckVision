// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/help_viewer.hpp"

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/text_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;
}  // namespace

void MemoryHelpProvider::add_topic(std::string key, HelpTopic topic) { topics_[std::move(key)] = std::move(topic); }

HelpTopic MemoryHelpProvider::topic(const std::string& key) const {
    auto it = topics_.find(key);
    if (it != topics_.end()) return it->second;
    return HelpTopic{"Not Found", "No help is available for this topic.", {}};
}

WindowHandle make_help_viewer(const HelpProvider& provider, std::string initial_topic_key,
                               const ui::Theme& theme, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to) {
    auto window = std::make_unique<Window>("Help");
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    Window* window_ptr = window.get();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto text_view = std::make_unique<TextView>(theme, roles.static_text, roles.dialog_frame,
                                                 roles.dialog_background);
    auto* text_view_ptr =
        static_cast<TextView*>(column->add_item(std::move(text_view), LayoutSpec{SizePolicy::Expanding, 1}));

    auto links_list = std::make_unique<ListView>(theme, roles.input_normal, roles.input_focused, roles.dialog_frame,
                                                  roles.dialog_background, /*multi_select=*/false);
    links_list->set_preferred_size(Size{0, 4});
    auto* links_list_ptr =
        static_cast<ListView*>(column->add_item(std::move(links_list), LayoutSpec{SizePolicy::Fixed, 1}));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto back_button = std::make_unique<Button>("Back");
    auto* back_ptr =
        static_cast<Button*>(button_row->add_item(std::move(back_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto close_button = std::make_unique<Button>("Close");
    close_button->set_default(true);
    auto* close_ptr =
        static_cast<Button*>(button_row->add_item(std::move(close_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    auto current_key = std::make_shared<std::string>(std::move(initial_topic_key));
    auto history = std::make_shared<std::vector<std::string>>();
    auto link_keys = std::make_shared<std::vector<std::string>>();
    auto refresh = std::make_shared<std::function<void()>>();
    *refresh = [&provider, current_key, link_keys, text_view_ptr, links_list_ptr]() {
        const HelpTopic t = provider.topic(*current_key);
        text_view_ptr->set_text(t.title + "\n\n" + t.body);
        std::vector<std::string> labels;
        link_keys->clear();
        for (const auto& [key, label] : t.links) {
            link_keys->push_back(key);
            labels.push_back(label);
        }
        links_list_ptr->set_items(std::move(labels));
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
    window_ptr->on_closed = [&app, restore_focus_to, window_ptr]() {
        if (restore_focus_to != nullptr && restore_focus_to->focusable()) app.set_focus(restore_focus_to);
        schedule_self_detach(*window_ptr, app);
    };

    return WindowHandle{std::move(window), text_view_ptr};
}

}  // namespace ckv::widgets
