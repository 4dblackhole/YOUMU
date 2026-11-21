#pragma once
#include "PhantomCore.h"
#include "Mode/PlayOptions.h"
#include "Presentation/UiModal.h"
#include "Texts/TextCatalog.h"
#include <algorithm>
#include <array>
#include <optional>

// Options that differ from a plain play, counted by group as the Play options badge shows them: scroll speed or
// unit, Accelerate, either pitch setting, JudgeLevel override, Mirror, Random, Ignore scroll effects, Metronome,
// a non-default HP gauge and a non-default max strokes.
[[nodiscard]] inline std::size_t ChangedPlayOptionCount(const PlayOptions &options) noexcept
{
    const PlayOptions plain;
    const bool scroll = options.scrollUnit != plain.scrollUnit || options.scrollSpeed != plain.scrollSpeed;
    const bool rate = options.accelerateUnit != plain.accelerateUnit || options.accelerate != plain.accelerate;
    const bool pitch = options.preserveMusicPitch != plain.preserveMusicPitch ||
                       options.preserveHitSoundPitch != plain.preserveHitSoundPitch;
    const std::array changed{scroll, rate, pitch, options.judgeLevel.has_value(),
                             options.mirror, options.random, options.ignoreScrollEffects, options.metronome,
                             options.hpGauge != plain.hpGauge, options.maxStrokes != plain.maxStrokes};
    return static_cast<std::size_t>(std::ranges::count(changed, true));
}

// Play options (UI-02 flat modal): Difficulty, Layout and Assist columns. Edits apply when the panel closes
// (Done, Escape, Enter, F1); there is no cancel. Reset (or F2) restores PlayOptions{} in the edit.
class PlayOptionsPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts, PhantomCore::IWindowControl *window,
                    PhantomCore::ScreenVisual2DManager *visuals = nullptr);
    void Open(const PlayOptions &options);
    // Done/Esc validates and publishes the edited options.
    void Close();
    // Lifetime cleanup does not publish unfinished input.
    void Dismiss() noexcept;
    void Shutdown() noexcept;
    void SetViewport(std::uint32_t height);
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return modal_.IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input);
    [[nodiscard]] std::optional<PlayOptions> TakeApplied();

  private:
    enum Field : std::size_t { Speed, Rate, Judge, Seed, Probability, FieldCount };
    enum Flag : std::size_t { Mirror, Random, IgnoreScroll, Metronome, MusicPitch, HitPitch, OverrideJudge, FlagCount };
    struct SwitchRow
    {
        PhantomCore::Visual2DNode *row{}, *label{}, *toggle{}, *rule{};
    };
    void Layout();
    float LayoutDifficulty(float height);
    float LayoutLayoutColumn(float height);
    void LayoutAssist(float height);
    void PlaceSwitchRow(Flag flag, float left, float top, float width, float height);
    void UpdateInputs();
    void SynchronizeText();
    void RefreshValues();
    void Reset();
    void SetFlag(Flag flag, bool value);
    bool &FlagValue(Flag flag);
    [[nodiscard]] float ColumnHeight(std::size_t column) const noexcept;
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    UiModal modal_;
    std::array<PhantomCore::Visual2DNode *, 3> headings_{};
    std::array<PhantomCore::Visual2DNode *, 2> columnRules_{}, units_{}, rateUnits_{};
    std::array<PhantomCore::Visual2DNode *, 2> subRules_{};
    PhantomCore::Visual2DNode *rateRange_{}, *rateMore_{}, *rateMoreIcon_{}, *judgeUnit_{}, *probabilityUnit_{};
    PhantomCore::Visual2DNode *gaugeLabel_{}, *gaugeClear_{}, *gauge_{};
    // Max strokes 1..4 as tabs under the Assist switches.
    PhantomCore::Visual2DNode *strokeLabel_{}, *strokeRange_{};
    std::array<PhantomCore::Visual2DNode *, PlayOptions::MaximumStrokes> strokeTabs_{};
    std::array<PhantomCore::Visual2DNode *, FieldCount> labels_{}, fields_{};
    std::array<std::unique_ptr<PhantomCore::ITextInput>, FieldCount> inputs_;
    std::array<std::wstring, FieldCount> numbers_;
    std::array<SwitchRow, FlagCount> flags_{};
    PhantomCore::ScreenVisual2DManager *visuals_{};
    PlayOptions draft_;
    std::optional<PlayOptions> applied_;
    std::optional<Field> errorField_;
    std::string error_;
    std::uint32_t height_{720};
    bool rateExpanded_{}, overrideJudge_{};
};
