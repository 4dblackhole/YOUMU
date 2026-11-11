#pragma once
#include "PhantomCore.h"
#include "Records/RecordFilter.h"
#include "Texts/TextCatalog.h"

class RecordFilterPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts, PhantomCore::IWindowControl *window);
    void Open(const RecordFilter &filter);
    void Close() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t height);
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input);
    [[nodiscard]] std::optional<RecordFilter> TakeApplied();

  private:
    PhantomCore::Visual2DNode &Label(PhantomCore::Visual2DNode &parent, std::string name, float size = 16);
    void Layout();
    void RefreshValues();
    void SynchronizeText();
    void UpdateInputVisibility();
    void TryApply();
    void EditCondition();
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *title_{}, *close_{}, *apply_{}, *status_{};
    PhantomCore::Visual2DNode *minimum_{}, *maximum_{}, *seedLabel_{}, *condition_{}, *conditionText_{};
    std::array<PhantomCore::Visual2DNode *, RecordFilterFlagCount + 1> choiceLabels_{}, choices_{};
    std::array<PhantomCore::Visual2DNode *, 4> rangeLabels_{};
    std::array<PhantomCore::Visual2DNode *, 9> fields_{};
    std::array<std::unique_ptr<PhantomCore::ITextInput>, 9> inputs_;
    RecordFilterDraft draft_;
    std::optional<RecordFilter> applied_;
    std::string error_;
    std::uint32_t height_{720};
    bool discardKeys_{}, waitRelease_{};
};
