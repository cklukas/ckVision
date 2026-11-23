// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/wizard.hpp"
#include "cvision/widgets/tooltip.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>
#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
using detail::is_press;
using detail::stepped;
using detail::enclosing_desktop;
using detail::tail_to_width;
using detail::without_last_grapheme;

namespace {

// The first view in `view`'s subtree, itself included, that can take the
// focus, in the order Tab walks: a page arriving hands the focus to it.
ui::View* first_focusable_in(ui::View& view) {
    if (view.focusable()) return &view;
    for (const auto& child : view.children())
        if (ui::View* found = first_focusable_in(*child)) return found;
    return nullptr;
}

bool is_within(const ui::View* view, const ui::View& ancestor) noexcept {
    for (; view != nullptr; view = view->parent())
        if (view == &ancestor) return true;
    return false;
}

// The title row keeps at least this many cells for the page's own title
// before the step indicator gives way to it.
constexpr int kWizardTitleMinimum = 4;

}  // namespace

Wizard::Wizard() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    back_button_ = make<Button>(labels_.wizard_back);
    next_button_ = make<Button>(labels_.wizard_next);
    finish_button_ = make<Button>(labels_.wizard_finish);
    cancel_button_ = make<Button>(labels_.wizard_cancel);
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        button->set_presentation(ButtonPresentation::Flat);
    next_button_->set_default(true);
    finish_button_->set_default(true);
    back_button_->on_press = [this] { back(); };
    next_button_->on_press = [this] { next(); };
    finish_button_->on_press = [this] { finish(); };
    cancel_button_->on_press = [this] { cancel(); };
    refresh_navigation();
}
int Wizard::chrome_rows() const noexcept { return presentation_ == WizardPresentationStyle::Compact ? 1 : 3; }
int Wizard::rail_width() const noexcept { return presentation_ == WizardPresentationStyle::StepRail ? rail_width_ : 0; }
Rect Wizard::header_bounds() const noexcept {
    return Rect{0, 0, std::max(0, bounds().width), std::min(std::max(0, bounds().height), chrome_rows())};
}
Rect Wizard::footer_bounds() const noexcept {
    const int header = header_bounds().height;
    const int height = std::min(chrome_rows(), std::max(0, bounds().height - header));
    return Rect{0, std::max(header, bounds().height - height), std::max(0, bounds().width), height};
}
Rect Wizard::content_bounds() const noexcept {
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    const int x = rail_width() + inset;
    const int top = header_bounds().height;
    return Rect{x, top, std::max(0, bounds().width - x - inset), std::max(0, footer_bounds().y - top)}
        .intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect Wizard::step_rail_bounds() const noexcept {
    return Rect{0, header_bounds().height, rail_width(), std::max(0, footer_bounds().y - header_bounds().height)}
        .intersected(Rect{0, 0, bounds().width, bounds().height});
}
void Wizard::prepare_captions() {
    indicators_.clear();
    rail_captions_.clear();
    indicators_.reserve(pages_.size());
    rail_captions_.reserve(pages_.size());
    rail_width_ = 12;
    for (std::size_t i = 0; i < pages_.size(); ++i) {
        indicators_.push_back(labels_.wizard_step ? labels_.wizard_step(i + 1, pages_.size()) : std::string{});
        char number[32];
        const auto converted = std::to_chars(std::begin(number), std::end(number), i + 1);
        std::string caption(number, converted.ptr);
        caption += " ";
        caption += pages_[i].title;
        rail_width_ = std::max(rail_width_, std::min(24, text::text_width(caption) + 4));
        rail_captions_.push_back(std::move(caption));
    }
}
void Wizard::set_presentation(WizardPresentationStyle presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        button->set_presentation(presentation == WizardPresentationStyle::Compact ? ButtonPresentation::Flat : ButtonPresentation::Padded);
    on_resized();
    size_hint_changed();
    invalidate();
}
void Wizard::set_pages(std::vector<WizardPage> pages) {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_}) button->cancel_press();
    for (ui::View* content : contents_) if (content != nullptr) remove_child(content);
    pages_ = std::move(pages);
    contents_.assign(pages_.size(), nullptr);
    current_page_ = 0;
    prepare_captions();
    refresh_navigation();
    on_resized();
    invalidate();
    size_hint_changed();
}
ui::View* Wizard::set_page_content(std::size_t page, std::unique_ptr<ui::View> content) {
    CKV_ASSERT(page < pages_.size());
    CKV_ASSERT(content != nullptr);
    if (contents_[page] != nullptr) remove_child(contents_[page]);
    ui::View* const added = add_child(std::move(content));
    added->set_bounds(content_bounds());
    added->set_visible(page == current_page_);
    contents_[page] = added;
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_}) raise_to_front(button);
    invalidate();
    size_hint_changed();
    return added;
}
ui::View* Wizard::page_content(std::size_t page) const noexcept { return page < contents_.size() ? contents_[page] : nullptr; }
void Wizard::set_labels(ui::UiStrings labels) {
    labels_ = std::move(labels);
    back_button_->set_text(labels_.wizard_back);
    next_button_->set_text(labels_.wizard_next);
    finish_button_->set_text(labels_.wizard_finish);
    cancel_button_->set_text(labels_.wizard_cancel);
    prepare_captions();
    on_resized();
    invalidate();
    size_hint_changed();
}
bool Wizard::page_allows_leaving() const {
    return !pages_.empty() && (!pages_[current_page_].can_continue || pages_[current_page_].can_continue());
}
bool Wizard::can_go_next() const { return !last_page() && page_allows_leaving(); }
void Wizard::refresh_navigation() {
    const bool forward = page_allows_leaving();
    back_button_->set_visible(can_go_back() && !back_button_->bounds().empty());
    back_button_->set_enabled(can_go_back());
    next_button_->set_visible(!last_page() && !next_button_->bounds().empty());
    finish_button_->set_visible(last_page() && !finish_button_->bounds().empty());
    cancel_button_->set_visible(!cancel_button_->bounds().empty());
    for (Button* button : {next_button_, finish_button_}) {
        if (button->enabled() != forward) button->cancel_press();
        button->set_enabled(forward);
    }
    const bool cancel_available = static_cast<bool>(on_complete);
    if (cancel_button_->enabled() != cancel_available) cancel_button_->cancel_press();
    cancel_button_->set_enabled(cancel_available);
}
void Wizard::show_page(std::size_t page) {
    ui::Application* const app = context().app;
    ui::View* const leaving = contents_[current_page_];
    const bool focus_was_inside = app != nullptr && leaving != nullptr && is_within(app->focused(), *leaving);
    ui::View* const previous_action = &forward_button();
    const bool focus_was_action = app != nullptr && app->focused() == previous_action;
    const bool focus_was_back = app != nullptr && app->focused() == back_button_;
    current_page_ = page;
    ui::View* const arriving = contents_[page];
    if (arriving != nullptr) arriving->set_visible(true);
    refresh_navigation();
    if (focus_was_inside) {
        ui::View* target = arriving != nullptr ? first_focusable_in(*arriving) : nullptr;
        if (target == nullptr && focusable()) target = this;
        app->set_focus(target);
    } else if (focus_was_action && previous_action != &forward_button()) {
        app->set_focus(forward_button().focusable() ? static_cast<ui::View*>(&forward_button()) : this);
    }
    if (focus_was_back && !back_button_->focusable())
        app->set_focus(forward_button().focusable() ? static_cast<ui::View*>(&forward_button()) : this);
    if (leaving != nullptr) leaving->set_visible(false);
    invalidate();
}
bool Wizard::next() {
    if (!can_go_next()) return false;
    show_page(current_page_ + 1);
    return true;
}
bool Wizard::back() {
    if (!can_go_back()) return false;
    show_page(current_page_ - 1);
    return true;
}
bool Wizard::finish() {
    if (pages_.empty() || !last_page() || !page_allows_leaving()) return false;
    const std::function<void(WizardOutcome)> complete = on_complete;
    if (complete) complete(WizardOutcome::Finished);
    return true;
}
bool Wizard::cancel() {
    if (!on_complete) return false;
    const std::function<void(WizardOutcome)> complete = on_complete;
    complete(WizardOutcome::Cancelled);
    return true;
}
int Wizard::action_width() const {
    return std::max(next_button_->horizontal_size_hint().preferred, finish_button_->horizontal_size_hint().preferred);
}
void Wizard::layout_navigation() {
    if (back_button_ == nullptr || next_button_ == nullptr || finish_button_ == nullptr || cancel_button_ == nullptr) return;
    const Rect footer = footer_bounds();
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    const int y = footer.y + footer.height / 2;
    const int back_width = back_button_->horizontal_size_hint().preferred;
    const int forward_x = inset + back_width + 1;
    const int forward_width = action_width();
    const int cancel_width = cancel_button_->horizontal_size_hint().preferred;
    const int cancel_x = std::max(forward_x + forward_width + 1, bounds().width - inset - cancel_width);
    back_button_->set_bounds(Rect{inset, y, back_width, 1}.intersected(footer));
    next_button_->set_bounds(Rect{forward_x, y, forward_width, 1}.intersected(footer));
    finish_button_->set_bounds(next_button_->bounds());
    cancel_button_->set_bounds(Rect{cancel_x, y, cancel_width, 1}.intersected(footer));
}
void Wizard::draw(scene::Painter& target) {
    refresh_navigation();
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const auto& theme = *context().theme;
    Style normal = theme.resolve(role_);
    const bool enabled = enabled_in_tree();
    if (!enabled) { normal = accent_style(normal, theme.resolve(disabled_role_)); normal.attrs |= Attr::Dim; }
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    const bool compact = presentation_ == WizardPresentationStyle::Compact;
    Style header = compact ? normal : theme.resolve(header_role_);
    Style footer = compact ? normal : theme.resolve(footer_role_);
    Style rail = theme.resolve(rail_role_);
    if (!enabled) { header.attrs |= Attr::Dim; footer.attrs |= Attr::Dim; rail.attrs |= Attr::Dim; }
    painter.fill(header_bounds(), Cell::from_grapheme(" ", header));
    painter.fill(footer_bounds(), Cell::from_grapheme(" ", footer));
    const Rect rail_bounds = step_rail_bounds();
    if (!rail_bounds.empty()) {
        auto rail_painter = painter.clipped(rail_bounds);
        rail_painter.fill(rail_bounds, Cell::from_grapheme(" ", rail));
        rail_painter.vline(Point{rail_bounds.right() - 1, rail_bounds.y}, rail_bounds.height, scene::LineStyle::Single, rail);
        for (std::size_t i = 0; i < pages_.size() && i < static_cast<std::size_t>((rail_bounds.height + 1) / 2); ++i) {
            const int y = rail_bounds.y + static_cast<int>(i) * 2;
            Style style = rail;
            if (i == current_page_) style.attrs |= Attr::Bold;
            else style.attrs |= Attr::Dim;
            rail_painter.draw_text(Point{1, y}, i < current_page_ ? "✓" : i == current_page_ ? "●" : "○", style);
            rail_painter.draw_text(Point{3, y}, text::clip_to_width_view(rail_captions_[i], std::max(0, rail_bounds.width - 5)), style);
        }
    }
    const Rect header_rect = header_bounds();
    auto header_painter = painter.clipped(header_rect);
    const int inset = compact ? 0 : 1;
    const int x = rail_width() + inset;
    const int y = compact ? 0 : 1;
    const int width = std::max(0, bounds().width - x - inset);
    const std::string_view indicator = pages_.empty() ? std::string_view{} : std::string_view(indicators_[current_page_]);
    const int indicator_width = text::text_width(indicator);
    int title_width = width;
    if (indicator_width > 0 && width - indicator_width - 1 >= kWizardTitleMinimum) {
        title_width = width - indicator_width - 1;
        header_painter.draw_text(Point{x + width - indicator_width, y}, indicator, header);
    }
    const std::string_view title = pages_.empty() ? "Wizard" : std::string_view(pages_[current_page_].title);
    Style title_style = enabled ? accent_style(header, theme.resolve(selected_role_)) : header;
    title_style.attrs |= Attr::Bold;
    if (enabled && has_focus()) title_style.attrs |= Attr::Underline;
    const bool elided = text::text_width(title) > title_width;
    const auto shown = text::clip_to_width_view(title, elided ? std::max(0, title_width - 1) : title_width);
    header_painter.draw_text(Point{x, y}, shown, title_style);
    if (elided && title_width > 0) header_painter.draw_text(Point{x + text::text_width(shown), y}, "…", title_style);
}
void Wizard::on_focus(const FocusEvent&) { invalidate(); }
bool Wizard::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    refresh_navigation();
    if (event.chord.key == Key::Right || event.chord.key == Key::Enter) return next() || finish();
    if (event.chord.key == Key::Left) return back();
    if (event.chord.key == Key::Escape) return cancel();
    return false;
}
std::optional<PointerShape> Wizard::pointer_shape_at(Point local) const {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        if (button->visible() && button->bounds().contains(local))
            return enabled_in_tree() && button->enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    return std::nullopt;
}
void Wizard::on_resized() {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        if (button != nullptr) button->cancel_press();
    const Rect content = content_bounds();
    for (ui::View* page : contents_) if (page != nullptr) page->set_bounds(content);
    layout_navigation();
    refresh_navigation();
    auto* app = context().app;
    if (app != nullptr && app->focused() != nullptr &&
        (app->focused() == back_button_ || app->focused() == next_button_ ||
         app->focused() == finish_button_ || app->focused() == cancel_button_) &&
        !app->focused()->focusable()) app->set_focus(this);
}
void Wizard::on_child_size_hint_changed(ui::View&) { layout_navigation(); size_hint_changed(); }
ui::SizeHint Wizard::horizontal_size_hint() const {
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    int preferred = inset * 2 + back_button_->horizontal_size_hint().preferred + 1 + action_width() + 1 + cancel_button_->horizontal_size_hint().preferred;
    for (std::size_t i = 0; i < pages_.size(); ++i) {
        const int indicator_width = text::text_width(indicators_[i]);
        preferred = std::max(preferred, rail_width() + inset * 2 + text::text_width(pages_[i].title) + (indicator_width > 0 ? indicator_width + 1 : 0));
        if (contents_[i] != nullptr) preferred = std::max(preferred, rail_width() + inset * 2 + contents_[i]->horizontal_size_hint().preferred);
    }
    return {0, preferred, ui::kUnboundedExtent};
}
ui::SizeHint Wizard::vertical_size_hint() const {
    int content = 0;
    for (const ui::View* page : contents_) if (page != nullptr) content = std::max(content, page->vertical_size_hint().preferred);
    if (presentation_ == WizardPresentationStyle::StepRail) content = std::max(content, static_cast<int>(pages_.size()) * 2 - 1);
    return {chrome_rows() * 2, content + chrome_rows() * 2, ui::kUnboundedExtent};
}
void Wizard::on_attached() {
    role_ = context().roles->find("ckv.dialog.background");
    selected_role_ = context().roles->find("ckv.label.text");
    disabled_role_ = context().roles->find("ckv.label.disabled");
    header_role_ = context().roles->find("ckv.wizard.header");
    footer_role_ = context().roles->find("ckv.wizard.footer");
    rail_role_ = context().roles->find("ckv.wizard.rail");
    if (context().app != nullptr && labels_.wizard_back == "< Back") {
        set_labels(context().app->strings());
    }
    on_resized();
}

