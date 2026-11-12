#pragma once
#include "PhantomCore.h"
#include "RecordFilterPanel.h"
#include "Records/PlayRecordStore.h"
#include "Texts/TextCatalog.h"
#include <filesystem>
#include <future>
#include <span>
#include <utility>

struct SongCatalogPattern;
enum class SongRecordAction { None, OpenFilter, PageChanged };

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
    void OpenFilter();
    void CloseFilter();
    [[nodiscard]] bool FilterVisible() const noexcept { return filterPanel_.IsVisible(); }
    void UpdateFilter(std::span<const PhantomCore::Action> actions, const PhantomCore::InputState& input);
    [[nodiscard]] SongRecordAction ProcessAction(const PhantomCore::Action& action);
    void Shutdown() noexcept;

private:
    void Show(const std::vector<PlayRecord>& records);
    TextCatalog& texts_;
    PhantomCore::ScreenVisual2DManager& visuals_;
    std::filesystem::path database_;
    RecordFilterPanel filterPanel_;
    RecordFilter filter_;
    std::future<std::vector<PlayRecord>> job_;
    std::filesystem::path pattern_;
    std::string mode_, query_;
    std::size_t offset_{};
    std::uint64_t revision_{1}, jobRevision_{}, displayRevision_{};
    bool next_{};
    PhantomCore::Visual2DNode *panel_{}, *selector_{}, *emptyMessage_{};
    PhantomCore::Visual2DNode *queryButton_{}, *previousButton_{}, *nextButton_{}, *rows_{};
};
