// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/dialog.hpp"

#include <algorithm>
#include <charconv>

#include <optional>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/layout_metrics.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// One blank row between the things a reader answers, and between the last
// of them and the buttons. Named because the pane below has to reproduce
// exactly the gap the field Column itself uses; two literal 1s that have to
// agree are a rule waiting to be half-changed.
constexpr int kFieldSpacing = 1;

ui::View* find_first_focusable(ui::View& view) {
    if (view.focusable()) return &view;
    for (const auto& child : view.children())
        if (ui::View* found = find_first_focusable(*child)) return found;
    return nullptr;
}

// Scrolls `view` into sight in whichever viewport it lives in, if any. A
// control the reader cannot see is one the reader cannot use, and a FOCUSED
// control they cannot see is worse: the terminal cursor is blinking off
// screen and the keyboard is typing into it.
//
// Written as a walk up from the view rather than a call on a known viewport,
// so a caller needs to know only which view took focus — which is all
// Application tells anybody.
void reveal_in_scroll_viewport(ui::View& view) {
    for (ui::View* ancestor = view.parent(); ancestor != nullptr; ancestor = ancestor->parent())
        if (auto* viewport = dynamic_cast<ScrollViewport*>(ancestor)) {
            viewport->ensure_visible(view);
            return;
        }
}

// A descriptor dialog's content root (U4-g): the fields inside a
// ScrollViewport, the button row a sibling BELOW it, and one layout rule
// applied to whatever height the window currently has —
//
//   fits:         the viewport is exactly as tall as the fields need, the
//                 buttons sit directly under it, and no bar appears (the
//                 viewport's vertical bar is ScrollbarPolicy::Auto).
//                 Identical to the plain Column this replaced, cell for cell.
//   does not fit: the buttons keep the bottom of the pane, the viewport
//                 takes what is left, and the fields scroll.
//
// The decision is made HERE, in layout, rather than once at materialization,
// because the height a dialog gets is not knowable when its tree is built
// and does not stay the same afterwards: terminals are resized, docks
// appear, the desktop shrinks and grows. A tree that chose its shape once
// would be right until the first resize.
class DialogPane final : public ui::View {
public:
    DialogPane(ui::Alignment button_alignment, bool anchor_buttons_to_bottom) noexcept
        : button_alignment_(button_alignment), anchor_buttons_to_bottom_(anchor_buttons_to_bottom) {}

    // Takes the field Column and returns the viewport now holding it.
    ScrollViewport* set_fields(std::unique_ptr<ui::View> fields) {
        CKV_ASSERT(viewport_ == nullptr);
        CKV_ASSERT(fields != nullptr);
        auto viewport = std::make_unique<ScrollViewport>();
        // Vertical: Auto, so the bar exists exactly while there is more form
        // than window. Horizontal: none at all — a form scrolled sideways
        // has lost its left column, which is where every label is.
        viewport->set_vertical_scrollbar_policy(ScrollbarPolicy::Auto);
        viewport->set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
        fields_ = fields.get();
        viewport->set_content(std::move(fields));
        viewport_ = add(std::move(viewport));
        return viewport_;
    }

    void set_buttons(std::unique_ptr<Row> buttons) {
        CKV_ASSERT(buttons_ == nullptr);
        CKV_ASSERT(buttons != nullptr);
        buttons_ = add(std::move(buttons));
        relayout();
    }

    ui::SizeHint horizontal_size_hint() const override {
        // The cross axis maxes, exactly as the Column this replaced did.
        // Deliberately no allowance for the vertical bar: a dialog must not
        // open one column wider on the chance that it might scroll.
        ui::SizeHint hint{0, 0, ui::kUnboundedExtent};
        for (const ui::View* part : {static_cast<const ui::View*>(fields_),
                                      static_cast<const ui::View*>(buttons_)}) {
            if (part == nullptr) continue;
            const ui::SizeHint part_hint = part->horizontal_size_hint();
            hint.min = std::max(hint.min, part_hint.min);
            hint.preferred = std::max(hint.preferred, part_hint.preferred);
        }
        return hint;
    }

