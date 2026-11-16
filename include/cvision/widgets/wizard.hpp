// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/ui_strings.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"

namespace ckv::widgets {

// One step of a Wizard.
struct WizardPage {
    // Drawn on the wizard's top row while this page is current.
    std::string title;
    // Whether the reader may go on from this page, to the next or, on the
    // last, to finish. Asked afresh whenever that is decided or drawn; empty
    // means always.
    std::function<bool()> can_continue;
};

// How a wizard ended: the reader finished it on its last page, or cancelled
// it -- by Escape, the Cancel control, or, for a presented wizard, its
// window's close control or the application quitting.
enum class WizardOutcome { Finished, Cancelled };

// Explicit wizard layouts; alternate chrome is opt-in.
enum class WizardPresentationStyle { Compact, Bands, StepRail };

// A multi-step flow: the current page's title on the top row with the step
// indicator ("Step 2 of 4") at its right end, the current page's content
// below it, and on the bottom row the navigation -- "< Back" while there is
// a page before this one, then "Next >" or, on the last page, "Finish", and
// "Cancel" at the right end. The forward action is greyed while the page's
// can_continue refuses. Wizard focus underlines the title; its buttons show
// their own focus and default-action emphasis.
//
// Right or Enter goes next, or finishes on the last page; Left goes back;
// Escape cancels. Keys a page's own controls leave unused reach the wizard
// the same way, so Enter in a page's last field moves on. A left click on a
// navigation control does what its key does. Finishing and cancelling end
// the flow through on_complete, typed by WizardOutcome; present_modal_wizard turns
// that into a typed, non-blocking DialogPresentation.
//
// Each page may have content of its own (set_page_content), which the wizard
// owns and lays out between its top and bottom rows, showing only the
// current page's. When the page changes while the focus is inside the page
// being left, the focus moves to the first control of the page arriving, or
// to the wizard when it has none. A page without content leaves those rows
// for the host to draw over.
//
// A Tab stop from construction. Resolves dialog/label roles and the dedicated
// wizard header/footer/rail surfaces on attach; Buttons resolve their own.
class Wizard : public ui::View {
public:
    // A focusable flow with owned flat navigation buttons.
    Wizard();
    void set_presentation(WizardPresentationStyle presentation);
    WizardPresentationStyle presentation() const noexcept { return presentation_; }
    // Clipped local geometry, shared with content layout and Button hit testing.
    Rect content_bounds() const noexcept;
    Rect header_bounds() const noexcept;
    Rect footer_bounds() const noexcept;
    Rect step_rail_bounds() const noexcept;
    Button& back_button() noexcept { return *back_button_; }
    Button& forward_button() noexcept { return last_page() ? *finish_button_ : *next_button_; }
    Button& cancel_button() noexcept { return *cancel_button_; }
    // Re-evaluate an external validation dependency and repaint navigation.
    void refresh_navigation();
    // Replaces the pages and returns to the first. Content set for the old
    // pages is destroyed.
    void set_pages(std::vector<WizardPage> pages);
    // The number of pages, and the zero-based index of the current one.
    std::size_t page_count() const noexcept { return pages_.size(); }
    std::size_t current_page() const noexcept { return current_page_; }
    // Makes `content` the view shown under the title while page `page` is
    // current, destroying any content that page had, and returns it. Asserts
    // that `page` exists.
    ui::View* set_page_content(std::size_t page, std::unique_ptr<ui::View> content);
    // Page `page`'s content, or nullptr when it has none or does not exist.
    ui::View* page_content(std::size_t page) const noexcept;
    // Replaces the words the wizard draws.
    void set_labels(ui::UiStrings labels);
    const ui::UiStrings& labels() const noexcept { return labels_; }
    // Whether next() would move: there is a later page and the current
    // page's can_continue allows it.
    bool can_go_next() const;
    // Whether back() would move: the current page is not the first.
    bool can_go_back() const noexcept { return current_page_ > 0; }
    // Move one page forward or back when allowed; false, with nothing
    // changed, when not. Neither fires a callback.
    bool next();
    bool back();
    // On the last page, when its can_continue allows it, fires on_complete
    // with WizardOutcome::Finished and returns true. Anywhere else, or with
    // no pages, returns false. The page stays where it is either way.
    bool finish();
    // Fires on_complete with WizardOutcome::Cancelled and returns true;
    // returns false, doing nothing, while on_complete is empty.
    bool cancel();
    // The end of the flow, finished or cancelled; see WizardOutcome. Escape
    // is not consumed while this is empty. present_modal_wizard sets it: a caller
    // of that function leaves it alone and awaits the presentation.
    std::function<void(WizardOutcome)> on_complete;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    void on_resized() override;
    void on_child_size_hint_changed(ui::View& child) override;
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Wide enough for the widest page content, the navigation row and the
    // widest title with its step indicator; tall enough for the tallest
    // content between the chosen chrome rows, including the step rail.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    void layout_navigation();
    void prepare_captions();
    int rail_width() const noexcept;
    int chrome_rows() const noexcept;
    int action_width() const;
    bool last_page() const noexcept { return pages_.empty() || current_page_ + 1 >= pages_.size(); }
    bool page_allows_leaving() const;
    void show_page(std::size_t page);

    WizardPresentationStyle presentation_ = WizardPresentationStyle::Compact;
    Button* back_button_ = nullptr;
    Button* next_button_ = nullptr;
    Button* finish_button_ = nullptr;
    Button* cancel_button_ = nullptr;
    std::vector<std::string> indicators_;
    std::vector<std::string> rail_captions_;
    int rail_width_ = 12;
    std::vector<WizardPage> pages_;
    std::vector<ui::View*> contents_;
    std::size_t current_page_ = 0;
    ui::UiStrings labels_{};
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId header_role_ = ui::kInvalidRole;
    ui::RoleId footer_role_ = ui::kInvalidRole;
    ui::RoleId rail_role_ = ui::kInvalidRole;
};

// The caller's handle on a presented wizard, completing once with how it
// ended after its window has detached.
using WizardPresentation = DialogPresentation<WizardOutcome>;

// Presents `wizard` modally in a dialog window titled `title`, without
// starting a nested loop (D-038). The window is sized to the wizard's own
// size hints, clamped to the desktop, and opens with the focus on the first
// control of the current page, or on the wizard when the page has none.
// Finishing records WizardOutcome::Finished and closes the window; Escape,
// Cancel, the close control, an external detach and a host quit all complete
// with WizardOutcome::Cancelled. This function sets the wizard's
// on_complete; the caller reads each page's own controls once the
// presentation completes with Finished, and must keep what the page
// callbacks capture alive until then.
[[nodiscard]] WizardPresentation present_modal_wizard(std::unique_ptr<Wizard> wizard, std::string title,
                                                      ui::Application& app, Desktop& desktop);

}  // namespace ckv::widgets
