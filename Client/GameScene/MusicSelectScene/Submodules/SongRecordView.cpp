#include "SongRecordView.h"
#include "App/GameDataPaths.h"
#include "Catalog/SongCatalog.h"
#include "Presentation/AccuracyText.h"
#include "Presentation/RankName.h"
#include "Presentation/UiDropdown.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
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

    // Only what differs from a plain play, after the judge level the play actually used.
    [[nodiscard]] std::wstring OptionSummary(const PlayRecord &record)
    {
        const auto &o = record.options;
        std::wstring text = std::format(L"JL {:.2f}", record.judgeLevel.Value());
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
    const auto button = [this, &panel](const wchar_t *icon, const char *name) -> Visual2DNode * {
        auto &node = CreateButton(panel, {}, L"", name);
        ApplySpriteStyle(node, theme::Surface, PureWhite);
        SetCornerRadius(node, 2.0F);
        auto &glyph = CreateSprite(node, {(ControlSize - 18.0F) * 0.5F, (ControlSize - 18.0F) * 0.5F, 18.0F, 18.0F},
                                   visuals_.RegisterImage(Common(icon)), "Icon");
        RequireComponent<SpriteVisualComponent>(glyph).SetTint(theme::BrandText);
        return &node;
    };
    queryButton_ = button(L"Icons.filter", "RecordQuery");
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
    return SongRecordAction::None;
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
            ids.push_back(child->Id());
        for (auto id : ids)
            static_cast<void>(rows_->RemoveChild(id));
        // A song without a chart leaves the record column empty.
        ShowMessage(!pattern.empty());
        SetMessage(std::wstring(texts_.Get("Records.loading")));
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
                ShowMessage(true);
                SetMessage(std::wstring(texts_.Get("Records.loadError")) + DecodeDisplayText(error.what()));
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
                                    return PlayRecordStore(database.empty() ? PlayRecordDatabasePath() : database)
                                        .Query(pattern, mode, condition, offset, PageSize + 1);
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

void SongRecordView::Show(const std::vector<PlayRecord> &records)
{
    ShowMessage(records.empty());
    SetMessage(std::wstring(texts_.Get("MusicSelect.noRecord")));
    next_ = records.size() > PageSize;
    ShowPager(true);
    const float height = rows_->NodeSize().height;
    constexpr float mainLeft = ContentLeft + 58.0F, detailsWidth = 144.0F;
    for (std::size_t i = 0; i < std::min(records.size(), PageSize); ++i)
    {
        const auto &r = records[i];
        auto &row = CreatePanel(*rows_, TopLeft({0.0F, static_cast<float>(i) * RowPitch, RowWidth, RowHeight}, height),
                                "PlayRecord");
        ApplySkinPart(row, visuals_, "Records.Row", theme::SurfacePlate, 0.0F);
        static_cast<void>(CreateNumber(row, {ContentLeft, 7.0F, 48.0F, 38.0F}, std::wstring(RankName(r.accuracy)), 20.0F,
                                       800, theme::BrandText, "Rank", TextAlignment::Center));
        static_cast<void>(CreateNumber(row, {mainLeft, 5.0F, 150.0F, 24.0F}, AccuracyText(r.accuracy), 20.0F, 800,
                                       theme::Ink, "Accuracy"));
        auto &details = CreateNumber(row, {mainLeft, 29.0F, detailsWidth, 18.0F}, OptionSummary(r), 13.0F, 500,
                                     theme::Caption, "Details", TextAlignment::Leading, false);
        details.SetClipRect({0.0F, 0.0F, detailsWidth, 18.0F});

        // Clear, a finished play below the clear line, or a play that was left early (shown as Failed).
        const char *statusKey = r.cleared ? "Records.cleared" : (r.completed ? "Records.practice" : "Records.failed");
        const char *chipKey = r.cleared ? "Records.Status.Clear" : (r.completed ? "Records.Status.Practice" : "Records.Status.Retired");
        const Color statusColor = r.cleared ? theme::Ok : (r.completed ? theme::InkSoft : theme::Danger);
        std::wstring status(texts_.Get(statusKey));
        const float chipWidth = std::clamp(EstimateCaptionWidth(status, 12.0F) + 14.0F, 42.0F, 96.0F);
        auto &chip = CreatePanel(row, TopLeft({ContentRight - chipWidth, 6.0F, chipWidth, 20.0F}, RowHeight), "Status");
        ApplySkinPart(chip, visuals_, chipKey, {0.89F, 0.965F, 0.925F, 1.0F});
        static_cast<void>(CreateCaption(chip, {0.0F, 0.0F, chipWidth, 20.0F}, std::move(status), 12.0F, 700, statusColor,
                                        "StatusText", TextAlignment::Center));

        const auto timestamp =
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{r.playedAtMilliseconds}};
        static_cast<void>(CreateNumber(row, {ContentRight - 84.0F, 29.0F, 84.0F, 18.0F},
                                       std::format(L"{:%Y.%m.%d}", std::chrono::floor<std::chrono::days>(timestamp)),
                                       13.0F, 500, theme::Caption, "Date", TextAlignment::Trailing, false));
    }
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
    ++revision_;
}