    // The dialog's RECOMMENDED height: what showing the whole form with its
    // buttons would take. Desktop::present_modal opens the window at this,
    // clamped to the space there is — so a dialog that fits is the size it
    // has always been, and one that does not starts out scrolling.
    ui::SizeHint vertical_size_hint() const override {
        ui::SizeHint hint{0, 0, ui::kUnboundedExtent};
        if (fields_ != nullptr) {
            const ui::SizeHint fields = fields_->vertical_size_hint();
            hint.min += fields.min;
            hint.preferred += fields.preferred;
        }
        if (buttons_ != nullptr) {
            const ui::SizeHint buttons = buttons_->vertical_size_hint();
            const int between = field_button_gap() + anchor_slack();
            hint.min += buttons.min + between;
            hint.preferred += buttons.preferred + between;
        }
        return hint;
    }

    int height_for_width(int width) const override {
        const int usable = std::max(0, width);
        int total = fields_ != nullptr ? fields_->height_for_width(usable) : 0;
        if (buttons_ != nullptr)
            total += field_button_gap() + anchor_slack() +
                     ui::detail::preferred_height_for_width(*buttons_, button_row_width(usable));
        return total;
    }

    // A row of nothing but buttons already ends in a cast shadow, and the
    // hosting Window drops its bottom margin when its content says so.
    // Answering from whichever part occupies this pane's own last row keeps
    // that unchanged — a dialog that grew a blank row under its buttons
    // would be a visible difference, which this package does not get to make.
    bool trailing_row_is_shadow() const noexcept override {
        if (buttons_ != nullptr) return buttons_->trailing_row_is_shadow();
        return fields_ != nullptr && fields_->trailing_row_is_shadow();
    }

    void on_resized() override { relayout(); }
    void on_child_size_hint_changed(ui::View&) override { relayout(); }

    bool on_key(const KeyEvent&) override {
        // Never consumes anything. A key that reaches this pane climbed the
        // focus route from the focused field without being handled, which
        // makes it a candidate for MOVING focus — Tab, Shift-Tab, an Alt
        // mnemonic — and Application performs that move after this route
        // returns. So the reveal is POSTED rather than done here: posted
        // work runs once the dispatch is over, with focus already wherever
        // the key sent it, and still before the frame is painted.
        //
        // Nothing is posted while the form fits, which is every dialog with
        // nothing to scroll.
        if (viewport_ == nullptr || !viewport_->can_scroll_vertically()) return false;
        ui::Application* const app = context().app;
        if (app == nullptr) return false;
        app->post([app] {
            if (ui::View* focused = app->focused()) reveal_in_scroll_viewport(*focused);
        });
        return false;
    }

private:
    // Zero when there is nothing on one side of it: a buttons-only dialog
    // had no gap before this pane existed and must not grow one.
    int field_button_gap() const noexcept {
        const bool has_fields = fields_ != nullptr && !fields_->children().empty();
        return has_fields && buttons_ != nullptr ? kFieldSpacing : 0;
    }

    // An anchored dialog used to carry an expanding spacer as a Column item
    // of its own, and so counted one extra spacing row in its natural
    // height. The spacer is gone — this pane places the buttons at the
    // bottom directly — but the number it produced is what such dialogs are
    // sized from today, so the number stays.
    int anchor_slack() const noexcept { return anchor_buttons_to_bottom_ && buttons_ != nullptr ? 1 : 0; }

    int button_row_width(int width) const {
        if (buttons_ == nullptr) return 0;
        const auto [offset, extent] = ui::align_cross_axis(
            width, buttons_->horizontal_size_hint().preferred, button_alignment_, 0, 0);
        (void)offset;
        return extent;
    }

