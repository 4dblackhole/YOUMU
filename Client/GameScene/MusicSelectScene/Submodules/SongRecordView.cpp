#include "SongRecordView.h"
#include "App/AssetPaths.h"
#include "App/SkinSetSelection.h"
#include "App/GameDataPaths.h"
#include "Catalog/SongCatalog.h"
#include "Presentation/AccuracyText.h"
#include "Presentation/RankName.h"
#include "Presentation/ScoreText.h"
#include "Presentation/UiDropdown.h"
#include "Records/RecordJudgements.h"
#include "Records/RecordOptionText.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include <algorithm>
#include <chrono>
#include <format>

using namespace PhantomCore;
using namespace song_select;

namespace
{
    namespace theme = ui_colors::theme;

    constexpr std::size_t PageSize = 6;
    // The column has no sheet behind it, so every row, notice and control sits on its own
    // plate over the chart BG. Rows run from the screen's left edge at one width; their
    // content starts at ContentLeft and ends at ContentRight, before the slanted end.
    constexpr float RowWidth = 336.0F, RowHeight = 52.0F, RowPitch = 58.0F, RowsTop = 56.0F;
    constexpr float ContentLeft = 48.0F, ContentRight = 308.0F;
    constexpr float ControlSize = 32.0F, PagerTop = 560.0F, PagerCenter = 192.0F, PagerWidth = 124.0F;
    // Max strokes on a row: the result tile's afterimage icon and the count, left of the date. Every row
    // shows it, the default 2 included (user decision), so rankings with different counts read apart.
    constexpr float StrokeIconSize = 14.0F, StrokeGap = 2.0F, StrokeMargin = 8.0F;
    // The options sit at the top right, after the score (osu!'s place for mods).
    constexpr float ScoreSize = 20.0F, OptionSize = 13.0F, OptionTop = 8.0F, OptionHeight = 18.0F, OptionGap = 8.0F;

    // Clear, a finished play below the clear line, or a play that was left early (shown as Failed).
    [[nodiscard]] const char *StatusKey(const PlayRecord &record) noexcept
    {
        if (record.cleared)
            return "Records.cleared";
        return record.completed ? "Records.practice" : "Records.failed";
    }
    // The status colours the chips used: green, navy, red.
    [[nodiscard]] Color StatusColor(const PlayRecord &record) noexcept
    {
        if (record.cleared)
            return theme::Ok;
        return record.completed ? theme::InkSoft : theme::Danger;
    }

    // Only what differs from a plain play, each part after " · ".
    [[nodiscard]] std::wstring OptionTokens(const PlayRecord &record)
    {
        const auto &o = record.options;
        std::wstring text;
        const auto add = [&text](const std::wstring &token) { text += L" · " + token; };
        if (o.accelerate != 1.0)
            add(std::format(L"{:.2f}x", o.accelerate));
        if (o.scrollUnit == PlayOptions::ScrollUnit::Bpm || o.scrollSpeed != 1.0)
            add(std::format(L"{:g}{}", o.scrollSpeed, o.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L" BPM" : L"x"));
        if (o.ignoreScrollEffects)
            add(L"FIX");
        if (o.metronome)
            add(L"MET");
        if (o.mirror)
            add(L"MIR");
        if (o.random)
            add(L"RND");
        return text;
    }

    // What a row does not show (the mode is the selector's, so it is left out): the status the band's colour
    // stands for, the judge level the play used and its combo, the judgement counts two to a line, every
    // option, the stroke count and the time it was played (UTC, like the row's date). Shown large to the
    // right of the row (user decision).
    [[nodiscard]] TooltipText RecordTooltip(const PlayRecord &record,
                                            const std::vector<std::pair<std::string_view, std::size_t>> &judgements,
                                            const TextCatalog &texts)
    {
        std::wstring text = std::wstring(texts.Get(StatusKey(record))) + L"\n" +
                            std::format(L"{} {:.2f} · {} {}", texts.Get("Records.judge"), record.judgeLevel.Value(),
                                        texts.Get("Records.combo"), record.maximumCombo);
        for (std::size_t i = 0; i < judgements.size(); ++i)
        {
            text += i % 2 == 0 ? L"\n" : L" · ";
            text += std::wstring(texts.Get(judgements[i].first)) + L" " + std::to_wstring(judgements[i].second);
        }
        if (const auto options = OptionTokens(record); !options.empty())
            text += L"\n" + options.substr(3);
        text += std::format(L"\n{} {}", texts.Get("Records.stroke"), record.options.maxStrokes);
        const auto played = std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{record.playedAtMilliseconds}};
        text += std::format(L"\n{:%Y.%m.%d %H:%M} UTC", std::chrono::floor<std::chrono::minutes>(played));
        if (record.pauseMenuUsed)
            text += L" · " + std::wstring(texts.Get("Records.pauseMenuUsed"));
        TooltipText hint{std::move(text)};
        hint.placement = TooltipPlacement::Right;
        hint.large = true;
        return hint;
    }
} // namespace

