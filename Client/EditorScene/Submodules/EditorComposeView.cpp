#include "App/AssetPaths.h"
#include "EditorTimeAxis.h"
#include "EditorView.h"
#include <algorithm>
#include <format>
#include <sstream>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;
using layout::Channel;

namespace
{
    struct ChannelInfo
    {
        Channel channel;
        std::wstring_view icon;
        std::string_view caption;
    };
    constexpr std::array Channels{
        ChannelInfo{Channel::Music, L"volume", "Editor.channelMusic"},
        ChannelInfo{Channel::Timing, L"clock", "Editor.channelTiming"},
        ChannelInfo{Channel::ScrollSpeed, L"sliders", "Editor.channelScrollSpeed"},
        ChannelInfo{Channel::Volume, L"volume", "Editor.channelVolume"},
        ChannelInfo{Channel::AudioEffects, L"fx", "Editor.channelAudioEffects"},
        ChannelInfo{Channel::HitSounds, L"drum", "Editor.channelHitSounds"},
        ChannelInfo{Channel::Zones, L"zones", "Editor.channelZones"},
    };
    std::wstring PositionLabel(const MusicalTimeline &timeline, double milliseconds)
    {
        const auto whole = std::max(0.0L, timeline.WholeNotesAtTime(Microseconds(milliseconds)));
        const auto position = timeline.PositionAtWholeNotes(
            Rational{static_cast<std::int64_t>(std::floor(whole * 16)), 16});
        return std::format(L"{} · {}", position.measure + 1, Wide(Fraction(position.fraction)));
    }
} // namespace

void EditorView::DrawCompose()
{
    const bool realtime = state_.Score().realtime && !state_.Score().preview;
    if (realtime)
    {
        DrawHeads();
        DrawRuler();
        DrawChannels();
    }
    state_.Mode().DrawChart(*this, state_, texts_);
    if (realtime)
    {
        DrawCutsAndRange();
        // The current time crosses the ruler, lane and channels at the judgement line.
        const auto areas = Areas();
        Box({areas.judgeX - 1, layout::TrackTop, 2, layout::ChannelsBottom - layout::TrackTop}, theme::Sun);
        Box({areas.judgeX - 4, layout::TrackTop, 8, 4}, theme::Sun);
    }
    if (state_.Mode().HasPendingPlacement() && !state_.Score().preview)
        Label({layout::HeadWidth + 12, layout::FooterTop - 30, ContentRight() - layout::HeadWidth - 24, 24},
              std::wstring(Texts().finishLongNote), {13, theme::Brand, TextAlignment::Leading, 700});
    DrawPanel();
    DrawToolRow();
    state_.Mode().DrawToolMenu(*this, state_, texts_);
}

void EditorView::DrawToolRow()
{
    Box({0, layout::BodyTop, frame_.width, layout::ToolRowHeight}, theme::Surface);
    Box({0, layout::BodyTop, frame_.width, layout::ToolRowHeight}, {1, 1, 1, .5F});
    Box({0, layout::TrackTop - 1, frame_.width, 1}, theme::RuleStrong);
    float x = layout::ViewTabsLeft;
    const auto &score = state_.Score();
    const std::array<std::string_view, 3> views{"Editor.realtime", "Editor.overview", "Editor.preview"};
    for (std::size_t i = 0; i < views.size(); ++i)
    {
        std::wstring caption(texts_.Get(views[i]));
        const float w = EstimateTextWidth(caption, 14) + 2;
        const Rect r{x, layout::BodyTop, w, 40};
        const bool on = i == 2 ? score.preview : !score.preview && score.realtime == (i == 0);
        Label(r, caption, {14, on ? theme::BrandText : theme::Caption, TextAlignment::Leading, 800});
        if (on)
            Box({r.x, r.y + r.height - 3, r.width, 3}, theme::Brand);
        Control(r, [this, i] { i == 2 ? state_.SetPreview() : state_.SetRealtime(i == 0); });
        x += w + layout::ViewTabGap;
    }
    // Preview is read-only: no note tools or beat grid.
    if (score.preview)
        return;
    state_.Mode().DrawTools(*this, state_, texts_);
    DrawBeatControl();
    if (!score.realtime)
        DrawLineBreakToggle();
}