WizardPresentation present_modal_wizard(std::unique_ptr<Wizard> wizard, std::string title, ui::Application& app,
                                        Desktop& desktop) {
    CKV_ASSERT(wizard != nullptr);
    const ui::StandardRoles& roles = app.standard_roles();
    using Access = detail::DialogPresentationAccess<WizardOutcome>;
    auto parts = Access::make();
    auto window = std::make_unique<Window>(std::move(title));
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    window->set_resizable(false);
    window->set_content_margin(1, 1);
    Window* const window_ptr = window.get();
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    Wizard* const wizard_ptr = wizard.get();
    wizard_ptr->set_labels(app.strings());
    // The reader starts on the page's first control, as a dialog's first
    // field takes the focus; a page with none leaves it on the navigation.
    ui::View* const content = wizard_ptr->page_content(wizard_ptr->current_page());
    ui::View* const page_control = content != nullptr ? first_focusable_in(*content) : nullptr;
    wizard_ptr->on_complete = [state = parts.state, window_ptr, window_liveness](WizardOutcome outcome) {
        // Recorded before the window goes, so the completion carries it
        // whatever closes the window afterwards.
        Access::record(state, outcome);
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->set_content(std::move(wizard));
    // Escape reaches the wizard first and cancels through on_complete; a
    // window-level cancel is the same answer by the fallback.
    window->cancel_request = [window_ptr, window_liveness] {
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->on_closed = [&app, window_ptr, window_liveness] {
        if (!window_liveness.expired()) schedule_self_detach(*window_ptr, app);
    };
    auto previous_on_detached = std::move(window->on_detached);
    window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, WizardOutcome::Cancelled);
    };
    desktop.present_modal(WindowHandle{std::move(window), page_control != nullptr ? page_control : wizard_ptr},
                          app);
    return std::move(parts.presentation);
}