void SongRecordView::InitializeContent(Visual2DNode& panel)
{
    const auto &text = MusicSelect(texts_);
    panel_ = &panel;
    auto &selector = CreateDropdown(panel, {}, {std::wstring(text.personalRecord)}, "RecordSelector");
    selector_ = &selector;
    ApplySkinPart(selector, visuals_, "Records.Selector", theme::Surface);
    RequireDropdown(selector).SetValueStyle(13.0F, 600, theme::Ink);
    // Icon-only controls; their names stay available to tooltips.
    const auto button = [this, &panel](const wchar_t *icon, const char *name,
                                       Visual2DNode **glyphOut = nullptr) -> Visual2DNode * {
        auto &node = CreateButton(panel, {}, L"", name);
        ApplySpriteStyle(node, theme::Surface, PureWhite);
        SetCornerRadius(node, 2.0F);
        auto &glyph = CreateSprite(node, {(ControlSize - 18.0F) * 0.5F, (ControlSize - 18.0F) * 0.5F, 18.0F, 18.0F},
                                   visuals_.RegisterImage(Common(icon)), "Icon");
        RequireComponent<SpriteVisualComponent>(glyph).SetTint(theme::BrandText);
        if (glyphOut) *glyphOut = &glyph;
        return &node;
    };
    // The magnifier; the button inverts while a filter is on.
    queryButton_ = button(L"Icons.search", "RecordQuery", &queryIcon_);
    RefreshQueryButton();
    strokesIcon_ = visuals_.RegisterImage(Result(L"Options.MaxStrokes"));
    statusBand_ = visuals_.RegisterImage(SkinSetSelection::Instance().Image("MusicSelect", "Records.Band"));
    pagerPlate_ = &CreatePanel(panel, {}, "RecordPagerPlate");
    ApplyFlatStyle(*pagerPlate_, theme::SurfacePlate);
    SetCornerRadius(*pagerPlate_, 0.0F);
    previousButton_ = button(L"Icons.chev-left", "RecordPrevious");
    nextButton_ = button(L"Icons.chev-right", "RecordNext");
    pageLabel_ = &CreateNumber(panel, {}, L"", 13.0F, 500, theme::InkSoft, "RecordPage", TextAlignment::Center, false);
    rows_ = &panel.CreateChild("RecordRows");
    // NO RECORD, loading and lookup errors use a row plate of their own.
    messagePlate_ = &CreatePanel(panel, {}, "RecordMessagePlate");
    ApplySkinPart(*messagePlate_, visuals_, "Records.Row", theme::SurfacePlate, 0.0F);
    emptyMessage_ = &CreateNumber(panel, {}, std::wstring(text.noRecord), 20.0F, 700, theme::Caption, "NoRecord",
                                  TextAlignment::Center, false);
}

void SongRecordView::RefreshQueryButton()
{
    if (!queryButton_ || !queryIcon_) return;
    const bool active = !query_.empty();
    if (active) ApplySpriteStyle(*queryButton_, theme::Brand, theme::BrandDeep);
    else ApplySpriteStyle(*queryButton_, theme::Surface, PureWhite);
    RequireComponent<SpriteVisualComponent>(*queryIcon_).SetTint(active ? PureWhite : theme::BrandText);
}

void SongRecordView::InitializeFilter(Visual2DCanvas& canvas, IWindowControl* window)
{
    filterPanel_.Initialize(canvas, texts_, window, &visuals_);
}

