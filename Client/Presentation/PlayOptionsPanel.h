#pragma once
#include "PhantomCore.h"
#include "Mode/PlayOptions.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <optional>

class PlayOptionsPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts, PhantomCore::IWindowControl *window);
    void Open(const PlayOptions &options);
    // User Close/Esc validates and publishes the edited options.
    void Close();
    // Lifetime cleanup does not publish unfinished input.
    void Dismiss() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t height);
    void RefreshTexts();
    void Update(double deltaSeconds);
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> pointer);
    [[nodiscard]] std::optional<PlayOptions> TakeApplied();

  private:
    enum Field : std::size_t { Speed, Rate, Judge, Seed, Probability, FieldCount };
    enum Flag : std::size_t { Mirror, Random, IgnoreScroll, Metronome, MusicPitch, HitPitch, OverrideJudge, FlagCount };
    PhantomCore::Visual2DNode &Label(PhantomCore::Visual2DNode &parent, std::string name, float size = 16);
    void Layout();
    void UpdateInputs();
    void SynchronizeText();
    void RefreshValues();
    void Reset();
    bool &FlagValue(Flag flag);
    [[nodiscard]] bool Accepts(PhantomCore::NodeId source) const noexcept;
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *title_{}, *close_{}, *reset_{}, *status_{};
    PhantomCore::Visual2DNode *viewport_{}, *content_{}, *unitLabel_{}, *unit_{};
    std::array<PhantomCore::Visual2DNode *, 3> headings_{};
    std::array<PhantomCore::Visual2DNode *, FieldCount> labels_{}, fields_{};
    std::array<std::unique_ptr<PhantomCore::ITextInput>, FieldCount> inputs_;
    std::array<std::wstring, FieldCount> numbers_;
    std::array<PhantomCore::Visual2DNode *, FlagCount> flags_{};
    PlayOptions draft_;
    std::optional<PlayOptions> applied_;
    std::optional<Field> errorField_;
    std::string error_;
    std::uint32_t height_{720};
    float progress_{}, scroll_{}, contentHeight_{}, viewportHeight_{};
    bool rateExpanded_{}, overrideJudge_{};
};
