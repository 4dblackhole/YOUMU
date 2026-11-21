#include "Automation/InterpolationExpression.h"
#include "EditorTimeAxis.h"
#include "EditorView.h"
#include <algorithm>
#include <cmath>
#include <format>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;
using layout::Channel;

namespace
{
    constexpr float Step = 3;
    struct Track
    {
        std::vector<const EffectCommand *> commands;
        std::vector<std::pair<RhythmTime, RhythmTime>> spans;
    };
    // One automation lane: commands of one type and target in compiled order. Commands at positions
    // a negative delay cut away do not take effect, as in play.
    Track BuildTrack(const IEditorDocument &document, EffectCommandType type, std::string_view target, bool anyTarget)
    {
        Track track;
        const auto &timeline = document.Timeline();
        for (const auto &command : document.Effects().commands)
        {
            if (command.type != type || !timeline.IsPositionActive(command.position))
                continue;
            if (!anyTarget && command.target != target)
                continue;
            track.commands.push_back(&command);
        }
        std::ranges::stable_sort(track.commands, {}, [&](const EffectCommand *c) { return c->position; });
        for (const auto *command : track.commands)
        {
            const auto begin = timeline.Compile(command->position);
            track.spans.emplace_back(begin, command->endPosition ? timeline.Compile(*command->endPosition) : begin);
        }
        return track;
    }
    double ValueAt(const Track &track, RhythmTime time, double fallback)
    {
        double value = fallback;
        for (std::size_t i = 0; i < track.commands.size(); ++i)
        {
            const auto [begin, end] = track.spans[i];
            if (time < begin)
                break;
            const auto &command = *track.commands[i];
            if (command.endPosition && time < end && end > begin)
            {
                const double progress = static_cast<double>((time - begin).count()) / static_cast<double>((end - begin).count());
                value = EvaluateInterpolation(command, std::clamp(progress, 0.0, 1.0));
            }
            else
                value = command.endPosition ? command.endValue : command.beginValue;
        }
        return value;
    }
    std::wstring Decimal(double value)
    {
        return std::format(L"{:g}", value);
    }
    std::wstring Hertz(double value)
    {
        if (value >= 1000)
            return std::format(L"{:g}k", std::round(value / (value >= 10000 ? 1000 : 100)) / (value >= 10000 ? 1 : 10));
        return std::format(L"{}", std::lround(value));
    }
    struct CurveColor
    {
        EffectCommandType type;
        std::string_view target;
        Color color;
    };
} // namespace

void EditorView::Chip(float x, float middle, std::wstring text, Color background, bool selected, float h)
{
    const float size = h < 18 ? 11.0F : 12.0F;
    const float w = EstimateTextWidth(text, size) * .92F + 12;
    if (selected)
        Box({x + 2, middle - h * .5F - 1, w + 2, h + 2}, theme::Sun);
    Box({x + 3, middle - h * .5F, w, h}, background);
    Label({x + 9, middle - h * .5F, w, h}, std::move(text), {size, White, TextAlignment::Leading, 800, true});
}