NotificationCenter::NotificationCenter() { set_focus_policy(ui::FocusPolicy::TabStop); }

std::size_t NotificationCenter::add(Notification notification) {
    // A persistent one is never due; nor is anything added while no interval
    // has been set, or while there is no clock to read. The deadline is fixed
    // HERE rather than derived at each sweep, so a notification's life is
    // measured from when the host posted it and not from when the widget
    // happened to look.
    std::int64_t deadline = kNever;
    ui::Application* const app = context().app;
    if (!notification.persistent && auto_dismiss_nanos_ > 0 && app != nullptr)
        deadline = app->clock().now_nanos() + auto_dismiss_nanos_;
    notifications_.push_back(std::move(notification));
    deadlines_.push_back(deadline);
    if (deadline != kNever) arm_expiry(deadline);
    changed();
    return notifications_.size() - 1;
}

void NotificationCenter::dismiss(std::size_t index) {
    if (index >= notifications_.size()) return;
    notifications_.erase(notifications_.begin() + static_cast<std::ptrdiff_t>(index));
    deadlines_.erase(deadlines_.begin() + static_cast<std::ptrdiff_t>(index));
    changed();
}

void NotificationCenter::set_auto_dismiss(std::int64_t nanos) {
    // Negative is read as off rather than asserted on: this is a duration a
    // host computes from a setting, and the answer to a nonsensical one is a
    // centre that keeps its notifications, not a dead application.
    auto_dismiss_nanos_ = std::max<std::int64_t>(0, nanos);
    // Re-time what is already on screen — see the header. Persistent entries
    // keep kNever whatever the interval says.
    ui::Application* const app = context().app;
    const std::int64_t now = app != nullptr ? app->clock().now_nanos() : 0;
    std::int64_t soonest = kNever;
    for (std::size_t i = 0; i < notifications_.size(); ++i) {
        if (notifications_[i].persistent || auto_dismiss_nanos_ == 0 || app == nullptr) {
            deadlines_[i] = kNever;
            continue;
        }
        deadlines_[i] = now + auto_dismiss_nanos_;
        if (soonest == kNever || deadlines_[i] < soonest) soonest = deadlines_[i];
    }
    if (soonest != kNever) arm_expiry(soonest);
}

