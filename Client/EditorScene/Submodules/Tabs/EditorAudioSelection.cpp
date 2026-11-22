#include "../EditorTime.h"
#include "EditorAudioTab.h"
#include <algorithm>
#include <string>

using namespace PhantomCore;

using namespace editor_ui;

void EditorAudioTab::InitializeAudioSelection()
{
    for (std::size_t index = 0; index < audioSelectionNodes_.size(); ++index)
    {
        // Bounds follow the frame in ReloadSize.
        auto &track = node->CreateChild("Audio.Selection." + std::to_string(index));
        track.SetZIndex(2);
        track.AddComponent<RectangleCollider2DComponent>();
        track.AddComponent<PointerReceiverComponent>(
            [this](Visual2DNode &target, const PointerEvent &event, std::vector<Action> &)
            { UpdateAudioSelection(target, event); });
        audioSelectionNodes_[index] = &track;
    }
}

void EditorAudioTab::UpdateAudioSelection(Visual2DNode &target, const PointerEvent &event)
{
    if (state_.Tab() != EditorTab::Audio || view_.BuildFailed())
        return;
    const bool press = event.type == PointerEventType::Press && event.button == PointerButton::Left;
    const bool release = event.type == PointerEventType::Release && event.button == PointerButton::Left;
    const bool drag = event.type == PointerEventType::Move && target.IsPressed();
    if (!press && !release && !drag)
        return;
    if (press)
    {
        audioSelectionAnchor_.reset();
        // Keep the coordinate mapping stable until release, even during playback.
        audioSelectionViewBegin_ = state_.TimeMilliseconds() / 1000.0 - state_.audioWindow * .25;
        audioSelectionViewWindow_ = state_.audioWindow;
    }
    const double fraction =
        std::clamp(static_cast<double>(event.localPosition.x) / std::max(1.0F, audioTracks_[0].width), 0.0, 1.0);
    const auto time =
        Microseconds((audioSelectionViewBegin_ + audioSelectionViewWindow_ * fraction) * 1000.0);
    if (press)
        audioSelectionAnchor_ = time;
    if (audioSelectionAnchor_)
        state_.SelectAudioRange(*audioSelectionAnchor_, time);
    if (release)
        audioSelectionAnchor_.reset();
}

void EditorAudioTab::DrawAudioSelection(const double begin)
{
    const auto waveform = audioTracks_[0];
    const Rect line{waveform.x, waveform.y + waveform.height + 8, waveform.width, 20};
    const auto &selection = state_.SelectedAudioRange();
    if (!selection)
    {
        Label(line, std::wstring(texts_.Get("Editor.audioSelectionHelp")), {12, ui_colors::theme::Caption, TextAlignment::Leading, 600});
        return;
    }
    const double firstMs = selection->begin.count() / 1000.0;
    const double lastMs = selection->end.count() / 1000.0;
    Label(line, texts_.Format("Editor.audioSelectionFormat", firstMs, lastMs, lastMs - firstMs),
          {12, ui_colors::theme::InkSoft, TextAlignment::Leading, 700});
    const double first = (firstMs / 1000.0 - begin) / state_.audioWindow;
    const double last = (lastMs / 1000.0 - begin) / state_.audioWindow;
    if (last < 0 || first > 1)
        return;
    const float left = static_cast<float>(std::clamp(first, 0.0, 1.0)) * waveform.width;
    const float right = static_cast<float>(std::clamp(last, 0.0, 1.0)) * waveform.width;
    const Color fill{Blue.red, Blue.green, Blue.blue, .24F};
    Box({waveform.x + left, waveform.y, right - left, waveform.height}, fill);
    Box({waveform.x + std::min(left, waveform.width - 1), waveform.y, 1, waveform.height}, Blue);
    Box({std::min(waveform.x + right, waveform.x + waveform.width - 1), waveform.y, 1, waveform.height}, Blue);
    for (std::size_t track = 0; track < audioOverlays_.size(); ++track)
    {
        auto *overlay = audioOverlays_[track];
        const float trackHeight = audioTracks_[track + 1].height;
        overlay->AddRectangle({left, 0, right - left, trackHeight}, fill, 0);
        overlay->AddRectangle({std::min(left, waveform.width - 1), 0, 1, trackHeight}, Blue, 0);
        overlay->AddRectangle({std::min(right, waveform.width - 1), 0, 1, trackHeight}, Blue, 0);
    }
}
