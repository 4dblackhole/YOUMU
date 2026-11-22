#include "OptionsDisplay.h"
#include "OptionsPanelStyle.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

using namespace PhantomCore;

using namespace options_ui;

namespace
{
    // Screen position row: an axis letter and a number box for X and Y, and Center on the right.
    constexpr float PositionAxisWidth = 18, PositionNumberWidth = 84, PositionAxisGap = 12;
    constexpr float PositionCenterWidth = 76, PositionCenterHeight = 32;
    // The picture stays large enough to grab on a very large monitor.
    constexpr float PositionPictureMinimum = 8;
    constexpr std::array PositionAxisNames{"X", "Y"};
    constexpr auto PositionBoxColor = ui_colors::Rgb(0xFFFFFF, .5F);
    constexpr auto PositionBackdropColor = ui_colors::Rgb(0x0B1220), PositionDesktopColor = ui_colors::Rgb(0x7C8AA5);
    constexpr auto PositionPictureColor = ui_colors::theme::Brand, PositionPictureHover = ui_colors::theme::BrandDeep;
    constexpr auto PositionPictureDisabled = ui_colors::Rgb(0xB9C4D8);

    [[nodiscard]] std::int32_t Extent(const std::uint32_t pixels) noexcept
    {
        return static_cast<std::int32_t>(std::min<std::uint32_t>(pixels, 16384));
    }
    // The farthest a picture of this size can sit from the monitor's top-left while staying on it.
    [[nodiscard]] std::int32_t Travel(const std::uint32_t monitor, const std::uint32_t size) noexcept
    {
        return std::max(Extent(monitor) - Extent(size), 0);
    }
} // namespace