void EditorView::DrawLineBreakToggle()
{
    // --- starts a new score row at the cursor's measure; lit while that measure already starts one.
    const float middle = layout::BodyTop + layout::ToolRowHeight * .5F;
    Button({layout::LineBreakLeft, middle - 16, layout::LineBreakWidth, 32},
           L"---  " + std::wstring(texts_.Get("Editor.lineBreak")), [this] { state_.ToggleSystemBreak(); },
           state_.HasSystemBreakAtCursor());
}

void EditorView::DrawBeatControl()
{
    const float middle = layout::BodyTop + layout::ToolRowHeight * .5F;
    float x = layout::BeatLeft;
    Label({x, middle - 10, 30, 20}, std::wstring(Texts().beatDivider), {13, theme::InkSoft, TextAlignment::Leading, 700});
    x += 38;
    divisionRect_ = {x - 8, middle - 9, layout::BeatSliderWidth + 16, 18};
    const float value = (std::min(state_.Score().division, 16) - 1) / 15.0F;
    Box({x, middle - 2, layout::BeatSliderWidth, 4}, ui_colors::Rgb(0x0F2A47, .14F));
    Box({x, middle - 2, layout::BeatSliderWidth * value, 4}, theme::Brand);
    Box({x + layout::BeatSliderWidth * value - 3, middle - 9, 6, 18}, theme::Brand);
    x += layout::BeatSliderWidth + 10;
    Label({x, middle - 12, 44, 24}, L"1/" + std::to_wstring(state_.Score().division),
          {20, theme::Brand, TextAlignment::Leading, 800, true});
    x += 50;
    Label({x, middle - 10, 16, 20}, L"1/", {14, theme::Caption, TextAlignment::Leading, 800, true});
    const Rect input{x + 16, middle - 16, 84, 32};
    const bool custom = state_.Score().division > 16;
    InputField(input, std::wstring(Texts().beatDivisionDialog), custom ? std::to_wstring(state_.Score().division) : L"",
               [this](const std::string &typed) {
                   if (!typed.empty())
                       state_.SetDivision(Integer(typed));
               },
               true, false);
}

std::wstring EditorView::HeadValue(Channel channel) const
{
    const auto &document = state_.Document();
    const auto &timeline = document.Timeline();
    const auto whole = std::max(0.0L, timeline.WholeNotesAtTime(Microseconds(state_.TimeMilliseconds())));
    const auto position = timeline.PositionAtWholeNotes(
        Rational{static_cast<std::int64_t>(std::floor(whole * 1024)), 1024});
    switch (channel)
    {
    case Channel::Notes:
        return std::to_wstring(std::ranges::count_if(document.Pattern().notes,
                                                     [](const PatternNote &note) { return note.actionType != 2; }));
    case Channel::Music:
        return std::format(L"{:g} ms", document.Pattern().patternOffsetMilliseconds);
    case Channel::Timing:
    {
        double bpm = timeline.BaseBpm();
        std::optional<MusicalPosition> latest;
        for (const auto &directive : document.Timing())
        {
            const bool before = directive.type == TimingDirectiveType::Bpm && directive.position <= position;
            if (before && (!latest || *latest <= directive.position))
            {
                latest = directive.position;
                bpm = directive.value;
            }
        }
        // Lengths read in quarter notes, as in the Timing panel (4/4, 3/4 ...).
        return std::format(L"{:g} · {}", bpm, Wide(MeasureLengthText(timeline.MeasureLength(position.measure))));
    }
    case Channel::ScrollSpeed:
        return std::format(L"×{:.2f}", document.EffectValueAt(EffectCommandType::ScrollSpeed, position));
    case Channel::Volume:
        return std::format(L"{}%", std::lround(AutomationValueAt(EffectCommandType::BusVolume, "HitSound", 1) * 100));
    case Channel::AudioEffects:
    {
        // Only effects away from their neutral value are listed.
        std::wstring text;
        const auto append = [&text](std::wstring part) { text += (text.empty() ? L"" : L" · ") + part; };
        const double reverb = AutomationValueAt(EffectCommandType::ReverbSend, {}, 0);
        const double lowPass = AutomationValueAt(EffectCommandType::LowPassCutoff, {}, 20000);
        const double highPass = AutomationValueAt(EffectCommandType::HighPassCutoff, {}, 20);
        if (reverb != 0)
            append(std::format(L"Rev {:.2f}", reverb));
        if (lowPass != 20000)
            append(std::format(L"LP {}", std::lround(lowPass)));
        if (highPass != 20)
            append(std::format(L"HP {}", std::lround(highPass)));
        return text.empty() ? L"—" : text;
    }
    case Channel::HitSounds:
    {
        // The current sound of each key: the latest change at or before the cursor.
        std::wstring text;
        for (const auto &choice : state_.Mode().SoundChoices(texts_))
        {
            std::string sound = "Def";
            std::optional<MusicalPosition> latest;
            for (const auto &change : document.Effects().hitSoundChanges)
            {
                const bool applies = change.keyType == choice.keyType && change.position <= position;
                if (applies && (!latest || *latest <= change.position))
                {
                    latest = change.position;
                    sound = change.soundIndex;
                }
            }
            text += (text.empty() ? L"" : L" · ") + Wide(sound);
        }
        return text;
    }
    default:
        return {};
    }
}

