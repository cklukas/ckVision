// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Help viewer: context topics (F1 from any view via help-context keys,
// D-027 — already routed by Application::set_help_provider), activatable
// cross-links, back navigation, and provider-backed index/keyword lookup
// (the widget catalog M6c baseline). The library defines no help file format —
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

enum class HelpViewerResult { Closed };
using HelpViewerPresentation = DialogPresentation<HelpViewerResult>;

struct HelpTopic {
    std::string title;
    std::string body;
    std::vector<std::pair<std::string, std::string>> links;  // {topic_key, display_label}
};

struct HelpIndexEntry {
    std::string key;
    std::string title;

    friend bool operator==(const HelpIndexEntry&, const HelpIndexEntry&) = default;
};

class HelpProvider {
public:
    virtual ~HelpProvider() = default;
    // Implementation-defined (but never throwing/crashing) for an
    // unknown key — MemoryHelpProvider returns a "Not Found" topic.
    virtual HelpTopic topic(const std::string& key) const = 0;
    virtual std::vector<HelpIndexEntry> index() const = 0;
    virtual std::vector<HelpIndexEntry> search(std::string_view keyword) const = 0;
};

class MemoryHelpProvider final : public HelpProvider {
public:
    void add_topic(std::string key, HelpTopic topic);
    HelpTopic topic(const std::string& key) const override;
    std::vector<HelpIndexEntry> index() const override;
    std::vector<HelpIndexEntry> search(std::string_view keyword) const override;

private:
    std::unordered_map<std::string, HelpTopic> topics_;
};

// `provider` must outlive the returned Window (the installed closures
// capture it by reference, matching file_dialog's own FileSystem
// contract). Desktop::present_modeless attaches the returned handle
// and focuses its initial_focus in one call; modal presentation is
// explicit through Desktop::present_modal. The returned standard dialog window
// is non-resizable by default.
WindowHandle make_help_viewer(const HelpProvider& provider, std::string initial_topic_key,
                               const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                               const StandardStrings& strings = english_standard_strings());

// Presents help modally without a nested loop. Completion occurs only after
// detachment; close, external detach, and quit all resolve to Closed because
// the viewer returns no separate selection value.
[[nodiscard]] HelpViewerPresentation present_help_viewer(const HelpProvider& provider,
                                                          std::string initial_topic_key, ui::Application& app,
                                                          Desktop& desktop, const ui::StandardRoles& roles,
                                                          const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