void SongRecordView::Layout()
{
    if (!rows_)
        return;
    const float height = panel_->NodeSize().height;
    selector_->SetBounds(TopLeft({ContentLeft, 16.0F, 248.0F, ControlSize}, height));
    queryButton_->SetBounds(TopLeft({ContentLeft + 256.0F, 16.0F, ControlSize, ControlSize}, height));
    rows_->SetBounds(TopLeft({0.0F, RowsTop, RowWidth, RowPitch * static_cast<float>(PageSize)}, height));
    messagePlate_->SetBounds(TopLeft({0.0F, RowsTop, RowWidth, RowHeight}, height));
    emptyMessage_->SetBounds(TopLeft({ContentLeft, RowsTop + 12.0F, ContentRight - ContentLeft, 28.0F}, height));
    pagerPlate_->SetBounds(TopLeft({PagerCenter - PagerWidth * 0.5F, PagerTop, PagerWidth, ControlSize}, height));
    previousButton_->SetBounds(TopLeft({PagerCenter - 24.0F - ControlSize, PagerTop, ControlSize, ControlSize}, height));
    pageLabel_->SetBounds(TopLeft({PagerCenter - 24.0F, PagerTop, 48.0F, ControlSize}, height));
    nextButton_->SetBounds(TopLeft({PagerCenter + 24.0F, PagerTop, ControlSize, ControlSize}, height));
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
        RefreshQueryButton();
    }
}

SongRecordAction SongRecordView::ProcessAction(const Action& action)
{
    if (action.type != ActionType::Clicked) return SongRecordAction::None;
    if (queryButton_ && action.source == queryButton_->Id()) return SongRecordAction::OpenFilter;
    if (previousButton_ && action.source == previousButton_->Id())
    {
        offset_ = offset_ >= PageSize ? offset_ - PageSize : 0;
        ++revision_;
        return SongRecordAction::PageChanged;
    }
    if (nextButton_ && action.source == nextButton_->Id() && next_)
    {
        offset_ += PageSize;
        ++revision_;
        return SongRecordAction::PageChanged;
    }
    const auto shown = std::ranges::find(shownRecords_, action.source, &std::pair<NodeId, PlayRecord>::first);
    if (shown == shownRecords_.end())
        return SongRecordAction::None;
    openedRecord_ = shown->second;
    return SongRecordAction::OpenRecord;
}

void SongRecordView::ShowPager(const bool visible)
{
    // A chart with one page of history, or none, shows no page controls.
    const bool paged = visible && (offset_ > 0 || next_);
    pagerPlate_->SetVisible(paged);
    previousButton_->SetVisible(paged);
    nextButton_->SetVisible(paged);
    pageLabel_->SetVisible(paged);
    previousButton_->SetEnabled(offset_ > 0);
    nextButton_->SetEnabled(next_);
    RequireComponent<TextVisualComponent>(*pageLabel_).SetText(std::to_wstring(offset_ / PageSize + 1));
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
        ShowPager(false);
        std::vector<NodeId> ids;
        for (const auto &child : rows_->Children())
        {
            ids.push_back(child->Id());
            if (tooltip_ != nullptr)
                tooltip_->Set(*child, {});
        }
        for (auto id : ids)
            static_cast<void>(rows_->RemoveChild(id));
        shownRecords_.clear();
        optionLines_.clear();
        // A song without a chart leaves the record column empty.
        ShowMessage(!pattern.empty());
        SetMessage(std::wstring(texts_.Get("Records.loading")));
    }
    if (job_.valid() && job_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
        try
        {
            auto page = job_.get();
            if (!page.deleteError.empty())
                deleteError_ = std::move(page.deleteError);
            if (jobRevision_ == revision_ && !pattern.empty())
                Show(page.rows);
        }
        catch (const std::exception &error)
        {
            if (jobRevision_ == revision_)
            {
                ShowMessage(true);
                SetMessage(std::wstring(texts_.Get("Records.loadError")) + DecodeDisplayText(error.what()));
            }
        }
    }
    FitOptions();
    if (!job_.valid() && jobRevision_ != revision_)
    {
        jobRevision_ = revision_;
        const auto deleteId = std::exchange(pendingDelete_, std::nullopt);
        if (pattern.empty() && !deleteId)
            return;
        const auto database = database_;
        job_ = std::async(std::launch::async,
                                [database, pattern, mode, condition = query_, offset = offset_, deleteId]
                                {
                                    const PhantomCore::BackgroundWork work("read records");
                                    const PlayRecordStore store(database.empty() ? PlayRecordDatabasePath() : database);
                                    Page page;
                                    // The delete runs before the page is read again, on this worker, so the page
                                    // never shows it. A failure keeps the record and is reported.
                                    if (deleteId)
                                    {
                                        try
                                        {
                                            static_cast<void>(store.Delete(*deleteId));
                                        }
                                        catch (const std::exception &error)
                                        {
                                            page.deleteError = error.what();
                                        }
                                    }
                                    if (pattern.empty())
                                        return page;
                                    auto &rows = page.rows;
                                    for (auto &record : store.Query(pattern, mode, condition, offset, PageSize + 1))
                                        rows.push_back({std::move(record), {}});
                                    // The shown rows' details give their tooltips' judgement counts.
                                    for (std::size_t i = 0; i < std::min(rows.size(), PageSize); ++i)
                                    {
                                        auto &record = rows[i].record;
                                        try
                                        {
                                            record.detail = store.LoadDetail(record.id);
                                        }
                                        catch (const std::exception &)
                                        {
                                            // The row shows without counts; opening it reports the error.
                                        }
                                        rows[i].judgements = RecordJudgementCounts(record);
                                        // Opening the record reads the detail again; the list stays light.
                                        std::string{}.swap(record.detail);
                                    }
                                    return page;
                                });
    }
}

