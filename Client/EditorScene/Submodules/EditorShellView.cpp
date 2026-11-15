#include "EditorView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

namespace
{
    std::wstring TimeLabel(const double milliseconds)
    {
        const auto rounded = static_cast<std::int64_t>(std::llround(milliseconds));
        const auto absolute = rounded < 0 ? -rounded : rounded;
        std::wostringstream text;
        if (rounded < 0)
            text << L'−';
        text << absolute / 60'000 << L':' << std::setfill(L'0') << std::setw(2) << absolute / 1000 % 60 << L'.'
             << std::setw(3) << absolute % 1000;
        return text.str();
    }
    constexpr std::array<std::string_view, 3> TabKeys{"Editor.tabs.0", "Editor.tabs.1", "Editor.tabs.2"};
} // namespace

void EditorView::DrawTabs()
{
    Box({0, layout::TabsTop, frame_.width, layout::TabsHeight}, {1, 1, 1, .55F});
    Box({0, layout::BodyTop - 1, frame_.width, 1}, theme::RuleStrong);
    float x = layout::TabsLeft;
    for (std::size_t i = 0; i < TabKeys.size(); ++i)
    {
        const auto tab = static_cast<EditorTab>(i);
        const bool on = state_.Tab() == tab;
        // A new difficulty has only Metadata until it is created.
        const bool locked = state_.Creating() && tab != EditorTab::Metadata;
        std::wstring caption(texts_.Get(TabKeys[i]));
        const float tabWidth = EstimateTextWidth(caption, 15) + 2;
        const Rect r{x, layout::TabsTop + 4, tabWidth, 40};
        auto color = on ? theme::BrandText : theme::Caption;
        if (locked)
            color.alpha = .38F;
        Label(r, caption, {15, color, TextAlignment::Leading, 800});
        if (on)
            Box({r.x, r.y + r.height - 3, r.width, 3}, theme::Brand);
        if (!locked)
            Control(r, [this, tab] { state_.SelectTab(tab); });
        x += tabWidth + layout::TabGap;
    }
    // Status and the last saved file read at the right end of the tab bar.
    const auto savedPath = state_.SavedPath().wstring();
    const bool saved = state_.Status().empty() && !savedPath.empty();
    const auto status = saved ? texts_.Format("Editor.savedFormat", savedPath) : Wide(state_.Status());
    if (!status.empty())
        Label({x + 24, layout::TabsTop, std::max(0.0F, frame_.width - x - 48), layout::TabsHeight}, status,
              {13, saved ? theme::Caption : theme::Danger, TextAlignment::Trailing, 600});
}

void EditorView::DrawFooter()
{
    const float top = layout::FooterTop;
    Box({0, top, frame_.width, layout::FooterHeight}, {1, 1, 1, .7F});
    Box({0, top, frame_.width, 1}, theme::RuleStrong);

    const Rect back{0, top, layout::BackWidth, layout::FooterHeight};
    Slanted(back, 0, layout::Slant, theme::BrandDeep);
    Icon({28, top + 23, 18, 18}, L"arrow-left", White);
    Label({54, top, 80, layout::FooterHeight}, std::wstring(texts_.Get("Editor.back")), {18, White, TextAlignment::Leading, 800});
    Control(back, [this] { leaveRequested_ = true; });

    const Rect save{frame_.width - layout::SaveWidth, top, layout::SaveWidth, layout::FooterHeight};
    Slanted(save, layout::Slant, 0, theme::Brand);
    Label({save.x + 40, top, save.width - 64, layout::FooterHeight},
          std::wstring(state_.Creating() ? texts_.Get("Editor.create") : Texts().save),
          {22, White, TextAlignment::Center, 900});
    Control(save, [this] { SaveDocument(); });

    DrawTransport({layout::BackWidth + 12, top, save.x - layout::BackWidth - 32, layout::FooterHeight});
}

void EditorView::DrawTransport(Rect area)
{
    const float middle = area.y + area.height * .5F;
    const Rect play{area.x, middle - layout::PlaySize * .5F, layout::PlaySize, layout::PlaySize};
    Icon({play.x + 12, play.y + 12, 20, 20}, state_.Playing() ? L"pause" : L"play", theme::InkSoft);
    Control(play, [this] { state_.TogglePlayback(); });

    DrawTimeInput({play.x + play.width + 12, middle - layout::ControlHeight * .5F, layout::TimeWidth,
                   layout::ControlHeight});
    const float rateX = play.x + play.width + 12 + layout::TimeWidth + 12;
    DrawPlaybackRateControl(rateX);

    const auto [begin, end] = state_.TimelineRangeMilliseconds();
    const auto length = TimeLabel(end);
    const float lengthWidth = EstimateTextWidth(length, 14) + 4;
    const float timelineX = rateX + layout::RateStepWidth * 2 + layout::RateValueWidth + 12;
    const float timelineRight = area.x + area.width - lengthWidth - 12;
    DrawTimeline({timelineX, middle - layout::TimelineHeight * .5F, std::max(40.0F, timelineRight - timelineX),
                  layout::TimelineHeight});
    Label({timelineRight + 12, area.y, lengthWidth, area.height}, length,
          {14, theme::Caption, TextAlignment::Leading, 700, true});
}

