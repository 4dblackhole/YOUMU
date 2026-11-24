#include "GameplayPresenter.h"
#include "GameplaySupport.h"
#include "Presentation/AccuracyText.h"
#include "Texts/GameScene/RhythmTestScene/GameplayTexts.h"
#include <format>

using namespace PhantomCore;

void GameplayPresenter::PresentJudgementFeedback(const GameplayFeedback &feedback)
{

    // Match the key beam's input priority. A passive miss or a duplicate
    // Completed/HoldStarted event must not overwrite this press's actual hit.
    const NoteEvent *selected = feedback.keyPressed
        ? LaneKeyBeam::InputResponse(feedback.result) : nullptr;
    if (!feedback.keyPressed)
        for (const auto &event : feedback.result.events)
            if (event.type == NoteEventType::Missed)
                selected = &event;
    if (selected != nullptr)
        judgementView_.Present(*selected);
    for (const auto &event : feedback.result.events)
    {
        if (event.type != NoteEventType::Completed && event.type != NoteEventType::Missed)
            continue;
        const auto *presentation = session_->FindNotePresentation(event.noteId);
        if (presentation == nullptr || !presentation->hasEndTime)
            continue;
        const auto final = std::ranges::find(feedback.result.finalizedAccuracies, event.noteId, &NoteAccuracy::noteId);
        if (final == feedback.result.finalizedAccuracies.end())
            continue;
        if (selected != nullptr && selected->noteId != event.noteId &&
            IsAtLeastAsAccurateAs(selected->judgement.grade, JudgementGrade::Bad))
            continue;
        // A finished long note shows the grade of how much of it was processed.
        judgementView_.PresentCompletion(final->CompletionGrade());
    }
}

void GameplayPresenter::UpdateAccuracyPresentation()
{
    const auto accuracy = session_->Accuracy();
    RequireComponent<TextVisualComponent>(*accuracyIndicator_)
        .SetText(accuracy ? AccuracyText(*accuracy) : std::wstring(EmptyAccuracyText));
}

#if defined(_DEBUG)
void GameplayPresenter::UpdateDebugPresentation(const RhythmTime time)
{
    const auto &text = Gameplay(texts_);
    std::wstring debug(text.focusNone);
    if (session_->Gear().LaneCount() > 0)
    {
        if (const auto *note = session_->Gear().Lanes().front()->CurrentNote())
        {
            debug = std::wstring(text.focusPrefix) + note->DebugText();
        }
    }
    if (const auto &last = session_->LastNoteAccuracy())
    {
        const auto noteId = last->noteId;
        const std::wstring details = last->DebugText();
        debug += std::vformat(text.lastFormat, std::make_wformat_args(noteId, details));
    }
    else
    {
        debug += text.lastNone;
    }
    RequireComponent<TextVisualComponent>(*noteDebugLabel_)
        .SetText(std::format(L"DEBUG Timer: {} us ({:.3f} ms)\n{}",
                             time.count(), static_cast<double>(time.count()) / 1000.0, debug));
}
#endif