void NotificationCenter::expire_due() {
    ui::Application* const app = context().app;
    if (app == nullptr) return;
    const std::int64_t now = app->clock().now_nanos();
    bool removed = false;
    for (std::size_t i = notifications_.size(); i-- > 0;) {
        if (deadlines_[i] == kNever || deadlines_[i] > now) continue;
        notifications_.erase(notifications_.begin() + static_cast<std::ptrdiff_t>(i));
        deadlines_.erase(deadlines_.begin() + static_cast<std::ptrdiff_t>(i));
        removed = true;
    }
    // Whatever is still waiting gets the next wake-up. Recomputed from what
    // remains rather than remembered, because a dismissal by the reader may
    // have taken the entry this timer was armed for.
    std::int64_t soonest = kNever;
    for (const std::int64_t deadline : deadlines_)
        if (deadline != kNever && (soonest == kNever || deadline < soonest)) soonest = deadline;
    if (soonest != kNever) arm_expiry(soonest);
    if (removed) changed();
}

void NotificationCenter::arm_expiry(std::int64_t deadline_nanos) {
    ui::Application* const app = context().app;
    if (app == nullptr) return;
    // An armed wake-up that comes sooner already covers this one; one that
    // comes later is replaced, so a toast posted now does not wait behind a
    // persistent-looking interval that was armed before it.
    if (expiry_timer_ != 0 && expiry_wake_nanos_ <= deadline_nanos) return;
    if (expiry_timer_ != 0) app->cancel_timer(expiry_timer_);
    const std::int64_t delay = std::max<std::int64_t>(0, deadline_nanos - app->clock().now_nanos());
    const std::weak_ptr<void> liveness = lifetime_token();
    expiry_wake_nanos_ = deadline_nanos;
    expiry_timer_ = app->start_timer(delay, /*repeating=*/false, [this, liveness] {
        // One-shot, so nothing has to be cancelled on the way out — but this
        // view can be destroyed between arming and firing, and a callback
        // reading `this` then would be reading freed storage.
        if (liveness.expired()) return;
        expiry_timer_ = 0;
        expiry_wake_nanos_ = 0;
        expire_due();
    });
}

