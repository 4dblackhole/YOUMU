#include "GameplayPauseMenu.h"
#include "App/AssetPaths.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include "Texts/GameScene/RhythmTestScene/PauseTexts.h"
#include <algorithm>
#include <format>

using namespace PhantomCore;

namespace
{
    // Redesigned pause menu (UI-02), in logical units of the 1280x720 screen. The card
    // is 520 wide: a slanted PAUSED title, then Resume and Rewind with value steppers,
    // a rule, and Retry and Retire with a short description. Rows jut 6.5 to the left.
    constexpr float MenuWidth = 520, MenuHeight = 307, TitleHeight = 30;
    constexpr float RowWidth = 533, RowHeight = 52, RowLeft = -6.5F;
    constexpr std::array<float, 4> RowTops{46, 108, 193, 255};
    constexpr float RuleTop = 176, RuleInset = 12;
    constexpr float CaptionLeft = 30, StepperLeft = 346.5F, StepperWidth = 172, StepperHeight = 36;
    constexpr float PrepTop = 35, PrepHeight = 6, PrepWidth = 497;
    constexpr Color White{1, 1, 1, 1}, Clear{0, 0, 0, 0};

    VisualStyle Transparent()
    {
        VisualStyle style;
        style.normal = style.hovered = style.pressed = style.disabled = Clear;
        return style;
    }
} // namespace

Visual2DNode &GameplayPauseMenu::Label(Visual2DNode &parent, Rect bounds, std::wstring text,
                                          float size, Color color, std::uint16_t weight)
{
    auto font = ui_fonts::Text(text);
    auto &node = CreateLabel(parent, bounds, std::move(text), "Pause.Label");
    auto &component = *node.GetComponent<TextVisualComponent>();
    component.SetFont(std::move(font));
    component.SetFontSize(size);
    component.SetFontWeight(weight);
    component.SetTextColor(color);
    node.SetClipRect({0, 0, bounds.width, bounds.height});
    return node;
}

void GameplayPauseMenu::Build()
{
    auto &canvas = *handle_.Get();
    router_.Reset(canvas);
    if (rootId_)
        static_cast<void>(canvas.RemoveNode(rootId_));
    const auto size = canvas.LogicalSize();
    auto &root = canvas.CreateNode(Anchor::Center, "Pause.Root");
    rootId_ = root.Id();
    // A hatched navy veil instead of blurred glass; an image sprite so it blends over the play screen.
    static_cast<void>(CreateSprite(root, {-size.width / 2, -size.height / 2, size.width, size.height},
        visuals_.RegisterImage(InGame(L"PauseBackdrop")), "Pause.Dim"));
    auto &menu = root.CreateChild("Pause.Panel");
    menu.SetSize({MenuWidth, MenuHeight});
    menu.SetPivot({.5F, .5F});
    const float scale = std::min(1.0F, std::max(size.width - 32, 1.0F) / (RowWidth + 13));
    menu.Transform().SetScale(scale, scale, 1);

    auto &title = CreateLabel(menu, {0, MenuHeight - TitleHeight, MenuWidth, TitleHeight},
                              std::wstring(Pause(texts_).title), "Pause.Title");
    auto &titleText = *title.GetComponent<TextVisualComponent>();
    auto face = ui_fonts::Number(800);
    titleText.SetFont(std::move(face.font));
    titleText.SetFontWeight(face.weight);
    titleText.SetFontSize(30);
    titleText.SetTextColor(White);

    // Thin progress band under the title while Resume/Rewind are still preparing.
    auto &prep = CreateSprite(menu, {0, MenuHeight - PrepTop - PrepHeight, PrepWidth, PrepHeight},
                              visuals_.RegisterImage(InGame(L"Pause.PrepTrack")), "Pause.Preparing");
    prepId_ = prep.Id();
    auto &piece = CreateSprite(prep, {0, 0, PrepWidth * 0.25F, PrepHeight},
                               visuals_.RegisterImage(InGame(L"Pause.PrepFill")), "Pause.PreparingPiece");
    piece.GetComponent<SpriteVisualComponent>()->SetUvTransform({0.25F, 1.0F}, {});
    prepPieceId_ = piece.Id();

    auto &rule = CreatePanel(menu, {RuleInset, MenuHeight - RuleTop - 1, MenuWidth - RuleInset * 2, 1}, "Pause.Rule");
    auto &ruleSprite = *rule.GetComponent<SpriteVisualComponent>();
    ruleSprite.SetTint({1, 1, 1, .5F});
    ruleSprite.SetCornerRadius(0);

    rowImage_ = visuals_.RegisterImage(InGame(L"Pause.Row"));
    rowSelectedImage_ = visuals_.RegisterImage(InGame(L"Pause.Row.Selected"));
    stepperImage_ = visuals_.RegisterImage(InGame(L"Pause.Stepper"));
    stepperSelectedImage_ = visuals_.RegisterImage(InGame(L"Pause.Stepper.Selected"));
    for (std::size_t i = 0; i < rows_.size(); ++i)
        BuildRow(menu, i);
    textRevision_ = texts_.Revision();
    RefreshRows();
    router_.InvalidateHitTest();
}

