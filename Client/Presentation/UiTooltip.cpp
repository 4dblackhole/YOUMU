#include "UiTooltip.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
// The shared line breaking of the options notices (the engine lays text out on one line).
#include "Presentation/OptionsPanel/Submodules/OptionsPanelStyle.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    constexpr float PaddingX = 10, PaddingY = 6, Gap = 10, Offset = 8, Margin = 8, ArrowSize = 10;
    constexpr float TextSize = 13, LineHeight = 18, ChipSize = 12, ChipHeight = 20, ChipMinWidth = 20;
    constexpr float ChipPadding = 5, MaxWidth = 300;
    constexpr auto PlateColour = ui_colors::theme::BrandDeep;
    constexpr Color NoteColour{1, 1, 1, .66F}, ChipColour{1, 1, 1, .14F}, ChipLineColour{1, 1, 1, .26F};
    constexpr Color SeparatorColour{1, 1, 1, .55F};

    template <typename Component> Component &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (component == nullptr)
            throw std::logic_error("A tooltip node is missing a required component.");
        return *component;
    }
    void Paint(Visual2DNode &node, const Color colour)
    {
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        Require<SpriteVisualComponent>(node).SetStyle(style);
    }
    Visual2DNode &Text(Visual2DNode &parent, std::string name, const Color colour, const std::uint16_t weight)
    {
        auto &label = CreateLabel(parent, {}, L"", std::move(name));
        auto &text = Require<TextVisualComponent>(label);
        text.SetFontSize(TextSize);
        text.SetFontWeight(weight);
        text.SetTextColor(colour);
        return label;
    }
    // Width before the renderer has measured a string.
    float EstimateWidth(const std::wstring_view text, const float size) noexcept
    {
        float width = 0;
        for (const wchar_t character : text)
            width += size * (character >= 0x1100 ? 1.0F : (character == L' ' ? 0.3F : 0.58F));
        return width;
    }
    std::wstring Trim(std::wstring_view value)
    {
        while (!value.empty() && value.front() == L' ')
            value.remove_prefix(1);
        while (!value.empty() && value.back() == L' ')
            value.remove_suffix(1);
        return std::wstring(value);
    }
} // namespace

void UiTooltip::Initialize(Visual2DCanvas &canvas)
{
    canvas_ = &canvas;
    root_ = &canvas.Root().CreateChild("Tooltip");
    root_->SetZIndex(300);
    arrow_ = &CreateSprite(*root_, {0, 0, ArrowSize, ArrowSize}, {}, "Arrow");
    Paint(*arrow_, PlateColour);
    arrow_->SetPivot({.5F, .5F});
    arrow_->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, std::numbers::pi_v<float> * .25F);
    plate_ = &CreateSprite(*root_, {}, {}, "Plate");
    Paint(*plate_, PlateColour);
    label_ = &Text(*root_, "Label", ui_colors::White, 600);
    note_ = &Text(*root_, "Note", NoteColour, 500);
    root_->SetVisible(false);
}

void UiTooltip::Shutdown() noexcept
{
    hints_.clear();
    lines_.clear();
    chips_.clear();
    separators_.clear();
    keyTexts_.clear();
    separatorTexts_.clear();
    root_ = arrow_ = plate_ = label_ = note_ = nullptr;
    canvas_ = nullptr;
    armed_ = dismissed_ = {};
    shown_ = false;
}

void UiTooltip::Set(const Visual2DNode &target, TooltipText text)
{
    const auto id = target.Id();
    if (text.Empty())
    {
        hints_.erase(id);
        lines_.erase(id);
        if (armed_ == id)
            Hide();
        return;
    }
    const bool changed = shown_ && armed_ == id;
    hints_.insert_or_assign(id, std::move(text));
    if (changed)
        Show();
}

void UiTooltip::SetLine(const Visual2DNode &line, TooltipText text)
{
    const bool empty = text.Empty();
    Set(line, std::move(text));
    if (!empty)
        lines_.insert(line.Id());
}

NodeId UiTooltip::ResolveTarget(const NodeId hovered, const std::optional<Point> pointer) const
{
    if (canvas_ == nullptr || hovered == NodeId{})
        return {};
    for (const auto *node = canvas_->FindNode(hovered); node != nullptr; node = node->Parent())
    {
        if (lines_.contains(node->Id()))
            continue;
        if (hints_.contains(node->Id()))
            return node->Id();
    }
    return pointer.has_value() ? ResolveLine(hovered, *pointer) : NodeId{};
}

NodeId UiTooltip::ResolveLine(const NodeId hovered, const Point pointer) const
{
    for (const auto id : lines_)
    {
        const auto *line = canvas_->FindNode(id);
        if (line == nullptr || !line->IsVisible() || !line->BoundsInCanvas().Contains(pointer))
            continue;
        // The pointed control must hold the line, so a control drawn over it hides its hint.
        for (const auto *owner = line->Parent(); owner != nullptr; owner = owner->Parent())
            if (owner->Id() == hovered)
                return id;
    }
    return {};
}