void NotificationCenter::changed() {
    size_hint_changed();
    invalidate();
    if (on_change) on_change();
}

void NotificationCenter::on_focus(const FocusEvent&) { invalidate(); }

void NotificationCenter::set_presentation(NotificationPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    size_hint_changed();
    invalidate();
}
ui::SizeHint NotificationCenter::vertical_size_hint() const {
    const auto count = notifications_.size();
    const std::size_t stride = presentation_ == NotificationPresentation::Lines ? 1 : 4;
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    const int rows = count == 0 ? 0 : count > maximum / stride
        ? std::numeric_limits<int>::max() : static_cast<int>(count * stride - (stride == 1 ? 0 : 1));
    return {0, std::max(rows, preferred_size().height), ui::kUnboundedExtent};
}
Rect NotificationCenter::notification_bounds(std::size_t index) const noexcept {
    if (index >= notifications_.size()) return {};
    const int stride = presentation_ == NotificationPresentation::Lines ? 1 : 4;
    const int height = std::max(0, bounds().height);
    if (height == 0 || index >= static_cast<std::size_t>((height - 1) / stride + 1)) return {};
    const int row = static_cast<int>(index) * stride;
    return Rect{0, row, bounds().width, presentation_ == NotificationPresentation::Lines ? 1 : 3}.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect NotificationCenter::dismiss_bounds(std::size_t index) const noexcept {
    const Rect card = notification_bounds(index);
    if (card.empty()) return {};
    if (presentation_ == NotificationPresentation::Lines) return card;
    const int row = static_cast<int>(index) * 4;
    return Rect{std::max(0, bounds().width - 3), row + 1, std::min(2, bounds().width), 1}.intersected(card);
}
std::optional<PointerShape> NotificationCenter::pointer_shape_at(Point local) const {
    if (local.x < 0 || local.y < 0 || local.x >= bounds().width || local.y >= bounds().height) return std::nullopt;
    const auto index = static_cast<std::size_t>(local.y / (presentation_ == NotificationPresentation::Lines ? 1 : 4));
    if (!dismiss_bounds(index).contains(local)) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}
void NotificationCenter::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    Style base = context().theme->resolve(role_);
    if (!enabled) {
        const Style muted = accent_style(base, context().theme->resolve(disabled_role_));
        if (muted.fg != base.bg) base.fg = muted.fg;
        base.attrs |= Attr::Dim;
    }
    for (std::size_t i = 0; i < notifications_.size(); ++i) {
        const Rect card = notification_bounds(i);
        if (card.empty()) break;
        const auto severity = notifications_[i].severity;
        const std::string_view prefix = severity == NotificationSeverity::Info ? "i " : severity == NotificationSeverity::Warning ? "! " : "x ";
        const auto role = severity == NotificationSeverity::Info ? info_role_ : severity == NotificationSeverity::Warning ? warning_role_ : error_role_;
        Style line = base;
        const bool cards = presentation_ != NotificationPresentation::Lines;
        if (enabled_in_tree() && has_focus() && i + 1 == notifications_.size()) line.attrs |= cards ? Attr::Underline : Attr::Reverse;
        const Style accent = enabled ? accent_style(line, context().theme->resolve(role)) : line;
        auto clipped = painter.clipped(card);
        clipped.fill(card, Cell::from_grapheme(" ", cards ? base : line));
        if (presentation_ == NotificationPresentation::Framed) clipped.draw_box(Rect{0, static_cast<int>(i) * 4, bounds().width, 3}, scene::LineStyle::Single, accent);
        if (presentation_ == NotificationPresentation::Banners) clipped.vline(Point{0, card.y}, card.height, scene::LineStyle::Single, accent);
        const int y = static_cast<int>(i) * (cards ? 4 : 1) + (cards ? 1 : 0);
        const int inset = cards ? 1 : 0;
        const int reserved = cards ? 4 : 0;
        const int text_x = inset + 2;
        const int available = std::max(0, bounds().width - reserved - text_x);
        auto content = clipped.clipped(Rect{inset, y, std::max(0, bounds().width - reserved - inset), 1});
        content.draw_text(Point{inset, y}, prefix, accent);
        const auto& message = notifications_[i].text;
        const bool elided = text::text_width(message) > available;
        const auto shown = text::clip_to_width_view(message, elided ? std::max(0, available - 1) : available);
        content.draw_text(Point{text_x, y}, shown, line);
        if (elided && available > 0) content.draw_text(Point{text_x + text::text_width(shown), y}, "…", line);
        if (cards) {
            const Rect dismissal = dismiss_bounds(i);
            auto control = clipped.clipped(dismissal);
            control.draw_text(Point{dismissal.x, y}, "×", base);
        }
    }
}

