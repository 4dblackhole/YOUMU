#include "SongRecordView.h"
#include "Catalog/SongCatalog.h"
#include "Presentation/AccuracyText.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include <chrono>
#include <format>

using namespace PhantomCore;
using namespace song_select;

void SongRecordView::InitializeContent(Visual2DNode& panel)
{
    const auto &text = MusicSelect(texts_);
    panel_ = &panel;
    auto &selector = CreateComboBox(
        panel, ScaleTopLeftBounds(layout::RecordSelector, panel.NodeSize().height),
        {std::wstring(text.personalRecord)}, "RecordSelector");
    selector_ = &selector;
    ApplySpriteStyle(selector, PaleBlue, PaleBlue);
    SetCornerRadius(selector, 10.0F);
    BindSkinImage(selector, visuals_, "MusicSelect", "RecordSelector");
    auto &recordBehavior = RequireComponent<ComboBoxBehaviorComponent>(selector);
    recordBehavior.SetFontSize(CanvasFontSize(10.0F));
    texts_.ApplyFont(recordBehavior);
    recordBehavior.SetTextColor(DeepBlue);
    recordBehavior.SetSelectedTextColor(PureWhite);
    recordBehavior.SetPopupBackgroundColor(PureWhite);
    const auto button = [this, &panel](const char *key, const char *name) -> Visual2DNode * {
        auto &node = CreateButton(panel, {}, std::wstring(texts_.Get(key)), name);
        ApplyButtonStyle(node, PaleBlue, PureWhite, AccentBlue);
        auto &caption = RequireComponent<TextVisualComponent>(node);
        texts_.ApplyFont(caption); caption.SetFontSize(14); caption.SetTextColor(DeepBlue);
        return &node;
    };
    queryButton_ = button("Records.query", "RecordQuery");
    previousButton_ = button("Records.previous", "RecordPrevious");
    nextButton_ = button("Records.next", "RecordNext");
    rows_ = &panel.CreateChild("RecordRows");
    emptyMessage_ = &CreateSongLabel(
        panel, layout::EmptyRecordMessage, std::wstring(text.noRecord), 28.0F, DeepBlue,
        "NoRecord", texts_, TextAlignment::Center);
}

void SongRecordView::InitializeFilter(Visual2DCanvas& canvas, IWindowControl* window)
{
    filterPanel_.Initialize(canvas, texts_, window);
}

void SongRecordView::Layout()
{
    if (!rows_)
        return;
    const float width = panel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(width - 44.0F, 1.0F);
    selector_->SetBounds(ScaleTopLeftBounds({22, 22, innerWidth, layout::RecordSelector.height},
                                                panel_->NodeSize().height));
    emptyMessage_->SetBounds(ScaleTopLeftBounds({22, 400, innerWidth, layout::EmptyRecordMessage.height},
                                                    panel_->NodeSize().height));
    const float height = panel_->NodeSize().height / DesignToCanvasScale;
    queryButton_->SetBounds(ScaleTopLeftBounds({22, 78, width - 44, 40}, panel_->NodeSize().height));
    rows_->SetBounds(ScaleTopLeftBounds({22, 130, width - 44, height - 220}, panel_->NodeSize().height));
    rows_->SetClipRect({0, 0, rows_->NodeSize().width, rows_->NodeSize().height});
    previousButton_->SetBounds(
        ScaleTopLeftBounds({22, height - 65, (width - 56) / 2, 40}, panel_->NodeSize().height));
    nextButton_->SetBounds(ScaleTopLeftBounds({34 + (width - 56) / 2, height - 65, (width - 56) / 2, 40},
                                                    panel_->NodeSize().height));
}

void SongRecordView::SetViewport(std::uint32_t height) { filterPanel_.SetViewport(height); }
void SongRecordView::OpenFilter() { filterPanel_.Open(filter_); }
void SongRecordView::CloseFilter() { filterPanel_.Close(); }

void SongRecordView::UpdateFilter(std::span<const Action> actions, const InputState& input)
{
    for (const auto& action : actions) filterPanel_.ProcessAction(action);
    filterPanel_.ProcessInput(input);
    if (auto applied = filterPanel_.TakeApplied())
    {
        auto query = applied->Query();
        filter_ = std::move(*applied);
        query_ = std::move(query);
        offset_ = 0;
        ++revision_;
    }
}

SongRecordAction SongRecordView::ProcessAction(const Action& action)
{
    if (action.type != ActionType::Clicked) return SongRecordAction::None;
    if (queryButton_ && action.source == queryButton_->Id()) return SongRecordAction::OpenFilter;
    if (previousButton_ && action.source == previousButton_->Id())
    {
        offset_ = offset_ >= 6 ? offset_ - 6 : 0;
        ++revision_;
        return SongRecordAction::PageChanged;
    }
    if (nextButton_ && action.source == nextButton_->Id() && next_)
    {
        offset_ += 6;
        ++revision_;
        return SongRecordAction::PageChanged;
    }
    return SongRecordAction::None;
}

