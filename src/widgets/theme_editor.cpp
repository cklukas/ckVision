// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/theme_editor.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/style.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/theme_format.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/table.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// The role table's columns, in order, and their widths. With the blank cell
// after each column and the scrollbar they fill the 74 cells the table asks
// for, which with a cell of air either side is the 78-cell window's interior:
// as wide as an 80-column terminal holds with the window's shadow. Six rows
// are the heading and five roles, which with the editors below makes the
// window 22 rows tall, the height an 80x24 terminal has under a menu bar and
// a status line.
enum TableField : std::size_t { kRoleField, kSampleField, kForegroundField, kBackgroundField, kAttributesField };
constexpr std::array<int, 5> kFieldWidths{30, 6, 10, 10, 12};
constexpr Size kTableSize{74, 6};

// The three colour slots of a style, each with an editor of its own.
enum ColorSlot : std::size_t { kForeground, kBackground, kUnderline };

// The three kinds a Color can be, in the order the kind choice lists them.
enum class ColorKind : int { Default = 0, Palette = 1, Rgb = 2 };

// ckv::Attr's flags in bit order: the order of the check boxes and of the
// names in the role table.
constexpr std::array<Attr, 6> kAttrFlags{Attr::Bold, Attr::Dim, Attr::Italic, Attr::Underline, Attr::Reverse,
                                         Attr::Strike};

// ckv::UnderlineShape's values in declaration order: the order of the shape
// choices.
constexpr std::array<UnderlineShape, 5> kUnderlineShapes{UnderlineShape::Straight, UnderlineShape::Double,
                                                         UnderlineShape::Curly, UnderlineShape::Dotted,
                                                         UnderlineShape::Dashed};

int shape_index(UnderlineShape shape) noexcept {
    return static_cast<int>(std::find(kUnderlineShapes.begin(), kUnderlineShapes.end(), shape) -
                            kUnderlineShapes.begin());
}

ColorKind kind_of(Color color) noexcept {
    if (color.is_indexed()) return ColorKind::Palette;
    if (color.is_rgb()) return ColorKind::Rgb;
    return ColorKind::Default;
}

// What the value field shows for `color`: the index of a palette entry, the
// #RRGGBB of an RGB colour, and nothing for the default, which has no value.
std::string value_text(Color color) {
    if (color.is_indexed()) return std::to_string(static_cast<int>(color.index()));
    if (color.is_rgb()) return ui::format_color(color);
    return {};
}

// The colour a value field's text names under `kind`, or nullopt when it
// names none: canonical decimal 0-255 for a palette entry, #RRGGBB for RGB.
std::optional<Color> parse_value(ColorKind kind, const std::string& text) {
    switch (kind) {
        case ColorKind::Default: return Color::default_color();
        case ColorKind::Palette: {
            if (text.empty() || text.front() == '@') return std::nullopt;
            return ui::parse_color("@" + text);
        }
        case ColorKind::Rgb: {
            const std::optional<Color> color = ui::parse_color(text);
            if (!color || !color->is_rgb()) return std::nullopt;
            return color;
        }
    }
    return std::nullopt;
}

// The palette entry closest to an RGB colour by squared distance in RGB
// space; the lowest index wins a tie, so the answer is deterministic.
Color nearest_palette_entry(Color rgb) noexcept {
    int best = 0;
    int best_distance = std::numeric_limits<int>::max();
    for (int index = 0; index < 256; ++index) {
        const Color entry = palette_color(index);
        const int dr = int{entry.r()} - int{rgb.r()};
        const int dg = int{entry.g()} - int{rgb.g()};
        const int db = int{entry.b()} - int{rgb.b()};
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < best_distance) {
            best = index;
            best_distance = distance;
        }
    }
    return Color::indexed(static_cast<std::uint8_t>(best));
}

// The colour `slot` names in `style`, writable when `style` is.
template <typename StyleT>
auto& slot_color(StyleT& style, ColorSlot slot) noexcept {
    if (slot == kForeground) return style.fg;
    if (slot == kBackground) return style.bg;
    return style.underline_color;
}

// The colour `slot` of `style` is seen in. The terminal's default has no
// value of its own, so it stands for the entry a terminal conventionally
// shows for a foreground (7) or a background (0), and for an underline the
// text's colour, which a default underline follows.
Color shown_color(const Style& style, ColorSlot slot) noexcept {
    const Color color = slot_color(style, slot);
    if (!color.is_default()) return color;
    switch (slot) {
        case kForeground: return Color::indexed(7);
        case kBackground: return Color::indexed(0);
        case kUnderline: break;
    }
    return shown_color(style, kForeground);
}