void SongRecordView::ShowMessage(const bool visible)
{
    messagePlate_->SetVisible(visible);
    emptyMessage_->SetVisible(visible);
}

void SongRecordView::SetMessage(std::wstring message)
{
    // NO RECORD is a number-face line; loading and error sentences may need CJK glyphs.
    auto &text = RequireComponent<TextVisualComponent>(*emptyMessage_);
    if (ui_fonts::NeedsCjkFace(message))
        text.SetFont(ui_fonts::Text(message));
    else
        text.SetFont(ui_fonts::Number(700, false).font);
    text.SetFontSize(message.size() > 24 ? 13.0F : 20.0F);
    text.SetText(std::move(message));
}

std::optional<PlayRecord> SongRecordView::RecordAt(const NodeId hovered) const
{
    if (hovered == 0)
        return std::nullopt;
    const auto shown = std::ranges::find(shownRecords_, hovered, &std::pair<NodeId, PlayRecord>::first);
    if (shown == shownRecords_.end())
        return std::nullopt;
    return shown->second;
}

std::wstring SongRecordView::DescribeRecord(const PlayRecord &record) const
{
    std::wstring text = std::wstring(RankName(record.accuracy)) + L" · " + ScoreText(record.score) + L" · " +
                        AccuracyText(record.accuracy) + L"\n" + std::wstring(texts_.Get(StatusKey(record))) +
                        OptionTokens(record);
    const auto played =
        std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{record.playedAtMilliseconds}};
    return text + std::format(L"\n{:%Y.%m.%d %H:%M} UTC", std::chrono::floor<std::chrono::minutes>(played));
}

void SongRecordView::DeleteRecord(const std::int64_t id)
{
    pendingDelete_ = id;
    ++revision_;
}

void SongRecordView::Show(const std::vector<LoadedRecord> &records)
{
    // The last row of a later page was deleted: the page before it is the one to show.
    if (records.empty() && offset_ > 0)
    {
        offset_ = offset_ >= PageSize ? offset_ - PageSize : 0;
        ++revision_;
        return;
    }
    ShowMessage(records.empty());
    SetMessage(std::wstring(texts_.Get("MusicSelect.noRecord")));
    next_ = records.size() > PageSize;
    ShowPager(true);
    const float height = rows_->NodeSize().height;
    constexpr float mainLeft = ContentLeft + 58.0F;
    for (std::size_t i = 0; i < std::min(records.size(), PageSize); ++i)
    {
        const auto &r = records[i].record;
        // A row is a button: clicking it opens the record's result screen.
        auto &row = CreateButton(*rows_, TopLeft({0.0F, static_cast<float>(i) * RowPitch, RowWidth, RowHeight}, height),
                                 L"", "PlayRecord");
        shownRecords_.emplace_back(row.Id(), r);
        if (tooltip_ != nullptr)
            tooltip_->Set(row, RecordTooltip(r, records[i].judgements, texts_));
        ApplySkinPart(row, visuals_, "Records.Row", theme::SurfacePlate, 0.0F);
        static_cast<void>(CreateNumber(row, {ContentLeft, 7.0F, 48.0F, 38.0F}, std::wstring(RankName(r.accuracy)), 20.0F,
                                       800, theme::BrandText, "Rank", TextAlignment::Center));
        // The play's status is a band along the plate's slanted right end, in the colour its chip had
        // (user decision 2026-11-22); the tooltip's first line names it.
        auto &band = CreateSprite(row, {0.0F, 0.0F, RowWidth, RowHeight}, statusBand_, "Status");
        RequireComponent<SpriteVisualComponent>(band).SetTint(StatusColor(r));

        // Top line: the score, then the options that change the play at the right end. Options that do
        // not fit beside the score go, last first, with "…" (FitOptions).
        auto &score = CreateNumber(row, {mainLeft, 5.0F, 150.0F, 24.0F}, ScoreText(r.score), ScoreSize, 800, theme::Ink,
                                   "Score");
        auto tokens = RecordOptionTokens(r.options);
        auto &options = CreateNumber(row, {mainLeft, OptionTop, ContentRight - mainLeft, OptionHeight},
                                     JoinRecordOptions(tokens, tokens.size()), OptionSize, 700, theme::BrandText,
                                     "Options", TextAlignment::Trailing, false);
        optionLines_.push_back({&options, &score, std::move(tokens), 0});
        optionLines_.back().shown = optionLines_.back().tokens.size();

        // Bottom line: the accuracy, the stroke count and the date.
        const auto timestamp =
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{r.playedAtMilliseconds}};
        auto date = std::format(L"{:%Y.%m.%d}", std::chrono::floor<std::chrono::days>(timestamp));
        const float accuracyRight = ShowStrokes(row, r, ContentRight - EstimateCaptionWidth(date, 13.0F));
        static_cast<void>(CreateNumber(row, {mainLeft, 29.0F, std::max(accuracyRight - mainLeft, 1.0F), 18.0F},
                                       AccuracyText(r.accuracy), 13.0F, 500, theme::Caption, "Accuracy",
                                       TextAlignment::Leading, false));

        static_cast<void>(CreateNumber(row, {ContentRight - 84.0F, 29.0F, 84.0F, 18.0F}, std::move(date), 13.0F, 500,
                                       theme::Caption, "Date", TextAlignment::Trailing, false));
    }
}

