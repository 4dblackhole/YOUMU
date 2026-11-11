#include "RecordFilterPanel.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Presentation/ValueDialog.h"
#include "Records/PlayRecordStore.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace PhantomCore;

using namespace song_select;
namespace
{
    constexpr std::array flagKeys{"PlayOptions.mirror", "PlayOptions.random", "PlayOptions.metronome", "PlayOptions.ignoreScroll",
        "PlayOptions.musicPitch", "PlayOptions.hitPitch", "PlayOptions.overrideJudge", "PlayOptions.scrollUnit"};
    void Text(Visual2DNode &node, std::wstring_view text)
    { RequireComponent<TextVisualComponent>(node).SetText(std::wstring{text}); }
    std::string ConditionAscii(std::wstring_view text)
    {
        std::string value;
        for (const auto ch : text)
        {
            if (ch > 127) throw std::invalid_argument("Query field names and operators use ASCII.");
            value.push_back(static_cast<char>(ch));
        }
        return value;
    }
}
Visual2DNode &RecordFilterPanel::Label(Visual2DNode &parent, std::string name, float size)
{
    auto &node = CreateLabel(parent, {}, L"", std::move(name));
    auto &text = RequireComponent<TextVisualComponent>(node);
    text.SetFontSize(size);
    text.SetTextColor(DeepBlue);
    texts_->ApplyFont(text);
    return node;
}
void RecordFilterPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window)
{
    canvas_ = &canvas;
    texts_ = &texts;
    overlay_ = &CreatePanel(canvas.Root(), {}, "RecordFilter.Overlay");
    overlay_->SetZIndex(1000);
    ApplyFlatStyle(*overlay_, {0, 0, 0, .3F});
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &CreatePanel(*overlay_, {}, "RecordFilter.Panel");
    ApplySpriteStyle(*panel_, PanelWhite, PanelWhite);
    title_ = &Label(*panel_, "RecordFilter.Title", 24);
    for (std::size_t i = 0; i < choices_.size(); ++i)
    {
        choiceLabels_[i] = &Label(*panel_, "RecordFilter.Label." + std::to_string(i), 15);
        choices_[i] = &CreateComboBox(*panel_, {}, {}, "RecordFilter.Choice." + std::to_string(i));
        choices_[i]->SetZIndex(20);
        ApplySpriteStyle(*choices_[i], PaleBlue, FocusBlue);
        auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*choices_[i]);
        combo.SetFontSize(16); combo.SetItemHeight(30); combo.SetMaxVisibleItems(3);
        combo.SetTextColor(DeepBlue); combo.SetSelectedTextColor(DeepBlue); combo.SetPopupBackgroundColor(PureWhite);
    }
    for (std::size_t i = 0; i < rangeLabels_.size(); ++i)
        rangeLabels_[i] = &Label(*panel_, "RecordFilter.RangeLabel." + std::to_string(i));
    minimum_ = &Label(*panel_, "RecordFilter.Minimum", 14);
    maximum_ = &Label(*panel_, "RecordFilter.Maximum", 14);
    seedLabel_ = &Label(*panel_, "RecordFilter.SeedLabel");
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        fields_[i] = &CreatePanel(*panel_, {}, "RecordFilter.Field." + std::to_string(i));
        ApplySpriteStyle(*fields_[i], PureWhite, PureWhite);
        auto &text = fields_[i]->AddComponent<TextVisualComponent>();
        text.SetFontSize(16); text.SetTextColor(DeepBlue); texts_->ApplyFont(text);
        if (window)
        {
            inputs_[i] = window->CreateTextInput();
            if (!inputs_[i]) throw std::runtime_error("Cannot create record filter input.");
            inputs_[i]->SetColors(0x285374, 0xFFFFFF);
        }
    }
    apply_ = &CreateButton(*panel_, {}, L"", "RecordFilter.Apply");
    close_ = &CreateButton(*panel_, {}, L"", "RecordFilter.Close");
    condition_ = &CreateButton(*panel_, {}, L"", "RecordFilter.Condition");
    for (auto *button : {apply_, close_, condition_})
    {
        ApplyButtonStyle(*button, button == apply_ ? AccentBlue : PaleBlue, FocusBlue, button == apply_ ? PureWhite : DeepBlue);
        RequireComponent<TextVisualComponent>(*button).SetFontSize(16);
    }
    conditionText_ = &Label(*panel_, "RecordFilter.ConditionText", 14);
    status_ = &Label(*panel_, "RecordFilter.Status", 14);
    overlay_->SetVisible(false);
    RefreshTexts();
    Layout();
}
void RecordFilterPanel::Open(const RecordFilter &filter)
{
    draft_ = FormatRecordFilter(filter);
    error_.clear(); applied_.reset();
    discardKeys_ = waitRelease_ = false;
    overlay_->SetVisible(true);
    RefreshTexts(); RefreshValues(); Layout();
}
void RecordFilterPanel::Close() noexcept
{
    for (auto &input : inputs_) if (input) input->SetVisible(false);
    if (!overlay_) return;
    for (auto *choice : choices_) RequireComponent<ComboBoxBehaviorComponent>(*choice).Collapse();
    overlay_->SetVisible(false);
}
void RecordFilterPanel::Shutdown() noexcept
{
    Close();
    for (auto &input : inputs_) input.reset();
    overlay_ = panel_ = nullptr;
    canvas_ = nullptr; texts_ = nullptr;
}
void RecordFilterPanel::SetViewport(std::uint32_t height) { height_ = height; if (panel_) Layout(); }
void RecordFilterPanel::Layout()
{
    if (!height_) return;
    const auto viewport = canvas_->LogicalSize();
    const float width = std::max(320.0F, std::min(860.0F, viewport.width - 24));
    constexpr float height = 650;
    overlay_->SetBounds({-viewport.width / 2, -viewport.height / 2, viewport.width, viewport.height});
    panel_->SetBounds({(viewport.width - width) / 2, (viewport.height - height) / 2, width, height});
    title_->SetBounds({24, 596, width - 150, 30});
    close_->SetBounds({width - 104, 596, 80, 30});
    const float half = (width - 64) / 2;
    for (std::size_t i = 0; i < choices_.size(); ++i)
    {
        const float x = 24 + static_cast<float>(i % 2) * (half + 16);
        const float y = 546 - static_cast<float>(i / 2) * 38;
        choiceLabels_[i]->SetBounds({x, y, half * .55F, 30});
        choices_[i]->SetBounds({x + half * .55F, y, half * .45F, 30});
    }
    const float labelWidth = (width - 48) * .32F;
    const float x = 24 + labelWidth + 12;
    const float fieldWidth = (width - 48 - labelWidth - 24) / 2;
    minimum_->SetBounds({x, 392, fieldWidth, 22});
    maximum_->SetBounds({x + fieldWidth + 12, 392, fieldWidth, 22});
    for (std::size_t row = 0; row < 4; ++row)
    {
        const float y = 352 - static_cast<float>(row) * 42;
        rangeLabels_[row]->SetBounds({24, y, labelWidth, 30});
        fields_[row * 2]->SetBounds({x, y, fieldWidth, 30});
        fields_[row * 2 + 1]->SetBounds({x + fieldWidth + 12, y, fieldWidth, 30});
    }
    seedLabel_->SetBounds({24, 180, labelWidth, 30});
    fields_[8]->SetBounds({x, 180, width - x - 24, 30});
    condition_->SetBounds({24, 134, width - 48, 30});
    conditionText_->SetBounds({24, 96, width - 48, 30});
    conditionText_->SetClipRect({0, 0, width - 48, 30});
    status_->SetBounds({24, 48, width - 48, 38});
    apply_->SetBounds({width - 144, 8, 120, 32});
    const float scale = static_cast<float>(height_) / viewport.height;
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        const auto size = fields_[i]->NodeSize();
        RequireComponent<TextVisualComponent>(*fields_[i]).SetContentBounds({6, 0, std::max(size.width - 12, 1.0F), size.height});
        if (!inputs_[i]) continue;
        const auto bounds = fields_[i]->BoundsInCanvas();
        const auto font = static_cast<std::uint32_t>(std::max(1.0F, std::round(16 * scale)));
        const float editHeight = std::min(static_cast<float>(font) + 4, 26 * scale);
        inputs_[i]->SetBounds({static_cast<std::int32_t>(std::round((bounds.x + viewport.width / 2 + 6) * scale)),
            static_cast<std::int32_t>(std::round((viewport.height / 2 - bounds.y - bounds.height / 2) * scale - editHeight / 2)),
            static_cast<std::uint32_t>(std::max(1.0F, std::round((bounds.width - 12) * scale))),
            static_cast<std::uint32_t>(std::max(1.0F, std::round(editHeight)))});
        const auto &profile = texts_->CurrentProfile().font;
        inputs_[i]->SetFont(profile.source == TextFontSource::System ? std::wstring_view(profile.value) : L"Segoe UI", font);
    }
    UpdateInputVisibility();
}
void RecordFilterPanel::UpdateInputVisibility()
{
    const bool expanded = std::ranges::any_of(choices_, [](auto *choice)
        { return RequireComponent<ComboBoxBehaviorComponent>(*choice).IsExpanded(); });
    for (auto &input : inputs_) if (input) input->SetVisible(IsVisible() && !expanded);
}
void RecordFilterPanel::RefreshTexts()
{
    if (!panel_) return;
    for (const auto &[node, key] : std::array<std::pair<Visual2DNode *, const char *>, 8>{{
        {title_, "RecordFilter.title"}, {apply_, "RecordFilter.apply"}, {close_, "RecordFilter.close"},
        {condition_, "RecordFilter.condition"}, {minimum_, "SongFilter.minimum"}, {maximum_, "SongFilter.maximum"},
        {seedLabel_, "PlayOptions.seed"}, {rangeLabels_[0], "PlayOptions.judgeLevel"}}})
    { Text(*node, texts_->Get(key)); texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node)); }
    constexpr std::array rangeKeys{"PlayOptions.judgeLevel", "RecordFilter.accelerate", "RecordFilter.scrollSpeed", "PlayOptions.probability"};
    for (std::size_t i = 0; i < rangeLabels_.size(); ++i)
    { Text(*rangeLabels_[i], texts_->Get(rangeKeys[i])); texts_->ApplyFont(RequireComponent<TextVisualComponent>(*rangeLabels_[i])); }
    for (std::size_t i = 0; i < choices_.size(); ++i)
    {
        Text(*choiceLabels_[i], texts_->Get(flagKeys[i]));
        texts_->ApplyFont(RequireComponent<TextVisualComponent>(*choiceLabels_[i]));
        auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*choices_[i]);
        if (i == RecordFilterFlagCount) combo.SetItems({std::wstring{texts_->Get("RecordFilter.any")}, L"x", L"BPM"});
        else combo.SetItems({std::wstring{texts_->Get("RecordFilter.any")}, std::wstring{texts_->Get("RecordFilter.on")}, std::wstring{texts_->Get("RecordFilter.off")}});
        combo.SetSelectedIndex(draft_.choices[i]); texts_->ApplyFont(combo);
    }
    Text(*conditionText_, draft_.condition.empty() ? texts_->Get("RecordFilter.noCondition") : std::wstring_view{draft_.condition});
    Text(*status_, error_.empty() ? std::wstring{texts_->Get("RecordFilter.hint")} :
        std::wstring{texts_->Get("RecordFilter.error")} + DecodeDisplayText(error_));
    for (auto *node : {conditionText_, status_}) texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node));
    RequireComponent<TextVisualComponent>(*status_).SetTextColor(error_.empty() ? MutedBlue : ui_colors::Error);
}
void RecordFilterPanel::RefreshValues()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        Text(*fields_[i], inputs_[i] ? L"" : std::wstring_view{draft_.values[i]});
        if (inputs_[i]) inputs_[i]->SetText(draft_.values[i]);
    }
}
void RecordFilterPanel::SynchronizeText()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (inputs_[i] && inputs_[i]->TakeText(text)) draft_.values[i] = std::move(text);
    }
}
void RecordFilterPanel::TryApply()
{
    for (auto &input : inputs_) if (input) input->Blur();
    SynchronizeText();
    try { applied_ = ParseRecordFilter(draft_); Close(); }
    catch (const std::exception &exception) { error_ = exception.what(); RefreshTexts(); }
}
void RecordFilterPanel::EditCondition()
{
    for (auto &input : inputs_) if (input) input->Blur();
    SynchronizeText();
    std::vector<DialogValue> fields{{std::wstring{texts_->Get("Records.condition")}, draft_.condition}};
    const bool accepted = ShowValueDialog(std::wstring{texts_->Get("RecordFilter.condition")}, fields,
        std::wstring{texts_->Get("PlayOptions.ok")}, std::wstring{texts_->Get("PlayOptions.cancel")},
        [&] { PlayRecordStore::ValidateQuery(ConditionAscii(fields[0].value)); });
    discardKeys_ = waitRelease_ = true;
    if (accepted) draft_.condition = std::move(fields[0].value);
    RefreshTexts();
}
bool RecordFilterPanel::ProcessAction(const Action &action)
{
    if (!IsVisible()) return false;
    if (action.type == ActionType::SelectionChanged)
        for (std::size_t i = 0; i < choices_.size(); ++i) if (action.source == choices_[i]->Id())
        { draft_.choices[i] = action.selectedIndex; return true; }
    if (action.type != ActionType::Clicked) return false;
    if (action.source == close_->Id()) { Close(); return true; }
    if (action.source == apply_->Id()) { TryApply(); return true; }
    if (action.source == condition_->Id()) { EditCondition(); return true; }
    return false;
}
void RecordFilterPanel::ProcessInput(const InputState &input)
{
    if (!IsVisible()) return;
    UpdateInputVisibility(); SynchronizeText();
    bool focus = false, submit = false, cancel = false;
    for (auto &field : inputs_) if (field)
    {
        focus = field->HasFocus() || focus; submit = field->TakeSubmit() || submit; cancel = field->TakeCancel() || cancel;
    }
    if (discardKeys_) { discardKeys_ = false; return; }
    if (waitRelease_)
    {
        if (!input.IsKeyDown(VK_RETURN) && !input.IsKeyDown(VK_ESCAPE) && !input.WasKeyPressed(VK_RETURN) && !input.WasKeyPressed(VK_ESCAPE)) waitRelease_ = false;
        return;
    }
    if (cancel || (!focus && input.WasKeyPressed(VK_ESCAPE))) Close();
    else if (submit || (!focus && input.WasKeyPressed(VK_RETURN))) TryApply();
}
std::optional<RecordFilter> RecordFilterPanel::TakeApplied() { return std::exchange(applied_, std::nullopt); }