void UiTooltip::Update(const double deltaSeconds, const InputState &input, const NodeId hovered, const bool enabled,
                       const std::optional<Point> pointer)
{
    if (root_ == nullptr)
        return;
    clock_ += std::max(deltaSeconds, 0.0);
    const auto target = enabled ? ResolveTarget(hovered, pointer) : NodeId{};
    // A press, the wheel or a key hides the hint until the pointer moves to another control.
    bool dismissed = input.MouseWheelDelta() != 0.0F;
    for (const auto &event : input.Events())
        dismissed |= event.type == InputEventType::MouseButtonPressed || event.type == InputEventType::KeyPressed;
    if (dismissed)
    {
        Hide();
        dismissed_ = target;
        return;
    }
    if (target != dismissed_)
        dismissed_ = {};
    if (target == NodeId{} || target == dismissed_)
    {
        Hide();
        return;
    }
    if (target != armed_)
    {
        Hide();
        armed_ = target;
        const bool warm = hiddenAt_ >= 0.0 && clock_ - hiddenAt_ < WarmSeconds;
        waitSeconds_ = warm ? WarmDelaySeconds : DelaySeconds;
    }
    if (!shown_)
    {
        waitSeconds_ -= std::max(deltaSeconds, 0.0);
        if (waitSeconds_ <= 0.0)
            Show();
        return;
    }
    // The control may move (scrolling) and the first frame used estimated text widths.
    Layout();
}

void UiTooltip::Hide() noexcept
{
    if (shown_)
        hiddenAt_ = clock_;
    shown_ = false;
    armed_ = {};
    if (root_ != nullptr)
        root_->SetVisible(false);
}

void UiTooltip::Clear() noexcept
{
    Hide();
    hints_.clear();
    lines_.clear();
    dismissed_ = {};
}

bool UiTooltip::IsShown() const noexcept
{
    return shown_;
}

void UiTooltip::Show()
{
    const auto hint = hints_.find(armed_);
    if (hint == hints_.end())
    {
        Hide();
        return;
    }
    BuildContent(hint->second);
    shown_ = true;
    root_->SetVisible(true);
    Layout();
}

void UiTooltip::BuildContent(const TooltipText &text)
{
    // A long name on its own breaks into lines at the plate's 300 width, like the mockup's tooltip.
    const bool alone = text.keys.empty() && text.note.empty();
    auto shown = alone ? options_ui::WrapText(text.label, TextSize, MaxWidth - PaddingX * 2) : text.label;
    labelLines_ = 1 + static_cast<int>(std::ranges::count(shown, L'\n'));
    auto &label = Require<TextVisualComponent>(*label_);
    label.SetFont(ui_fonts::Text(shown));
    label.SetText(std::move(shown));
    auto &note = Require<TextVisualComponent>(*note_);
    note.SetText(text.note);
    note.SetFont(ui_fonts::Text(text.note));
    // "Ctrl+O" is one chord of chips joined by "+"; "Esc / Enter" offers alternatives.
    keyTexts_.clear();
    separatorTexts_.clear();
    std::wstring_view keys = text.keys;
    while (!keys.empty())
    {
        const auto alternative = keys.find(L" / ");
        const auto chord = keys.substr(0, alternative);
        std::size_t start = 0;
        for (;;)
        {
            const auto plus = chord.find(L'+', start);
            keyTexts_.push_back(Trim(chord.substr(start, plus == std::wstring_view::npos ? chord.npos : plus - start)));
            if (plus == std::wstring_view::npos)
                break;
            separatorTexts_.push_back(L"+");
            start = plus + 1;
        }
        if (alternative == std::wstring_view::npos)
            break;
        separatorTexts_.push_back(L"/");
        keys.remove_prefix(alternative + 3);
    }
    for (std::size_t i = 0; i < std::max(chips_.size(), keyTexts_.size()); ++i)
    {
        auto &chip = ChipAt(i);
        const bool used = i < keyTexts_.size();
        chip.plate->SetVisible(used);
        if (used)
            Require<TextVisualComponent>(*chip.text).SetText(keyTexts_[i]);
    }
    for (std::size_t i = 0; i < std::max(separators_.size(), separatorTexts_.size()); ++i)
    {
        auto &separator = SeparatorAt(i);
        const bool used = i < separatorTexts_.size();
        separator.SetVisible(used);
        if (used)
            Require<TextVisualComponent>(separator).SetText(separatorTexts_[i]);
    }
    note_->SetVisible(!text.note.empty());
    label_->SetVisible(!text.label.empty());
}

