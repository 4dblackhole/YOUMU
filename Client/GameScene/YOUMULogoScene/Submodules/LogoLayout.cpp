#include "App/AssetPaths.h"
#include "LogoStyle.h"
#include "LogoView.h"
#include "Presentation/SkinVisualComponent.h"
#include "Texts/GameScene/YOUMULogoScene/LogoTexts.h"
#include <cmath>
using namespace PhantomCore;

using namespace logo_ui;

namespace
{
    [[nodiscard]] ImageHandle SkinImage(ScreenVisual2DManager &visuals, const char *key)
    {
        const auto path = SkinSetSelection::Instance().Image("Logo", key);
        return path.empty() ? ImageHandle{} : visuals.RegisterImage(path);
    }
} // namespace

void LogoView::CreateSky()
{
    // The board holds the 16:9-or-wider arrangement; it is scaled as a whole on narrower screens.
    board_ = &canvas_->CreateNode(Anchor::Center, "YOUMU.Title");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize(ReferenceSize);
    background_ = &CreatePanel(*board_, {0.0F, 0.0F, ReferenceSize.width, ReferenceSize.height}, "YOUMU.LogoBackground");
    auto &sky = RequireComponent<SpriteVisualComponent>(*background_);
    sky.SetTint(ui_colors::sky::Background);
    sky.SetCornerRadius(0.0F);
    background_->SetZIndex(-1);

    // Only feathers drift across the sky; there are no clouds.
    for (auto &feather : feathers_)
    {
        feather = &CreateSprite(*board_, {0.0F, 0.0F, FeatherSize.width, FeatherSize.height}, {}, "YOUMU.Feather");
        RequireComponent<SpriteVisualComponent>(*feather).SetTint(ui_colors::White);
        feather->SetZIndex(0);
    }

    auto &frame = canvas_->CreateNode(Anchor::Center, "YOUMU.Letterbox");
    frame.SetZIndex(5);
    for (auto &band : letterbox_)
    {
        band = &CreatePanel(frame, {}, "YOUMU.LetterboxBand");
        auto &visual = RequireComponent<SpriteVisualComponent>(*band);
        visual.SetTint(ui_colors::theme::Letterbox);
        visual.SetCornerRadius(0.0F);
        band->SetVisible(false);
    }
}

void LogoView::CreateMenu()
{
    const auto &text = Logo(texts_);
    constexpr std::array names{"YOUMU.GameStart", "YOUMU.Editor", "YOUMU.Option", "YOUMU.Exit"};
    for (std::size_t i = 0; i < menuButtons_.size(); ++i)
    {
        auto &button = CreateButton(*board_, {}, std::wstring(text.menu[i]), names[i]);
        button.SetZIndex(10);
        auto &caption = RequireComponent<TextVisualComponent>(button);
        caption.SetFontSize(20.0F);
        caption.SetFontWeight(800);
        caption.SetHorizontalAlignment(TextAlignment::Center);
        menuButtons_[i] = &button;
        menuButtonIds_[i] = button.Id();
    }
    // The gold feather sits inside the chosen item, at its left.
    selectionCursor_ = &CreateSprite(*board_, {0.0F, 0.0F, CursorSize.width, CursorSize.height}, {},
                                     "YOUMU.SelectionCursor");
    RequireComponent<SpriteVisualComponent>(*selectionCursor_).SetTint(ui_colors::White);
    selectionCursor_->SetZIndex(11);
    SetSelectedMenuItem(GameStartIndex);
}

void LogoView::RefreshSkinImages()
{
    const auto revision = SkinSetSelection::Instance().Revision();
    if (revision == skinRevision_ || background_ == nullptr || selectionCursor_ == nullptr)
        return;
    auto &sky = RequireComponent<SpriteVisualComponent>(*background_);
    const auto background = SkinImage(screenVisuals_, "Background");
    sky.SetImage(background);
    // An authored sky keeps its own colours; without one the flat sky colour remains.
    sky.SetTint(background ? ui_colors::White : ui_colors::sky::Background);
    const auto feather = SkinImage(screenVisuals_, "Sky.Feather");
    for (auto *node : feathers_)
    {
        RequireComponent<SpriteVisualComponent>(*node).SetImage(feather);
        node->SetVisible(static_cast<bool>(feather));
    }
    RequireComponent<SpriteVisualComponent>(*selectionCursor_).SetImage(SkinImage(screenVisuals_, "SelectionCursor"));
    for (auto *button : menuButtons_)
        button->GetComponent<SkinVisualComponent>()->Update(0);
    skinRevision_ = revision;
}