    void relayout() {
        if (viewport_ == nullptr) return;
        const int width = bounds().width;
        const int height = bounds().height;
        const int gap = field_button_gap();
        const int buttons_height =
            buttons_ != nullptr ? std::max(1, buttons_->vertical_size_hint().preferred) : 0;
        const int natural =
            fields_ != nullptr ? ui::detail::preferred_height_for_width(*fields_, width) : 0;

        // THE decision, and the only one: is there room for the whole form
        // AND its buttons? An anchored dialog takes the same branch as one
        // that does not fit, because both mean "buttons at the bottom edge".
        const bool fits = natural + gap + buttons_height <= height;
        const int viewport_height = (fits && !anchor_buttons_to_bottom_)
                                        ? natural
                                        : std::max(0, height - gap - buttons_height);
        viewport_->set_bounds(Rect{0, 0, width, viewport_height});
        if (buttons_ == nullptr) return;
        // Never past this pane's own last row, however little height there is.
        const int y = std::min(viewport_height + gap, std::max(0, height - buttons_height));
        const auto [x, button_width] = ui::align_cross_axis(
            width, buttons_->horizontal_size_hint().preferred, button_alignment_, 0, 0);
        buttons_->set_bounds(Rect{x, y, button_width, buttons_height});
    }

    ScrollViewport* viewport_ = nullptr;
    ui::View* fields_ = nullptr;  // owned by viewport_; kept for measurement
    Row* buttons_ = nullptr;
    ui::Alignment button_alignment_ = ui::Alignment::Start;
    bool anchor_buttons_to_bottom_ = false;
};

// A Number field's text as a number, or nothing. The whole field has to be
// the number: "12 or so" is not 12, and a partial parse is how a typo becomes
// a setting nobody chose.
std::optional<long long> parse_number(const FieldDescriptor& field, std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
    if (begin == end) return std::nullopt;
    long long value = 0;
    const auto result = std::from_chars(text.data() + begin, text.data() + end, value);
    if (result.ec != std::errc{} || result.ptr != text.data() + end) return std::nullopt;
    if (field.minimum && value < *field.minimum) return std::nullopt;
    if (field.maximum && value > *field.maximum) return std::nullopt;
    return value;
}

// Gives every ButtonRole::Dismiss button the rest of what its role promises:
// its own handler, and then the dialog gone. Called by whoever put the
// materialized tree into a window, since `dismiss` is that window's cancel
// path -- a dismissing button and Esc must not be two behaviours.
void wire_dismiss_buttons(const DialogDescriptor& descriptor, const std::vector<Button*>& buttons,
                          const std::function<void()>& dismiss) {
    CKV_ASSERT(buttons.size() == descriptor.buttons.size());
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (descriptor.buttons[i].role != ButtonRole::Dismiss) continue;
        buttons[i]->on_press = [handler = descriptor.buttons[i].on_press, dismiss] {
            if (handler) handler();
            dismiss();
        };
    }
}

bool validate_inputs(const std::vector<InputLine*>& inputs, const DialogDescriptor& descriptor,
                      ui::Application& app) {
    CKV_ASSERT(inputs.size() == descriptor.fields.size());
    InputLine* first_invalid = nullptr;
    bool all_valid = true;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        // A checkbox has no text to validate and no invalid state to show;
        // its slot in the parallel array is empty by construction.
        if (inputs[i] == nullptr) continue;
        const FieldDescriptor& field = descriptor.fields[i];
        const auto& validate = field.validate;
        // A Number is a number before it is anything else. Its own validator
        // still runs, and runs second, so it never has to parse the text
        // again to answer a question about the value.
        const bool numeric =
            field.kind != FieldKind::Number || parse_number(field, inputs[i]->text()).has_value();
        const bool valid = numeric && (!validate || validate(inputs[i]->text()));
        inputs[i]->set_valid(valid);
        if (!valid) {
            all_valid = false;
            if (first_invalid == nullptr) first_invalid = inputs[i];
        }
    }
    if (!all_valid) {
        app.set_focus(first_invalid);
        // A veto that focuses a field the reader cannot see is a dialog that
        // refuses to close and will not say why. Scrolled forms are the only
        // case where that can happen, and this is the one moment the dialog
        // moves focus itself rather than leaving it to Tab.
        reveal_in_scroll_viewport(*first_invalid);
        return false;
    }
    return true;
}

