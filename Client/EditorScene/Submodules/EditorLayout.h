#pragma once
#include "PhantomCore.h"
#include <algorithm>
#include <array>

// Chart editor arrangement (UI-02). Coordinates are logical pixels of the
// 720-high frame, measured from its top-left corner. The frame keeps the 16:9
// arrangement on narrower screens (scaled between letterbox bands) and widens
// the time axis up to 32:9.
namespace editor_layout
{
    inline constexpr float Height = 720, BaseWidth = 1280, MaximumWidth = 2560;
    inline constexpr float MenuHeight = 36;
    inline constexpr float TabsTop = MenuHeight, TabsHeight = 44, TabsLeft = 24, TabGap = 28;
    inline constexpr float BodyTop = TabsTop + TabsHeight;
    inline constexpr float FooterHeight = 64, FooterTop = Height - FooterHeight;
    inline constexpr float BodyHeight = FooterTop - BodyTop;

    // Footer: Back block, transport, timeline, Save block. Blocks lean by Slant.
    inline constexpr float Slant = 24, BackWidth = 172, SaveWidth = 196;
    inline constexpr float PlaySize = 44, TimeWidth = 128, ControlHeight = 36;
    inline constexpr float RateStepWidth = 30, RateValueWidth = 62;
    inline constexpr float TimelineHeight = 28, TimelineTrack = 6;

    // Compose: tool row above the track area of channel heads and time axis.
    inline constexpr float ToolRowHeight = 52, TrackTop = BodyTop + ToolRowHeight;
    inline constexpr float ViewTabsLeft = 16, ViewTabGap = 18;
    inline constexpr float ToolButtonWidth = 48, ToolStripPadding = 4, ToolStripLeft = 234;
    inline constexpr float ToolStripWidth = ToolButtonWidth * 7 + ToolStripPadding * 2;
    // Beat: caption, 1/1..1/16 slider, value and direct 1/N input.
    inline constexpr float BeatLeft = ToolStripLeft + ToolStripWidth + 20, BeatSliderWidth = 120;
    inline constexpr float HeadWidth = 184, HeadButtonWidth = 36, InspectorWidth = 348;
    inline constexpr float RulerHeight = 28, LaneHeight = 152;
    inline constexpr float LaneTop = TrackTop + RulerHeight;
    // The lane is the game's 228-pixel lane at the editor's height.
    inline constexpr float LaneScale = LaneHeight / 228.0F;
    inline constexpr float JudgeOffset = LaneHeight * .62F;

    enum class Channel
    {
        Notes,
        Music,
        Timing,
        ScrollSpeed,
        Volume,
        AudioEffects,
        HitSounds,
        Zones,
    };
    inline constexpr std::size_t ChannelCount = 8;
    inline constexpr std::array<float, ChannelCount> ChannelHeights{LaneHeight, 40, 44, 52, 46, 44, 42, 57};
    [[nodiscard]] constexpr float ChannelTop(Channel channel) noexcept
    {
        float top = TrackTop + RulerHeight;
        for (std::size_t i = 0; i < static_cast<std::size_t>(channel); ++i)
            top += ChannelHeights[i];
        return top;
    }
    [[nodiscard]] constexpr float ChannelHeight(Channel channel) noexcept
    {
        return ChannelHeights[static_cast<std::size_t>(channel)];
    }
    inline constexpr float ChannelsBottom = ChannelTop(Channel::Zones) + ChannelHeights.back();

    struct Frame
    {
        float width{BaseWidth};
        float scale{1};
    };
    // A screen narrower than 16:9 scales the 16:9 frame; a wider one widens it up to 32:9.
    [[nodiscard]] constexpr Frame CalculateFrame(float logicalWidth) noexcept
    {
        if (logicalWidth < BaseWidth)
            return {BaseWidth, std::max(.001F, logicalWidth / BaseWidth)};
        return {std::min(logicalWidth, MaximumWidth), 1};
    }

    // Right edge of the time axis: the inspector pushes the content instead of covering it.
    [[nodiscard]] constexpr float ContentRight(float frameWidth, bool inspectorOpen) noexcept
    {
        return inspectorOpen ? frameWidth - InspectorWidth : frameWidth;
    }
} // namespace editor_layout