void LogoView::ApplyTexts()
{
    const auto &text = Logo(texts_);
    const auto hint = [this](const Visual2DNode *node, TooltipText value) {
        if (node != nullptr)
            tooltip_.Set(*node, std::move(value));
    };
    hint(player_.PlayButton(), {std::wstring(texts_.Get("Tooltips.playPause")), L"Space"});
    hint(player_.StopButton(), {std::wstring(texts_.Get("Tooltips.stop"))});
    hint(player_.NextButton(), {std::wstring(texts_.Get("Tooltips.nextSong")), L"→"});
    for (std::size_t index = 0; index < menuButtons_.size(); ++index)
    {
        auto &component =
            RequireComponent<TextVisualComponent>(*menuButtons_[index]);
        component.SetText(std::wstring(text.menu[index]));
        component.SetFont(ui_fonts::Text(text.menu[index]));
    }
    options_.RefreshTexts();
    textRevision_ = texts_.Revision();
}

void LogoView::UpdateLayout()
{
    if (canvas_ == nullptr || board_ == nullptr || background_ == nullptr)
    {
        return;
    }
    const Size logical = canvas_->LogicalSize();
    const TitleFrame frame = CalculateFrame(logical.width);
    board_->SetSize({frame.width, ReferenceSize.height});
    board_->SetPosition({0.0F, 0.0F});
    board_->Transform().SetScale(frame.scale, frame.scale, 1.0F);
    background_->SetBounds({0.0F, 0.0F, frame.width, ReferenceSize.height});
    LayoutMenu();
    player_.Layout(board_->NodeSize());

    const float contentWidth = frame.width * frame.scale, contentHeight = ReferenceSize.height * frame.scale;
    const float vertical = (logical.height - contentHeight) * 0.5F, horizontal = (logical.width - contentWidth) * 0.5F;
    const std::array<Rect, 4> bands{Rect{-logical.width * 0.5F, contentHeight * 0.5F, logical.width, vertical},
                                    Rect{-logical.width * 0.5F, -logical.height * 0.5F, logical.width, vertical},
                                    Rect{-logical.width * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight},
                                    Rect{contentWidth * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight}};
    for (std::size_t i = 0; i < bands.size(); ++i)
    {
        const bool visible = bands[i].width > 0.01F && bands[i].height > 0.01F;
        letterbox_[i]->SetVisible(visible);
        if (visible)
            letterbox_[i]->SetBounds(bands[i]);
    }
}

void LogoView::LayoutMenu()
{
    if (board_ == nullptr || selectionCursor_ == nullptr)
        return;
    const Size board = board_->NodeSize();
    const float left = (board.width - MenuButton.width) * 0.5F + MenuCenterOffsetX - SelectedShift * 0.5F;
    for (std::size_t i = 0; i < menuButtons_.size(); ++i)
    {
        if (menuButtons_[i] == nullptr)
            continue;
        const bool selected = i == selectedMenuIndex_;
        const Rect bounds{left + (selected ? SelectedShift : 0.0F), MenuTop + MenuPitch * static_cast<float>(i),
                          MenuButton.width, MenuButton.height};
        menuButtons_[i]->SetBounds(TopLeft(bounds, board.height));
        if (selected)
            selectionCursor_->SetBounds(TopLeft({bounds.x + CursorLeft, bounds.y + (bounds.height - CursorSize.height) * 0.5F,
                                                 CursorSize.width, CursorSize.height},
                                                board.height));
    }
}

void LogoView::AnimateFeathers(const double deltaSeconds)
{
    if (board_ == nullptr)
        return;
    featherSeconds_ += deltaSeconds;
    const Size board = board_->NodeSize();
    for (std::size_t i = 0; i < feathers_.size(); ++i)
    {
        if (feathers_[i] == nullptr)
            continue;
        // Each feather falls slowly on its own column and sways sideways; positions follow
        // from elapsed time alone, so a resize or a long frame never makes them jump.
        const auto index = static_cast<float>(i);
        const float column = (index + 0.5F) / static_cast<float>(feathers_.size());
        const float speed = 9.0F + 2.5F * static_cast<float>(i % 3);
        const float travel = board.height + FeatherSize.height * 2.0F;
        const float fallen = std::fmod(static_cast<float>(featherSeconds_) * speed + index * 137.0F, travel);
        const float sway = 26.0F * std::sin(static_cast<float>(featherSeconds_) * (0.35F + 0.05F * index) + index * 1.7F);
        feathers_[i]->SetPosition({board.width * column + sway, board.height + FeatherSize.height - fallen});
    }
}
