// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Help viewer: context topics (F1 from any view via help-context keys,
// D-027 — already routed by Application::set_help_provider), activatable
// cross-links, back navigation, and provider-backed index/keyword lookup
// (plans/widgets.md M6c baseline). The library defines no help file format —
// HelpProvider is the injected content interface an application implements
// over whatever storage it wants (MemoryHelpProvider, provided here, is the
// in-memory/test form).
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

// How a help viewer ended. There is only one outcome — the viewer returns no
// selection — so Closed covers the Close button, Escape, an external detach
// and application quit alike.
enum class HelpViewerResult { Closed };
// The completion handle present_help_viewer returns (see DialogPresentation).
using HelpViewerPresentation = DialogPresentation<HelpViewerResult>;

// One page of help, as a HelpProvider returns it.
struct HelpTopic {
    // The heading; also the topic's entry in the viewer's index pane.
    std::string title;
    // The prose, shown under the title, word-wrapped by the viewer. '\n'
    // breaks lines.
    std::string body;
    // Related topics as (topic key, display label) pairs. The viewer lists the
    // labels in a "see also" line after the body as plain text — every topic
    // is already reachable from the index — so the keys serve providers (for
    // example MemoryHelpProvider::search) rather than navigation.
    std::vector<std::pair<std::string, std::string>> links;  // {topic_key, display_label}
};

// One row of a provider's index or search result.
struct HelpIndexEntry {
    // The key HelpProvider::topic resolves; what the viewer navigates to.
    std::string key;
    // The text the viewer lists for it.
    std::string title;

    // Memberwise: equal key and equal title.
    friend bool operator==(const HelpIndexEntry&, const HelpIndexEntry&) = default;
};

// The injected content interface behind the help viewer: an application
// implements it over whatever storage it keeps its help in. The viewer calls
// it as the reader navigates and searches, so every call should be cheap.
class HelpProvider {
public:
    // Providers are used through this interface and may be destroyed through it.
    virtual ~HelpProvider() = default;
    // Implementation-defined (but never throwing/crashing) for an
    // unknown key — MemoryHelpProvider returns a "Not Found" topic.
    virtual HelpTopic topic(const std::string& key) const = 0;
    // Every topic, in the order the viewer's index pane lists them.
    virtual std::vector<HelpIndexEntry> index() const = 0;
    // The topics matching `keyword`, in the order the index pane lists them
    // while a search is active. The viewer calls it with the search box's
    // query whenever that is non-empty; how a match is decided is the
    // provider's business.
    virtual std::vector<HelpIndexEntry> search(std::string_view keyword) const = 0;
};

// A HelpProvider over topics held in memory: the form for small applications
// and tests. index() and search() are sorted by title, then key. search()
// matches `keyword` as an ASCII case-insensitive substring of a topic's key,
// title, body, or any link key or label; an empty keyword matches every
// topic. An unknown key yields a "Not Found" topic.
class MemoryHelpProvider final : public HelpProvider {
public:
    // Adds the topic under `key`, replacing any topic already there.
    void add_topic(std::string key, HelpTopic topic);
    HelpTopic topic(const std::string& key) const override;
    std::vector<HelpIndexEntry> index() const override;
    std::vector<HelpIndexEntry> search(std::string_view keyword) const override;

private:
    std::unordered_map<std::string, HelpTopic> topics_;
};

// Builds the help viewer window, unattached: the topic index (with a search
// box above it) on the left, the current topic's prose on the right, and Back
// and Close buttons below. Choosing a topic in the index shows it and records
// the one left in the history Back walks; Escape and Close close the window,
// restore focus to `restore_focus_to` (which may be nullptr) and schedule the
// window's own detach. Enter is not claimed by the window, so typing a search
// and pressing Enter never dismisses it. `initial_topic_key` is shown first.
//
// `provider` must outlive the returned Window (the installed closures
// capture it by reference, matching file_dialog's own FileSystem
// contract), and so must `strings`, which the viewer reads again on every
// navigation. Desktop::present_modeless attaches the returned handle
// and focuses its initial_focus (the index list) in one call; modal
// presentation is explicit through Desktop::present_modal. The returned
// window is resizable, with a minimum size of 40x12 cells.
WindowHandle make_help_viewer(const HelpProvider& provider, std::string initial_topic_key,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               const StandardStrings& strings = english_standard_strings());

// Builds the viewer (make_help_viewer, restoring focus to whatever was focused
// when called) and presents it MODELESSLY on `desktop`: help is consulted
// while working, so it does not scope input to itself. Returns without a
// nested loop. Completion occurs only after detachment; close, external
// detach, and quit all resolve to Closed because the viewer returns no
// separate selection value. `provider` and `strings` must outlive the window.
[[nodiscard]] HelpViewerPresentation present_help_viewer(const HelpProvider& provider,
                                                          std::string initial_topic_key, ui::Application& app,
                                                          Desktop& desktop, const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
