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

// A run of UTF-8 text in one style. It wraps between words at spaces; a "\n" inside the text
// ends the row like a FlowLineBreak. Zero-width graphemes are not shown.
struct FlowText {
    // The text to flow.
    std::string text;
    // Attributes added to the view's text role style for this run.
    Attr attrs = static_cast<Attr>(0);
    // When set, the whole run is one link, drawn underlined, that reports this string
    // through FlowView::on_link_activate. Links are numbered in document order, one per
    // linked run, even when several runs share a target.
    std::optional<std::string> link_target;
};

// Ends the current row. It always ends a row, even an empty one, so consecutive breaks
// leave blank rows.
struct FlowLineBreak {};

// The image is an inline document atom, but reserves a rectangular run of flow
// rows. Text never wraps alongside that rectangle, preserving deterministic
// layout and making raster fallback legible in narrow terminals.
struct FlowImage {
    // The raster, shared with the application. A null or empty image shows the fallback
    // text instead.
    std::shared_ptr<const Image> image;
    // The rectangle the image occupies, in cells, each dimension clamped to at least 1. It
    // starts on a new row at column 0 and is cropped to the view's content width.
    Size cell_extent{1, 1};
    // Text drawn on the image's first row, in the view's text style, where no raster can be
    // shown: no image, or a terminal without raster graphics. Empty draws a blank area.
    std::string fallback = "[image]";
};

// One element of a block's content.
using FlowInline = std::variant<FlowText, FlowLineBreak, FlowImage>;

// A paragraph: inline content laid out as consecutive rows. FlowView puts one blank row
// between adjacent blocks.
struct FlowBlock {
    // The inline elements, laid out in order.
    std::vector<FlowInline> content;
};

// The whole content of a FlowView, as an application-owned value the view keeps a copy of.
struct FlowDocument {
    // The paragraphs, top to bottom.
    std::vector<FlowBlock> blocks;
};

// A read-only, scrolling view of a FlowDocument: styled text word-wrapped to the view's
// width, block images, and links a reader can step through and follow. The rightmost column
// always holds a vertical scrollbar, so text wraps to the width less one cell; a word wider
// than that is broken between graphemes. Layout is recomputed whenever the document or the
// width changes.
//
// The view is a tab stop. It draws in the theme role "ckv.flow.text", resolved on attach.
// The current link is drawn in reverse video while the view has the focus; Tab and
// Shift+Tab move it through the links and Enter follows it.
class FlowView : public ui::View {
public:
    // An empty document; the view takes focus by Tab.
    FlowView();

    // Draws with `text_role` instead of "ckv.flow.text". An override set before
    // attachment survives it; a change repaints.
    void set_role_override(ui::RoleId text_role) noexcept {
        if (text_role_ == text_role) return;
        text_role_ = text_role;
        invalidate();
    }
    // Replaces the whole document and lays it out again. The current link becomes the
    // first link, or none when the document has none.
    void set_document(FlowDocument document);
    // The document as last set, appended to and replaced into.
    const FlowDocument& document() const noexcept { return document_; }
    // Adds `block` after the last one and lays the document out again. The current link is
    // kept.
    void append_block(FlowBlock block);
    // Replaces one existing application-owned block. A stale index is rejected
    // without altering the document. When a valid layout exists and this is
    // the final block, only that block's derived layout is rebuilt; all other
    // replacements rebuild the affected layout as a whole. Successful
    // replacement resets link selection to the first link, or none.
    bool replace_block(std::size_t index, FlowBlock block);

    // The number of laid-out rows at the current width, counting the blank rows between
    // blocks and the rows images reserve; at least 1.
    int line_count() const;
    // The index of the first row shown, which is the scrollbar's position.
    int top_line() const noexcept;
    // The number of links in the document, one per FlowText with a link_target.
    std::size_t link_count() const;
    // The link Enter would follow, as an index in document order, or none. Setting an index
    // at or past link_count() clears it. Setting it repaints but fires nothing, and does
    // not scroll the link into view.
    std::optional<std::size_t> current_link() const noexcept { return current_link_; }
    void set_current_link(std::optional<std::size_t> index);
    // Fires on_link_activate with the current link's target, if the callback is set.
    // Returns whether there was a current link, whether or not a callback ran.
    bool activate_current_link();

    // Called with a link's target when the reader follows it (Enter, or a left click on the
    // link, which also makes it current) and on every activate_current_link() call.
    std::function<void(const std::string&)> on_link_activate;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    // Consumes Tab and Shift+Tab while the document has links, cycling the current link
    // with wrap-around instead of moving the focus; without links they pass on. Enter is
    // consumed only when it follows a current link. Up, Down, PageUp, PageDown, Home and
    // End scroll the rows and are always consumed; key releases never are.
    bool on_key(const KeyEvent& event) override;
    // Consumes a left press on a link, which makes it current and follows it, and the
    // wheel, which scrolls ui::kWheelRows rows per notch. Every other mouse event passes on.
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;
    // The current link is marked only while focused, so focus repaints.
    void on_focus(const FocusEvent& event) override;

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
