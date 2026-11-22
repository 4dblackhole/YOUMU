#pragma once
#include "OptionsCategory.h"

// General: the description language and the Songs folder (its path, Browse and the default folder).
class OptionsGeneral final : public OptionsCategory
{
  public:
    using OptionsCategory::OptionsCategory;

    [[nodiscard]] std::vector<float> FieldHeights() const override;
    void Create() override;
    void Refresh() override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool ProcessAction(const PhantomCore::Action &action) override;
    void AppendCombos(std::vector<PhantomCore::Visual2DNode *> &combos) const override;

  private:
    void CreateLanguageControls();
    void CreateSongsControls();
    void RefreshSongsControls();
    // False for an index the catalogue does not have; the action then stays unhandled.
    bool SelectLanguage(std::size_t index);
    void ChooseSongsFolder();
    void UseDefaultSongsFolder();

    PhantomCore::Visual2DNode *languageLabel_{}, *languageCombo_{};
    PhantomCore::Visual2DNode *songsLabel_{}, *songsField_{}, *songsBrowse_{}, *songsDefault_{};
};
