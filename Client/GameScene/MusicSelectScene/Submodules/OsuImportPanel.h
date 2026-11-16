#pragma once
#include "EditorOsuImport.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

// What the user answered in the difficulty list.
struct OsuImportChoice
{
    bool import{};                    // False: cancelled, nothing is imported.
    std::vector<std::size_t> indices; // Into the scanned difficulties, in list order.
};

// Modal list of the difficulties found in an osu! song folder. Difficulties that can be imported
// start checked; the others show why they cannot and stay unchecked.
class OsuImportPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts);
    void Shutdown() noexcept;
    void Open(std::wstring folder, std::vector<OsuImportDifficulty> difficulties);
    // Closing an open list without an answer counts as Cancel, so its owner always learns the
    // import ended and can drop the converted files.
    void Close() noexcept;
    void SetViewport();
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> pointer);
    [[nodiscard]] std::optional<OsuImportChoice> TakeChoice();

  private:
    PhantomCore::Visual2DNode &Button(PhantomCore::Visual2DNode &parent, std::string name);
    void Layout();
    void RefreshRows();
    void RefreshStatus();
    void MoveList(int delta);
    void Answer(bool import);
    [[nodiscard]] std::wstring RowText(const OsuImportDifficulty &difficulty) const;
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *title_{}, *folder_{}, *list_{}, *status_{};
    PhantomCore::Visual2DNode *import_{}, *cancel_{}, *previous_{}, *next_{};
    std::vector<std::pair<PhantomCore::NodeId, std::size_t>> rowIds_, checkIds_;
    std::vector<OsuImportDifficulty> difficulties_;
    std::vector<bool> checked_;
    std::optional<OsuImportChoice> choice_;
    std::optional<std::size_t> selected_;
    std::wstring folderName_;
    std::size_t first_{};
    bool waitRelease_{};
};
