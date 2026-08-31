// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// FlowView: wrapped, styled, read-only flow content with link navigation and
// inline raster atoms. Its document is an application-owned value; semantic
// parsing and raster creation remain outside the widget (D-043).
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

struct FlowText {
    std::string text;
    Attr attrs = static_cast<Attr>(0);
    std::optional<std::string> link_target;
};

struct FlowLineBreak {};

// The image is an inline document atom, but reserves a rectangular run of flow
// rows. Text never wraps alongside that rectangle, preserving deterministic
// layout and making raster fallback legible in narrow terminals.
struct FlowImage {
    std::shared_ptr<const Image> image;
    Size cell_extent{1, 1};
    std::string fallback = "[image]";
};

using FlowInline = std::variant<FlowText, FlowLineBreak, FlowImage>;

struct FlowBlock {
    std::vector<FlowInline> content;
};

struct FlowDocument {
    std::vector<FlowBlock> blocks;
};

class FlowView : public ui::View {
public:
    FlowView();

    void set_role_override(ui::RoleId text_role) noexcept { text_role_ = text_role; }
    void set_document(FlowDocument document);
    const FlowDocument& document() const noexcept { return document_; }
    void append_block(FlowBlock block);
    // Replaces one existing application-owned block. A stale index is rejected
    // without altering the document. When a valid layout exists and this is
    // the final block, only that block's derived layout is rebuilt; all other
    // replacements rebuild the affected layout as a whole. Successful
    // replacement resets link selection.
    bool replace_block(std::size_t index, FlowBlock block);

    int line_count() const;
    int top_line() const noexcept;
    std::size_t link_count() const;
    std::optional<std::size_t> current_link() const noexcept { return current_link_; }
    void set_current_link(std::optional<std::size_t> index);
    bool activate_current_link();

    std::function<void(const std::string&)> on_link_activate;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    struct LayoutRun {
        std::string text;
        Attr attrs = static_cast<Attr>(0);
        std::optional<std::size_t> link;
    };
    struct LayoutRow {
        std::vector<LayoutRun> runs;
    };
    struct LayoutImage {
        int top = 0;
        Size cell_extent;
        std::shared_ptr<const Image> image;
        std::string fallback;
    };
    struct BlockLayoutOffset {
        std::size_t row_begin = 0;
        std::size_t image_begin = 0;
        std::size_t link_begin = 0;
    };

    void invalidate_layout();
    void ensure_layout() const;
    void rebuild_layout(int content_width) const;
    void append_block_layout(std::size_t block_index, int content_width) const;
    void update_scrollbar_range() const;
    std::optional<std::size_t> link_at(int line, int column) const;
    int content_width() const noexcept;
    void scroll_to(int position);

    FlowDocument document_;
    mutable int layout_width_ = -1;
    mutable std::vector<LayoutRow> rows_;
    mutable std::vector<LayoutImage> images_;
    mutable std::vector<std::string> link_targets_;
    mutable std::vector<BlockLayoutOffset> block_layout_offsets_;
    mutable std::optional<std::size_t> current_link_;
    Scrollbar* scrollbar_ = nullptr;
    ui::RoleId text_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