UiTooltip::Chip &UiTooltip::ChipAt(const std::size_t index)
{
    while (chips_.size() <= index)
    {
        Chip chip;
        chip.plate = &CreateSprite(*root_, {}, {}, "KeyChip");
        Paint(*chip.plate, ChipColour);
        chip.line = &CreateSprite(*chip.plate, {}, {}, "Line");
        Paint(*chip.line, ChipLineColour);
        chip.text = &CreateLabel(*chip.plate, {}, L"", "Key");
        auto face = ui_fonts::Number(700, true);
        auto &text = Require<TextVisualComponent>(*chip.text);
        text.SetFont(std::move(face.font));
        text.SetFontWeight(face.weight);
        text.SetFontSize(ChipSize);
        text.SetTextColor(ui_colors::White);
        text.SetHorizontalAlignment(TextAlignment::Center);
        chips_.push_back(chip);
    }
    return chips_[index];
}

Visual2DNode &UiTooltip::SeparatorAt(const std::size_t index)
{
    while (separators_.size() <= index)
    {
        auto &separator = Text(*root_, "KeySeparator", SeparatorColour, 600);
        Require<TextVisualComponent>(separator).SetFont(ui_fonts::Text(L"+"));
        Require<TextVisualComponent>(separator).SetFontSize(11);
        Require<TextVisualComponent>(separator).SetHorizontalAlignment(TextAlignment::Center);
        separators_.push_back(&separator);
    }
    return *separators_[index];
}

float UiTooltip::TextWidth(Visual2DNode &node, const float size) const
{
    const auto &text = Require<TextVisualComponent>(node);
    if (const auto measured = text.MeasuredSize())
        return measured->width;
    return EstimateWidth(text.Text(), size);
}

void UiTooltip::Layout()
{
    const auto *target = canvas_->FindNode(armed_);
    if (target == nullptr)
    {
        Hide();
        return;
    }
    // Widths: label, note, then the chips with their separators.
    const bool hasLabel = label_->IsVisible(), hasNote = note_->IsVisible();
    const float labelWidth = hasLabel ? TextWidth(*label_, TextSize) : 0.0F;
    const float noteWidth = hasNote ? TextWidth(*note_, TextSize) : 0.0F;
    std::vector<float> chipWidths(keyTexts_.size()), separatorWidths(separatorTexts_.size());
    float keysWidth = 0;
    for (std::size_t i = 0; i < keyTexts_.size(); ++i)
    {
        chipWidths[i] = std::max(ChipMinWidth, TextWidth(*chips_[i].text, ChipSize) + ChipPadding * 2);
        keysWidth += chipWidths[i];
    }
    for (std::size_t i = 0; i < separatorTexts_.size(); ++i)
    {
        separatorWidths[i] = separatorTexts_[i] == L"/" ? 14.0F : 8.0F;
        keysWidth += separatorWidths[i];
    }
    float contentWidth = labelWidth;
    if (hasNote)
        contentWidth += (contentWidth > 0 ? Gap : 0.0F) + noteWidth;
    if (!keyTexts_.empty())
        contentWidth += (contentWidth > 0 ? Gap : 0.0F) + keysWidth;
    const float width = std::min(MaxWidth, contentWidth + PaddingX * 2);
    const float height = PaddingY * 2 + (keyTexts_.empty() ? LineHeight * static_cast<float>(labelLines_) : ChipHeight);
    laidOutWidth_ = width;

    // Under the control unless that leaves the screen; the arrow points at the control's centre.
    const auto canvas = canvas_->LogicalSize();
    const auto bounds = target->BoundsInCanvas();
    const float centre = bounds.x + bounds.width * .5F;
    float bottom = bounds.y - Offset - height;
    const bool above = bottom < -canvas.height * .5F + Margin;
    if (above)
        bottom = bounds.y + bounds.height + Offset;
    const float left = std::clamp(centre - width * .5F, -canvas.width * .5F + Margin,
                                  std::max(canvas.width * .5F - width - Margin, -canvas.width * .5F + Margin));
    plate_->SetBounds({left, bottom, width, height});
    arrow_->SetPosition({std::clamp(centre, left + ArrowSize, left + width - ArrowSize), above ? bottom : bottom + height});

    float x = left + PaddingX;
    if (hasLabel)
    {
        label_->SetBounds({x, bottom, labelWidth + 2, height});
        x += labelWidth + Gap;
    }
    if (hasNote)
    {
        note_->SetBounds({x, bottom, noteWidth + 2, height});
        x += noteWidth + Gap;
    }
    // Chips and separators alternate: key, separator, key...
    for (std::size_t i = 0; i < keyTexts_.size(); ++i)
    {
        auto &chip = chips_[i];
        chip.plate->SetBounds({x, bottom + PaddingY, chipWidths[i], ChipHeight});
        chip.line->SetBounds({0, 0, chipWidths[i], 2});
        chip.text->SetBounds({0, 0, chipWidths[i], ChipHeight});
        x += chipWidths[i];
        if (i < separatorTexts_.size())
        {
            separators_[i]->SetBounds({x, bottom, separatorWidths[i], height});
            x += separatorWidths[i];
        }
    }
}