bool NotificationCenter::on_key(const KeyEvent& event) {
    if (enabled_in_tree() && is_press(event) && event.chord.key == Key::Escape && !notifications_.empty()) { dismiss(notifications_.size() - 1); return true; }
    return false;
}

bool NotificationCenter::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!pointer_shape_at(local)) return false;
    dismiss(static_cast<std::size_t>(local.y / (presentation_ == NotificationPresentation::Lines ? 1 : 4)));
    return true;
}

void NotificationCenter::on_attached() {
    disabled_role_ = context().roles->find("ckv.label.disabled");
    // The focus policy is NOT set here — see the constructor. Attaching is
    // where a view learns its context, not where it overrules decisions its
    // host has already made about it.
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (info_role_ == ui::kInvalidRole) info_role_ = context().roles->find("ckv.message.info.text");
    if (warning_role_ == ui::kInvalidRole) warning_role_ = context().roles->find("ckv.message.warning.text");
    if (error_role_ == ui::kInvalidRole) error_role_ = context().roles->find("ckv.message.error.text");
    // Anything posted before there was a clock to read has no deadline yet.
    // Re-timing here is what makes "post first, attach later" behave the same
    // as the ordinary way round, rather than leaving those entries immortal.
    if (auto_dismiss_nanos_ > 0) set_auto_dismiss(auto_dismiss_nanos_);
}

