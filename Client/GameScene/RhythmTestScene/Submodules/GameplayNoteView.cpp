#include "GameplayNoteView.h"
#include "GameplaySupport.h"

#include <format>
#include <limits>

using namespace PhantomCore;

namespace
{
    constexpr std::wstring_view NoteImageKeys[]{L"note",
                                                L"noteoverlay",
                                                L"bignote",
                                                L"bigcircleoverlay",
                                                L"LNBody",
                                                L"LNBodyOverlay",
                                                L"LNTail",
                                                L"LNTailOverlay",
                                                L"BigLNBody",
                                                L"BigLNBodyOverlay",
                                                L"BigLNTail",
                                                L"BigLNTailOverlay",
                                                L"TickMarker",
                                                L"BuzzTickDiamond",
                                                L"Balloon",
                                                L"DengDeng",
                                                L"BalloonProcessing",
                                                L"BalloonBurst",
                                                L"DengDengProcessing",
                                                L"HitCounterCloud"};
}

void GameplayNoteView::AppendImagePaths(std::vector<std::filesystem::path> &paths)
{
    for (const auto file : NoteImageKeys)
        paths.push_back(InGameSkinAssetPath(file));
}

void GameplayNoteView::Attach(Visual2DCanvas &canvas, Visual2DNode &laneRoot)
{
    canvas_ = &canvas;
    laneRoot_ = &laneRoot;
    // Note order is local to this layer, above the key beam and judgement circle.
    noteRoot_ = &laneRoot.CreateChild("Lane.Notes");
    noteRoot_->SetZIndex(4);
}

void GameplayNoteView::Initialize(ScreenVisual2DManager &visuals, TextCatalog &texts, const PlaySession &session)
{
    if (!laneRoot_)
        throw std::logic_error("Note visuals require a Lane root.");
    visuals_ = &visuals;
    texts_ = &texts;
    session_ = &session;
    for (const auto file : NoteImageKeys)
        static_cast<void>(NoteImage(file));
    const auto normalSize = ScaledImageSize(visuals, NoteImage(L"note"));
    pixelsPerWholeNote_ = normalSize.width * 16.0F * SixteenthSpacingInHeadDiameters;
    for (const auto file : {L"note", L"bignote", L"Balloon", L"DengDeng"})
        largestHeadRadius_ = std::max(largestHeadRadius_, ScaledImageSize(visuals, NoteImage(file)).width * .5F);
    visibility_.Bind(session.Gear());
}

void GameplayNoteView::SetLaneLayout(const float laneWidth, const float judgementLocalY)
{
    laneWidth_ = laneWidth;
    judgementLocalY_ = judgementLocalY;
    UpdateTravelLayout();
}

RhythmTime GameplayNoteView::InitialTimelineTime() const
{
    return visibility_.InitialTimelineTime(*session_);
}

void GameplayNoteView::Shutdown() noexcept
{
    noteVisuals_.clear();
    visibility_.Reset();
    noteImages_.clear();
    presentationGeneration_ = 0;
    presentedNoteIds_.clear();
    completionEffects_.clear();
    measureLineVisuals_.clear();
    largestHeadRadius_ = 0.0F;
    noteTravelDistance_ = 1.0F;
    pixelsPerWholeNote_ = 1.0F;
    noteRoot_ = nullptr;
    laneRoot_ = nullptr;
    canvas_ = nullptr;
    session_ = nullptr;
    texts_ = nullptr;
    visuals_ = nullptr;
}

void GameplayNoteView::ApplyFeedback(const GameplayFeedback &feedback)
{
    if (feedback.reset)
        completionEffects_.clear();
    // A completed focus note keeps its burst/processing effect visible for
    // FocusSuccessDuration even after the note itself leaves the lane.
    for (const NoteEvent &event : feedback.result.events)
    {
        if (event.type != NoteEventType::Completed)
            continue;
        if (!noteVisuals_.contains(event.noteId))
            if (const auto *model = visibility_.Find(event.noteId))
                CreateNoteVisual(*model);
        const auto visual = noteVisuals_.find(event.noteId);
        if (visual != noteVisuals_.end() && visual->second.focusType != FocusNoteType::None)
            completionEffects_[event.noteId] = event.eventTime;
    }
}

float GameplayNoteView::TravelPixelsAt(RhythmTime target, RhythmTime current, float multiplier) const
{
    return static_cast<float>(session_->ScrollDistance(target, current) *
        session_->Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote_ * multiplier);
}