void GameplayPauseMenu::BuildRow(Visual2DNode &parent, std::size_t index)
{
    const auto &text = Pause(texts_);
    auto &frame = CreateSprite(parent, {RowLeft, MenuHeight - RowTops[index] - RowHeight, RowWidth, RowHeight}, rowImage_,
                               "Pause.Row." + std::to_string(index));
    frames_[index] = frame.Id();
    // The action covers the row up to the stepper; a stepper click only changes its value.
    auto &action = CreateButton(frame, {0, 0, index < 2 ? StepperLeft - 8 : RowWidth, RowHeight}, L"",
                                   std::format("Pause.{}", index));
    action.GetComponent<SpriteVisualComponent>()->SetStyle(Transparent());
    rows_[index] = action.Id();
    captions_[index] = Label(action, {CaptionLeft, 0, 220, RowHeight}, std::wstring(text.actions[index]), 19,
                             ui_colors::theme::Ink, 900).Id();
    if (index < 2)
    {
        BuildSelector(frame, index);
        return;
    }
    // Retry and Retire say what they do; Retire is set apart by the rule and its danger colour.
    auto &description = Label(action, {250, 0, RowWidth - 250 - 14.5F, RowHeight},
                              std::wstring(index == 2 ? text.retryDescription : text.retireDescription), 12,
                              ui_colors::theme::Caption, 400);
    description.GetComponent<TextVisualComponent>()->SetHorizontalAlignment(TextAlignment::Trailing);
    descriptions_[index - 2] = description.Id();
}

void GameplayPauseMenu::BuildSelector(Visual2DNode &parent, std::size_t index)
{
    // The stepper art already contains both chevrons; the buttons are their hit areas.
    auto &selector = CreateSprite(parent, {StepperLeft, (RowHeight - StepperHeight) * 0.5F, StepperWidth, StepperHeight},
                                  stepperImage_, "Pause.Selector");
    selectors_[index] = selector.Id();
    for (const int direction : {-1, 1})
    {
        auto &button = CreateButton(selector, {direction < 0 ? 2.0F : StepperWidth - 34, 2, 32, 32}, L"",
                                       std::format("Pause.{}.{}", index, direction < 0 ? "Previous" : "Next"));
        button.GetComponent<SpriteVisualComponent>()->SetStyle(Transparent());
        (direction < 0 ? previous_ : next_)[index] = button.Id();
    }
    auto &value = CreateLabel(selector, {36, 0, StepperWidth - 72, StepperHeight}, L"", "Pause.Value");
    auto &valueText = *value.GetComponent<TextVisualComponent>();
    auto face = ui_fonts::Number(700, false);
    valueText.SetFont(std::move(face.font));
    valueText.SetFontWeight(face.weight);
    valueText.SetFontSize(13);
    valueText.SetTextColor(ui_colors::theme::Ink);
    valueText.SetHorizontalAlignment(TextAlignment::Center);
    values_[index] = value.Id();
}
