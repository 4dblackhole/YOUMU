#include "EditorAudioTab.h"
#include <algorithm>
#include <format>

using namespace PhantomCore;
using namespace editor_ui;
namespace theme = ui_colors::theme;

void EditorAudioTab::DrawAudioTools()
{
    // Metronome, visible range and pattern offset share one row above the tracks.
    const float top = editor_layout::BodyTop + 10, middle = top + 20;
    DrawSwitch({24, top, 150, 40}, std::wstring(texts_.Get("Editor.metronome")), state_.Metronome().Enabled(),
               [this] { state_.ToggleMetronome(); });
    const auto step = [this](float x, std::wstring caption, std::function<void()> action) {
        const Rect r{x, middle - 13, 26, 26};
        Box(r, {1, 1, 1, .85F});
        Box({r.x, r.y + r.height - 2, r.width, 2}, ui_colors::Rgb(0x9CCBFA));
        Label(r, std::move(caption), {15, theme::Brand, TextAlignment::Center, 800});
        Control(r, std::move(action));
    };
    const float zoomX = 24 + 150 + 28;
    step(zoomX, L"−", [this] {
        state_.audioWindow = std::min(120.0, state_.audioWindow * 2);
        state_.RequestRebuild();
    });
    Label({zoomX + 28, middle - 10, 50, 20}, std::format(L"{:.1f} s", state_.audioWindow),
          {13, theme::Ink, TextAlignment::Center, 800, true});
    step(zoomX + 80, L"+", [this] {
        state_.audioWindow = std::max(.25, state_.audioWindow / 2);
        state_.RequestRebuild();
    });

    const float right = frame_.width - 24;
    const auto offset = state_.Document().Pattern().patternOffsetMilliseconds;
    const float plusX = right - 40, valueX = plusX - 6 - 92, minusX = valueX - 6 - 40;
    Label({minusX - 60, middle - 10, 54, 20}, std::wstring(texts_.Get("Editor.offset")),
          {13, theme::InkSoft, TextAlignment::Trailing, 700});
    const auto offsetStep = [this, &step](float x, std::string_view key, double delta) {
        step(x, std::wstring(texts_.Get(key)), [this, delta] {
            state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds + delta);
        });
    };
    offsetStep(minusX, "Editor.offsetDecrease", -1);
    const Rect value{valueX, middle - 16, 92, 32};
    InputField(value, std::wstring(Texts().offsetMilliseconds), std::format(L"{:g}", offset),
               [this](const std::string &typed) { state_.SetPatternOffset(Number(typed)); }, true, false);
    offsetStep(plusX, "Editor.offsetIncrease", 1);
}

void EditorAudioTab::DrawAudioAlignment(double begin)
{
    const auto waveform = audioTracks_[0];
    const auto hits = audioTracks_[2];
    const double end = begin + state_.audioWindow;
    const auto xAt = [&](double seconds)
    { return waveform.x + static_cast<float>((seconds - begin) / state_.audioWindow) * waveform.width; };
    const auto &beats = state_.Metronome().Beats();
    for (auto beat = std::ranges::lower_bound(beats, begin, {}, &EditorMetronome::Beat::seconds);
         beat != beats.end() && beat->seconds < end; ++beat)
    {
        const float x = xAt(beat->seconds);
        Box({x, waveform.y, beat->accent ? 2.0F : 1.0F, 6}, Blue);
        Box({x, waveform.y + 6, 1, waveform.height - 6}, {Blue.red, Blue.green, Blue.blue, beat->accent ? .5F : .2F});
    }
    // Expected hits are marked on the spectrum's own overlay so its tiles cannot cover them.
    auto &overlay = *audioOverlays_[1];
    const auto &markers = state_.Analysis().Markers();
    for (auto marker = std::ranges::lower_bound(markers, begin, {}, &AudioMarker::seconds);
         marker != markers.end() && marker->seconds < end; ++marker)
        overlay.AddRectangle({xAt(marker->seconds) - waveform.x, hits.height - 8, 2, 8}, Blue, 0);
}
