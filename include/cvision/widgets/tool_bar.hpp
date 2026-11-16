// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "cvision/ui/command.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/command_presentation.hpp"

namespace ckv::widgets {

// Command-button presentations and grouping follow D-118. Compact and Padded
// occupy one row; Framed uses three. One geometry drives drawing, overflow and
// pointer routing. Dedicated ckv.toolbar.* roles separate interaction states.
// Items are CommandPresentations: labels, chords, enablement and checked state
// come from the registry. Context restoration and keyboard walking follow D-099.
enum class ToolBarPresentation { Compact, Padded, Framed };

// A configurable command-button row with shared layout and input handling.
class ToolBar : public ui::View {
public:
    // An empty bar; a Tab stop.
    ToolBar();

    // The buttons, left to right. Moves the keyboard back to the first.
    void set_items(std::vector<CommandPresentation> items);
    const std::vector<CommandPresentation>& items() const noexcept { return items_; }
    // Flatten nonempty groups in order, with separators between visible groups.
    void set_groups(std::vector<std::vector<CommandPresentation>> groups);
    // Change chrome without losing commands, groups or keyboard position.
    void set_presentation(ToolBarPresentation presentation);
    ToolBarPresentation presentation() const noexcept { return presentation_; }
    // Whether each face also states its chord, "Save Ctrl+S". Off by
    // default: a tool bar is the compact surface, and the menus and the
    // overflow menu state chords already.
    void set_show_chords(bool show);
    bool show_chords() const noexcept { return show_chords_; }

    // The items shown on the bar, and those in the overflow menu, at the
    // current width; indices into items().
    std::vector<std::size_t> shown_items() const;
    std::vector<std::size_t> overflow_items() const;
    // The item the keyboard is on, or std::nullopt while it is on the
    // overflow control -- including when the item it was on has overflowed.
    std::optional<std::size_t> focused_item() const;

    // Gives the bar the keyboard, remembering the focus it came from and that
    // focus's command contexts, and puts the walk on the first button. The bar
    // must be attached under an Application. On a bar that already has the
    // keyboard it only moves the walk back to the first button.
    void activate();
    // Ends a walk activate() began: the focus goes back where it came from.
    void deactivate();

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    bool on_key_release(const KeyEvent& event) override;
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_hover_changed(bool now_hovered) override;
    void on_resized() override;
    void on_detaching() override;
    // Losing the keyboard ends a walk however it happened, forgetting the
    // focus it would have handed back.
    void on_focus(const FocusEvent& event) override;
    // At least the overflow control, preferring every button, free to grow;
    // presentation-dependent height.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_attached() override;

private:
    // One thing on the bar: a button, or the overflow control, which has no
    // item.
    struct Slot {
        std::optional<std::size_t> item;
        int x = 0;
        int width = 0;
    };
    struct Layout {
        std::vector<Slot> slots;
        std::vector<std::size_t> overflow;
        std::vector<int> separators;
    };
    // A walk: the focus to hand back when it ends, and that focus's command
    // contexts, which every button is judged and run against meanwhile.
    struct Walk {
        ui::Application::FocusBookmark focus;
        std::vector<std::string> contexts;
    };
    struct PreparedItem {
        MnemonicText label;
        std::string unchecked_face;
        std::string checked_face;
        bool toggle = false;
        int width = 0;
    };
    void prepare() const;
    const Layout& layout() const;
    // The item's face text, its padding and mark column included, and its
    // label as parsed for the mnemonic accent.
    std::string_view face(std::size_t item) const;
    const MnemonicText& label(std::size_t item) const;
    bool available(std::size_t item) const;
    std::size_t focused_slot(const Layout& bar) const;
    void focus_slot(const Layout& bar, std::size_t slot);
    int slot_at(const Layout& bar, Point local) const;
    int presentation_height() const;
    int caption_inset() const;
    int control_width() const;
    bool slot_available(const Slot& slot) const;
    void cancel_press();
    void begin_walk();
    // Ends the walk, handing the focus back; returns the contexts it held,
    // or std::nullopt when there was none.
    std::optional<std::vector<std::string>> end_walk();
    // Enter, Space, a completed click or a mnemonic: runs the item's command
    // or opens the overflow menu. Returns whether anything was done.
    bool activate_slot(const Layout& bar, std::size_t slot);
    bool run_item(std::size_t item);
    void open_overflow(const Layout& bar);

