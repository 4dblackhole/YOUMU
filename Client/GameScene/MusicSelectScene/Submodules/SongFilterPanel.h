#pragma once
#include "PhantomCore.h"
#include "SongFilter.h"
#include "Texts/TextCatalog.h"
#include <array>

class SongFilterPanel final
{
  public:
    void Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window);
    void Open(const SongFilter &filter, const SongCatalogLoadResult &catalog);
    void Close() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t width, std::uint32_t height);
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const Action &action);
    void ProcessInput(const InputState &input);
    [[nodiscard]] std::optional<SongFilter> TakeApplied();

  private:
    Visual2DNode &Label(Visual2DNode &parent, std::string name, float size = 16);
    void Layout();
    void RefreshValues();
    void SynchronizeText();
    void UpdateInputVisibility();
    void TryApply();
    Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    Visual2DNode *overlay_{}, *panel_{}, *title_{}, *modeLabel_{}, *mode_{}, *minimum_{}, *maximum_{};
    Visual2DNode *apply_{}, *reset_{}, *close_{}, *status_{};
    std::array<Visual2DNode *, 4> labels_{};
    std::array<Visual2DNode *, 8> fields_{};
    std::array<std::unique_ptr<ITextInput>, 8> inputs_;
    std::vector<std::string> modes_;
    SongFilterDraft draft_;
    SongFilterError error_{};
    std::optional<SongFilter> applied_;
    std::uint32_t width_{1280}, height_{720};
    bool difficultyAvailable_{};
};