// `style`'s colour in `slot` re-expressed as `kind`: a palette entry as the
// RGB value it names, an RGB colour as the nearest palette entry, and the
// default as the colour it is seen in (shown_color).
Color convert(const Style& style, ColorSlot slot, ColorKind kind) noexcept {
    if (kind == ColorKind::Default) return Color::default_color();
    const Color color = shown_color(style, slot);
    if (kind == ColorKind::Palette) return color.is_rgb() ? nearest_palette_entry(color) : color;
    return color.is_indexed() ? palette_color(color.index()) : color;
}

// The preview's ground: the dialog surface as the edited theme draws it, so
// the sample widgets stand on what they would stand on in a dialog.
class PreviewSurface final : public Column {
public:
    void on_attached() override { background_ = context().roles->find("ckv.dialog.background"); }
    void draw(scene::Painter& painter) override {
        const Style style = background_ != ui::kInvalidRole ? context().theme->resolve(background_) : Style{};
        painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    }

private:
    ui::RoleId background_ = ui::kInvalidRole;
};

// Everything the editor's controls share: the theme being edited, the role
// table's rows, and the controls that show the role under the cursor. It is
// also the role table's model. The closures installed on the window's
// controls keep it alive, the table's own among them, so it outlives every
// view that refers to it.
class ThemeEditorState final : public TableModel {
public:
    ThemeEditorState(const ui::Theme& theme, const StandardStrings& strings)
        : initial_(theme),
          working_(theme),
          sample_text_(strings.theme_sample),
          attribute_names_{strings.theme_bold,      strings.theme_dim,     strings.theme_italic,
                           strings.theme_underline, strings.theme_reverse, strings.theme_strike} {
        const ui::RoleRegistry& registry = theme.registry();
        order_.reserve(registry.size());
        for (std::size_t index = 0; index < registry.size(); ++index) order_.push_back(static_cast<ui::RoleId>(index));
        sort_rows();
    }

    const ui::Theme& working() const noexcept { return working_; }
    ui::RoleId current() const noexcept { return current_; }

    // The controls, wired once the window is built.
    Table* table = nullptr;
    std::array<RadioGroup*, 3> kinds{};
    std::array<InputLine*, 3> values{};
    CheckGroup* attributes = nullptr;
    RadioGroup* shapes = nullptr;
    // The underline's shape and colour editors together, enabled only while
    // the role is underlined.
    ui::View* underline_editors = nullptr;
    StaticText* sample = nullptr;
    PreviewSurface* preview = nullptr;

    // TableModel: one row per role, identified by its id plus one.
    std::size_t row_count() const override { return order_.size(); }
    TableRowId row_id_at(std::size_t display_index) const override {
        return static_cast<TableRowId>(order_[display_index]) + 1;
    }
    std::optional<std::size_t> index_of(TableRowId row) const override {
        if (row == kInvalidTableRowId) return std::nullopt;
        const auto found = std::find(order_.begin(), order_.end(), static_cast<ui::RoleId>(row - 1));
        if (found == order_.end()) return std::nullopt;
        return static_cast<std::size_t>(found - order_.begin());
    }
    TableCell cell(TableCellRef reference) const override {
        const auto role = static_cast<ui::RoleId>(reference.row - 1);
        TableCell cell;
        cell.editable = false;
        cell.display = field_text(role, reference.column);
        cell.value = cell.display;
        if (reference.column == kSampleField) cell.style = working_.resolve(role);
        return cell;
    }
    void request_sort(std::optional<std::size_t> column, bool ascending) override {
        sort_field_ = column;
        sort_ascending_ = ascending;
        sort_rows();
    }

    // Shows `role` in the editors, the sample and the preview.
    void load(ui::RoleId role) {
        current_ = role;
        const Style style = working_.resolve(role);
        loading_ = true;
        for (const ColorSlot slot : {kForeground, kBackground}) show_color(slot, slot_color(style, slot));
        for (std::size_t index = 0; index < kAttrFlags.size(); ++index)
            attributes->set_checked(index, has_attr(style.attrs, kAttrFlags[index]));
        show_underline(style);
        loading_ = false;
        sample->set_text(" " + working_.registry().name(role) + " ");
        sample->set_role_override(role);
        preview->set_theme_override(working_);
    }

