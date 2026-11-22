#pragma once
#include "PhantomCore.h"
#include "RecordFilterPanel.h"
#include "Records/PlayRecordStore.h"
#include "Texts/TextCatalog.h"
#include "Presentation/UiTooltip.h"
#include <filesystem>
#include <future>
#include <optional>
#include <span>
#include <utility>

struct SongCatalogPattern;
enum class SongRecordAction { None, OpenFilter, PageChanged, OpenRecord };

// Canvas owns nodes. This view owns their observers and record query/filter state.
class SongRecordView final
{
public:
    SongRecordView(TextCatalog& texts, PhantomCore::ScreenVisual2DManager& visuals,
                   std::filesystem::path database = {})
        : texts_(texts), visuals_(visuals), database_(std::move(database)) {}
    void InitializeContent(PhantomCore::Visual2DNode& panel);
    void InitializeFilter(PhantomCore::Visual2DCanvas& canvas, PhantomCore::IWindowControl* window);
    void Layout();
    void SetViewport(std::uint32_t height);
    void RefreshTexts();
    void Update(const SongCatalogPattern* selected);
    void Invalidate() noexcept { ++revision_; }
    // Rows hint what they do not show (judgement counts, mode, time) on this tooltip.
    void SetTooltip(UiTooltip &tooltip) noexcept { tooltip_ = &tooltip; }
    void OpenFilter();
    void CloseFilter();
    [[nodiscard]] bool FilterVisible() const noexcept { return filterPanel_.IsVisible(); }
    [[nodiscard]] const PhantomCore::Visual2DNode *QueryButton() const noexcept { return queryButton_; }
    void UpdateFilter(std::span<const PhantomCore::Action> actions, const PhantomCore::InputState& input);
    [[nodiscard]] SongRecordAction ProcessAction(const PhantomCore::Action& action);
    // The record whose row was clicked (OpenRecord), once.
    [[nodiscard]] std::optional<PlayRecord> TakeOpenedRecord() noexcept { return std::exchange(openedRecord_, std::nullopt); }
    void Shutdown() noexcept;

private:
    // A filter that limits the records shows the query button inverted: cobalt fill, white icon.
    void RefreshQueryButton();
    // A page row with the judgement counts the worker read from its detail.
    struct LoadedRecord
    {
        PlayRecord record;
        std::vector<std::pair<std::string_view, std::size_t>> judgements;
    };
    void Show(const std::vector<LoadedRecord>& records);
    // The afterimage icon and the stroke count left of the date; returns where the details line ends.
    float ShowStrokes(PhantomCore::Visual2DNode &row, const PlayRecord &record, float dateLeft);
    // Drops options from a row's top line, one per frame, until the measured text fits beside the score.
    void FitOptions();
    void ShowPager(bool visible);
    void ShowMessage(bool visible);
    void SetMessage(std::wstring message);
    TextCatalog& texts_;
    PhantomCore::ScreenVisual2DManager& visuals_;
    std::filesystem::path database_;
    RecordFilterPanel filterPanel_;
    RecordFilter filter_;
    std::future<std::vector<LoadedRecord>> job_;
    UiTooltip *tooltip_{};
    std::filesystem::path pattern_;
    std::string mode_, query_;
    std::size_t offset_{};
    std::uint64_t revision_{1}, jobRevision_{}, displayRevision_{};
    bool next_{};
    // Rows on screen and their records, for opening one.
    std::vector<std::pair<PhantomCore::NodeId, PlayRecord>> shownRecords_;
    std::optional<PlayRecord> openedRecord_;
    // Each row's option line, its score and the options still shown (FitOptions).
    struct OptionLine
    {
        PhantomCore::Visual2DNode *label{}, *score{};
        std::vector<std::wstring> tokens;
        std::size_t shown{};
    };
    std::vector<OptionLine> optionLines_;
    PhantomCore::ImageHandle strokesIcon_, statusBand_;
    PhantomCore::Visual2DNode *panel_{}, *selector_{}, *messagePlate_{}, *emptyMessage_{};
    PhantomCore::Visual2DNode *queryButton_{}, *queryIcon_{}, *pagerPlate_{}, *previousButton_{}, *nextButton_{}, *rows_{}, *pageLabel_{};
};
