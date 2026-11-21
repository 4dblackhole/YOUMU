#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace PhantomCore;

using namespace song_select;
namespace theme = ui_colors::theme;

void MusicSelectView::CreateImportButton()
{
    if (!IsEditorSongSelect())
        return;
    // The left column of the editor song select has no records; its top holds osu! import.
    constexpr float width = layout::RecordWidth - 24.0F, height = 64.0F;
    importButton_ = &CreateButton(*board_, TopLeft({layout::Margin, 100.0F, width, height}, layout::CanvasSize.height),
                                  L"", "SongAddition.ImportOsu");
    ApplyButtonStyle(*importButton_, theme::Brand, theme::BrandDeep, PureWhite);
    SetCornerRadius(*importButton_, 6);
    auto &accent = CreatePanel(*importButton_, {0, 0, 5, height}, "Accent");
    ApplyFlatStyle(accent, theme::Sun);
    auto &icon = CreateSprite(*importButton_, {20, (height - 26.0F) * 0.5F, 26, 26},
                              screenVisuals_.RegisterImage(Common(L"Icons.folder")), "Icon");
    RequireComponent<SpriteVisualComponent>(icon).SetTint(PureWhite);
    importLabel_ = &CreateCaption(*importButton_, {62, 10, width - 72, 24}, L"", 17.0F, 800, PureWhite, "Label");
    importHelp_ = &CreateCaption(*importButton_, {62, 36, width - 72, 18}, L"", 12.0F, 600,
                                 PhantomCore::Color{1, 1, 1, .78F}, "Help");
}
void MusicSelectView::OpenOsuImportPanel(std::wstring folder, std::vector<OsuImportDifficulty> difficulties)
{
    SetSearchFocus(false);
    osuImportPanel_.Open(std::move(folder), std::move(difficulties));
    inputRouter_.Reset(*canvas_);
    UpdateSearchInputVisibility();
}
bool MusicSelectView::ProcessOsuImportPanel(const InputState &input)
{
    if (!osuImportPanel_.IsVisible())
        return false;
    for (const auto &action : canvas_->TakeActions())
        if (osuImportPanel_.ProcessAction(action))
            inputRouter_.Reset(*canvas_);
    osuImportPanel_.ProcessInput(input, canvasPointer_);
    inputRouter_.InvalidateHitTest();
    if (!osuImportPanel_.IsVisible())
        inputRouter_.Reset(*canvas_);
    return true;
}