void EditorView::DrawChannels()
{
    const auto &document = state_.Document();
    const auto &timeline = document.Timeline();
    const float left = layout::HeadWidth, right = ContentRight();
    const auto axis = EditorTimeAxis::Realtime(timeline, state_.Score().realtimeZoom, Areas().judgeX,
                                               state_.TimeMilliseconds());
    const auto visible = [&](float x, float margin) { return x >= left - margin && x <= right + margin; };
    const auto selectedRow = [this](std::size_t row) { return state_.Panel().has_value() && state_.IsListRowSelected(row); };
    const auto rowOf = [&document](const EffectCommand &command) {
        return static_cast<std::size_t>(&command - document.Effects().commands.data());
    };

    // Music: the waveform of the music around the cursor.
    {
        constexpr float top = layout::ChannelTop(Channel::Music), h = layout::ChannelHeight(Channel::Music) - 1;
        Box({left, top, right - left, h}, ui_colors::Rgb(0x0E1424, .92F));
        const auto &sounds = state_.Analysis().Data().sounds;
        const auto music = sounds.find("Music");
        if (music != sounds.end() && music->second.pcm)
        {
            const double begin = axis.TimeAt(left).count() / 1'000'000.0;
            const double window = (axis.TimeAt(right).count() - axis.TimeAt(left).count()) / 1'000'000.0;
            const auto columns = static_cast<std::size_t>(std::max(1.0F, (right - left) / Step));
            const auto peaks = BuildAudioWaveform(*music->second.pcm, begin, window, columns);
            const float middle = top + h * .5F;
            for (std::size_t column = 0; column < peaks.size(); ++column)
            {
                const float upper = std::clamp(peaks[column].maximum, -1.0F, 1.0F) * h * .45F;
                const float lower = std::clamp(peaks[column].minimum, -1.0F, 1.0F) * h * .45F;
                if (upper > lower)
                    Box({left + Step * static_cast<float>(column), middle - upper, Step - 1, upper - lower},
                        ui_colors::Rgb(0x35C47A));
            }
        }
    }

    // Timing: BPM and length chips, delay chips (red cuts the section before it, dark gold pushes).
    {
        constexpr float top = layout::ChannelTop(Channel::Timing), h = layout::ChannelHeight(Channel::Timing);
        const auto &timing = document.Timing();
        for (std::size_t i = 0; i < timing.size(); ++i)
        {
            const auto &directive = timing[i];
            const float x = axis.X(timeline.Compile(directive.position));
            if (!visible(x, 120))
                continue;
            const bool delay = directive.type == TimingDirectiveType::DelayMilliseconds;
            const bool cut = delay && directive.value < 0;
            const auto color = !delay ? theme::Brand : cut ? ui_colors::Rgb(0xC8323A) : ui_colors::Rgb(0xB7790B);
            Box({x - 1, top + 4, 2, h - 8}, color);
            const auto text = directive.type == TimingDirectiveType::Bpm ? Decimal(directive.value) + L" BPM"
                              : directive.type == TimingDirectiveType::MeasureLength
                                  ? Wide(MeasureLengthText(directive.ratio))
                                  : std::format(L"Delay {}{:g} ms", directive.value > 0 ? L"+" : L"−", std::abs(directive.value));
            const bool selected = state_.Panel() == EditorPanel::Timing && state_.IsListRowSelected(i);
            Chip(x, top + h * .5F, text, color, selected);
        }
    }

    // Curves share one sampler: a polyline of short boxes from left to right.
    const auto plot = [&](const Track &track, double fallback, Channel channel, Color color, float bottomValue,
                          float topValue, bool logarithmic) {
        const float top = layout::ChannelTop(channel) + 6, bottom = layout::ChannelTop(channel) + layout::ChannelHeight(channel) - 6;
        const auto yOf = [&](double value) {
            double t = logarithmic ? std::log(std::clamp(value, 20.0, 20000.0) / 20.0) / std::log(1000.0)
                                   : (value - bottomValue) / (topValue - bottomValue);
            return bottom - static_cast<float>(std::clamp(t, 0.0, 1.0)) * (bottom - top);
        };
        float previous = yOf(ValueAt(track, axis.TimeAt(left), fallback));
        for (float x = left; x < right; x += Step)
        {
            const float y = yOf(ValueAt(track, axis.TimeAt(x + Step), fallback));
            Box({x, std::min(previous, y) - 1, Step, std::abs(y - previous) + 2}, color);
            previous = y;
        }
        for (std::size_t i = 0; i < track.commands.size(); ++i)
        {
            const float x = axis.X(track.spans[i].first);
            if (!visible(x, 8))
                continue;
            const auto &command = *track.commands[i];
            const bool selected = selectedRow(rowOf(command));
            const float y = yOf(command.beginValue), r = selected ? 6.0F : 4.0F;
            if (selected)
                Box({x - r - 2, y - r - 2, (r + 2) * 2, (r + 2) * 2}, theme::Sun, r + 2);
            Box({x - r, y - r, r * 2, r * 2}, color, r);
        }
        return yOf;
    };

    // Scroll speed: Whole as a curve around ×1; Separate as gold markers with their value.
    {
        const auto whole = BuildTrack(document, EffectCommandType::ScrollSpeed, {}, true);
        const auto yOf = plot(whole, 1, Channel::ScrollSpeed, theme::Brand, .4F, 2, false);
        const float one = yOf(1);
        for (float x = left; x < right; x += 6)
            Box({x, one, 3, 1}, ui_colors::Rgb(0x1F4C99, .35F));
        const auto separate = BuildTrack(document, EffectCommandType::NoteSpeed, {}, true);
        for (std::size_t i = 0; i < separate.commands.size(); ++i)
        {
            const float x = axis.X(separate.spans[i].first);
            if (!visible(x, 40))
                continue;
            const auto &command = *separate.commands[i];
            const float y = yOf(command.beginValue);
            const bool selected = selectedRow(rowOf(command));
            Box({x - 5, y - 5, 10, 10}, selected ? theme::BrandDeep : theme::Sun, 2);
            Label({x + 9, y - 8, 60, 16}, L"×" + Decimal(command.beginValue), {12, theme::BrandDeep, TextAlignment::Leading, 800, true});
        }
    }

    // Volume: one line per target that the chart uses (HitSound always).
    {
        const std::array targets{CurveColor{EffectCommandType::BusVolume, "HitSound", ui_colors::Rgb(0x2A93E8)},
                                 CurveColor{EffectCommandType::BusVolume, "TickSound", ui_colors::Rgb(0xE39A1E)},
                                 CurveColor{EffectCommandType::BusVolume, "UserInputFeedback", ui_colors::Rgb(0x7C8DA6)}};
        for (std::size_t k = 0; k < targets.size(); ++k)
        {
            const auto track = BuildTrack(document, targets[k].type, targets[k].target, false);
            if (k != 0 && track.commands.empty())
                continue;
            plot(track, 1, Channel::Volume, targets[k].color, 0, 1, false);
        }
    }

    // Audio FX: only the effects in use, each on its own scale, with a short name on its points.
    {
        const std::array effects{CurveColor{EffectCommandType::ReverbSend, "Rev", ui_colors::Rgb(0x14A39A)},
                                 CurveColor{EffectCommandType::LowPassCutoff, "LP", ui_colors::Rgb(0xE0603A)},
                                 CurveColor{EffectCommandType::HighPassCutoff, "HP", ui_colors::Rgb(0x7A5AE0)}};
        constexpr float top = layout::ChannelTop(Channel::AudioEffects);
        for (const auto &effect : effects)
        {
            const auto track = BuildTrack(document, effect.type, {}, true);
            if (track.commands.empty())
                continue;
            const bool filter = effect.type != EffectCommandType::ReverbSend;
            const double fallback = effect.type == EffectCommandType::LowPassCutoff ? 20000 : effect.type == EffectCommandType::HighPassCutoff ? 20 : 0;
            plot(track, fallback, Channel::AudioEffects, effect.color, 0, 1, filter);
            for (std::size_t i = 0; i < track.commands.size(); ++i)
            {
                const float x = axis.X(track.spans[i].first);
                if (!visible(x, 90))
                    continue;
                const auto &command = *track.commands[i];
                const auto value = [&](double v) { return filter ? Hertz(v) : std::format(L"{:.2f}", v); };
                auto text = std::wstring(effect.target.begin(), effect.target.end()) + L" " + value(command.beginValue);
                if (command.endPosition)
                    text += L"→" + value(command.endValue);
                Chip(x + 4, top + 10, text + (filter ? L" Hz" : L""), effect.color, false, 15);
            }
        }
    }

    // Hit sounds: Don changes above, Kat changes below.
    {
        constexpr float top = layout::ChannelTop(Channel::HitSounds), h = layout::ChannelHeight(Channel::HitSounds);
        const auto &effects = document.Effects();
        const auto choices = state_.Mode().SoundChoices(texts_);
        for (std::size_t i = 0; i < effects.hitSoundChanges.size(); ++i)
        {
            const auto &change = effects.hitSoundChanges[i];
            if (!timeline.IsPositionActive(change.position))
                continue;
            const float x = axis.X(timeline.Compile(change.position));
            if (!visible(x, 90))
                continue;
            const auto choice = std::ranges::find(choices, change.keyType, &EditorSoundChoice::keyType);
            const bool first = choice == choices.begin();
            const auto color = first ? ui_colors::Rgb(0xE0353F) : ui_colors::Rgb(0x2380DA);
            Box({x - 1, top + 3, 2, h - 6}, color);
            const auto name = choice == choices.end() ? std::to_wstring(change.keyType) : choice->label;
            Chip(x, top + h * (first ? .3F : .72F), name + L" " + Wide(change.soundIndex), color,
                 selectedRow(effects.commands.size() + i), 16);
        }
    }

    // Zones: Syncopation, Highlight and bar lines on three rows; bar lines off are dotted.
    {
        constexpr float top = layout::ChannelTop(Channel::Zones), h = layout::ChannelHeight(Channel::Zones);
        const float rowHeight = (h - 3) / 3;
        const std::array rows{CurveColor{EffectCommandType::SyncopationZone, "", ui_colors::Rgb(0xA640E6)},
                              CurveColor{EffectCommandType::Highlight, "", ui_colors::Rgb(0xF29A1E)},
                              CurveColor{EffectCommandType::MeasureLineVisible, "", theme::Brand}};
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            const float rowTop = top + 1.5F + rowHeight * static_cast<float>(r), middle = rowTop + rowHeight * .5F;
            const bool bars = rows[r].type == EffectCommandType::MeasureLineVisible;
            if (r != 0)
                Box({left, rowTop, right - left, 1}, ui_colors::Rgb(0x1F4C99, .08F));
            const auto track = BuildTrack(document, rows[r].type, {}, true);
            for (float x = left; x < right; x += Step)
            {
                const bool on = ValueAt(track, axis.TimeAt(x), bars ? 1 : 0) >= .5;
                if (bars)
                {
                    Box({x, middle - (on ? 1.0F : 0.0F), Step, on ? 2.0F : 1.0F}, ui_colors::Rgb(0x1F4C99, on ? .45F : .16F));
                    if (!on && static_cast<int>(x) % 9 < 3)
                        Box({x, middle - 4, 1, 8}, ui_colors::Rgb(0x1F4C99, .16F));
                }
                else if (on)
                    Box({x, rowTop + 3, Step, rowHeight - 6}, {rows[r].color.red, rows[r].color.green, rows[r].color.blue, .38F});
            }
            for (std::size_t i = 0; i < track.commands.size(); ++i)
            {
                const float x = axis.X(track.spans[i].first);
                if (!visible(x, 80))
                    continue;
                const auto &command = *track.commands[i];
                const bool on = command.beginValue >= .5;
                std::wstring text = on ? L"On" : L"Off";
                if (on && rows[r].type == EffectCommandType::SyncopationZone && !command.excludedDivisions.empty())
                {
                    text += L" −";
                    for (const auto division : command.excludedDivisions)
                        text += L" 1/" + std::to_wstring(division * 4);
                }
                Chip(x, middle, text, on ? rows[r].color : ui_colors::Rgb(0x142F5E, .7F), selectedRow(rowOf(command)),
                     rowHeight - 3);
            }
        }
    }
}