Point GameplayNoteView::LaneObjectPosition(const RhythmTime target, const RhythmTime current,
                                           const float multiplier) const
{
    // Notes and measure lines are centred lane objects on the same time axis.
    // Whole integrates the shared distance; Separate scales that distance per object.
    return {laneWidth_ * 0.5F, judgementLocalY_ + TravelPixelsAt(target, current, multiplier)};
}

void GameplayNoteView::UpdateTravelLayout()
{
    if (laneRoot_ == nullptr || session_ == nullptr)
        return;

    float largestHeadRadius = largestHeadRadius_;
    for (const auto &[id, layers] : noteVisuals_)
    {
        static_cast<void>(id);
        largestHeadRadius = std::max(largestHeadRadius, layers.diameter * 0.5F);
    }
    // The local +Y lane end meets the screen edge after the mode's lane
    // rotation. Keep the entire largest head beyond that edge at spawn time.
    noteTravelDistance_ = std::max(laneRoot_->NodeSize().height - judgementLocalY_ + largestHeadRadius + 1.0F, 1.0F);
    visibility_.Rebuild(*session_, pixelsPerWholeNote_, noteTravelDistance_);

    // Ticks retain their chart spacing when the window width changes.
    for (const auto &lane : session_->Gear().Lanes())
    {
        for (const auto &note : lane->Notes())
        {
            const auto found = noteVisuals_.find(note->Id());
            if (found == noteVisuals_.end())
                continue;
            const NoteVisualLayers &layers = found->second;
            const auto *presentation = session_->FindNotePresentation(note->Id());
            const float speed = presentation == nullptr ? 1.0F : static_cast<float>(presentation->scrollMultiplier);
            for (const TimedVisual &tick : layers.ticks)
            {
                const auto bounds = tick.node->Bounds();
                const float offset = TravelPixelsAt(tick.timing, layers.timing, speed);
                tick.node->SetBounds({(layers.diameter - bounds.width) * 0.5F,
                                      layers.diameter * 0.5F + offset - bounds.height * 0.5F, bounds.width,
                                      bounds.height});
            }
        }
    }
}

void GameplayNoteView::UpdateMeasureLines(RhythmTime time, bool enabled)
{
    for (auto &visual : measureLineVisuals_)
        visual.node->SetVisible(false);
    if (!enabled)
        return;
    const auto &lines = session_->MeasureLines();
    std::size_t used = 0;
    const auto distance = session_->ScrollDistance(time, {});
    visibility_.VisitMeasureLines(distance,
                                  [&](std::size_t index)
                                  {
                                      const auto it = lines.begin() + static_cast<std::ptrdiff_t>(index);
                                      const auto position =
                                          LaneObjectPosition(it->timing, time, static_cast<float>(it->scrollMultiplier));
                                      if (position.y > judgementLocalY_ + noteTravelDistance_ ||
                                          position.y < judgementLocalY_ - noteTravelDistance_)
                                          return;
                                      if (used == measureLineVisuals_.size())
                                      {
                                          auto &line = CreatePanel(*laneRoot_, {}, "Lane.MeasureLine");
                                          line.SetPivot({0.5F, 0.5F});
                                          SetColor(line, {.78F, .85F, .90F, .72F});
                                          line.SetZIndex(1);
                                          measureLineVisuals_.push_back({{}, &line});
                                      }
                                      auto &visual = measureLineVisuals_[used++];
                                      visual.timing = it->timing;
                                      visual.node->SetVisible(true);
                                      visual.node->SetSize({laneWidth_, 4});
                                      visual.node->SetPosition(position);
                                  });
}

