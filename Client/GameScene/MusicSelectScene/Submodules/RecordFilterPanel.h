#pragma once
#include "PhantomCore.h"
#include "Presentation/UiModal.h"
#include "Records/RecordFilter.h"
#include "Texts/TextCatalog.h"
#include <vector>

// Record filters (UI-02): Any/On/Off rows for the play options and the play results, an Any/1-4 row for the
// stroke count, and value rows with a switch for the ranges and the seed. Apply commits a validated copy; Cancel, ✕, Escape and hiding the Scene
// discard the edit.
class RecordFilterPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts, PhantomCore::IWindowControl *window,
                    PhantomCore::ScreenVisual2DManager *visuals = nullptr);
    void Open(const RecordFilter &filter);
    void Close() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t height);
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return modal_.IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input);
    [[nodiscard]] std::optional<RecordFilter> TakeApplied();

  private:
    static constexpr std::size_t ChoiceCount = RecordFilterFlagCount + 1; // Flags, then the scroll unit.
    static constexpr std::size_t FieldCount = 9;
    static constexpr std::size_t StrokeItemCount = 5; // Any, then 1..4.
    void LayoutStrokes(float bodyHeight);
    void Layout();
    void RefreshValues();
    void RefreshRows();
    void SynchronizeText();
    void UpdateInputs();
    void TryApply();
    void Reset();
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    UiModal modal_;
    PhantomCore::Visual2DNode *optionsHeading_{}, *rangesHeading_{}, *resultsHeading_{}, *minimum_{}, *maximum_{};
    std::array<PhantomCore::Visual2DNode *, ChoiceCount> choiceLabels_{};
    std::array<std::array<PhantomCore::Visual2DNode *, 3>, ChoiceCount> choices_{};
    std::array<PhantomCore::Visual2DNode *, ChoiceCount> choiceRules_{};
    PhantomCore::Visual2DNode *strokeLabel_{}, *strokeRule_{};
    std::array<PhantomCore::Visual2DNode *, StrokeItemCount> strokeTabs_{};
    std::array<PhantomCore::Visual2DNode *, RecordFilterValueRowCount> switches_{}, rowLabels_{};
    std::array<PhantomCore::Visual2DNode *, FieldCount> fields_{};
    std::array<std::unique_ptr<PhantomCore::ITextInput>, FieldCount> inputs_;
    RecordFilterDraft draft_;
    std::optional<RecordFilter> applied_;
    std::string error_;
    std::uint32_t height_{720};
};