void EditorView::DrawChannelHead(Channel channel, std::wstring_view icon, std::wstring name, std::wstring value)
{
    const float top = layout::ChannelTop(channel), h = layout::ChannelHeight(channel);
    const bool open = state_.Panel() == channel;
    const Rect button{0, top, layout::HeadButtonWidth, h};
    Box(button, open ? theme::Brand : ui_colors::Rgb(0x1F4C99, .08F));
    if (open)
        Box({0, top, 4, h}, theme::Sun);
    Icon({9, top + (h - 18) * .5F, 18, 18}, icon, open ? White : theme::Brand);
    Control(button, [this, channel, open] { open ? state_.ClosePanel() : state_.OpenPanel(channel); });
    Label({layout::HeadButtonWidth + 8, top, layout::HeadWidth - layout::HeadButtonWidth - 70, h}, std::move(name),
          {13, theme::Ink, TextAlignment::Leading, 700});
    Label({layout::HeadWidth - 70, top, 60, h}, std::move(value), {14, theme::Brand, TextAlignment::Trailing, 800, true});
    Box({0, top + h - 1, layout::HeadWidth, 1}, theme::Rule);
}

void EditorView::DrawHeads()
{
    const float bottom = layout::FooterTop;
    Box({0, layout::TrackTop, layout::HeadWidth, bottom - layout::TrackTop}, {1, 1, 1, .62F});
    Box({layout::HeadWidth - 1, layout::TrackTop, 1, bottom - layout::TrackTop}, theme::RuleStrong);
    Box({0, layout::TrackTop, layout::HeadWidth, layout::RulerHeight}, ui_colors::Rgb(0xEAF1FA, .97F));
    Box({0, layout::LaneTop - 1, layout::HeadWidth, 1}, theme::Rule);
    Label({0, layout::TrackTop, layout::HeadWidth - 10, layout::RulerHeight},
          PositionLabel(state_.Document().Timeline(), state_.TimeMilliseconds()),
          {14, theme::Brand, TextAlignment::Trailing, 800, true});

    // Lane head: name and note count above, the note spacing −/+ below.
    const float laneTop = layout::LaneTop, laneHeight = layout::LaneHeight;
    const bool notesOpen = state_.Panel() == Channel::Notes;
    Box({0, laneTop, layout::HeadWidth, laneHeight}, theme::Brand);
    Image({0, laneTop, layout::HeadWidth, laneHeight}, EditorSound(L"LaneHead.png"), White);
    const Rect button{0, laneTop, layout::HeadButtonWidth, laneHeight};
    Box(button, notesOpen ? theme::Brand : Color{1, 1, 1, .1F});
    if (notesOpen)
        Box({0, laneTop, 4, laneHeight}, theme::Sun);
    Icon({9, laneTop + (laneHeight - 18) * .5F, 18, 18}, L"music", White);
    Control(button, [this, notesOpen] { notesOpen ? state_.ClosePanel() : state_.OpenPanel(Channel::Notes); });
    const float left = layout::HeadButtonWidth + 8, right = layout::HeadWidth - 10;
    Label({left, laneTop + 10, 80, 20}, std::wstring(texts_.Get("Editor.channelNotes")), {14, White, TextAlignment::Leading, 700});
    Label({right - 60, laneTop + 10, 60, 20}, HeadValue(Channel::Notes), {14, theme::Sun, TextAlignment::Trailing, 800, true});
    Label({left, laneTop + 78, 100, 16}, std::wstring(Texts().realtimeZoom), {12, {1, 1, 1, .62F}, TextAlignment::Leading, 700});
    const float controlTop = laneTop + laneHeight - 14 - 28;
    const Rect narrower{left, controlTop, 30, 28}, wider{right - 30, controlTop, 30, 28};
    Box(narrower, {1, 1, 1, .12F});
    Box(wider, {1, 1, 1, .12F});
    Label(narrower, L"−", {16, White, TextAlignment::Center, 800});
    Label(wider, L"+", {16, White, TextAlignment::Center, 800});
    Label({narrower.x + 30, controlTop, wider.x - narrower.x - 30, 28},
          std::format(L"{}%", std::lround(state_.Score().realtimeZoom * 100)), {16, White, TextAlignment::Center, 800, true});
    Control(narrower, [this] { StepSpacing(-1); });
    Control(wider, [this] { StepSpacing(1); });

    for (const auto &info : Channels)
        DrawChannelHead(info.channel, info.icon, std::wstring(texts_.Get(info.caption)), HeadValue(info.channel));
    DrawZoneHeads();
}

