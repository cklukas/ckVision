// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// TextView: read-only styled text, scrolling, activatable links, and a
// deterministic OSC 8 export path (plans/widgets.md M6b). Text is split into
// rows on '\n' only — no word-wrap. Long lines are horizontally clipped by the
// Painter rather than wrapped, and there is no horizontal scrollbar in 0.1.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

struct TextSpan {
    std::string text;
    Attr attrs = static_cast<Attr>(0);
    std::optional<std::string> link_target;
};

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.textview.text"; its embedded Scrollbar resolves
// its own roles independently.
class TextView : public ui::View {
public:
    TextView();

    void set_role_override(ui::RoleId text_role) noexcept { text_role_ = text_role; }
    // A containing ScrollViewport can own the visible scrollbars for a
    // document surface. Hiding this view's internal vertical track keeps the
    // text preformatted while avoiding duplicate controls.
    void set_vertical_scrollbar_visible(bool visible) noexcept;
    bool vertical_scrollbar_visible() const noexcept { return vertical_scrollbar_visible_; }

    void set_text(std::string text);
    void set_spans(std::vector<TextSpan> spans);
    const std::string& text() const noexcept { return raw_text_; }

    int line_count() const noexcept { return static_cast<int>(lines_.size()); }
    int top_line() const noexcept;
    std::size_t link_count() const noexcept { return link_targets_.size(); }
    std::optional<std::size_t> current_link() const noexcept { return current_link_; }
    void set_current_link(std::optional<std::size_t> index);
    bool activate_current_link();
    std::string osc8_text() const;

    std::function<void(const std::string&)> on_link_activate;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    struct LineRun {
        std::string text;
        Attr attrs = static_cast<Attr>(0);
        std::optional<std::size_t> link_index;
    };

    void split_lines();
    void rebuild_from_spans();
    std::optional<std::size_t> link_at(int line, int column) const;

    std::string raw_text_;
    std::vector<std::string> lines_;
    std::vector<TextSpan> spans_;
    std::vector<std::vector<LineRun>> line_runs_;
    std::vector<std::string> link_targets_;
    std::optional<std::size_t> current_link_;
    Scrollbar* scrollbar_ = nullptr;
    bool vertical_scrollbar_visible_ = true;

    ui::RoleId text_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