    // The reader chose another kind of colour for `slot`.
    void choose_kind(ColorSlot slot, int index) {
        if (loading_ || current_ == ui::kInvalidRole) return;
        Style style = working_.resolve(current_);
        Color& color = slot_color(style, slot);
        color = convert(style, slot, static_cast<ColorKind>(index));
        loading_ = true;
        show_color(slot, color);
        loading_ = false;
        apply(style);
    }

    // The reader chose the underline shape at `index`. Only an underlined
    // role reaches here: the choice is disabled otherwise.
    void choose_shape(int index) {
        if (loading_ || current_ == ui::kInvalidRole || index < 0) return;
        Style style = working_.resolve(current_);
        style.underline = kUnderlineShapes[static_cast<std::size_t>(index)];
        apply(style);
    }

    // The reader edited `slot`'s value field. An invalid value changes
    // nothing; the field shows it as invalid through its validator.
    void edit_value(ColorSlot slot) {
        if (loading_ || current_ == ui::kInvalidRole) return;
        const std::optional<Color> color = parse_value(kind(slot), values[slot]->text());
        if (!color) return;
        Style style = working_.resolve(current_);
        slot_color(style, slot) = *color;
        apply(style);
    }

    // Whether `text` is a value `slot`'s field can hold under its kind.
    bool valid_value(ColorSlot slot, const std::string& text) const {
        return kind(slot) == ColorKind::Default || parse_value(kind(slot), text).has_value();
    }

    // Whether the reader may type `grapheme` into `slot`'s field: a decimal
    // digit for a palette index; '#' or a hexadecimal digit for RGB.
    bool admits(ColorSlot slot, std::string_view grapheme) const {
        if (grapheme.size() != 1) return false;
        const char c = grapheme.front();
        const bool digit = c >= '0' && c <= '9';
        if (kind(slot) == ColorKind::Palette) return digit;
        return digit || c == '#' || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    // The reader toggled attribute `index`.
    void toggle_attribute(std::size_t index, bool on) {
        if (loading_ || current_ == ui::kInvalidRole) return;
        Style style = working_.resolve(current_);
        const auto bits = static_cast<std::uint8_t>(style.attrs);
        const auto flag = static_cast<std::uint8_t>(kAttrFlags[index]);
        style.attrs = static_cast<Attr>(on ? (bits | flag) : (bits & ~flag));
        if (!has_attr(style.attrs, Attr::Underline)) {
            style.underline = UnderlineShape::Straight;
            style.underline_color = Color::default_color();
        }
        apply(style);
        if (kAttrFlags[index] != Attr::Underline) return;
        loading_ = true;
        show_underline(style);
        loading_ = false;
    }

    // Puts the current role back the way the editor found it.
    void revert() {
        if (current_ == ui::kInvalidRole) return;
        apply(initial_.resolve(current_));
        load(current_);
    }

private:
    // Fills `slot`'s editor with `color`: its kind, its value, and a value
    // field enabled only when the kind has a value. Called while loading_.
    void show_color(ColorSlot slot, Color color) {
        kinds[slot]->set_selected(static_cast<int>(kind_of(color)));
        values[slot]->set_text(value_text(color));
        values[slot]->set_enabled(!color.is_default());
    }

    // Fills the underline's editors from `style`, enabled only while `style`
    // is underlined. Called while loading_.
    void show_underline(const Style& style) {
        shapes->set_selected(shape_index(style.underline));
        show_color(kUnderline, style.underline_color);
        underline_editors->set_enabled(has_attr(style.attrs, Attr::Underline));
    }

    ColorKind kind(ColorSlot slot) const noexcept {
        const int selected = kinds[slot] != nullptr ? kinds[slot]->selected() : 0;
        return selected < 0 ? ColorKind::Default : static_cast<ColorKind>(selected);
    }

    std::string field_text(ui::RoleId role, std::size_t field) const {
        const Style style = working_.resolve(role);
        switch (field) {
            case kRoleField: return working_.registry().name(role);
            case kSampleField: return sample_text_;
            case kForegroundField: return ui::format_color(style.fg);
            case kBackgroundField: return ui::format_color(style.bg);
            default: break;
        }
        std::string names;
        for (std::size_t index = 0; index < kAttrFlags.size(); ++index) {
            if (!has_attr(style.attrs, kAttrFlags[index])) continue;
            if (!names.empty()) names += ' ';
            names += attribute_names_[index];
        }
        return names;
    }

    // Name order first, so rows that tie under the sort column stay grouped
    // by family; then the sort column, if one was asked for.
    void sort_rows() {
        const ui::RoleRegistry& registry = working_.registry();
        std::sort(order_.begin(), order_.end(),
                  [&registry](ui::RoleId a, ui::RoleId b) { return registry.name(a) < registry.name(b); });
        if (!sort_field_) return;
        const std::size_t field = *sort_field_;
        std::stable_sort(order_.begin(), order_.end(), [this, field](ui::RoleId a, ui::RoleId b) {
            return sort_ascending_ ? field_text(a, field) < field_text(b, field)
                                   : field_text(b, field) < field_text(a, field);
        });
    }

    // Makes `style` the current role's, and shows it everywhere it appears.
    void apply(const Style& style) {
        working_.set(current_, style);
        if (sort_field_) sort_rows();
        table->model_changed();
        preview->set_theme_override(working_);
    }

    ui::Theme initial_;
    ui::Theme working_;
    std::string sample_text_;
    std::array<std::string, 6> attribute_names_;
    std::vector<ui::RoleId> order_;
    std::optional<std::size_t> sort_field_;
    bool sort_ascending_ = true;
    ui::RoleId current_ = ui::kInvalidRole;
    // Set while load() fills the controls, whose change notifications would
    // otherwise write the values they are being given straight back.
    bool loading_ = false;
};

// Completion state outlives the dialog because the result callback may
// detach and destroy it; the single-shot mark also refuses a callback's
// reentrant attempt to answer twice.
struct ThemeEditorCompletion {
    std::weak_ptr<void> window_liveness;
    std::function<void(ThemeEditorResult)> on_result;
    bool delivered = false;

