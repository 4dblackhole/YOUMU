#pragma once
#include "PhantomCore.h"
#include "Presentation/UiModal.h"
#include "SongFilter.h"
#include "Texts/TextCatalog.h"
#include <array>

// Song filters (UI-02): Game mode, then one row per filter with a switch, minimum and maximum (Tag has one
// field). Apply commits a validated copy; Cancel, ✕, Escape and hiding the Scene discard the edit.
class SongFilterPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts, PhantomCore::IWindowControl *window,
                    PhantomCore::ScreenVisual2DManager *visuals = nullptr);
    void Open(const SongFilter &filter, const SongCatalogLoadResult &catalog);
    void Close() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t width, std::uint32_t height);
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return modal_.IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input);
    [[nodiscard]] std::optional<SongFilter> TakeApplied();

  private:
    static constexpr std::size_t FieldCount = SongFilterRangeCount * 2 + 1; // Range pairs, then the tag.
    // The range pairs hold numbers; the tag is text.
    [[nodiscard]] static constexpr bool IsNumberField(std::size_t field) noexcept { return field < SongFilterRangeCount * 2; }
    void Layout();
    void RefreshValues();
    void RefreshRows();
    void SynchronizeText();
    void UpdateInputs();
    void TryApply();
    void Reset();
    [[nodiscard]] bool RowAvailable(std::size_t row) const noexcept;
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    UiModal modal_;
    PhantomCore::Visual2DNode *modeLabel_{}, *mode_{}, *minimum_{}, *maximum_{};
    std::array<PhantomCore::Visual2DNode *, SongFilterRowCount> switches_{}, labels_{};
    std::array<PhantomCore::Visual2DNode *, SongFilterRangeCount> dashes_{};
    std::array<PhantomCore::Visual2DNode *, FieldCount> fields_{};
    std::array<std::unique_ptr<PhantomCore::ITextInput>, FieldCount> inputs_;
    std::vector<std::string> modes_;
    SongFilterDraft draft_;
    SongFilterError error_{};
    std::optional<SongFilter> applied_;
    std::uint32_t height_{720};
    bool difficultyAvailable_{};
};
