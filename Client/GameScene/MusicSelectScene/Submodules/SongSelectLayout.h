#pragma once
#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "Texts/TextCatalog.h"
#include "PhantomCore.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <format>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace song_select
{
    constexpr double PreviewFadeSeconds = 0.2;

    // Popups that still use the earlier sky palette (filters, favourites, creation menu).
    constexpr auto CanvasBlue = ui_colors::sky::Background, PanelWhite = ui_colors::sky::Surface;
    constexpr auto PureWhite = ui_colors::White, AccentBlue = ui_colors::sky::Accent;
    constexpr auto AccentHover = ui_colors::sky::AccentHover, BorderBlue = ui_colors::sky::Accent;
    constexpr auto SubtleBorder = ui_colors::sky::Border, PaleBorder = ui_colors::sky::PaleBorder;
    constexpr auto PaleBlue = ui_colors::sky::Pale, ViewportBlue = ui_colors::sky::Viewport;
    constexpr auto FocusBlue = ui_colors::sky::Focus, RowBlue = ui_colors::sky::Row;
    constexpr auto DeepBlue = ui_colors::sky::Ink, MutedBlue = ui_colors::sky::Muted;
    constexpr auto SoftTextBlue = ui_colors::sky::SoftText, DisabledBlue = ui_colors::sky::Disabled;

    // Song select (UI-02) is authored in logical pixels on a 1280x720 screen with a
    // top-left origin. The screen keeps that height; a wider screen only widens the middle.
    namespace layout
    {
        constexpr PhantomCore::Size CanvasSize{1280.0F, 720.0F};
        constexpr float MaximumWidth = 2560.0F; // 32:9. Wider screens get side bands.
        constexpr float Margin = 32.0F;
        constexpr float BodyTop = 24.0F;
        constexpr float BodyHeight = 608.0F;
        constexpr float RecordWidth = 320.0F;
        constexpr float ListWidth = 400.0F;
        constexpr float ColumnGap = 16.0F;
        constexpr float FooterHeight = 64.0F;

        // One sheet lies under the song details and the song list. Its left edge leans 26 over
        // the screen height (top further right), with a shadow band along it. The record
        // column has no sheet, so the chart BG shows there.
        constexpr float SheetLeft = 362.0F;
        constexpr float SheetImageWidth = 918.0F; // Reaches the right edge of a 16:9 screen.
        constexpr float SheetEdgeLeft = 352.0F;
        constexpr float SheetEdgeWidth = 36.0F;
        constexpr float ColumnRuleInset = 16.0F; // The rule between details and list stops short.

        // Song list column (local to the list column).
        constexpr PhantomCore::Rect Tabs{16.0F, 16.0F, 336.0F, 34.0F};
        constexpr PhantomCore::Rect SearchField{28.0F, 68.0F, 336.0F, 40.0F};
        constexpr PhantomCore::Rect SortSelector{16.0F, 118.0F, 248.0F, 32.0F};
        constexpr PhantomCore::Rect FilterButton{272.0F, 118.0F, 80.0F, 32.0F};
        constexpr float ViewportTop = 160.0F;
        constexpr float ContentTop = 4.0F;
        constexpr float SongLeft = 38.0F;
        constexpr float SongWidth = ListWidth - SongLeft; // Cards run to the screen's right edge.
        constexpr float SongHeight = 58.0F;
        constexpr float SongPitch = 64.0F;
        constexpr float FocusedOutset = 20.0F; // The selected card sticks out to the left.
        constexpr float DifficultyTop = 58.0F;
        constexpr float DifficultyHeight = 32.0F;
        constexpr float DifficultyPitch = 36.0F;
        constexpr float DifficultyLeft = 34.0F; // Inside the selected card, which starts FocusedOutset further left.
        constexpr float DifficultyWidth = SongWidth + FocusedOutset - DifficultyLeft; // Rows end at the card's right edge.
        constexpr float DifficultyLevelInset = 20.0F; // The level's right gap, clear of the scrollbar strip.
        constexpr float FocusedPadding = 6.0F;
        constexpr float ScrollStep = 64.0F;
        constexpr float ScrollbarWidth = 12.0F;

        // Song detail column (local to the detail column, centred horizontally).
        constexpr float JacketSize = 196.0F;
        constexpr float JacketTop = 51.5F;
        constexpr float ArtistTop = 265.5F;
        constexpr float TitleTop = 293.5F;
        constexpr float CreatorTop = 335.5F;
        constexpr float DifficultyBlockTop = 367.5F;
        constexpr PhantomCore::Size DifficultyBlock{196.0F / 1.5F, 32.0F};
        // The block may reach the figures table's width less a margin on each side, so it never crowds the column
        // edges or the table below; a longer chart name is clipped instead of stretching it further.
        constexpr float DifficultyBlockMargin = 24.0F;
        constexpr float DifficultyBlockMaxWidth = 448.0F - 2.0F * DifficultyBlockMargin;
        // Figures: a 3x2 table 448 wide. Strong rules above and below, light rules between rows and columns.
        constexpr float StatWidth = 448.0F / 3.0F;
        constexpr float StatTop = 429.5F;
        constexpr float StatPitch = 69.0F;
        constexpr float StatTableTop = StatTop - 10.0F;
        constexpr float StatTableHeight = 137.0F;
        constexpr float StatRowRule = StatTop + StatPitch - 11.0F;
        constexpr float NameIconSize = 16.0F, NameIconGap = 6.0F;

        // Footer.
        constexpr PhantomCore::Size Back{203.0F / 1.5F, 54.0F};
        constexpr PhantomCore::Size Go{268.0F, 64.0F};
        constexpr float GoRise = 20.0F; // GO stands above the footer band.
        constexpr PhantomCore::Size ModeChip{203.0F / 1.5F, 54.0F};
        constexpr PhantomCore::Size PlayOptions{160.0F, 54.0F};
        constexpr PhantomCore::Size Gear{64.0F, 54.0F};
        constexpr PhantomCore::Size GoError{320.0F, 38.0F};
    } // namespace layout

    // Top-left rectangles of the three columns and the footer for one logical width.
    struct SongSelectFrame
    {
        float width{};
        float scale{};
        PhantomCore::Rect record;
        PhantomCore::Rect detail;
        PhantomCore::Rect list;
        PhantomCore::Rect footer;
        PhantomCore::Rect back;
        PhantomCore::Rect go;
    };

    // The 16:9 arrangement is never squeezed: a narrower screen scales it down between
    // top and bottom bands, and a screen wider than 32:9 keeps the 32:9 arrangement.
    [[nodiscard]] constexpr SongSelectFrame CalculateFrame(const float logicalWidth) noexcept
    {
        const float width = std::clamp(logicalWidth, layout::CanvasSize.width, layout::MaximumWidth);
        const float scale = std::min(1.0F, std::max(logicalWidth, 1.0F) / layout::CanvasSize.width);
        const float height = layout::CanvasSize.height;
        const float detailX = layout::Margin + layout::RecordWidth + layout::ColumnGap;
        const float listX = width - layout::ListWidth;
        return {width,
                scale,
                // Record rows start at the screen's left edge, so the column has no margin.
                {0.0F, layout::BodyTop, layout::Margin + layout::RecordWidth, layout::BodyHeight},
                {detailX, layout::BodyTop, listX - layout::ColumnGap - detailX, layout::BodyHeight},
                {listX, layout::BodyTop, layout::ListWidth, layout::BodyHeight},
                {0.0F, height - layout::FooterHeight, width, layout::FooterHeight},
                {0.0F, height - layout::Back.height, layout::Back.width, layout::Back.height},
                {width - layout::Go.width, height - layout::FooterHeight - layout::GoRise, layout::Go.width,
                 layout::Go.height + layout::GoRise}};
    }

    constexpr SongSelectFrame ReferenceFrame = CalculateFrame(1280.0F);
    static_assert(ReferenceFrame.scale == 1.0F && ReferenceFrame.detail.x == 368.0F);
    static_assert(ReferenceFrame.detail.width == 496.0F && ReferenceFrame.list.x == 880.0F);
    static_assert(ReferenceFrame.record.x == 0.0F && ReferenceFrame.record.width == 352.0F);
    static_assert(layout::SheetLeft + layout::SheetImageWidth == layout::CanvasSize.width);
    constexpr SongSelectFrame NarrowFrame = CalculateFrame(960.0F);
    static_assert(NarrowFrame.width == 1280.0F && NarrowFrame.scale == 0.75F);
    constexpr SongSelectFrame UltrawideFrame = CalculateFrame(3000.0F);
    static_assert(UltrawideFrame.width == layout::MaximumWidth && UltrawideFrame.scale == 1.0F);

    // Converts a top-left rectangle into the parent's lower-left, Y-up coordinates.
    [[nodiscard]] constexpr PhantomCore::Rect TopLeft(const PhantomCore::Rect bounds,
                                                       const float parentHeight) noexcept
    {
        return {bounds.x, parentHeight - bounds.y - bounds.height, bounds.width, bounds.height};
    }

    [[nodiscard]] inline std::wstring BpmText(const double bpm)
    {
        std::wstring result = std::format(L"{:.2f}", bpm);
        while (result.ends_with(L'0'))
        {
            result.pop_back();
        }
        if (result.ends_with(L'.'))
        {
            result.pop_back();
        }
        return result;
    }

    template <typename ComponentType>
    [[nodiscard]] inline ComponentType &RequireComponent(PhantomCore::Visual2DNode &node)
    {
        ComponentType *const component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("Song-select node '" + node.Name() +
                                   "' is missing a required component.");
        }
        return *component;
    }

    inline void ApplyFlatStyle(PhantomCore::Visual2DNode &node, const PhantomCore::Color color)
    {
        PhantomCore::VisualStyle style{};
        style.normal = color;
        style.hovered = color;
        style.pressed = color;
        style.disabled = {color.red, color.green, color.blue, 0.45F};
        SetSkinBaseStyle(node, style);
    }

    inline void ApplySpriteStyle(PhantomCore::Visual2DNode &node,
                                 const PhantomCore::Color normal,
                                 const PhantomCore::Color hovered)
    {
        PhantomCore::VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = AccentBlue;
        style.disabled = DisabledBlue;
        SetSkinBaseStyle(node, style);
    }

    inline void ApplyButtonStyle(PhantomCore::Visual2DNode &node,
                                 const PhantomCore::Color normal,
                                 const PhantomCore::Color hovered,
                                 const PhantomCore::Color textColor)
    {
        ApplySpriteStyle(node, normal, hovered);
        RequireComponent<PhantomCore::TextVisualComponent>(node).SetTextColor(textColor);
    }

    inline void SetCornerRadius(PhantomCore::Visual2DNode &node, const float radius)
    {
        RequireComponent<PhantomCore::SpriteVisualComponent>(node).SetCornerRadius(radius);
    }

    // A skinned part: the image supplies the whole shape, so the node itself is plain white.
    // Without the image the fallback colour keeps the control visible.
    // Parts that touch a screen edge or another part pass a zero radius so no corner is cut.
    inline void ApplySkinPart(PhantomCore::Visual2DNode &node, PhantomCore::ScreenVisual2DManager &visuals,
                              const std::string_view key, const PhantomCore::Color fallback,
                              const float cornerRadius = 2.0F)
    {
        PhantomCore::VisualStyle style{};
        style.normal = style.hovered = style.pressed = fallback;
        style.disabled = {fallback.red, fallback.green, fallback.blue, fallback.alpha * 0.45F};
        SetSkinBaseStyle(node, style);
        RequireComponent<PhantomCore::SpriteVisualComponent>(node).SetCornerRadius(cornerRadius);
        BindSkinPart(node, visuals, "MusicSelect", std::string(key));
    }

    [[nodiscard]] inline std::int64_t LatestPointerTimestamp(
        const PhantomCore::InputState &input) noexcept
    {
        std::int64_t result{};
        for (const PhantomCore::InputEvent &event : input.Events())
        {
            if (event.type == PhantomCore::InputEventType::MouseMoved ||
                event.type == PhantomCore::InputEventType::MouseButtonPressed ||
                event.type == PhantomCore::InputEventType::MouseButtonReleased)
            {
                result = event.performanceCounterTicks;
            }
        }
        return result;
    }

    // Caption text: Nunito, or Noto Sans KR when the string holds Hangul/Kana/Han.
    inline void StyleText(PhantomCore::TextVisualComponent &text, const float size, const std::uint16_t weight,
                          const PhantomCore::Color color,
                          const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading)
    {
        text.SetFont(ui_fonts::Text(text.Text()));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }

    // Number text: Barlow Semi Condensed, italic unless stated.
    inline void StyleNumber(PhantomCore::TextVisualComponent &text, const float size, const std::uint16_t weight,
                            const PhantomCore::Color color,
                            const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading,
                            const bool italic = true)
    {
        auto face = ui_fonts::Number(weight, italic);
        text.SetFont(std::move(face.font));
        text.SetFontSize(size);
        text.SetFontWeight(face.weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }

    // Replaces caption text and re-selects the face, because the new string may need CJK glyphs.
    inline void SetCaption(PhantomCore::Visual2DNode &node, std::wstring value)
    {
        auto &text = RequireComponent<PhantomCore::TextVisualComponent>(node);
        text.SetFont(ui_fonts::Text(value));
        text.SetText(std::move(value));
    }

    [[nodiscard]] inline PhantomCore::Visual2DNode &CreateCaption(
        PhantomCore::Visual2DNode &parent, const PhantomCore::Rect topLeftBounds, std::wstring text,
        const float size, const std::uint16_t weight, const PhantomCore::Color color, std::string name,
        const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading)
    {
        auto &label = PhantomCore::CreateLabel(parent, TopLeft(topLeftBounds, parent.NodeSize().height),
                                               std::move(text), std::move(name));
        StyleText(RequireComponent<PhantomCore::TextVisualComponent>(label), size, weight, color, alignment);
        return label;
    }

    [[nodiscard]] inline PhantomCore::Visual2DNode &CreateNumber(
        PhantomCore::Visual2DNode &parent, const PhantomCore::Rect topLeftBounds, std::wstring text,
        const float size, const std::uint16_t weight, const PhantomCore::Color color, std::string name,
        const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading,
        const bool italic = true)
    {
        auto &label = PhantomCore::CreateLabel(parent, TopLeft(topLeftBounds, parent.NodeSize().height),
                                               std::move(text), std::move(name));
        StyleNumber(RequireComponent<PhantomCore::TextVisualComponent>(label), size, weight, color, alignment,
                    italic);
        return label;
    }

    // Rough caption width for sizing tabs before the engine has measured the glyphs.
    [[nodiscard]] inline float EstimateCaptionWidth(const std::wstring_view text, const float size) noexcept
    {
        float width{};
        for (const wchar_t character : text)
            width += size * (character >= 0x1100 ? 1.0F : (character == L' ' ? 0.3F : 0.58F));
        return width;
    }
} // namespace song_select