    void report(ThemeEditorResult result, Window* window) {
        if (delivered) return;
        delivered = true;
        if (on_result) on_result(std::move(result));
        if (!window_liveness.expired()) window->close();
    }
};

std::string without_mnemonic(const std::string& caption) { return parse_mnemonic(caption).display; }

// The label, kind choice and value field that edit one colour slot.
std::unique_ptr<Row> color_editor(const std::shared_ptr<ThemeEditorState>& state, ColorSlot slot,
                                  const std::string& caption, int label_width, const StandardStrings& strings) {
    auto row = std::make_unique<Row>();
    row->set_spacing(2);
    auto label = std::make_unique<Label>(caption);
    label->set_column_width(label_width);
    auto* label_ptr = static_cast<Label*>(row->add_item(std::move(label), LayoutSpec{SizePolicy::Fixed, 1}));

    auto kinds = std::make_unique<RadioGroup>(
        std::vector<std::string>{strings.theme_color_default, strings.theme_color_palette, strings.theme_color_rgb});
    kinds->set_columns(3);
    auto* kinds_ptr = static_cast<RadioGroup*>(row->add_item(std::move(kinds), LayoutSpec{SizePolicy::Fixed, 1}));
    kinds_ptr->on_changed = [state, slot](int index) { state->choose_kind(slot, index); };
    label_ptr->set_buddy(kinds_ptr);

    auto value = std::make_unique<InputLine>();
    auto* value_ptr = static_cast<InputLine*>(row->add_item(std::move(value), LayoutSpec{SizePolicy::Fixed, 1}));
    state->kinds[slot] = kinds_ptr;
    state->values[slot] = value_ptr;
    value_ptr->set_validator([state, slot](const std::string& text) { return state->valid_value(slot, text); });
    value_ptr->set_grapheme_filter([state, slot](std::string_view grapheme) { return state->admits(slot, grapheme); });
    value_ptr->on_edited = [state, slot]() { state->edit_value(slot); };
    return row;
}

// The underline's two editors, one row each: the shape it is drawn with, and
// the colour it is drawn in.
std::unique_ptr<Column> underline_editors(const std::shared_ptr<ThemeEditorState>& state, int label_width,
                                          const StandardStrings& strings) {
    auto column = std::make_unique<Column>();
    auto shape_row = std::make_unique<Row>();
    shape_row->set_spacing(2);
    auto label = std::make_unique<Label>(strings.theme_underline_shape);
    label->set_column_width(label_width);
    auto* label_ptr = static_cast<Label*>(shape_row->add_item(std::move(label), LayoutSpec{SizePolicy::Fixed, 1}));
    auto shapes = std::make_unique<RadioGroup>(
        std::vector<std::string>{strings.theme_shape_straight, strings.theme_shape_double, strings.theme_shape_curly,
                                 strings.theme_shape_dotted, strings.theme_shape_dashed});
    shapes->set_columns(static_cast<int>(kUnderlineShapes.size()));
    state->shapes =
        static_cast<RadioGroup*>(shape_row->add_item(std::move(shapes), LayoutSpec{SizePolicy::Fixed, 1}));
    state->shapes->on_changed = [state](int index) { state->choose_shape(index); };
    label_ptr->set_buddy(state->shapes);
    column->add_item(std::move(shape_row), LayoutSpec{SizePolicy::Fixed, 1});
    column->add_item(color_editor(state, kUnderline, strings.theme_underline_color, label_width, strings),
                     LayoutSpec{SizePolicy::Fixed, 1});
    state->underline_editors = column.get();
    return column;
}

// The live preview: the role's own name in its own style, then a row of
// representative controls. None of them takes the keyboard.
std::unique_ptr<PreviewSurface> preview(ThemeEditorState& state, const StandardStrings& strings) {
    auto surface = std::make_unique<PreviewSurface>();
    auto sample = std::make_unique<StaticText>(std::string{});
    sample->set_preformatted(true);
    state.sample = static_cast<StaticText*>(
        surface->add_item(std::move(sample), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start}));