void EditorView::DrawCutsAndRange()
{
    const auto &document = state_.Document();
    const auto &timeline = document.Timeline();
    const float left = layout::HeadWidth, right = ContentRight();
    const auto axis = EditorTimeAxis::Realtime(timeline, state_.Score().realtimeZoom, Areas().judgeX,
                                               state_.TimeMilliseconds());
    // A negative delay removes the section from its new time up to the directive's former time.
    const float top = layout::LaneTop, bottom = layout::ChannelsBottom;
    for (const auto &directive : document.Timing())
    {
        const bool cut = directive.type == TimingDirectiveType::DelayMilliseconds && directive.value < 0;
        if (!cut || !timeline.IsPositionActive(directive.position))
            continue;
        const auto begin = timeline.Compile(directive.position);
        const auto end = begin + Microseconds(-directive.value);
        const float x0 = std::max(left, axis.X(begin)), x1 = std::min(right, axis.X(end));
        if (x1 <= x0)
            continue;
        Box({x0, top, x1 - x0, bottom - top}, ui_colors::Rgb(0xE5484D, .16F));
        // Diagonal hatching over the lane, drawn as short steps.
        for (float start = x0 - layout::LaneHeight; start < x1; start += 10)
            for (float d = 0; d < layout::LaneHeight; d += 3)
            {
                const float x = start + d;
                if (x >= x0 && x < x1)
                    Box({x, top + layout::LaneHeight - d - 3, 1.5F, 3}, ui_colors::Rgb(0xE5484D, .45F));
            }
        Box({axis.X(begin) - 1, top, 2, bottom - top}, ui_colors::Rgb(0xE5484D));
        Box({axis.X(end) - 1, top, 2, bottom - top}, ui_colors::Rgb(0xE5484D));
    }

    // The dragged beat range: a wash on the ruler, gold edges and its extent with the note count.
    const auto &range = state_.NoteSelectionRange();
    if (!range)
        return;
    const float a = axis.X(timeline.Compile(range->first)), b = axis.X(timeline.Compile(range->second));
    const float x0 = std::clamp(std::min(a, b), left, right), x1 = std::clamp(std::max(a, b), left, right);
    Box({x0, layout::TrackTop, std::max(0.0F, x1 - x0), layout::RulerHeight}, ui_colors::Rgb(0x1F4C99, .16F));
    for (const float x : {a, b})
        if (x >= left && x <= right)
            Box({x - 1, layout::TrackTop, 2, layout::RulerHeight + layout::LaneHeight}, theme::Sun);
    std::size_t notes = 0;
    for (const auto id : state_.SelectedNoteIds())
        if (const auto note = document.FindNote(id); note && note->head.note.sourceOrder == id)
            ++notes;
    const auto label = [](MusicalPosition p) {
        return std::format(L"{} · {}", p.measure + 1, Wide(Fraction(p.fraction)));
    };
    Chip(std::max(x0, left + 4) - 3, layout::TrackTop + layout::RulerHeight * .5F,
         std::format(L"{} → {} · {} {}", label(range->first), label(range->second), notes,
                     texts_.Get("Editor.notesSuffix")),
         theme::BrandDeep, false);
}

double EditorView::AutomationValueAt(EffectCommandType type, std::string_view target, double fallback) const
{
    const auto track = BuildTrack(state_.Document(), type, target, target.empty());
    return ValueAt(track, Microseconds(state_.TimeMilliseconds()), fallback);
}