void SongRecordView::Update(const SongCatalogPattern* selected)
{
    if (!rows_)
        return;
    const auto pattern = selected ? selected->patternPath : std::filesystem::path{};
    const auto mode = selected ? selected->pattern.mode : std::string{};
    if (pattern != pattern_ || mode != mode_)
    {
        pattern_ = pattern;
        mode_ = mode;
        offset_ = 0;
        ++revision_;
    }
    if (displayRevision_ != revision_)
    {
        displayRevision_ = revision_;
        next_ = false;
        previousButton_->SetEnabled(offset_ > 0);
        nextButton_->SetEnabled(false);
        std::vector<NodeId> ids;
        for (const auto &child : rows_->Children())
            ids.push_back(child->Id());
        for (auto id : ids)
            static_cast<void>(rows_->RemoveChild(id));
        emptyMessage_->SetVisible(true);
        RequireComponent<TextVisualComponent>(*emptyMessage_)
            .SetText(std::wstring(texts_.Get(pattern.empty() ? "MusicSelect.noRecord" : "Records.loading")));
    }
    if (job_.valid() && job_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        try
        {
            auto rows = job_.get();
            if (jobRevision_ == revision_)
                Show(rows);
        }
        catch (const std::exception &error)
        {
            if (jobRevision_ == revision_)
            {
                emptyMessage_->SetVisible(true);
                RequireComponent<TextVisualComponent>(*emptyMessage_)
                    .SetText(std::wstring(texts_.Get("Records.loadError")) + DecodeDisplayText(error.what()));
            }
        }
    }
    if (!job_.valid() && jobRevision_ != revision_)
    {
        jobRevision_ = revision_;
        if (pattern.empty())
            return;
        const auto database = database_;
        job_ = std::async(std::launch::async,
                                [database, pattern, mode, condition = query_, offset = offset_]
                                {
                                    return PlayRecordStore(database.empty() ? PlayRecordStore::DefaultPath() : database)
                                        .Query(pattern, mode, condition, offset, 7);
                                });
    }
}

void SongRecordView::Show(const std::vector<PlayRecord> &records)
{
    emptyMessage_->SetVisible(records.empty());
    RequireComponent<TextVisualComponent>(*emptyMessage_)
        .SetText(std::wstring(texts_.Get("MusicSelect.noRecord")));
    next_ = records.size() > 6;
    nextButton_->SetEnabled(next_);
    const float width = rows_->NodeSize().width / DesignToCanvasScale;
    const float step = (rows_->NodeSize().height / DesignToCanvasScale) / 6;
    for (std::size_t i = 0; i < std::min(records.size(), std::size_t{6}); ++i)
    {
        const auto &r = records[i];
        const auto &o = r.options;
        const auto timestamp =
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{r.playedAtMilliseconds}};
        std::wstring description = std::format(L"{:%Y-%m-%d %H:%M} UTC  {}", timestamp, AccuracyText(r.accuracy));
        description +=
            L"\n" + std::wstring(texts_.Get(r.cleared ? "Records.cleared"
                                                      : (r.completed ? "Records.practice" : "Records.interrupted")));
        description += std::format(L"  JL {:.2f}  {:.2f}x", r.judgeLevel.Value(), o.accelerate);
        description += std::format(L"\nScroll {:g}{}{}{}{}{}", o.scrollSpeed,
                                   o.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L" BPM" : L"x",
                                   o.ignoreScrollEffects ? L" FIX" : L"", o.metronome ? L" MET" : L"",
                                   o.mirror ? L" MIR" : L"", o.random ? L" RND" : L"");
        if (o.random)
            description += std::format(L"\n{:g}%  #{}", o.randomAttemptProbability * 100, o.randomSeed);
        auto &row = CreateSongLabel(*rows_, {0, static_cast<float>(i) * step, width, step - 3}, std::move(description),
                             13, DeepBlue, "PlayRecord", texts_);
        row.SetClipRect({0, 0, row.NodeSize().width, row.NodeSize().height});
    }
}

void SongRecordView::RefreshTexts()
{
    const auto& text = MusicSelect(texts_);
    const auto setLabel = [this](Visual2DNode* node, std::wstring_view value)
    {
        if (!node) return;
        auto& content = RequireComponent<TextVisualComponent>(*node);
        content.SetText(std::wstring(value));
        texts_.ApplyFont(content);
    };
    setLabel(emptyMessage_, text.noRecord);
    if (selector_)
    {
        auto& combo = RequireComponent<ComboBoxBehaviorComponent>(*selector_);
        combo.SetItems({std::wstring(text.personalRecord)});
        texts_.ApplyFont(combo);
    }
    if (queryButton_)
    {
        setLabel(queryButton_, texts_.Get("Records.query"));
        setLabel(previousButton_, texts_.Get("Records.previous"));
        setLabel(nextButton_, texts_.Get("Records.next"));
        ++revision_;
    }
    filterPanel_.RefreshTexts();
}

void SongRecordView::Shutdown() noexcept
{
    filterPanel_.Shutdown();
    panel_ = selector_ = emptyMessage_ = nullptr;
    queryButton_ = previousButton_ = nextButton_ = rows_ = nullptr;
    ++revision_;
}