void EditorView::DrawZoneHeads()
{
    // Zones carries three rows in its head: Syncopation, Kiai and bar lines with their current state.
    constexpr float top = layout::ChannelTop(Channel::Zones), h = layout::ChannelHeight(Channel::Zones);
    Box({layout::HeadButtonWidth + 1, top, layout::HeadWidth - layout::HeadButtonWidth - 2, h - 1}, ui_colors::Rgb(0xF5F9FE));
    const std::array<std::tuple<std::string_view, EffectCommandType, Color>, 3> rows{
        std::tuple{std::string_view{"Editor.zoneSyncopation"}, EffectCommandType::SyncopationZone, ui_colors::Rgb(0xA640E6)},
        std::tuple{std::string_view{"Editor.zoneKiai"}, EffectCommandType::Kiai, ui_colors::Rgb(0xF29A1E)},
        std::tuple{std::string_view{"Editor.zoneBarLines"}, EffectCommandType::MeasureLineVisible, theme::Brand}};
    const float rowHeight = 18, first = top + (h - rowHeight * 3) * .5F;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto &[key, type, color] = rows[i];
        const float y = first + rowHeight * static_cast<float>(i);
        const bool bars = type == EffectCommandType::MeasureLineVisible;
        const bool on = AutomationValueAt(type, {}, bars ? 1 : 0) >= .5;
        Box({layout::HeadButtonWidth + 8, y + 3.5F, 3, 11}, color);
        Label({layout::HeadButtonWidth + 17, y, 90, rowHeight}, std::wstring(texts_.Get(key)),
              {12, theme::Ink, TextAlignment::Leading, 700});
        Label({layout::HeadWidth - 50, y, 40, rowHeight}, on ? L"On" : L"Off",
              {13, theme::Brand, TextAlignment::Trailing, 800, true});
    }
}

void EditorView::StepSpacing(int direction)
{
    const auto &levels = EditorScoreState::SpacingLevels;
    const double current = state_.Score().realtimeZoom;
    // Move to the next listed level beyond the current spacing.
    auto next = direction > 0 ? std::ranges::upper_bound(levels, current + 1e-9)
                              : std::ranges::lower_bound(levels, current - 1e-9);
    if (direction < 0)
    {
        if (next == levels.begin())
            return;
        --next;
    }
    if (next == levels.end())
        return;
    state_.SetRealtimeZoom(*next);
}