Tooltip::Tooltip(std::string text) : text_(std::move(text)) {
    set_visible(false);
    set_preferred_size(Size{shown_width(), 1});
}
int Tooltip::shown_width() const noexcept { return std::max(2, text::text_width(text_) + 2); }
void Tooltip::set_text(std::string text) {
    text_ = std::move(text);
    set_preferred_size(Size{shown_width(), 1});
    invalidate();
    size_hint_changed();
}
void Tooltip::show_at(Point position) {
    set_bounds(Rect{position.x, position.y, shown_width(), 1});
    set_visible(true);
}
void Tooltip::show_near(Rect anchor, Rect area) {
    const auto inside = [&area](int row) { return row >= area.y && row < area.bottom(); };
    const int width = std::min(shown_width(), std::max(0, area.width));
    const int below = anchor.bottom();
    const int above = anchor.y - 1;
    const int y = inside(below)   ? below
                  : inside(above) ? above
                                  : std::clamp(anchor.bottom() - 1, area.y, std::max(area.y, area.bottom() - 1));
    const int x = std::max(area.x, std::min(anchor.x, area.right() - width));
    set_bounds(Rect{x, y, width, 1});
    set_visible(true);
}
void Tooltip::hide() { set_visible(false); }
void Tooltip::dismiss() {
    // Held locally: the host normally takes this tooltip down from here.
    const std::function<void()> callback = on_dismiss;
    if (callback) callback();
}
void Tooltip::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{1, 0}, text::elide_to_width(text_, std::max(0, bounds().width - 2)), style);
}
bool Tooltip::on_key(const KeyEvent& event) {
    if (!held_ || !is_press(event)) return false;
    dismiss();
    return true;
}
bool Tooltip::on_text(const TextEvent&) {
    if (!held_) return false;
    dismiss();
    return true;
}
bool Tooltip::on_mouse(const MouseEvent& event) {
    if (!held_) return false;
    const Rect absolute = absolute_bounds();
    const bool inside = event.cell.x >= absolute.x && event.cell.x < absolute.right() && event.cell.y >= absolute.y &&
                        event.cell.y < absolute.bottom();
    // A press anywhere puts it away, as it puts away a menu: one outside is
    // light dismissal and stays unhandled, as a menu leaves it.
    if (event.action == MouseAction::Down) dismiss();
    return inside;
}
void Tooltip::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.tooltip");
}

TooltipController::TooltipController(ui::Application& app, Desktop& desktop)
    : app_(app), desktop_(desktop), desktop_liveness_(desktop.lifetime_token()) {
    observer_ = app_.add_attention_observer([this](ui::Application::AttentionChange change) { observe(change); });
    // Claimed only when unclaimed, as Desktop claims its own defaults: an
    // application that answers the key itself keeps its answer.
    const ui::CommandId command = app_.commands().standard().tooltip;
    if (!app_.commands().has_handler(command)) {
        app_.commands().set_handler(command, [this] { show_for_focus(); });
        installed_command_handler_ = true;
    }
}

TooltipController::~TooltipController() {
    app_.remove_attention_observer(observer_);
    if (installed_command_handler_) app_.commands().set_handler(app_.commands().standard().tooltip, {});
    dismiss();
}

const TooltipController::Tip* TooltipController::tip_entry_for(const ui::View& view) const {
    // The nearest view up the ancestry that has a tip: a control's parts
    // explain themselves by the control's tip, as they resolve help keys.
    for (const ui::View* current = &view; current != nullptr; current = current->parent())
        for (const Tip& tip : tips_)
            if (tip.view == current && !tip.liveness.expired()) return &tip;
    return nullptr;
}

std::string TooltipController::tip_for(const ui::View& view) const {
    const Tip* const tip = tip_entry_for(view);
    return tip != nullptr ? tip->text : std::string{};
}

void TooltipController::set_tip(const ui::View& view, std::string text) {
    std::erase_if(tips_, [&view](const Tip& tip) { return tip.liveness.expired() || tip.view == &view; });
    // A tip on show whose words change goes; the next time the reader turns
    // to the view it says the new ones.
    if (shown_owner_ == &view) take_down();
    if (!text.empty()) tips_.push_back(Tip{&view, view.lifetime_token(), std::move(text)});
}

void TooltipController::set_delay(std::int64_t nanos) { delay_nanos_ = std::max<std::int64_t>(1, nanos); }

void TooltipController::cancel_pending() {
    if (pending_timer_ != 0) app_.cancel_timer(pending_timer_);
    pending_timer_ = 0;
    pending_target_ = nullptr;
    pending_liveness_.reset();
}