void GameplayNoteView::UpdateTickVisuals(NoteVisualLayers &layers, const NotePresentationInfo &presentation,
                                         RhythmTime time, RhythmTime anchor, std::size_t processedTicks)
{
    std::size_t used = 0;
    const auto &ticks = presentation.tickTimes;
    const float speed = static_cast<float>(presentation.scrollMultiplier);
    const auto first = ticks.begin() + static_cast<std::ptrdiff_t>(std::min(processedTicks, ticks.size()));
    const long double distance = session_->ScrollDistance(time, {});
    const long double scale = session_->Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote_ * speed;
    const auto cumulative = [this](auto tick) { return session_->ScrollDistance(tick, {}); };
    const auto last = std::ranges::upper_bound(
        first, ticks.end(),
        distance + (laneRoot_->NodeSize().height + layers.tickSize.height - judgementLocalY_) / scale, {}, cumulative);
    for (auto it = std::ranges::lower_bound(first, last, distance - (judgementLocalY_ + layers.tickSize.height) / scale,
                                            {}, cumulative);
         it != last; ++it)
    {
        if (*it <= layers.timing)
            continue;
        const float y = LaneObjectPosition(*it, time, speed).y;
        if (y < 0 || y > laneRoot_->NodeSize().height + layers.tickSize.height)
            continue;
        if (used == layers.ticks.size())
        {
            auto &node = CreateSprite(*layers.root, {}, layers.tickImage, "LongNote.Tick");
            node.SetZIndex(4);
            layers.ticks.push_back({{}, &node});
        }
        auto &visual = layers.ticks[used++];
        visual.timing = *it;
        visual.node->SetVisible(true);
        visual.node->SetBounds({(layers.diameter - layers.tickSize.width) * .5F,
            layers.diameter * .5F - layers.tickSize.height * .5F + TravelPixelsAt(*it, anchor, speed),
            layers.tickSize.width, layers.tickSize.height});
    }
    for (; used < layers.ticks.size(); ++used)
        layers.ticks[used].node->SetVisible(false);
}

ImageHandle GameplayNoteView::NoteImage(std::wstring_view file)
{
    const auto found = noteImages_.find(file);
    if (found != noteImages_.end())
        return found->second;
    const auto image = visuals_->RegisterImage(InGameSkinAssetPath(file));
    noteImages_.emplace(file, image);
    return image;
}