void SongRecordView::FitOptions()
{
    for (auto &line : optionLines_)
    {
        // The room right of the score, measured once the renderer has laid the score out.
        const auto &scoreText = RequireComponent<TextVisualComponent>(*line.score);
        const auto scoreSize = scoreText.MeasuredSize();
        const float scoreWidth = scoreSize ? scoreSize->width : EstimateCaptionWidth(scoreText.Text(), ScoreSize);
        const float lineWidth = line.label->NodeSize().width;
        const float room = std::max(lineWidth - scoreWidth - OptionGap, 0.0F);
        line.label->SetClipRect({lineWidth - room, 0.0F, room, OptionHeight});
        // A changed text is measured the frame after, so one option goes per frame until the rest fit.
        auto &text = RequireComponent<TextVisualComponent>(*line.label);
        const auto measured = text.MeasuredSize();
        const bool fits = !measured || measured->width <= room + 0.5F;
        if (fits || line.shown == 0)
            continue;
        --line.shown;
        text.SetText(JoinRecordOptions(line.tokens, line.shown));
    }
}

float SongRecordView::ShowStrokes(Visual2DNode &row, const PlayRecord &record, const float dateLeft)
{
    // Returns where the details line must end: before the stroke count.
    auto count = std::to_wstring(record.options.maxStrokes);
    const float countWidth = EstimateCaptionWidth(count, 13.0F) + 2.0F;
    const float left = dateLeft - StrokeMargin - countWidth - StrokeGap - StrokeIconSize;
    auto &icon = CreateSprite(row, TopLeft({left, 31.0F, StrokeIconSize, StrokeIconSize}, RowHeight), strokesIcon_,
                              "Strokes.Icon");
    RequireComponent<SpriteVisualComponent>(icon).SetTint(theme::Caption);
    static_cast<void>(CreateNumber(row, {left + StrokeIconSize + StrokeGap, 29.0F, countWidth, 18.0F}, std::move(count),
                                   13.0F, 500, theme::Caption, "Strokes", TextAlignment::Leading, false));
    return left - StrokeMargin;
}

void SongRecordView::RefreshTexts()
{
    const auto& text = MusicSelect(texts_);
    if (selector_)
    {
        RequireDropdown(*selector_).SetItems({std::wstring(text.personalRecord)});
    }
    if (queryButton_)
        ++revision_;
    filterPanel_.RefreshTexts();
}

void SongRecordView::Shutdown() noexcept
{
    filterPanel_.Shutdown();
    panel_ = selector_ = messagePlate_ = emptyMessage_ = pageLabel_ = nullptr;
    queryButton_ = pagerPlate_ = previousButton_ = nextButton_ = rows_ = nullptr;
    shownRecords_.clear();
    optionLines_.clear();
    openedRecord_.reset();
    ++revision_;
}