void EditorView::DrawTimeInput(Rect rect)
{
    // While playing, the field shows the running time and does not take typing.
    if (state_.Playing() || !fields_.Available())
    {
        Box(rect, {1, 1, 1, .74F});
        Box({rect.x, rect.y + rect.height - 2, rect.width, 2}, ui_colors::Rgb(0xB5CFF5));
        Label(rect, TimeLabel(state_.TimeMilliseconds()), {17, theme::Ink, TextAlignment::Center, 800, true});
        if (!state_.Playing())
            Control(rect, [this] {
                if (auto value = EditText(std::wstring(Texts().currentTime), std::to_string(state_.TimeMilliseconds()), Texts()))
                    state_.Seek(Number(*value));
            });
        return;
    }
    InputField(rect, std::wstring(Texts().currentTime), std::format(L"{:g}", state_.TimeMilliseconds()),
               [this](const std::string &typed) { state_.Seek(Number(typed)); }, true, false);
}

void EditorView::DrawPlaybackRateControl(float x)
{
    const float top = layout::FooterTop + (layout::FooterHeight - layout::ControlHeight) * .5F;
    const float total = layout::RateStepWidth * 2 + layout::RateValueWidth;
    Box({x, top, total, 1}, theme::RuleStrong);
    Box({x, top + layout::ControlHeight - 1, total, 1}, theme::RuleStrong);
    Box({x, top, 1, layout::ControlHeight}, theme::RuleStrong);
    Box({x + total - 1, top, 1, layout::ControlHeight}, theme::RuleStrong);
    const Rect slower{x, top, layout::RateStepWidth, layout::ControlHeight};
    const Rect value{x + layout::RateStepWidth, top, layout::RateValueWidth, layout::ControlHeight};
    const Rect faster{value.x + value.width, top, layout::RateStepWidth, layout::ControlHeight};
    Box({value.x, top, 1, layout::ControlHeight}, theme::Rule);
    Box({faster.x - 1, top, 1, layout::ControlHeight}, theme::Rule);
    Label(slower, L"−", {16, theme::Brand, TextAlignment::Center, 800});
    Label(faster, L"+", {16, theme::Brand, TextAlignment::Center, 800});
    const bool changed = std::abs(state_.PlaybackRate() - 1) > 1e-9;
    Label(value, std::format(L"{:.2f}x", state_.PlaybackRate()),
          {17, changed ? theme::Brand : theme::Ink, TextAlignment::Center, 800, true});
    if (changed)
        Box({value.x + 1, top + layout::ControlHeight - 4, value.width - 2, 3}, theme::Sun);
    Control(slower, [this] { StepPlaybackRate(-1, shiftDown_); });
    Control(faster, [this] { StepPlaybackRate(1, shiftDown_); });
    Control(value, [this] { state_.SetPlaybackRate(1); });
}

void EditorView::StepPlaybackRate(int direction, bool coarse)
{
    const double step = coarse ? .25 : .05;
    const double next = std::round((state_.PlaybackRate() + step * direction) * 20) / 20;
    state_.SetPlaybackRate(std::clamp(next, EditorPlayback::MinimumRate, EditorPlayback::MaximumRate));
}

void EditorView::DrawTimeline(Rect rect)
{
    timelineRect_ = rect;
    PlaceNode(*timelineSliderNode_, rect);
    // The slider keeps 8 units of padding at each end of its track.
    const float left = rect.x + 8, track = std::max(1.0F, rect.width - 16);
    const auto [begin, end] = state_.TimelineRangeMilliseconds();
    const float fraction = static_cast<float>(std::clamp((state_.TimeMilliseconds() - begin) / (end - begin), 0.0, 1.0));
    const float middle = rect.y + rect.height * .5F;
    Box({left, middle - layout::TimelineTrack * .5F, track, layout::TimelineTrack}, ui_colors::Rgb(0x0F2A47, .14F));
    Box({left, middle - layout::TimelineTrack * .5F, track * fraction, layout::TimelineTrack}, theme::Brand);
    const float head = left + track * fraction;
    Box({head - 4, middle - 13, 8, 26}, White);
    Box({head - 2, middle - 11, 4, 22}, theme::Sun);
}

void EditorView::SyncInputControls()
{
    const bool compose = state_.Tab() == EditorTab::Compose && !buildFailed_;
    const bool showDivision = compose && !state_.Score().preview;
    const bool showAudio = state_.Tab() == EditorTab::Audio && !buildFailed_;
    for (auto *track : audioSelectionNodes_)
        track->SetVisible(showAudio);
    if (divisionSliderNode_->IsVisible() != showDivision)
    {
        if (!showDivision)
            sliderInput_.Reset(*canvas.Get());
        divisionSliderNode_->SetVisible(showDivision);
        sliderInput_.InvalidateHitTest();
    }
    if (showDivision)
        PlaceNode(*divisionSliderNode_, divisionRect_);
    timelineSliderNode_->SetVisible(true);
    SyncDivisionSlider();
    SyncTimelineSlider();
}

void EditorView::SyncTimelineSlider()
{
    const auto [begin, end] = state_.TimelineRangeMilliseconds();
    timelineSliderNode_->GetComponent<SliderBehaviorComponent>()->SetValue(
        static_cast<float>(std::clamp((state_.TimeMilliseconds() - begin) / (end - begin), 0.0, 1.0)));
}

void EditorView::SyncDivisionSlider() noexcept
{
    divisionSliderNode_->GetComponent<SliderBehaviorComponent>()->SetValue(
        (std::min(state_.Score().division, 16) - 1) / 15.0F);
}