void TooltipController::arm(Source source, const ui::View& target) {
    cancel_pending();
    pending_source_ = source;
    pending_target_ = &target;
    pending_liveness_ = target.lifetime_token();
    // The destructor cancels this timer, so the callback never outlives the
    // controller it names.
    pending_timer_ = app_.start_timer(delay_nanos_, /*repeating=*/false, [this] {
        pending_timer_ = 0;
        const ui::View* const target_view = pending_target_;
        const bool alive = !pending_liveness_.expired();
        const Source source_now = pending_source_;
        cancel_pending();
        if (!alive || target_view == nullptr) return;
        // Only while the reader is still where the wait began.
        if (source_now == Source::Focus && app_.focused() != target_view) return;
        if (source_now == Source::Pointer && app_.hovered_view() != target_view) return;
        const Tip* const tip = tip_entry_for(*target_view);
        if (tip == nullptr) return;
        Rect anchor = target_view->absolute_bounds();
        if (source_now == Source::Pointer && app_.last_mouse_event())
            anchor = Rect{app_.last_mouse_event()->cell.x, app_.last_mouse_event()->cell.y, 1, 1};
        show(source_now, *tip, anchor);
    });
}

void TooltipController::show(Source source, const Tip& tip, Rect anchor) {
    take_down();
    if (desktop_liveness_.expired()) return;
    const Rect desktop = desktop_.absolute_bounds();
    Tooltip* const view = desktop_.add_popup(std::make_unique<Tooltip>(tip.text));
    tooltip_ = view;
    tooltip_liveness_ = view->lifetime_token();
    shown_source_ = source;
    shown_owner_ = tip.view;
    view->show_near(Rect{anchor.x - desktop.x, anchor.y - desktop.y, anchor.width, anchor.height},
                    Rect{0, 0, desktop.width, desktop.height});
    if (source != Source::Command) return;
    // Asked for from the keyboard, it is held the way an open menu is: keys
    // come to it through a scope of its own, the pointer through capture,
    // and either one puts it away.
    view->set_held(true);
    view->on_dismiss = [this] { take_down(); };
    held_scope_ = app_.push_modal(*view);
    app_.set_input_capture(view);
}

void TooltipController::take_down() {
    if (tooltip_ == nullptr) return;
    Tooltip* const view = tooltip_;
    const bool alive = !tooltip_liveness_.expired();
    tooltip_ = nullptr;
    tooltip_liveness_.reset();
    shown_owner_ = nullptr;
    const ui::Application::ModalScopeId scope = held_scope_;
    held_scope_ = 0;
    if (!alive) return;  // the desktop took it down already, scope and all
    if (app_.input_capture() == view) app_.clear_input_capture();
    if (scope != 0) app_.pop_modal(scope);
    if (!desktop_liveness_.expired()) desktop_.remove_popup(view);  // destroys it
}

void TooltipController::dismiss() {
    cancel_pending();
    take_down();
}

const Tooltip* TooltipController::tooltip() const noexcept {
    return tooltip_liveness_.expired() ? nullptr : tooltip_;
}

bool TooltipController::show_for_focus() {
    const ui::View* const focused = app_.focused();
    const Tip* const tip = focused != nullptr ? tip_entry_for(*focused) : nullptr;
    if (tip == nullptr) return false;
    cancel_pending();
    show(Source::Command, *tip, focused->absolute_bounds());
    return true;
}

void TooltipController::observe(ui::Application::AttentionChange change) {
    // A held tip answers for itself, as an open menu does, until the reader
    // puts it away.
    if (tooltip() != nullptr && held_scope_ != 0) return;
    using Change = ui::Application::AttentionChange;
    if (change == Change::Input) {
        // The reader is doing something: an explanation in the way of it
        // goes, and one still waiting to appear does not.
        dismiss();
        return;
    }
    if (change == Change::Focus) {
        if (tooltip() != nullptr && shown_source_ == Source::Focus) take_down();
        const ui::View* const focused = app_.focused();
        if (focused != nullptr && tip_entry_for(*focused) != nullptr)
            arm(Source::Focus, *focused);
        else if (pending_timer_ != 0 && pending_source_ == Source::Focus)
            cancel_pending();
        return;
    }
    const ui::View* const hovered = app_.hovered_view();
    // Onto the tooltip itself: the reader has come to read it.
    if (hovered != nullptr && tooltip() != nullptr && is_within(hovered, *tooltip_)) return;
    const Tip* const tip = hovered != nullptr ? tip_entry_for(*hovered) : nullptr;
    const bool same_tip_on_show = tip != nullptr && tooltip() != nullptr && tip->view == shown_owner_;
    if (tooltip() != nullptr && shown_source_ == Source::Pointer && !same_tip_on_show) take_down();
    if (tip != nullptr && !same_tip_on_show)
        arm(Source::Pointer, *hovered);
    else if (pending_timer_ != 0 && pending_source_ == Source::Pointer)
        cancel_pending();
}

}  // namespace ckv::widgets