void EditorView::DrawRuler()
{
    const float right = ContentRight();
    const Rect ruler{layout::HeadWidth, layout::TrackTop, right - layout::HeadWidth, layout::RulerHeight};
    Box(ruler, ui_colors::Rgb(0xEAF1FA, .97F));
    Box({ruler.x, ruler.y + ruler.height - 1, ruler.width, 1}, ui_colors::Rgb(0x1F4C99, .25F));
    // Channel rows alternate light washes under the shared time axis.
    for (std::size_t i = 1; i < layout::ChannelCount; ++i)
    {
        const auto channel = static_cast<Channel>(i);
        const float top = layout::ChannelTop(channel), h = layout::ChannelHeight(channel);
        Box({ruler.x, top, ruler.width, h}, {1, 1, 1, i % 2 ? .8F : .6F});
        Box({ruler.x, top + h - 1, ruler.width, 1}, ui_colors::Rgb(0x1F4C99, .14F));
    }

    const auto &timeline = state_.Document().Timeline();
    const auto areas = Areas();
    const auto axis = EditorTimeAxis::Realtime(timeline, state_.Score().realtimeZoom, areas.judgeX,
                                               state_.TimeMilliseconds());
    const int division = state_.Score().division;
    const auto first = MeasureNearTime(timeline, axis.TimeAt(ruler.x).count() / 1000.0);
    const auto last = MeasureNearTime(timeline, axis.TimeAt(right).count() / 1000.0);
    constexpr float channelsTop = layout::ChannelTop(Channel::Music);
    for (auto m = std::max<std::int64_t>(0, first - 1); m <= std::min(last + 1, MaximumMeasureCount - 1); ++m)
    {
        const auto span = timeline.MeasureSpan(m);
        if (span.end <= span.begin)
            continue;
        const int count = static_cast<int>(
            std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * division * 4)));
        float lastX = -1e9F;
        for (int i = 0; i < count; ++i)
        {
            const MusicalPosition p{m, {i, division * 4}};
            if (!timeline.IsPositionActive(p))
                continue;
            const float x = axis.X(timeline.Compile(p));
            if (x < ruler.x || x > right || x - lastX < 3)
                continue;
            lastX = x;
            const bool measureStart = i == 0, beat = i % division == 0;
            const float top = measureStart ? 4.0F : beat ? 14.0F : 19.0F;
            Box({x, ruler.y + top, measureStart ? 2.0F : 1.0F, layout::RulerHeight - top - 1},
                measureStart ? theme::Brand : ui_colors::Rgb(0x1F4C99, beat ? .55F : .25F));
            if (beat)
                Box({x, channelsTop, 1, layout::ChannelsBottom - channelsTop},
                    ui_colors::Rgb(0x1F4C99, measureStart ? .28F : .09F));
            if (measureStart)
                Label({x + 5, ruler.y + 2, 60, 16}, std::to_wstring(m + 1), {13, theme::Brand, TextAlignment::Leading, 800, true});
        }
    }
    Control(ruler, [this, axis] {
        if (lastPointer_)
            state_.Seek(axis.TimeAt(lastPointer_->x).count() / 1000.0);
    });
}

Rect EditorView::PanelBody() const noexcept
{
    const float left = frame_.width - layout::InspectorWidth;
    return {left + 20, layout::TrackTop + 48 + 12, layout::InspectorWidth - 40,
            layout::FooterTop - layout::TrackTop - 48 - 12 - 16};
}

void EditorView::DrawPanelHeader(std::wstring title)
{
    const float left = frame_.width - layout::InspectorWidth;
    const float h = layout::FooterTop - layout::TrackTop;
    Box({left, layout::TrackTop, layout::InspectorWidth, h}, ui_colors::Rgb(0xF4F8FD));
    Box({left, layout::TrackTop, 4, h}, theme::Brand);
    Box({left, layout::TrackTop + 47, layout::InspectorWidth, 1}, theme::Rule);
    Label({left + 20, layout::TrackTop, layout::InspectorWidth - 80, 48}, std::move(title),
          {16, theme::Ink, TextAlignment::Leading, 900});
    const Rect close{frame_.width - 44, layout::TrackTop + 8, 32, 32};
    Icon({close.x + 7, close.y + 7, 18, 18}, L"x", theme::InkSoft);
    Control(close, [this] { state_.ClosePanel(); });
}

void EditorView::DrawPanel()
{
    if (!state_.Panel())
        return;
    const auto panel = *state_.Panel();
    const auto info = std::ranges::find(Channels, panel, &ChannelInfo::channel);
    DrawPanelHeader(std::wstring(texts_.Get(info == Channels.end() ? "Editor.channelNotes" : info->caption)));
    const auto body = PanelBody();
    if (panel == Channel::Notes)
        state_.Mode().DrawNotePanel(*this, state_, texts_);
    else if (panel == Channel::Music)
        DrawMusicPanel(body);
    else if (panel == Channel::Timing)
        DrawTimingPanel(body);
    else
        DrawEffectPanel(body, panel);
}