    auto controls = std::make_unique<Row>();
    controls->set_spacing(2);
    const LayoutSpec one_row{SizePolicy::Fixed, 1, ui::Alignment::Start};
    controls->add_item(std::make_unique<Label>(strings.theme_preview_label), one_row);
    auto field = std::make_unique<InputLine>();
    field->set_text(strings.theme_preview_text);
    field->set_focus_policy(ui::FocusPolicy::None);
    controls->add_item(std::move(field), one_row);
    auto ok = std::make_unique<Button>(strings.ok);
    ok->set_default(true);
    ok->set_focus_policy(ui::FocusPolicy::None);
    controls->add_item(std::move(ok), LayoutSpec{SizePolicy::Fixed, 1});
    auto cancel = std::make_unique<Button>(strings.cancel);
    cancel->set_focus_policy(ui::FocusPolicy::None);
    controls->add_item(std::move(cancel), LayoutSpec{SizePolicy::Fixed, 1});
    auto option = std::make_unique<CheckGroup>(std::vector<std::string>{strings.theme_preview_option});
    option->set_checked(0, true);
    option->set_focus_policy(ui::FocusPolicy::None);
    controls->add_item(std::move(option), one_row);
    auto list = std::make_unique<ListView>();
    list->set_items({strings.theme_preview_first, strings.theme_preview_second});
    list->set_scrollbar_policy(ScrollbarPolicy::Auto);
    list->set_focus_policy(ui::FocusPolicy::None);
    controls->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1, ui::Alignment::Fill});
    surface->add_item(std::move(controls), LayoutSpec{SizePolicy::Fixed, 1});
    return surface;
}

}  // namespace