struct DescriptorDialogCompletion {
    std::weak_ptr<void> window_liveness;
    std::optional<DialogResult> selected_result;
    bool closed = false;
};

struct BuiltDescriptorDialog {
    WindowHandle handle;
    std::shared_ptr<DescriptorDialogCompletion> completion;
};

// Both vectors are filled for every field, whatever its kind, so a caller
// indexes either by field position without first asking what kind it was.
DialogResult accepted_result(const DialogDescriptor& descriptor, const std::vector<InputLine*>& inputs,
                              const std::vector<CheckGroup*>& checks,
                              const std::vector<RadioGroup*>& radios,
                              const std::vector<ComboBox*>& combos) {
    CKV_ASSERT(inputs.size() == checks.size());
    CKV_ASSERT(inputs.size() == radios.size());
    CKV_ASSERT(inputs.size() == combos.size());
    DialogResult result;
    result.accepted = true;
    result.values.reserve(inputs.size());
    result.checked.reserve(checks.size());
    result.selected.reserve(inputs.size());
    result.numbers.reserve(inputs.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        std::string text = inputs[i] != nullptr ? inputs[i]->text() : std::string{};
        if (combos[i] != nullptr) text = combos[i]->text();
        int selected = -1;
        if (radios[i] != nullptr) selected = radios[i]->selected();
        if (combos[i] != nullptr && combos[i]->selected_index())
            selected = static_cast<int>(*combos[i]->selected_index());
        std::optional<long long> number;
        if (i < descriptor.fields.size() && descriptor.fields[i].kind == FieldKind::Number &&
            inputs[i] != nullptr)
            number = parse_number(descriptor.fields[i], inputs[i]->text());
        result.values.push_back(std::move(text));
        result.checked.push_back(checks[i] != nullptr && checks[i]->checked(0));
        result.selected.push_back(selected);
        result.numbers.push_back(number);
    }
    return result;
}

// A presented dialog is as tall as it recommends, or as tall as there is
// room for — whichever is smaller — and it re-answers that question every
// time the desktop changes size (U4-g).
//
// Desktop::on_resized already does one half: reposition_within() SHRINKS a
// window that no longer fits, which is what turns a too-small terminal into
// a scrolling dialog. The half it does not do is giving the room back when
// the terminal grows again, and without it a dialog that was once squeezed
// keeps scrolling for the rest of the session over a terminal that has been
// large enough for minutes.
void refit_dialog_height(Window& window, Rect area) {
    const Rect current = window.bounds();
    if (current.width <= 0 || current.height <= 0) return;  // not placed yet
    // The same measurement Desktop uses to place a dialog in the first
    // place, so opening at 30 rows and growing back to 30 rows are one rule.
    const int recommended =
        std::max(window.vertical_size_hint().preferred, window.height_for_width(current.width));
    const int height = std::clamp(recommended, 0, std::max(0, area.height));
    if (height == current.height) return;
    const int y = std::clamp(current.y, area.y, area.y + std::max(0, area.height - height));
    window.set_bounds(Rect{current.x, y, current.width, height});
}