    mutable std::vector<PreparedItem> prepared_;
    mutable Layout layout_;
    mutable bool preparation_dirty_ = true;
    mutable std::uint64_t prepared_revision_ = 0;
    mutable const ui::Application* prepared_app_ = nullptr;
    std::vector<CommandPresentation> items_;
    std::vector<std::size_t> group_starts_;
    ToolBarPresentation presentation_ = ToolBarPresentation::Compact;
    bool show_chords_ = false;
    std::size_t focused_ = 0;
    // The keyboard is on the overflow control rather than on focused_.
    bool overflow_focused_ = false;
    std::optional<Walk> walk_;
    // The slot a pointer press went down on, and whether the pointer is
    // still over it.
    std::optional<std::size_t> pressed_slot_;
    bool pressed_visible_ = false;
    std::optional<std::size_t> pressed_item_;
    std::optional<Point> hover_position_;
    std::optional<KeyChord> armed_key_;
    // The press in flight began the walk, borrowing the keyboard, and so
    // ends it.
    bool press_owns_walk_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId hovered_role_ = ui::kInvalidRole;
    ui::RoleId pressed_role_ = ui::kInvalidRole;
    ui::RoleId checked_role_ = ui::kInvalidRole;
    ui::RoleId separator_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
};

// Explicit path chrome; Connected uses chevrons between padded segments.
enum class BreadcrumbPresentation { Plain, Padded, Connected };

// A one-row path of segments, such as the folders leading to a file.
class BreadcrumbBar : public ui::View {
public:
    // A tab stop from construction: the segments are walked and activated
    // from the keyboard.
    BreadcrumbBar();
    // Plain is default; padded styles add one cell per side to each segment.
    void set_presentation(BreadcrumbPresentation presentation);
    BreadcrumbPresentation presentation() const noexcept { return presentation_; }
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_resized() override { ellipsis_pressed_ = false; cached_layout_.reset(); invalidate(); }
    // The segments, first to last. Moves the focus back to the first segment.
    void set_segments(std::vector<std::string> segments);
    const std::vector<std::string>& segments() const noexcept { return segments_; }
    // Drawn verbatim between adjacent segments, "/" by default; include any
    // spaces wanted around it.
    void set_separator(std::string separator);
    // Fires with the index of the segment activated, by Enter, a click or the
    // ellipsis's menu, so the host can navigate to that level. Enter is not
    // consumed, and the ellipsis opens no menu, while this is empty.
    std::function<void(std::size_t)> on_activate;

    // The segments the ellipsis stands for at the current width, first to
    // last; empty while the whole path fits.
    std::vector<std::size_t> hidden_segments() const;
    // The segment the keyboard is on, or std::nullopt while it is on the
    // ellipsis -- including when the segment it was on has been elided into
    // it by a narrower width.
    std::optional<std::size_t> focused_segment() const;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    // One thing drawn on the bar: a segment (possibly elided to fit) or the
    // ellipsis, which has no segment.
    struct Stop {
        std::optional<std::size_t> segment;
        int x = 0;
        std::string text;
    };
    // The stops at the current width, left to right, and the indices the
    // ellipsis stands for (empty when there is none).
    struct Layout {
        std::vector<Stop> stops;
        std::vector<std::size_t> hidden;
    };
    const Layout& layout() const;
    Layout build_layout() const;
    std::string_view separator_text() const noexcept;
    BreadcrumbPresentation presentation_ = BreadcrumbPresentation::Plain;
    mutable std::optional<Layout> cached_layout_;
    // Which stop carries the focus in `bar`.
    std::size_t focused_stop(const Layout& bar) const;
    void focus_stop(const Layout& bar, std::size_t stop);
    int stop_at_x(const Layout& bar, int x) const;
    // Enter or a click on the stop: activates a segment, or opens the
    // ellipsis's menu. Returns whether it acted.
    bool activate_stop(const Layout& bar, std::size_t stop);
    void activate_segment(std::size_t index);
    std::vector<std::string> segments_;
    std::string separator_ = "/";
    std::size_t focused_ = 0;
    // The keyboard is on the ellipsis rather than on focused_.
    bool ellipsis_focused_ = false;
    // A pointer press went down on the ellipsis; its release opens the menu.
    bool ellipsis_pressed_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
