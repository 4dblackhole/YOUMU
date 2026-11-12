#include "GameplayPresenter.h"
#include "GameplaySupport.h"
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
    if (selected == nullptr)
        for (const auto &event : feedback.result.events)
            if (event.type == NoteEventType::TickRejected)
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
        // Completion of tick traversal can include misses. OK requires all
        // requested hits/ticks, independently of the head's timing grade.
        const bool success = event.type == NoteEventType::Completed && final->acceptedHits >= final->target.hits &&
                             final->acceptedTicks >= final->target.ticks;
        if (selected != nullptr && selected->noteId != event.noteId &&
            IsAtLeastAsAccurateAs(selected->judgement.grade, JudgementGrade::Bad))
            continue;
        judgementView_.PresentCompletion(success);
    }
}

void GameplayPresenter::PresentErrorMessage(std::string_view message)
{
    if (errorLabel_ == nullptr)
    {
        return;
    }
    std::string combined;
    if (!backgroundWarning_.empty())
    {
        combined = backgroundWarning_;
        if (!message.empty())
            combined += " | " + std::string(message);
        message = combined;
    }
    errorLabel_->SetVisible(!message.empty());
    if (message.empty())
        return;
    const auto &text = Gameplay(texts_);
    RequireComponent<TextVisualComponent>(*errorLabel_)
        .SetText(std::wstring(text.errorPrefix) + Utf8ToWide(message, text.audioInitializationFailed));
}

void GameplayPresenter::UpdateAccuracyPresentation()
{
    const std::optional<double> accuracy = session_->AccuracyRate();
    const double percent = accuracy.value_or(0) * 100.0;
    RequireComponent<TextVisualComponent>(*accuracyIndicator_)
        .SetText(accuracy.has_value()
                     ? std::vformat(texts_.Get("Gameplay.accuracyFormat"), std::make_wformat_args(percent))
                     : std::wstring(texts_.Get("Gameplay.emptyAccuracy")));
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