BuiltDescriptorDialog build_descriptor_dialog(DialogDescriptor descriptor, const ui::StandardRoles& roles,
                                              ui::Application& app, ui::View* restore_focus_to,
                                              Desktop& desktop) {
    auto retained_descriptor = std::make_shared<DialogDescriptor>(std::move(descriptor));
    MaterializedDialog materialized = materialize_dialog(*retained_descriptor);
    std::vector<InputLine*> inputs = materialized.inputs;
    std::vector<CheckGroup*> checks = materialized.checks;
    std::vector<RadioGroup*> radios = materialized.radios;
    std::vector<ComboBox*> combos = materialized.combos;
    Button* const default_button = materialized.default_button;
    ui::View* const initial_focus = materialized.initial_focus;

    auto window = std::make_unique<Window>(retained_descriptor->title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    if (retained_descriptor->minimum_window_size.width > 0 ||
        retained_descriptor->minimum_window_size.height > 0)
        window->set_min_size(retained_descriptor->minimum_window_size);
    // A descriptor dialog is held off its frame like any other. Previously
    // it had no margin at all, which left fields and buttons against the
    // border and made each application add padding of its own -- the same
    // decision taken repeatedly, differently.
    window->set_content_margin(1, 1);
    Window* const window_ptr = window.get();
    auto completion = std::make_shared<DescriptorDialogCompletion>(
        DescriptorDialogCompletion{window_ptr->lifetime_token(), std::nullopt, false});
    const detail::DialogFocusRestore focus_restore{restore_focus_to};

    window->set_content(std::move(materialized.root));
    window->set_resizable(retained_descriptor->resizable);
    // The accept button's own callback, taken BEFORE the button is rewired
    // below: the accept path must run what the descriptor asked for, and the
    // button itself must run the accept path.
    const std::function<void()> accept_press =
        default_button != nullptr ? default_button->on_press : std::function<void()>{};
    window->accept_request = [window_ptr, completion, retained_descriptor, inputs, checks, radios, combos,
                              accept_press, &app]() {
        // This closure is owned by Window itself. Retain everything needed
        // after the descriptor callback before entering user code: that
        // callback is allowed to detach and destroy its own Window, which
        // destroys this std::function while it is executing.
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        Window* const held_window = window_ptr;
        if (!validate_inputs(inputs, *retained_descriptor, app)) return;
        // Record before application code runs: a descriptor button handler
        // may detach or destroy this Window synchronously, but its successful
        // acceptance still has one stable typed result after detachment.
        held_completion->selected_result =
            accepted_result(*retained_descriptor, inputs, checks, radios, combos);
        if (accept_press) accept_press();
        if (!held_completion->closed && !held_completion->window_liveness.expired()) held_window->close();
    };
    // Clicking the accept button IS accepting. Enter reaches the accept path
    // through the window; a mouse click reaches only the button, and a button
    // whose descriptor gave it nothing to do did nothing at all — OK buttons
    // everywhere worked from the keyboard and ignored the mouse. The button's
    // press is therefore the same request Enter makes, validation included.
    if (default_button != nullptr)
        default_button->on_press = [window_ptr]() {
            if (window_ptr->accept_request) window_ptr->accept_request();
        };
    std::function<void()> dismiss = [window_ptr, completion]() {
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        Window* const held_window = window_ptr;
        if (!held_completion->window_liveness.expired()) held_window->close();
    };
    window->cancel_request = dismiss;
    // The same closure, so a Cancel button is Esc rather than something that
    // resembles it. No result is recorded on this path, which is what makes
    // the completion's `accepted == false` true of both.
    wire_dismiss_buttons(*retained_descriptor, materialized.buttons, dismiss);
    // Re-fit on every desktop resize, for a dialog whose size is the
    // library's business. A `resizable` dialog is excluded on purpose: once
    // the reader has been offered a resize grip, the height is theirs, and
    // snapping it back to the descriptor's own idea of the right size at the
    // next terminal resize would undo a decision they made.
    //
    // Desktop::on_bounds_changed is View's documented geometry observer and
    // is CHAINED here, not claimed: whatever was installed still runs, and
    // on_closed puts it back. Modals close in the order they opened, so the
    // restore is in order too. What this deliberately does not see is a dock
    // appearing or leaving, which changes content_area() without changing
    // the Desktop's own bounds; a proper Desktop-side notification would
    // (see this package's report).
    Desktop* const desktop_ptr = &desktop;
    std::function<void(Rect)> previous_desktop_bounds_changed;
    if (!retained_descriptor->resizable) {
        previous_desktop_bounds_changed = desktop.on_bounds_changed;
        desktop.on_bounds_changed = [previous = previous_desktop_bounds_changed, desktop_ptr, window_ptr,
                                     liveness = window_ptr->lifetime_token()](Rect bounds) {
            if (previous) previous(bounds);
            if (liveness.expired()) return;
            refit_dialog_height(*window_ptr, desktop_ptr->content_area());
        };
    }
    const bool refit_installed = !retained_descriptor->resizable;
    window->on_closed = [&app, focus_restore, window_ptr, completion, desktop_ptr, refit_installed,
                         previous_desktop_bounds_changed]() {
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        Window* const held_window = window_ptr;
        held_completion->closed = true;
        if (refit_installed) desktop_ptr->on_bounds_changed = previous_desktop_bounds_changed;
        held_focus_restore.restore(app);
        if (!held_completion->window_liveness.expired()) schedule_self_detach(*held_window, app);
    };
    return BuiltDescriptorDialog{WindowHandle{std::move(window), initial_focus}, std::move(completion)};
}

}  // namespace