void GameplayNoteView::CreateNoteVisual(const INote &source)
{
    const auto *note = &source;
    auto &visuals = *visuals_;
    const auto noteImage = NoteImage(L"note");
    const auto noteOverlay = NoteImage(L"noteoverlay");
    const auto bigNoteImage = NoteImage(L"bignote");
    const auto bigOverlay = NoteImage(L"bigcircleoverlay");
    const auto bodyImage = NoteImage(L"LNBody");
    const auto bodyOverlayImage = NoteImage(L"LNBodyOverlay");
    const auto tailImage = NoteImage(L"LNTail");
    const auto tailOverlayImage = NoteImage(L"LNTailOverlay");
    const auto bigBodyImage = NoteImage(L"BigLNBody");
    const auto bigBodyOverlayImage = NoteImage(L"BigLNBodyOverlay");
    const auto bigTailImage = NoteImage(L"BigLNTail");
    const auto bigTailOverlayImage = NoteImage(L"BigLNTailOverlay");
    const auto tickImage = NoteImage(L"TickMarker");
    const auto buzzTickImage = NoteImage(L"BuzzTickDiamond");
    const auto balloonImage = NoteImage(L"Balloon");
    const auto dengDengImage = NoteImage(L"DengDeng");
    const auto balloonProcessingImage = NoteImage(L"BalloonProcessing");
    const auto balloonBurstImage = NoteImage(L"BalloonBurst");
    const auto dengDengProcessingImage = NoteImage(L"DengDengProcessing");
    const auto counterImage = NoteImage(L"HitCounterCloud");

    const auto noteSize = ScaledImageSize(visuals, noteImage);
    pixelsPerWholeNote_ = noteSize.width * 16.0F * SixteenthSpacingInHeadDiameters;
    const auto bigNoteSize = ScaledImageSize(visuals, bigNoteImage);
    const auto bodySize = ScaledImageSize(visuals, bodyImage);
    const auto tailSize = ScaledImageSize(visuals, tailImage);
    const auto bigBodySize = ScaledImageSize(visuals, bigBodyImage);
    const auto bigTailSize = ScaledImageSize(visuals, bigTailImage);
    const auto tickSize = ScaledImageSize(visuals, tickImage);
    const auto buzzTickSize = ScaledImageSize(visuals, buzzTickImage);
    const auto balloonSize = ScaledImageSize(visuals, balloonImage);
    const auto dengDengSize = ScaledImageSize(visuals, dengDengImage);
    const auto balloonProcessingSize = ScaledImageSize(visuals, balloonProcessingImage);
    const auto balloonBurstSize = ScaledImageSize(visuals, balloonBurstImage);
    const auto dengDengProcessingSize = ScaledImageSize(visuals, dengDengProcessingImage);
    const auto counterSize = ScaledImageSize(visuals, counterImage);

    const NotePresentationInfo *presentation = session_->FindNotePresentation(note->Id());
    const NoteVisualKind visualKind = presentation == nullptr ? NoteVisualKind::Don : presentation->visualKind;
    const bool big = IsBigVisual(visualKind);
    const bool balloon = visualKind == NoteVisualKind::Balloon;
    const bool dengDeng = visualKind == NoteVisualKind::DengDeng;
    const bool customHead = balloon || dengDeng;
    const bool roll = (visualKind == NoteVisualKind::Roll || visualKind == NoteVisualKind::BigRoll) &&
                      note->Progress().has_value();
    const bool longNote = presentation != nullptr && presentation->hasEndTime && IsLongVisual(visualKind) && !customHead;
    const bool bigLongParts = visualKind == NoteVisualKind::BigRoll;
    const bool buzz = visualKind == NoteVisualKind::DonBuzz || visualKind == NoteVisualKind::KatBuzz;
    const auto headImage = balloon ? balloonImage : (dengDeng ? dengDengImage : (big ? bigNoteImage : noteImage));
    const auto headSize = balloon ? balloonSize : (dengDeng ? dengDengSize : (big ? bigNoteSize : noteSize));
    const float diameter = headSize.width;
    const Color ambientColor = AmbientColor(visualKind);

    NoteVisualLayers layers;
    layers.focusType = balloon ? FocusNoteType::Balloon : (dengDeng ? FocusNoteType::DengDeng : FocusNoteType::None);
    layers.root = &noteRoot_->CreateChild(std::format("Lane.Note.{}", note->Id()));
    layers.root->SetPivot({0.5F, 0.5F});
    layers.root->SetSize({diameter, diameter});
    layers.root->SetPosition({laneWidth_ * 0.5F, judgementLocalY_});
    layers.root->SetVisible(false);
    layers.diameter = diameter;
    layers.timing = note->Timing();
    if (presentation != nullptr && presentation->hasEndTime)
        layers.endTime = presentation->endTime;

    // Body and tail are Ambient-colored and sit behind the circular
    // head. The white head overlay is a separate untinted draw packet.
    if (longNote)
    {
        const auto selectedBodyImage = bigLongParts ? bigBodyImage : bodyImage;
        const auto selectedBodyOverlayImage = bigLongParts ? bigBodyOverlayImage : bodyOverlayImage;
        const auto selectedTailImage = bigLongParts ? bigTailImage : tailImage;
        const auto selectedTailOverlayImage = bigLongParts ? bigTailOverlayImage : tailOverlayImage;
        const auto selectedBodySize = bigLongParts ? bigBodySize : bodySize;
        const auto selectedTailSize = bigLongParts ? bigTailSize : tailSize;
        layers.bodyWidth = selectedBodySize.width;
        layers.tailWidth = selectedTailSize.width;
        layers.tailHeight = selectedTailSize.height;
        const float bodyX = (diameter - layers.bodyWidth) * 0.5F;
        const float tailX = (diameter - layers.tailWidth) * 0.5F;
        layers.body = &CreateSprite(*layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                                    selectedBodyImage, "AmbientBody");
        RequireComponent<SpriteVisualComponent>(*layers.body).SetTint(ambientColor);
        layers.body->SetZIndex(0);
        layers.bodyOverlay = &CreateSprite(*layers.root, {bodyX, diameter * 0.5F, layers.bodyWidth, 1.0F},
                                           selectedBodyOverlayImage, "BodyOverlay");
        layers.bodyOverlay->SetZIndex(1);
        layers.tail = &CreateSprite(*layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                                    selectedTailImage, "AmbientTail");
        RequireComponent<SpriteVisualComponent>(*layers.tail).SetTint(ambientColor);
        layers.tail->SetZIndex(2);
        layers.tail->SetPivot({0.5F, 0.5F});
        layers.tail->SetPosition({tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});
        layers.tailOverlay = &CreateSprite(*layers.root, {tailX, 0.0F, layers.tailWidth, layers.tailHeight},
                                           selectedTailOverlayImage, "TailOverlay");
        layers.tailOverlay->SetZIndex(3);
        layers.tailOverlay->SetPivot({0.5F, 0.5F});
        layers.tailOverlay->SetPosition({tailX + layers.tailWidth * 0.5F, layers.tailHeight * 0.5F});

        layers.tickImage = buzz ? buzzTickImage : tickImage;
        layers.tickSize = buzz ? buzzTickSize : tickSize;
    }

    layers.ambient = &CreateSprite(*layers.root, {0.0F, 0.0F, headSize.width, headSize.height}, headImage,
                                   "AmbientHead");
    RequireComponent<SpriteVisualComponent>(*layers.ambient)
        .SetTint(customHead ? Color{1.0F, 1.0F, 1.0F, 1.0F} : ambientColor);
    layers.ambient->SetZIndex(3);
    if (!customHead)
    {
        layers.overlay = &CreateSprite(*layers.root, {0.0F, 0.0F, headSize.width, headSize.height},
                                       big ? bigOverlay : noteOverlay, "UntintedOverlay");
        RequireComponent<SpriteVisualComponent>(*layers.overlay).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
        layers.overlay->SetZIndex(4);
    }
    if (customHead)
    {
        const auto processingImage = balloon ? balloonProcessingImage : dengDengProcessingImage;
        layers.processingSize = balloon ? balloonProcessingSize : dengDengProcessingSize;
        layers.processing = &CreateSprite(canvas_->Root(),
                                          {0.0F, 0.0F, layers.processingSize.width, layers.processingSize.height},
                                          processingImage, std::format("Hud.NoteProcessing.{}", note->Id()));
        layers.processing->SetPivot({0.5F, 0.5F});
        layers.processing->SetZIndex(7);
        layers.processing->SetVisible(false);
        if (balloon)
        {
            layers.successSize = balloonBurstSize;
            layers.success = &CreateSprite(canvas_->Root(),
                                           {0.0F, 0.0F, balloonBurstSize.width, balloonBurstSize.height},
                                           balloonBurstImage, std::format("Hud.NoteSuccess.{}", note->Id()));
            layers.success->SetPivot({0.5F, 0.5F});
            layers.success->SetZIndex(9);
            layers.success->SetVisible(false);
        }
    }
    if (customHead || roll)
    {
        layers.counter = &CreateSprite(canvas_->Root(), {0.0F, 0.0F, counterSize.width, counterSize.height},
                                       counterImage, std::format("Hud.NoteCounter.{}", note->Id()));
        layers.counter->SetZIndex(8);
        layers.counter->SetVisible(false);
        layers.counterText = &CreateLabel(*layers.counter,
                                          {0.0F, counterSize.height * 0.22F, counterSize.width,
                                           counterSize.height * 0.58F},
                                          L"", "RemainingHits");
        auto &text = RequireComponent<TextVisualComponent>(*layers.counterText);
        texts_->ApplyFont(text);
        text.SetFontSize(22.0F);
        text.SetTextColor({1.0F, 1.0F, 1.0F, 1.0F});
        text.SetHorizontalAlignment(TextAlignment::Center);
    }
    noteVisuals_.emplace(note->Id(), std::move(layers));
}

