#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

using namespace PhantomCore;

using namespace options_ui;

namespace
{
    // In drop-down order.
    constexpr std::array RenderRateChoices{RenderRateMode::Display, RenderRateMode::DisplayDouble,
                                           RenderRateMode::Unlimited};
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

void OptionsPanel::CreatePositionControls()
{
    const float top = NextField(PositionControlHeight);
    const float boxTop = top + ControlOffset;
    positionLabel_ = &CreateFieldLabel(*content_, top, "Options.Position.Label");
    positionBox_ = &CreatePanel(*content_, TopLeft({ContentLeft, boxTop, FieldWidth, PositionBoxHeight}, contentHeight_),
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
        auto &letter = CreateCaption(*content_, {left, rowTop, PositionAxisWidth, ControlHeight}, 13, 600, LabelColor,
                                     name + ".Label");
        SetCaption(letter, axis == 0 ? L"X" : L"Y");
        positionFields_[axis] = &CreateButton(
            *content_,
            TopLeft({left + PositionAxisWidth, rowTop, PositionNumberWidth, ControlHeight}, contentHeight_), L"", name);
        StyleFieldButton(*positionFields_[axis], true);
        AddFieldLine(*content_, *positionFields_[axis], name + ".Line");
        left += PositionAxisWidth + PositionNumberWidth + PositionAxisGap;
    }
    positionCenter_ = &CreateButton(
        *content_,
        TopLeft({ContentLeft + FieldWidth - PositionCenterWidth, rowTop + (ControlHeight - PositionCenterHeight) * 0.5F,
                 PositionCenterWidth, PositionCenterHeight},
                contentHeight_),
        L"", "Options.Position.Center");
    SetColors(*positionCenter_, ui_colors::Rgb(0xFFFFFF, .72F), FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    StyleCaption(RequireComponent<TextVisualComponent>(*positionCenter_), 13, 800, ui_colors::theme::BrandText,
                 TextAlignment::Center);
    AddFieldLine(*content_, *positionCenter_, "Options.Position.CenterLine");
}

// What the box shows: the monitor the window is on and where the game picture sits on it. Without a
// window (offscreen contexts) the saved values stand in and nothing can be moved.
OptionsPanel::PositionView OptionsPanel::CurrentPosition() const
{
    PositionView view;
    const auto &saved = options_.Values();
    view.size = window_ ? window_->ClientSize() : WindowResolution{saved.windowWidth, saved.windowHeight};
    view.monitor = window_ ? window_->MonitorSize() : WindowResolution{};
    if (view.monitor.width == 0 || view.monitor.height == 0)
        view.monitor = view.size;
    const WindowPosition centred{Travel(view.monitor.width, view.size.width) / 2,
                                 Travel(view.monitor.height, view.size.height) / 2};
    view.position = window_ ? window_->Position() : saved.windowPosition.value_or(centred);
    const bool smaller = view.size.width < view.monitor.width || view.size.height < view.monitor.height;
    view.adjustable = window_ != nullptr && window_->Mode() == WindowMode::Borderless && smaller;
    return view;
}

void OptionsPanel::ShowPosition(const PositionView &view, const WindowPosition position)
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
    hitTestDirty_ = true;
}

void OptionsPanel::RefreshPositionControls()
{
    SetCaption(*positionLabel_, std::wstring(texts_.Get("Options.screenPosition")));
    SetCaption(*positionCenter_, std::wstring(texts_.Get("Options.screenPositionCenter")));
    const auto view = CurrentPosition();
    // The area around the picture is black in Borderless and the desktop otherwise.
    const bool borderless = window_ != nullptr && window_->Mode() == WindowMode::Borderless;
    SetPanelColor(*positionMonitor_, borderless ? PositionBackdropColor : PositionDesktopColor);
    SetColors(*positionPicture_, PositionPictureColor, PositionPictureHover, PositionPictureHover, PositionPictureDisabled);
    positionPicture_->SetEnabled(view.adjustable);
    positionCenter_->SetEnabled(view.adjustable);
    for (auto *field : positionFields_)
        field->SetEnabled(view.adjustable);
    // A drag in progress shows its draft until the release moves the window.
    ShowPosition(view, positionDrag_ ? positionDrag_->value : view.position);
}

bool OptionsPanel::ProcessPositionDrag(const InputState &input, const std::optional<Point> canvasPointer,
                                       const NodeId hoveredNode)
{
    if (!positionDrag_)
    {
        if (!targetOpen_ || !canvasPointer || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        if (hoveredNode != positionPicture_->Id())
            return false;
        const auto pressed = CurrentPosition();
        if (!pressed.adjustable)
            return false;
        CancelKeyboardEdit();
        CollapseCombos();
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

void OptionsPanel::StartPositionEdit(const std::size_t axis)
{
    CancelKeyboardEdit();
    const auto view = CurrentPosition();
    if (!view.adjustable)
        return;
    positionEdit_ = axis;
    positionDraft_ = std::to_wstring(axis == 0 ? view.position.x : view.position.y);
    replacePositionDraft_ = true;
    RefreshPositionControls();
}

void OptionsPanel::EditPosition(const std::uint16_t key)
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
        status_ = Status::InvalidInput;
        statusDetail_ = "The screen position must be a whole number of pixels.";
        RefreshStatus();
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
void OptionsPanel::ApplyPosition(const std::optional<WindowPosition> position)
{
    if (!window_)
        return;
    const auto previous = options_.Values().windowPosition;
    if (!window_->SetPosition(position, statusDetail_))
        status_ = Status::WindowFailed;
    else
    {
        auto candidate = options_.Values();
        candidate.windowPosition = position ? std::optional(window_->Position()) : std::nullopt;
        if (!SaveOptions(candidate))
        {
            std::string error;
            if (!window_->SetPosition(previous, error))
                statusDetail_ += " | Restore failed: " + error;
        }
    }
    RefreshDisplayControls();
    RefreshStatus();
}

void OptionsPanel::CreateDisplayControls()
{
    const float top = NextField(ControlHeight);
    resolutionLabel_ = &CreateFieldLabel(*content_, top, "Options.Resolution.Label");
    resolutionCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.Resolution.Combo");
    ConfigureCombo(*resolutionCombo_);
    AddFieldLine(*content_, *resolutionCombo_, "Options.Resolution.Line");
    const float windowModeTop = NextField(ControlHeight);
    windowModeLabel_ = &CreateFieldLabel(*content_, windowModeTop, "Options.WindowMode.Label");
    windowModeCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, windowModeTop + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.WindowMode.Combo");
    ConfigureCombo(*windowModeCombo_);
    AddFieldLine(*content_, *windowModeCombo_, "Options.WindowMode.Line");
    CreatePositionControls();
    // Song and chart texts: the first notation of the file, or the romanized line.
    const float notationTop = NextField(ControlHeight);
    notationLabel_ = &CreateFieldLabel(*content_, notationTop, "Options.Notation.Label");
    notationCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, notationTop + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.Notation.Combo");
    ConfigureCombo(*notationCombo_);
    AddFieldLine(*content_, *notationCombo_, "Options.Notation.Line");
    // How often the engine renders: the display refresh rate, twice that rate, or after every update.
    const float renderRateTop = NextField(ControlHeight);
    renderRateLabel_ = &CreateFieldLabel(*content_, renderRateTop, "Options.RenderRate.Label");
    renderRateCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, renderRateTop + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.RenderRate.Combo");
    ConfigureCombo(*renderRateCombo_);
    AddFieldLine(*content_, *renderRateCombo_, "Options.RenderRate.Line");
    CreateKeyBeamOpacityControl();
    CreateBackgroundBrightnessControl();
}

void OptionsPanel::RefreshDisplayControls()
{
    SetCaption(*resolutionLabel_, std::wstring(Options(texts_).resolution));
    resolutions_.clear();
    const auto current =
        window_ ? window_->ClientSize()
                : WindowResolution{options_.Values().windowWidth, options_.Values().windowHeight};
    try
    {
        if (window_)
            resolutions_ = window_->DisplayResolutions();
    }
    catch (const std::exception &error)
    {
        status_ = Status::WindowFailed;
        statusDetail_ = error.what();
    }
    if (std::ranges::find(resolutions_, current) == resolutions_.end())
        resolutions_.push_back(current);
    std::vector<std::wstring> names;
    for (const auto size : resolutions_)
        names.push_back(std::to_wstring(size.width) + L" × " + std::to_wstring(size.height));
    auto &combo = RequireDropdown(*resolutionCombo_);
    combo.SetItems(std::move(names));
    combo.SetSelectedIndex(
        static_cast<std::size_t>(std::ranges::find(resolutions_, current) - resolutions_.begin()));
    resolutionCombo_->SetEnabled(window_ != nullptr);
    SetCaption(*windowModeLabel_, std::wstring(texts_.Get("Options.windowMode")));
    auto &windowMode = RequireDropdown(*windowModeCombo_);
    windowMode.SetItems({std::wstring(texts_.Get("Options.windowModeWindowed")),
                         std::wstring(texts_.Get("Options.windowModeBorderless"))});
    const auto shownMode = window_ ? window_->Mode() : options_.Values().windowMode;
    windowMode.SetSelectedIndex(shownMode == WindowMode::Borderless ? 1 : 0);
    windowModeCombo_->SetEnabled(window_ != nullptr);
    RefreshPositionControls();
    SetCaption(*notationLabel_, std::wstring(texts_.Get("Options.notation")));
    auto &notation = RequireDropdown(*notationCombo_);
    notation.SetItems({std::wstring(texts_.Get("Options.notationFirst")), std::wstring(texts_.Get("Options.notationRomanized"))});
    notation.SetSelectedIndex(options_.Values().notation == TextNotation::Romanized ? 1 : 0);
    SetCaption(*renderRateLabel_, std::wstring(texts_.Get("Options.renderRate")));
    auto &renderRate = RequireDropdown(*renderRateCombo_);
    renderRate.SetItems({std::wstring(texts_.Get("Options.renderRateDisplay")),
                         std::wstring(texts_.Get("Options.renderRateDouble")),
                         std::wstring(texts_.Get("Options.renderRateUnlimited"))});
    const auto shown = std::ranges::find(RenderRateChoices, options_.Values().renderRate);
    renderRate.SetSelectedIndex(
        shown == RenderRateChoices.end() ? 0 : static_cast<std::size_t>(shown - RenderRateChoices.begin()));
}

void OptionsPanel::SelectRenderRate(const std::size_t index)
{
    if (index >= RenderRateChoices.size())
        return;
    auto candidate = options_.Values();
    candidate.renderRate = RenderRateChoices[index];
    // The game applies the saved value to the engine on its next update.
    static_cast<void>(SaveOptions(candidate));
    RefreshDisplayControls();
    RefreshStatus();
}

void OptionsPanel::SelectNotation(const std::size_t index)
{
    auto candidate = options_.Values();
    candidate.notation = index == 1 ? TextNotation::Romanized : TextNotation::First;
    // Screens read the saved value, so a failed save leaves the shown notation unchanged.
    static_cast<void>(SaveOptions(candidate));
    RefreshDisplayControls();
    RefreshStatus();
}

void OptionsPanel::SelectWindowMode(const std::size_t index)
{
    if (!window_)
        return;
    const auto previous = window_->Mode();
    const auto mode = index == 1 ? WindowMode::Borderless : WindowMode::Windowed;
    if (!window_->SetMode(mode, statusDetail_))
        status_ = Status::WindowFailed;
    else
    {
        auto candidate = options_.Values();
        candidate.windowMode = mode;
        if (!SaveOptions(candidate))
        {
            std::string error;
            if (!window_->SetMode(previous, error))
                statusDetail_ += " | Restore failed: " + error;
        }
    }
    RefreshDisplayControls();
    RefreshStatus();
}

void OptionsPanel::SelectResolution(std::size_t index)
{
    if (!window_ || index >= resolutions_.size())
        return;
    const auto previous = window_->ClientSize();
    const auto size = resolutions_[index];
    if (!window_->SetClientSize(size, statusDetail_))
        status_ = Status::WindowFailed;
    else
    {
        auto candidate = options_.Values();
        candidate.windowWidth = size.width;
        candidate.windowHeight = size.height;
        if (!SaveOptions(candidate))
        {
            std::string error;
            if (!window_->SetClientSize(previous, error))
                statusDetail_ += " | Restore failed: " + error;
        }
    }
    RefreshDisplayControls();
    RefreshStatus();
}