MaterializedDialog materialize_dialog(const DialogDescriptor& descriptor) {
    CKV_ASSERT(!descriptor.fields.empty() || !descriptor.buttons.empty());

    MaterializedDialog result;
    auto column = std::make_unique<Column>();
    column->set_spacing(kFieldSpacing);

    for (std::size_t index = 0; index < descriptor.fields.size(); ++index) {
        const FieldDescriptor& field = descriptor.fields[index];

        if (field.kind == FieldKind::Note) {
            // Consecutive notes are one paragraph, so they go into a column
            // of their own with no spacing. The form's own spacing of one
            // blank row is right between things a reader answers, and wrong
            // between the lines of a sentence — set loose there it turns an
            // explanation into a list of unrelated remarks.
            auto paragraph = std::make_unique<Column>();
            paragraph->set_spacing(0);
            int lines = 0;
            for (; index < descriptor.fields.size() &&
                   descriptor.fields[index].kind == FieldKind::Note;
                 ++index) {
                auto note = std::make_unique<Label>(descriptor.fields[index].label);
                auto* note_ptr = static_cast<Label*>(
                    paragraph->add_item(std::move(note), LayoutSpec{SizePolicy::Fixed, 1}));
                // Parallel arrays still get one entry per field: a note holds
                // the labels slot, since that is the only widget it made.
                result.labels.push_back(note_ptr);
                result.inputs.push_back(nullptr);
                result.checks.push_back(nullptr);
                result.radios.push_back(nullptr);
                result.combos.push_back(nullptr);
                ++lines;
            }
            --index;  // the loop's own ++ takes us past the last note
            column->add_item(std::move(paragraph), LayoutSpec{SizePolicy::Fixed, lines});
            continue;
        }

        auto row = std::make_unique<Row>();
        row->set_spacing(1);

        if (field.kind == FieldKind::Check) {
            // One box, carrying its own text. A CheckGroup of one is still a
            // CheckGroup: the reader gets the same "[X] label", the same
            // mnemonic handling and the same single Tab stop as in a cluster
            // of them, so a form does not read differently for having asked
            // only one yes/no question.
            auto check = std::make_unique<CheckGroup>(std::vector<std::string>{field.label});
            check->set_checked(0, field.initial_checked);
            auto* check_ptr =
                static_cast<CheckGroup*>(row->add_item(std::move(check), LayoutSpec{SizePolicy::Expanding, 1}));
            result.labels.push_back(nullptr);
            result.inputs.push_back(nullptr);
            result.checks.push_back(check_ptr);
            result.radios.push_back(nullptr);
            result.combos.push_back(nullptr);
            column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
            continue;
        }

        if (field.kind == FieldKind::Radio) {
            // The group carries its own caption above its choices, rather
            // than a Label beside them: a set of alternatives is one thing a
            // form asks about, and a label in the left column would put the
            // question level with the first answer.
            auto radio = std::make_unique<RadioGroup>(field.options);
            if (!field.label.empty()) radio->set_group_label(field.label);
            radio->set_selected(field.initial_selection);
            auto* radio_ptr =
                static_cast<RadioGroup*>(row->add_item(std::move(radio), LayoutSpec{SizePolicy::Expanding, 1}));
            result.labels.push_back(nullptr);
            result.inputs.push_back(nullptr);
            result.checks.push_back(nullptr);
            result.radios.push_back(radio_ptr);
            result.combos.push_back(nullptr);
            const int rows = static_cast<int>(field.options.size()) + (field.label.empty() ? 0 : 1);
            column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, std::max(1, rows)});
            continue;
        }

        Label* label_ptr = nullptr;
        if (!field.label.empty()) {
            auto label = std::make_unique<Label>(field.label);
            label_ptr = static_cast<Label*>(row->add_item(std::move(label), LayoutSpec{SizePolicy::Fixed, 1}));
        }

        if (field.kind == FieldKind::Combo) {
            auto combo = std::make_unique<ComboBox>(field.editable ? ComboBoxMode::Editable
                                                                   : ComboBoxMode::PickOnly);
            combo->set_items(field.options);
            if (field.initial_selection >= 0 &&
                field.initial_selection < static_cast<int>(field.options.size()))
                combo->set_selected_index(static_cast<std::size_t>(field.initial_selection));
            else if (!field.initial_text.empty())
                combo->set_text(field.initial_text);
            auto* combo_ptr =
                static_cast<ComboBox*>(row->add_item(std::move(combo), LayoutSpec{SizePolicy::Expanding, 1}));
            if (label_ptr != nullptr) label_ptr->set_buddy(combo_ptr);
            result.labels.push_back(label_ptr);
            result.inputs.push_back(nullptr);
            result.checks.push_back(nullptr);
            result.radios.push_back(nullptr);
            result.combos.push_back(combo_ptr);
            column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
            continue;
        }

        // Text and Number are the same widget; what separates them is the
        // rule the accept path applies, not what the reader types into.
        auto input = std::make_unique<InputLine>();
        input->set_text(field.initial_text);
        input->set_password_echo(field.password_echo, field.password_echo_char);
        auto* input_ptr =
            static_cast<InputLine*>(row->add_item(std::move(input), LayoutSpec{SizePolicy::Expanding, 1}));

        if (label_ptr != nullptr) label_ptr->set_buddy(input_ptr);
        result.labels.push_back(label_ptr);
        result.inputs.push_back(input_ptr);
        result.checks.push_back(nullptr);
        result.radios.push_back(nullptr);
        result.combos.push_back(nullptr);

        column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
    }

    // The fields go into the pane's viewport whatever their height: there is
    // one tree shape, and the fit is decided in layout (see DialogPane).
    auto pane = std::make_unique<DialogPane>(descriptor.button_alignment,
                                             descriptor.anchor_buttons_to_bottom);
    result.content_viewport = pane->set_fields(std::move(column));

    if (!descriptor.buttons.empty()) {
        auto button_row = std::make_unique<Row>();
        button_row->set_spacing(2);
        for (const ButtonDescriptor& bd : descriptor.buttons) {
            auto button = std::make_unique<Button>(bd.label);
            button->set_default(bd.role == ButtonRole::Accept);
            // What a Dismiss button does beyond its own handler needs a window
            // to do it to, so it is wired by whoever hosts this tree in one
            // (wire_dialog_window, present_dialog). Materializing alone leaves
            // a dialog that has no window to close.
            button->on_press = bd.on_press;
            auto* button_ptr =
                static_cast<Button*>(button_row->add_item(std::move(button), LayoutSpec{SizePolicy::Fixed, 1}));
            result.buttons.push_back(button_ptr);
            if (bd.role == ButtonRole::Accept) {
                CKV_ASSERT(result.default_button == nullptr);  // at most one accepting button
                result.default_button = button_ptr;
            }
        }
        // Outside the viewport, so the one thing a clipped dialog used to
        // lose is the one thing that can no longer scroll away.
        pane->set_buttons(std::move(button_row));
    }

    // Fields before buttons, exactly as the Column's own child order gave
    // it: the viewport is added first and is not itself a tab stop.
    result.initial_focus = find_first_focusable(*pane);
    result.root = std::move(pane);
    return result;
}