void GameplayNoteView::HideTransientNoteVisuals()
{
    for (const NoteId id : presentedNoteIds_)
    {
        const auto visual = noteVisuals_.find(id);
        if (visual == noteVisuals_.end())
            continue;
        NoteVisualLayers &layers = visual->second;
        layers.root->SetVisible(false);
        for (auto *node : {layers.processing, layers.success, layers.counter})
            if (node != nullptr)
                node->SetVisible(false);
    }
    presentedNoteIds_.clear();
}

void GameplayNoteView::Update(const RhythmTime time)
{
    if (!visibility_.Matches(session_->PresentationRevision()))
        visibility_.Rebuild(*session_, pixelsPerWholeNote_, noteTravelDistance_);
    const auto distance = session_->ScrollDistance(time, {});
    const auto visibleNotes = visibility_.VisibleNotes(distance, time, MissedTravelDuration);
    if (visibleNotes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Too many visible notes to order.");
    int noteZIndex = static_cast<int>(visibleNotes.size());
    ++presentationGeneration_;
    HideTransientNoteVisuals();
    bool showMeasureLines = true;
    for (const auto &value : session_->EvaluateAutomation(time))
        if (value.type == EffectCommandType::MeasureLineVisible)
            showMeasureLines = value.value >= 0.5;
    UpdateMeasureLines(time, showMeasureLines);
    bool focusClaimed{};
    for (const auto *model : visibleNotes)
        UpdateNote(*model, time, --noteZIndex, focusClaimed);
    PresentCompletionEffect(time);
    RemoveUnusedNoteVisuals();
}

void GameplayNoteView::UpdateNote(const INote &model, const RhythmTime time, const int zIndex, bool &focusClaimed)
{
    const ScrollNoteSnapshot note{.noteId = model.Id(),
                                  .timing = model.Timing(),
                                  .expireTime = model.ExpireTime(),
                                  .state = model.State(),
                                  .progress = model.Progress(),
                                  .processedTicks = model.ProcessedTickCount()};
    auto found = noteVisuals_.find(note.noteId);
    if (found == noteVisuals_.end())
    {
        CreateNoteVisual(model);
        found = noteVisuals_.find(note.noteId);
    }
    NoteVisualLayers &layers = found->second;
    // Reassign after visibility changes or backward seeks: creation order is
    // not chronological when Separate scroll speeds differ.
    layers.root->SetZIndex(zIndex);
    presentedNoteIds_.push_back(note.noteId);
    layers.presentedGeneration = presentationGeneration_;
    const NotePresentationInfo *presentation = session_->FindNotePresentation(note.noteId);
    const bool focusNote = layers.focusType != FocusNoteType::None;
    auto targetTime = layers.timing;
    const bool missed = note.state == NoteState::Missed;
    const bool completed = note.state == NoteState::Completed;
    const bool countingHits = layers.counter != nullptr && !missed && !completed && time >= note.timing &&
                              note.progress.has_value() && note.progress->accepted > 0;
    const bool processing = focusNote && countingHits;
    // A focus note stays on the judgement point while it is being hit, and a
    // missed one leaves from its expiry time.
    if (focusNote && time >= note.timing && !missed)
        targetTime = time;
    else if (focusNote && missed)
        targetTime = note.expireTime;
    const float speed = presentation != nullptr ? static_cast<float>(presentation->scrollMultiplier) : 1.0F;
    const auto position = LaneObjectPosition(targetTime, time, speed);
    layers.root->SetPosition(position);
    const bool showLaneHead = focusNote ? !completed && (!processing || missed) : !completed && !missed;
    layers.root->SetVisible(showLaneHead);

    // Only the earliest note being hit shows its counter or processing effect.
    if (countingHits && !focusClaimed)
    {
        if (focusNote)
            PresentFocusNoteProcessing(layers, *note.progress, time);
        else
            PresentHitCounter(layers, *note.progress, layers.diameter);
        focusClaimed = true;
    }

    if (presentation != nullptr && presentation->hasEndTime && layers.body != nullptr && layers.tail != nullptr)
        UpdateLongNoteParts(layers, *presentation, time, targetTime, position.y, speed, note.processedTicks);
}

void GameplayNoteView::UpdateLongNoteParts(NoteVisualLayers &layers, const NotePresentationInfo &presentation,
                                           const RhythmTime time, const RhythmTime anchor, const float localY,
                                           const float speed, const std::size_t processedTicks)
{
    UpdateTickVisuals(layers, presentation, time, anchor, processedTicks);
    const float tailTravel = std::clamp(TravelPixelsAt(layers.endTime, time, speed), -0.05F * noteTravelDistance_,
                                        1.65F * noteTravelDistance_);
    const float tailLocalY = judgementLocalY_ + tailTravel;
    const float length = std::max(tailLocalY - localY, 1.0F);
    const float bodyX = (layers.diameter - layers.bodyWidth) * 0.5F;
    layers.body->SetBounds({bodyX, layers.diameter * 0.5F, layers.bodyWidth, length});
    if (layers.bodyOverlay != nullptr)
        layers.bodyOverlay->SetBounds({bodyX, layers.diameter * 0.5F, layers.bodyWidth, length});
    const float tailX = (layers.diameter - layers.tailWidth) * 0.5F;
    layers.tail->SetSize({layers.tailWidth, layers.tailHeight});
    layers.tail->SetPosition({tailX + layers.tailWidth * 0.5F, length + layers.tailHeight * 0.5F});
    if (layers.tailOverlay != nullptr)
    {
        layers.tailOverlay->SetSize({layers.tailWidth, layers.tailHeight});
        layers.tailOverlay->SetPosition({tailX + layers.tailWidth * 0.5F, length + layers.tailHeight * 0.5F});
    }
}

void GameplayNoteView::PresentHitCounter(NoteVisualLayers &layers, const NoteProgress &progress,
                                         const float visualHeight)
{
    if (canvas_ == nullptr || layers.counter == nullptr || layers.counterText == nullptr)
        return;

    const auto bounds = layers.counter->Bounds();
    layers.counter->SetBounds({-bounds.width * 0.5F, visualHeight * 0.5F - bounds.height * 0.28F, bounds.width,
                               bounds.height});
    layers.counter->SetVisible(true);
    const std::size_t remaining = progress.required > progress.accepted ? progress.required - progress.accepted : 0;
    RequireComponent<TextVisualComponent>(*layers.counterText).SetText(std::to_wstring(remaining));
}

void GameplayNoteView::PresentFocusNoteProcessing(NoteVisualLayers &layers, const NoteProgress &progress,
                                                  const RhythmTime time)
{
    if (canvas_ == nullptr || layers.processing == nullptr || progress.required == 0)
        return;

    const float completion =
        std::clamp(static_cast<float>(progress.accepted) / static_cast<float>(progress.required), 0.0F, 1.0F);
    const float scale = layers.focusType == FocusNoteType::Balloon ? 0.72F + completion * 0.38F : 1.0F;
    const Size size{layers.processingSize.width * scale, layers.processingSize.height * scale};
    layers.processing->SetSize(size);
    layers.processing->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        layers.processing->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<SpriteVisualComponent>(*layers.processing).SetTint({1.0F, 1.0F, 1.0F, 1.0F});
    layers.processing->SetVisible(true);
    PresentHitCounter(layers, progress, size.height);
}

void GameplayNoteView::PresentCompletionEffect(const RhythmTime time)
{
    auto selected = completionEffects_.end();
    for (auto effect = completionEffects_.begin(); effect != completionEffects_.end();)
    {
        const auto elapsed = time - effect->second;
        if (elapsed >= FocusSuccessDuration)
        {
            effect = completionEffects_.erase(effect);
            continue;
        }
        if (elapsed >= RhythmDuration::zero() &&
            (selected == completionEffects_.end() || effect->second > selected->second))
            selected = effect;
        ++effect;
    }
    if (selected == completionEffects_.end() || canvas_ == nullptr)
        return;

    const auto visual = noteVisuals_.find(selected->first);
    if (visual == noteVisuals_.end())
        return;
    NoteVisualLayers &layers = visual->second;
    presentedNoteIds_.push_back(selected->first);
    const float progress = std::clamp(static_cast<float>((time - selected->second).count()) /
                                          static_cast<float>(FocusSuccessDuration.count()),
                                      0.0F, 1.0F);
    Visual2DNode *node = layers.processing;
    Size size = layers.processingSize;
    if (layers.focusType == FocusNoteType::Balloon && layers.success != nullptr)
    {
        node = layers.success;
        const float burstScale = 0.82F + progress * 0.28F;
        size = {layers.successSize.width * burstScale, layers.successSize.height * burstScale};
    }
    if (node == nullptr)
        return;

    node->SetSize(size);
    node->SetPosition({0.0F, 0.0F});
    if (layers.focusType == FocusNoteType::DengDeng)
    {
        const float turns = static_cast<float>(time.count()) / 600'000.0F;
        node->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, std::fmod(turns * TwoPi, TwoPi));
    }
    RequireComponent<SpriteVisualComponent>(*node).SetTint({1.0F, 1.0F, 1.0F, 1.0F - progress});
    node->SetVisible(true);
}

void GameplayNoteView::RemoveUnusedNoteVisuals()
{
    for (auto it = noteVisuals_.begin(); it != noteVisuals_.end();)
    {
        if (it->second.presentedGeneration == presentationGeneration_ || completionEffects_.contains(it->first))
        {
            ++it;
            continue;
        }
        // Model IDs/images stay cached, so seeking backward can recreate visuals.
        // Remove independent HUD nodes before the Lane subtree owning long parts.
        const auto &layers = it->second;
        for (auto *node : {layers.processing, layers.success, layers.counter, layers.root})
            if (node)
                static_cast<void>(canvas_->RemoveNode(node->Id()));
        it = noteVisuals_.erase(it);
    }
}