WindowHandle make_theme_editor(const ui::Theme& theme, const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(ThemeEditorResult)> on_result,
                               const StandardStrings& strings) {
    auto window = std::make_unique<Window>(strings.theme_editor_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    window->set_resizable(false);
    window->set_content_margin(1, 0);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<ThemeEditorCompletion>(
        ThemeEditorCompletion{window_ptr->lifetime_token(), std::move(on_result)});
    auto state = std::make_shared<ThemeEditorState>(theme, strings);

    auto column = std::make_unique<Column>();
    column->set_spacing(1);
    const LayoutSpec fixed{SizePolicy::Fixed, 1};

    auto table = std::make_unique<Table>();
    table->set_columns({
        TableColumn{strings.theme_role, kFieldWidths[kRoleField], 4},
        TableColumn{strings.theme_sample, kFieldWidths[kSampleField], 4},
        TableColumn{without_mnemonic(strings.theme_foreground), kFieldWidths[kForegroundField], 4},
        TableColumn{without_mnemonic(strings.theme_background), kFieldWidths[kBackgroundField], 4},
        TableColumn{without_mnemonic(strings.theme_attributes), kFieldWidths[kAttributesField], 4},
    });
    table->set_preferred_size(kTableSize);
    table->set_model(*state);
    auto* table_ptr = static_cast<Table*>(
        column->add_item(std::move(table), LayoutSpec{SizePolicy::Expanding, 1}));
    state->table = table_ptr;
    table_ptr->on_selection_changed = [state](TableCellRef reference) {
        const auto role = static_cast<ui::RoleId>(reference.row - 1);
        if (role != state->current()) state->load(role);
    };
    table_ptr->on_sort_requested = [state](std::optional<TableCellRef>, std::optional<std::size_t>, bool) {
        state->table->model_changed();
    };

    const int label_width = std::max({text::text_width(without_mnemonic(strings.theme_foreground)),
                                      text::text_width(without_mnemonic(strings.theme_background)),
                                      text::text_width(without_mnemonic(strings.theme_attributes)),
                                      text::text_width(without_mnemonic(strings.theme_underline_shape)),
                                      text::text_width(without_mnemonic(strings.theme_underline_color))});
    auto editors = std::make_unique<Column>();
    editors->add_item(color_editor(state, kForeground, strings.theme_foreground, label_width, strings),
                      LayoutSpec{SizePolicy::Fixed, 1});
    editors->add_item(color_editor(state, kBackground, strings.theme_background, label_width, strings),
                      LayoutSpec{SizePolicy::Fixed, 1});
    auto attributes_row = std::make_unique<Row>();
    attributes_row->set_spacing(2);
    auto attributes_label = std::make_unique<Label>(strings.theme_attributes);
    attributes_label->set_column_width(label_width);
    auto* attributes_label_ptr = static_cast<Label*>(
        attributes_row->add_item(std::move(attributes_label), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start}));
    auto attributes = std::make_unique<CheckGroup>(std::vector<std::string>{
        strings.theme_bold, strings.theme_dim, strings.theme_italic, strings.theme_underline, strings.theme_reverse,
        strings.theme_strike});
    attributes->set_columns(3);
    state->attributes =
        static_cast<CheckGroup*>(attributes_row->add_item(std::move(attributes), LayoutSpec{SizePolicy::Fixed, 1}));
    state->attributes->on_changed = [state](std::size_t index, bool on) { state->toggle_attribute(index, on); };
    attributes_label_ptr->set_buddy(state->attributes);
    editors->add_item(std::move(attributes_row), LayoutSpec{SizePolicy::Fixed, 1});
    editors->add_item(underline_editors(state, label_width, strings), LayoutSpec{SizePolicy::Fixed, 1});
    column->add_item(std::move(editors), fixed);

    state->preview = static_cast<PreviewSurface*>(column->add_item(preview(*state, strings), fixed));

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    auto ok_button = std::make_unique<Button>(strings.ok);
    ok_button->set_default(true);
    auto* ok_ptr = static_cast<Button*>(button_row->add_item(std::move(ok_button), LayoutSpec{SizePolicy::Fixed, 1}));
    auto* cancel_ptr = static_cast<Button*>(
        button_row->add_item(std::make_unique<Button>(strings.cancel), LayoutSpec{SizePolicy::Fixed, 1}));
    auto* revert_ptr = static_cast<Button*>(
        button_row->add_item(std::make_unique<Button>(strings.theme_revert), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), fixed);
    window->set_content(std::move(column));

    ok_ptr->on_press = [state, window_ptr, completion]() {
        const std::shared_ptr<ThemeEditorCompletion> held_completion = completion;
        held_completion->report(ThemeEditorResult{state->working()}, window_ptr);
    };
    cancel_ptr->on_press = [window_ptr, completion]() {
        const std::shared_ptr<ThemeEditorCompletion> held_completion = completion;
        held_completion->report(ThemeEditorResult{}, window_ptr);
    };
    revert_ptr->on_press = [state]() { state->revert(); };
    window_ptr->accept_request = [ok_ptr]() {
        if (ok_ptr->on_press) ok_ptr->on_press();
    };
    window_ptr->cancel_request = [cancel_ptr]() {
        if (cancel_ptr->on_press) cancel_ptr->on_press();
    };
    window_ptr->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    if (state->row_count() > 0) state->load(static_cast<ui::RoleId>(state->row_id_at(0) - 1));
    return WindowHandle{std::move(window), table_ptr};
}

ThemeEditorPresentation present_modal_theme_editor(const ui::Theme& theme, ui::Application& app, Desktop& desktop,
                                                   const ui::StandardRoles& roles, const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<ThemeEditorResult>;
    auto parts = Access::make();
    auto handle = make_theme_editor(
        theme, roles, app, app.focused(),
        [state = parts.state](ThemeEditorResult result) { Access::record(state, std::move(result)); }, strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, ThemeEditorResult{});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