bool validate_dialog(MaterializedDialog& dialog, const DialogDescriptor& descriptor, ui::Application& app) {
    return validate_inputs(dialog.inputs, descriptor, app);
}

void wire_dialog_window(Window& window, MaterializedDialog dialog, const DialogDescriptor& descriptor,
                         ui::Application& app, ui::View* restore_focus_to) {
    // Copy what accept_request needs before dialog.root (which owns
    // these views) is moved into the window's content — the raw
    // pointers stay valid afterward (set_content only reparents).
    // Captured by value (plain vectors/pointers, all copyable) rather
    // than the whole MaterializedDialog, which holds a unique_ptr and
    // so cannot be captured into a std::function at all.
    std::vector<InputLine*> inputs = dialog.inputs;
    std::vector<Button*> buttons = dialog.buttons;
    Button* default_button = dialog.default_button;

    window.set_content(std::move(dialog.root));
    window.set_resizable(descriptor.resizable);

    // A default-button callback may close, detach, or destroy its hosting
    // window. Keep the close state and per-instance lifetime identity outside
    // that Window so the accept path neither double-closes it nor dereferences
    // a destroyed object after application code returns.
    struct CloseState {
        std::weak_ptr<void> window_liveness;
        bool closed = false;
    };
    auto close_state = std::make_shared<CloseState>(CloseState{window.lifetime_token(), false});
    const detail::DialogFocusRestore focus_restore{restore_focus_to};

    const std::function<void()> accept_press =
        default_button != nullptr ? default_button->on_press : std::function<void()>{};
    window.accept_request = [&window, &app, inputs, &descriptor, accept_press, close_state]() {
        const std::shared_ptr<CloseState> held_close_state = close_state;
        Window* const held_window = &window;
        if (!validate_inputs(inputs, descriptor, app)) return;  // veto: invalid field already took focus
        if (accept_press) accept_press();
        if (!held_close_state->closed && !held_close_state->window_liveness.expired()) held_window->close();
    };
    // Same rewiring as build_descriptor_dialog, same reason: a click on the
    // accept button is the accept request, not a press of an unwired button.
    if (default_button != nullptr) {
        Window* const window_ptr = &window;
        default_button->on_press = [window_ptr]() {
            if (window_ptr->accept_request) window_ptr->accept_request();
        };
    }
    std::function<void()> dismiss = [&window]() { window.close(); };  // bypasses validation by definition
    window.cancel_request = dismiss;                                  // Esc
    wire_dismiss_buttons(descriptor, buttons, dismiss);               // and every button that says so

    auto previous_on_closed = window.on_closed;
    window.on_closed = [&app, focus_restore, previous_on_closed, close_state]() {
        const std::shared_ptr<CloseState> held_close_state = close_state;
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::function<void()> held_previous_on_closed = previous_on_closed;
        held_close_state->closed = true;
        held_focus_restore.restore(app);
        if (held_previous_on_closed) held_previous_on_closed();
    };
}

DescriptorDialogPresentation present_dialog(DialogDescriptor descriptor, ui::Application& app,
                                             Desktop& desktop, const ui::StandardRoles& roles) {
    using Access = detail::DialogPresentationAccess<DialogResult>;
    auto parts = Access::make();
    BuiltDescriptorDialog built = build_descriptor_dialog(std::move(descriptor), roles, app, app.focused(), desktop);
    auto previous_on_detached = std::move(built.handle.window->on_detached);
    built.handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state,
                                        completion = built.completion]() {
        if (previous) previous();
        if (completion->selected_result) Access::record(state, *completion->selected_result);
        Access::finish(state, DialogResult{});
    };
    desktop.present_modal(std::move(built.handle), app);
    return std::move(parts.presentation);
}

DialogResult exec_dialog(DialogDescriptor descriptor, ui::Application& app,
                         Desktop& desktop, const ui::StandardRoles& roles) {
    BuiltDescriptorDialog built = build_descriptor_dialog(std::move(descriptor), roles, app, app.focused(), desktop);
    const std::shared_ptr<DescriptorDialogCompletion> completion = built.completion;
    desktop.exec_modal(app, std::move(built.handle));
    return completion->selected_result.value_or(DialogResult{});
}

}  // namespace ckv::widgets