void OptionsDisplay::CreatePositionControls()
{
    auto &content = context_.Content();
    const float contentHeight = context_.ContentHeight();
    const float top = context_.NextField(PositionControlHeight);
    const float boxTop = top + ControlOffset;
    positionLabel_ = &CreateFieldLabel(content, top, "Options.Position.Label");
    positionBox_ = &CreatePanel(content, TopLeft({ContentLeft, boxTop, FieldWidth, PositionBoxHeight}, contentHeight),
                                "Options.Position.Box");
    SetPanelColor(*positionBox_, PositionBoxColor);
    positionMonitor_ = &CreateSprite(*positionBox_, {}, {}, "Options.Position.Monitor");
    // The picture is the drag target; the monitor only shows where it can go.
    positionPicture_ = &CreatePanel(*positionMonitor_, {}, "Options.Position.Picture");
    positionPicture_->AddComponent<RectangleCollider2DComponent>();
    positionPicture_->AddComponent<PointerReceiverComponent>();

    const float rowTop = boxTop + PositionBoxHeight + PositionRowGap;
    float left = ContentLeft;
    for (std::size_t axis = 0; axis < positionFields_.size(); ++axis)
    {
        const std::string name = std::string("Options.Position.") + PositionAxisNames[axis];
        auto &letter = CreateCaption(content, {left, rowTop, PositionAxisWidth, ControlHeight}, 13, 600, LabelColor,
                                     name + ".Label");
        SetCaption(letter, axis == 0 ? L"X" : L"Y");
        positionFields_[axis] = &CreateButton(
            content, TopLeft({left + PositionAxisWidth, rowTop, PositionNumberWidth, ControlHeight}, contentHeight), L"",
            name);
        StyleFieldButton(*positionFields_[axis], true);
        AddFieldLine(content, *positionFields_[axis], name + ".Line");
        left += PositionAxisWidth + PositionNumberWidth + PositionAxisGap;
    }
    positionCenter_ = &CreateButton(
        content,
        TopLeft({ContentLeft + FieldWidth - PositionCenterWidth, rowTop + (ControlHeight - PositionCenterHeight) * 0.5F,
                 PositionCenterWidth, PositionCenterHeight},
                contentHeight),
        L"", "Options.Position.Center");
    SetColors(*positionCenter_, ui_colors::Rgb(0xFFFFFF, .72F), FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    StyleCaption(RequireComponent<TextVisualComponent>(*positionCenter_), 13, 800, ui_colors::theme::BrandText,
                 TextAlignment::Center);
    AddFieldLine(content, *positionCenter_, "Options.Position.CenterLine");
}

// What the box shows: the monitor the window is on and where the game picture sits on it. Without a
// window (offscreen contexts) the saved values stand in and nothing can be moved.
OptionsDisplay::PositionView OptionsDisplay::CurrentPosition() const
{
    PositionView view;
    const auto &saved = context_.Settings().Values();
    auto *window = context_.Window();
    view.size = window ? window->ClientSize() : WindowResolution{saved.windowWidth, saved.windowHeight};
    view.monitor = window ? window->MonitorSize() : WindowResolution{};
    if (view.monitor.width == 0 || view.monitor.height == 0)
        view.monitor = view.size;
    const WindowPosition centred{Travel(view.monitor.width, view.size.width) / 2,
                                 Travel(view.monitor.height, view.size.height) / 2};
    view.position = window ? window->Position() : saved.windowPosition.value_or(centred);
    const bool smaller = view.size.width < view.monitor.width || view.size.height < view.monitor.height;
    view.adjustable = window != nullptr && window->Mode() == WindowMode::Borderless && smaller;
    return view;
}

void OptionsDisplay::ShowPosition(const PositionView &view, const WindowPosition position)
{
    // The monitor is fitted into the box; the picture uses the same scale inside it.
    const float monitorWidth = static_cast<float>(Extent(view.monitor.width));
    const float monitorHeight = static_cast<float>(Extent(view.monitor.height));
    positionScale_ = std::min(FieldWidth / monitorWidth, PositionBoxHeight / monitorHeight);
    const float boxWidth = monitorWidth * positionScale_, boxHeight = monitorHeight * positionScale_;
    positionMonitor_->SetBounds(TopLeft(
        {(FieldWidth - boxWidth) * 0.5F, (PositionBoxHeight - boxHeight) * 0.5F, boxWidth, boxHeight}, PositionBoxHeight));
    const float width =
        std::clamp(static_cast<float>(Extent(view.size.width)) * positionScale_, PositionPictureMinimum, boxWidth);
    const float height =
        std::clamp(static_cast<float>(Extent(view.size.height)) * positionScale_, PositionPictureMinimum, boxHeight);
    const float left = std::clamp(static_cast<float>(position.x) * positionScale_, 0.0F, boxWidth - width);
    const float top = std::clamp(static_cast<float>(position.y) * positionScale_, 0.0F, boxHeight - height);
    positionPicture_->SetBounds(TopLeft({left, top, width, height}, boxHeight));
    const std::array values{position.x, position.y};
    for (std::size_t axis = 0; axis < positionFields_.size(); ++axis)
    {
        auto &text = RequireComponent<TextVisualComponent>(*positionFields_[axis]);
        text.SetText(positionEdit_ == axis ? positionDraft_ + L"_" : std::to_wstring(values[axis]));
    }
    context_.InvalidateHitTest();
}

void OptionsDisplay::RefreshPositionControls()
{
    const auto &texts = context_.Texts();
    SetCaption(*positionLabel_, std::wstring(texts.Get("Options.screenPosition")));
    SetCaption(*positionCenter_, std::wstring(texts.Get("Options.screenPositionCenter")));
    const auto view = CurrentPosition();
    // The area around the picture is black in Borderless and the desktop otherwise.
    auto *window = context_.Window();
    const bool borderless = window != nullptr && window->Mode() == WindowMode::Borderless;
    SetPanelColor(*positionMonitor_, borderless ? PositionBackdropColor : PositionDesktopColor);
    SetColors(*positionPicture_, PositionPictureColor, PositionPictureHover, PositionPictureHover, PositionPictureDisabled);
    positionPicture_->SetEnabled(view.adjustable);
    positionCenter_->SetEnabled(view.adjustable);
    for (auto *field : positionFields_)
        field->SetEnabled(view.adjustable);
    // A drag in progress shows its draft until the release moves the window.
    ShowPosition(view, positionDrag_ ? positionDrag_->value : view.position);
}

bool OptionsDisplay::ProcessPositionDrag(const InputState &input, const std::optional<Point> canvasPointer,
                                         const NodeId hoveredNode)
{
    if (!positionDrag_)
    {
        if (!context_.IsOpen() || !canvasPointer || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        if (positionPicture_ == nullptr || hoveredNode != positionPicture_->Id())
            return false;
        const auto pressed = CurrentPosition();
        if (!pressed.adjustable)
            return false;
        context_.CancelKeyboardEdit();
        context_.CollapseCombos();
        positionDrag_ = PositionDrag{*canvasPointer, pressed.position, pressed.position};
    }
    // The window is only read while a drag is in progress, not on every idle frame.
    const auto view = CurrentPosition();
    if (canvasPointer)
    {
        // Canvas +Y points up, monitor y points down.
        const float right = (canvasPointer->x - positionDrag_->start.x) / positionScale_;
        const float down = (positionDrag_->start.y - canvasPointer->y) / positionScale_;
        positionDrag_->value = {
            std::clamp(positionDrag_->from.x + static_cast<std::int32_t>(std::lround(right)), 0,
                       Travel(view.monitor.width, view.size.width)),
            std::clamp(positionDrag_->from.y + static_cast<std::int32_t>(std::lround(down)), 0,
                       Travel(view.monitor.height, view.size.height))};
        ShowPosition(view, positionDrag_->value);
    }
    if (input.IsMouseButtonDown(MouseButton::Left) && !input.WasMouseButtonReleased(MouseButton::Left))
        return true;
    // Released: the window moves once per drag. Moving it during the drag would move the panel under the
    // pointer with it.
    const WindowPosition from = positionDrag_->from, value = positionDrag_->value;
    positionDrag_.reset();
    if (value == from)
        RefreshPositionControls();
    else
        ApplyPosition(value);
    return true;
}

bool OptionsDisplay::ProcessPositionAction(const Action &action)
{
    for (std::size_t axis = 0; axis < positionFields_.size(); ++axis)
        if (action.source == positionFields_[axis]->Id())
        {
            StartPositionEdit(axis);
            return true;
        }
    if (action.source != positionCenter_->Id())
        return false;
    context_.CancelKeyboardEdit();
    ApplyPosition(std::nullopt);
    return true;
}

void OptionsDisplay::StartPositionEdit(const std::size_t axis)
{
    context_.CancelKeyboardEdit();
    const auto view = CurrentPosition();
    if (!view.adjustable)
        return;
    positionEdit_ = axis;
    positionDraft_ = std::to_wstring(axis == 0 ? view.position.x : view.position.y);
    replacePositionDraft_ = true;
    RefreshPositionControls();
}

void OptionsDisplay::EditPosition(const std::uint16_t key)
{
    if (key != VK_RETURN)
    {
        EditNumberDraft(positionDraft_, replacePositionDraft_, key);
        RefreshPositionControls();
        return;
    }
    std::string draft;
    draft.reserve(positionDraft_.size());
    for (const auto character : positionDraft_)
        draft.push_back(static_cast<char>(character)); // Numeric ASCII draft only.
    std::int32_t value{};
    const auto parsed = std::from_chars(draft.data(), draft.data() + draft.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != draft.data() + draft.size())
    {
        context_.SetStatus(OptionsStatus::InvalidInput);
        context_.StatusDetail() = "The screen position must be a whole number of pixels.";
        context_.RefreshStatus();
        return;
    }
    auto position = CurrentPosition().position;
    (*positionEdit_ == 0 ? position.x : position.y) = value;
    positionEdit_.reset();
    ApplyPosition(position);
}

// Moves the window and saves where it ended up: the engine brings a position that does not fit back onto
// the monitor, and that corrected place is the one saved. An empty position returns to the centre and
// removes the saved one.
void OptionsDisplay::ApplyPosition(const std::optional<WindowPosition> position)
{
    auto *window = context_.Window();
    if (!window)
        return;
    const auto previous = context_.Settings().Values().windowPosition;
    if (!window->SetPosition(position, context_.StatusDetail()))
        context_.SetStatus(OptionsStatus::WindowFailed);
    else
    {
        auto candidate = context_.Settings().Values();
        candidate.windowPosition = position ? std::optional(window->Position()) : std::nullopt;
        if (!context_.Save(candidate))
        {
            std::string error;
            if (!window->SetPosition(previous, error))
                context_.StatusDetail() += " | Restore failed: " + error;
        }
    }
    RefreshControls();
    context_.RefreshStatus();
}
