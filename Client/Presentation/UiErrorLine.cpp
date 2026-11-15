#include "UiErrorLine.h"
#include "App/AssetPaths.h"
#include "UiColors.h"
#include "UiFonts.h"

#include <algorithm>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    constexpr float PaddingLeft = 14.0F, PaddingRight = 18.0F, PaddingY = 9.0F, LineHeight = 19.0F;
    constexpr float MarkWidth = 4.0F, IconSize = 18.0F, IconGap = 8.0F, PartGap = 6.0F, ChipPadding = 4.0F;
    constexpr float TextSize = 13.0F, LocationSize = 12.0F;
    constexpr Color PlateColour{1.0F, 1.0F, 1.0F, 0.96F};
    constexpr Color ChipColour{31.0F / 255.0F, 76.0F / 255.0F, 153.0F / 255.0F, 0.07F};

    template <typename T>
    T &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<T>();
        if (component == nullptr)
            throw std::logic_error("An error line node is missing its component.");
        return *component;
    }

    void Paint(Visual2DNode &node, const Color colour)
    {
        Require<SpriteVisualComponent>(node).SetTint(colour);
    }

    // Used only until the renderer has measured the text once.
    float EstimateWidth(const std::wstring_view text, const float size) noexcept
    {
        float width = 0.0F;
        for (const wchar_t character : text)
            width += (character >= 0x1100 ? 1.0F : 0.56F) * size;
        return width;
    }

    float TextWidth(Visual2DNode &node, const float size)
    {
        const auto &text = Require<TextVisualComponent>(node);
        if (const auto measured = text.MeasuredSize())
            return measured->width;
        return EstimateWidth(text.Text(), size);
    }

    void Style(Visual2DNode &node, TextFont font, const float size, const std::uint16_t weight, const Color colour)
    {
        auto &text = Require<TextVisualComponent>(node);
        text.SetFont(std::move(font));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(colour);
    }
} // namespace

void UiErrorLine::Present(const ScreenErrorText &text, const std::uint32_t width, const std::uint32_t height)
{
    if (text.Empty())
    {
        Hide();
        return;
    }
    if (!handle_)
        Build();
    handle_.Get()->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
    if (!(text == text_))
    {
        text_ = text;
        Require<TextVisualComponent>(*kind_).SetText(text.kind);
        Require<TextVisualComponent>(*location_).SetText(text.location);
        Require<TextVisualComponent>(*cause_).SetText(text.cause);
        // The face follows the text so Korean or Japanese causes keep one typeface.
        Require<TextVisualComponent>(*cause_).SetFont(ui_fonts::Text(text.cause));
    }
    if (!shown_)
        static_cast<void>(handle_.SetVisible(true));
    shown_ = true;
    // Measurements arrive after the first render, so the layout settles on the following frame.
    Layout();
}

void UiErrorLine::Build()
{
    handle_ = visuals_.CreateOwnedCanvas({{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
    auto *canvas = handle_.Get();
    if (canvas == nullptr)
        throw std::runtime_error("Failed to create the screen error line Canvas.");
    auto &root = canvas->CreateNode(Anchor::Center, "Hud.ErrorLine");
    plate_ = &CreatePanel(root, {}, "Hud.Error");
    Paint(*plate_, PlateColour);
    mark_ = &CreatePanel(*plate_, {0.0F, 0.0F, MarkWidth, Height}, "Hud.Error.Mark");
    Paint(*mark_, ui_colors::theme::Danger);
    icon_ = &CreateSprite(*plate_, {PaddingLeft, Height - PaddingY - 1.0F - IconSize, IconSize, IconSize},
                          visuals_.RegisterImage(Common(L"Icons.alert")), "Hud.Error.Icon");
    Paint(*icon_, ui_colors::theme::Danger);
    kind_ = &CreateLabel(*plate_, {}, L"", "Hud.Error.Kind");
    Style(*kind_, ui_fonts::Text(), TextSize, 800, ui_colors::theme::Danger);
    chip_ = &CreatePanel(*plate_, {}, "Hud.Error.LocationChip");
    Paint(*chip_, ChipColour);
    location_ = &CreateLabel(*chip_, {}, L"", "Hud.Error.Location");
    Style(*location_, TextFont{TextFontSource::System, L"Consolas"}, LocationSize, 500, ui_colors::theme::InkSoft);
    cause_ = &CreateLabel(*plate_, {}, L"", "Hud.Error.Cause");
    Style(*cause_, ui_fonts::Text(), TextSize, 600, ui_colors::theme::Ink);
}

void UiErrorLine::Layout()
{
    auto *canvas = handle_.Get();
    if (canvas == nullptr)
        return;
    const float textY = PaddingY;
    float x = PaddingLeft + IconSize + IconGap;
    const float kindWidth = TextWidth(*kind_, TextSize);
    kind_->SetBounds({x, textY, kindWidth + 1.0F, LineHeight});
    x += kindWidth + PartGap;
    const bool hasLocation = !text_.location.empty();
    chip_->SetVisible(hasLocation);
    if (hasLocation)
    {
        const float locationWidth = TextWidth(*location_, LocationSize);
        chip_->SetBounds({x, textY, locationWidth + ChipPadding * 2.0F, LineHeight});
        location_->SetBounds({ChipPadding, 0.0F, locationWidth + 1.0F, LineHeight});
        x += locationWidth + ChipPadding * 2.0F + PartGap;
    }
    // The strip keeps one line: past the maximum width the cause is cut at the strip's right padding.
    const float maximum = std::max(std::min(MaximumWidth, canvas->LogicalSize().width - ScreenMargin * 2.0F), x);
    const float causeWidth = TextWidth(*cause_, TextSize);
    const float width = std::min(x + causeWidth + PaddingRight, maximum);
    const float shownCause = std::max(width - PaddingRight - x, 0.0F);
    cause_->SetBounds({x, textY, shownCause, LineHeight});
    cause_->SetClipRect({0.0F, 0.0F, shownCause, LineHeight});
    plate_->SetBounds({-width * 0.5F, canvas->LogicalSize().height * 0.5F - Top - Height, width, Height});
}

void UiErrorLine::Hide() noexcept
{
    if (!shown_)
        return;
    shown_ = false;
    static_cast<void>(handle_.SetVisible(false));
}

void UiErrorLine::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    if (auto *canvas = handle_.Get())
    {
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        Layout();
    }
}

void UiErrorLine::Shutdown() noexcept
{
    plate_ = mark_ = icon_ = kind_ = chip_ = location_ = cause_ = nullptr;
    text_ = {};
    shown_ = false;
    handle_.Reset();
}
