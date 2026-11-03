#include "SongFilterPanel.h"
#include "SongSelectionText.h"
#include "Presentation/UiColors.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace
{
    template<class T> T &Component(Visual2DNode &node)
    {
        auto *component = node.GetComponent<T>();
        if (!component) throw std::logic_error("Song filter widget component missing.");
        return *component;
    }
    void SetText(Visual2DNode &node, std::wstring_view text)
    {
        Component<TextVisualComponent>(node).SetText(std::wstring{text});
    }
    void Fill(Visual2DNode &node, Color color)
    {
        auto &sprite = Component<SpriteVisualComponent>(node);
        auto style = sprite.Style();
        style.normal = style.hovered = style.pressed = color;
        sprite.SetStyle(style);
        sprite.SetCornerRadius(6);
    }
}

Visual2DNode &SongFilterPanel::Label(Visual2DNode &parent, std::string name, float size)
{
    auto &node = CreateLabel(parent, {}, L"", std::move(name));
    auto &text = Component<TextVisualComponent>(node);
    text.SetFontSize(size);
    text.SetTextColor(ui_colors::sky::Ink);
    texts_->ApplyFont(text);
    return node;
}

void SongFilterPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window)
{
    canvas_ = &canvas;
    texts_ = &texts;
    overlay_ = &CreatePanel(canvas.Root(), {}, "SongFilter.Overlay");
    overlay_->SetZIndex(1000);
    Fill(*overlay_, {0, 0, 0, .3F});
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &CreatePanel(*overlay_, {}, "SongFilter.Panel");
    Fill(*panel_, ui_colors::sky::Surface);
    title_ = &Label(*panel_, "SongFilter.Title", 24);
    modeLabel_ = &Label(*panel_, "SongFilter.ModeLabel");
    mode_ = &CreateComboBox(*panel_, {}, {}, "SongFilter.Mode");
    Fill(*mode_, ui_colors::sky::Pale);
    mode_->SetZIndex(20);
    auto &combo = Component<ComboBoxBehaviorComponent>(*mode_);
    combo.SetMaxVisibleItems(5);
    combo.SetItemHeight(30);
    combo.SetFontSize(16);
    combo.SetTextColor(ui_colors::sky::Ink);
    combo.SetSelectedTextColor(ui_colors::sky::Ink);
    combo.SetPopupBackgroundColor(ui_colors::White);
    minimum_ = &Label(*panel_, "SongFilter.Minimum", 14);
    maximum_ = &Label(*panel_, "SongFilter.Maximum", 14);
    for (std::size_t row = 0; row < labels_.size(); ++row)
        labels_[row] = &Label(*panel_, "SongFilter.Label." + std::to_string(row));
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        fields_[i] = &CreatePanel(*panel_, {}, "SongFilter.Field." + std::to_string(i));
        Fill(*fields_[i], ui_colors::White);
        auto &text = fields_[i]->AddComponent<TextVisualComponent>();
        text.SetFontSize(16);
        text.SetTextColor(ui_colors::sky::Ink);
        texts_->ApplyFont(text);
        if (window)
        {
            inputs_[i] = window->CreateTextInput();
            if (!inputs_[i]) throw std::runtime_error("Cannot create song filter input.");
            inputs_[i]->SetColors(0x285374, 0xFFFFFF);
        }
    }
    apply_ = &CreateButton(*panel_, {}, L"", "SongFilter.Apply");
    reset_ = &CreateButton(*panel_, {}, L"", "SongFilter.Reset");
    close_ = &CreateButton(*panel_, {}, L"", "SongFilter.Close");
    for (auto *button : {apply_, reset_, close_})
    {
        Fill(*button, button == apply_ ? ui_colors::sky::Accent : ui_colors::sky::Pale);
        Component<TextVisualComponent>(*button).SetTextColor(button == apply_ ? ui_colors::White : ui_colors::sky::Ink);
    }
    status_ = &Label(*panel_, "SongFilter.Status", 14);
    overlay_->SetVisible(false);
    RefreshTexts();
    Layout();
}

void SongFilterPanel::Open(const SongFilter &filter, const SongCatalogLoadResult &catalog)
{
    draft_ = FormatSongFilter(filter);
    error_ = SongFilterError::None;
    applied_.reset();
    difficultyAvailable_ = false;
    std::set<std::string> modes;
    if (!filter.mode.empty()) modes.insert(filter.mode);
    for (const auto &song : catalog.songs)
        for (const auto &entry : song.patterns)
        {
            if (!entry.pattern.mode.empty()) modes.insert(entry.pattern.mode);
            if (entry.calculatedDifficulty && std::isfinite(*entry.calculatedDifficulty)) difficultyAvailable_ = true;
        }
    modes_ = {""};
    modes_.insert(modes_.end(), modes.begin(), modes.end());
    overlay_->SetVisible(true);
    RefreshTexts();
    RefreshValues();
    Layout();
}

void SongFilterPanel::Close() noexcept
{
    for (auto &input : inputs_) if (input) input->SetVisible(false);
    if (overlay_)
    {
        Component<ComboBoxBehaviorComponent>(*mode_).Collapse();
        overlay_->SetVisible(false);
    }
}

void SongFilterPanel::Shutdown() noexcept
{
    Close();
    for (auto &input : inputs_) input.reset();
    canvas_ = nullptr;
    texts_ = nullptr;
    overlay_ = panel_ = nullptr;
}

void SongFilterPanel::SetViewport(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_) Layout();
}

void SongFilterPanel::Layout()
{
    if (!panel_ || height_ == 0) return;
    const auto logical = canvas_->LogicalSize();
    const float width = std::max(240.0F, std::min(600.0F, logical.width - 24));
    constexpr float height = 430;
    overlay_->SetBounds({-logical.width / 2, -logical.height / 2, logical.width, logical.height});
    panel_->SetBounds({(logical.width - width) / 2, (logical.height - height) / 2, width, height});
    const float labelWidth = (width - 48) * .32F;
    const float minimumX = 24 + labelWidth + 16;
    const float fieldWidth = (width - 48 - labelWidth - 32) / 2;
    const float maximumX = minimumX + fieldWidth + 16;
    title_->SetBounds({24, 378, width - 140, 30});
    close_->SetBounds({width - 104, 378, 80, 30});
    modeLabel_->SetBounds({24, 326, labelWidth, 32});
    mode_->SetBounds({minimumX, 326, width - minimumX - 24, 32});
    minimum_->SetBounds({minimumX, 290, fieldWidth, 24});
    maximum_->SetBounds({maximumX, 290, fieldWidth, 24});
    for (std::size_t row = 0; row < labels_.size(); ++row)
    {
        const float y = 246 - 48 * static_cast<float>(row);
        labels_[row]->SetBounds({24, y, labelWidth, 32});
        fields_[row * 2]->SetBounds({minimumX, y, fieldWidth, 32});
        fields_[row * 2 + 1]->SetBounds({maximumX, y, fieldWidth, 32});
        for (const auto index : {row * 2, row * 2 + 1})
            Component<TextVisualComponent>(*fields_[index]).SetContentBounds({6, 0, std::max(fieldWidth - 12, 1.0F), 32});
    }
    status_->SetBounds({24, 58, width - 48, 32});
    reset_->SetBounds({24, 16, 110, 32});
    apply_->SetBounds({width - 144, 16, 120, 32});
    const float scale = static_cast<float>(height_) / logical.height;
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        if (!inputs_[i]) continue;
        const auto bounds = fields_[i]->BoundsInCanvas();
        const auto font = static_cast<std::uint32_t>(std::max(1.0F, std::round(16 * scale)));
        const float editHeight = std::min(static_cast<float>(font) + 4, 26 * scale);
        inputs_[i]->SetBounds({static_cast<std::int32_t>(std::round((bounds.x + logical.width / 2 + 6) * scale)),
            static_cast<std::int32_t>(std::round((logical.height / 2 - bounds.y - bounds.height / 2) * scale - editHeight / 2)),
            static_cast<std::uint32_t>(std::max(1.0F, std::round((bounds.width - 12) * scale))),
            static_cast<std::uint32_t>(std::max(1.0F, std::round(editHeight)))});
        const auto &profile = texts_->CurrentProfile().font;
        inputs_[i]->SetFont(profile.source == TextFontSource::System ? std::wstring_view(profile.value) : L"Segoe UI", font);
    }
    UpdateInputVisibility();
}

void SongFilterPanel::UpdateInputVisibility()
{
    const bool visible = IsVisible() && !Component<ComboBoxBehaviorComponent>(*mode_).IsExpanded();
    for (std::size_t i = 0; i < inputs_.size(); ++i)
        if (inputs_[i]) inputs_[i]->SetVisible(visible && (i < 6 || difficultyAvailable_));
}

void SongFilterPanel::RefreshTexts()
{
    if (!panel_) return;
    for (const auto &[node, key] : std::array<std::pair<Visual2DNode *, const char *>, 8>{{
        {title_, "SongFilter.title"}, {modeLabel_, "SongFilter.mode"}, {minimum_, "SongFilter.minimum"},
        {maximum_, "SongFilter.maximum"}, {apply_, "SongFilter.apply"}, {reset_, "SongFilter.reset"},
        {close_, "SongFilter.close"}, {labels_[0], "SongFilter.notes"}}})
    {
        SetText(*node, texts_->Get(key));
        texts_->ApplyFont(Component<TextVisualComponent>(*node));
    }
    SetText(*labels_[1], texts_->Get("SongFilter.bpm"));
    SetText(*labels_[2], L"JudgeLevel");
    SetText(*labels_[3], texts_->Get(difficultyAvailable_ ? "SongFilter.difficulty" : "SongFilter.difficultyPending"));
    for (auto *label : labels_) texts_->ApplyFont(Component<TextVisualComponent>(*label));
    texts_->ApplyFont(Component<TextVisualComponent>(*status_));
    std::vector<std::wstring> items;
    items.push_back(std::wstring{texts_->Get("SongFilter.allModes")});
    for (std::size_t i = 1; i < modes_.size(); ++i) items.push_back(song_select::DecodeDisplayText(modes_[i]));
    auto &combo = Component<ComboBoxBehaviorComponent>(*mode_);
    combo.SetItems(std::move(items));
    const auto selected = std::ranges::find(modes_, draft_.mode);
    combo.SetSelectedIndex(selected == modes_.end() ? 0 : static_cast<std::size_t>(selected - modes_.begin()));
    texts_->ApplyFont(combo);
    const char *key = "SongFilter.hint";
    switch (error_)
    {
    case SongFilterError::Notes: key = "SongFilter.invalidNotes"; break;
    case SongFilterError::Bpm: key = "SongFilter.invalidBpm"; break;
    case SongFilterError::JudgeLevel: key = "SongFilter.invalidJudgeLevel"; break;
    case SongFilterError::Difficulty: key = "SongFilter.invalidDifficulty"; break;
    case SongFilterError::ReversedRange: key = "SongFilter.reversedRange"; break;
    default: break;
    }
    SetText(*status_, texts_->Get(key));
    Component<TextVisualComponent>(*status_).SetTextColor(error_ == SongFilterError::None ? ui_colors::sky::Muted : ui_colors::Error);
}

void SongFilterPanel::RefreshValues()
{
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        Fill(*fields_[i], i >= 6 && !difficultyAvailable_ ? ui_colors::sky::Pale : ui_colors::White);
        SetText(*fields_[i], inputs_[i] ? L"" : std::wstring_view(draft_.values[i]));
        if (inputs_[i]) inputs_[i]->SetText(draft_.values[i]);
    }
}

void SongFilterPanel::SynchronizeText()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (inputs_[i] && inputs_[i]->TakeText(text)) draft_.values[i] = std::move(text);
    }
}

void SongFilterPanel::TryApply()
{
    for (auto &input : inputs_) if (input) input->Blur();
    SynchronizeText();
    SongFilter filter;
    error_ = ParseSongFilter(draft_, filter);
    if (error_ == SongFilterError::None) { applied_ = std::move(filter); Close(); }
    else RefreshTexts();
}

bool SongFilterPanel::ProcessAction(const Action &action)
{
    if (!IsVisible()) return false;
    if (action.type == ActionType::SelectionChanged && action.source == mode_->Id())
    {
        if (action.selectedIndex < modes_.size()) draft_.mode = modes_[action.selectedIndex];
        return true;
    }
    if (action.type != ActionType::Clicked) return false;
    if (action.source == apply_->Id()) { TryApply(); return true; }
    if (action.source == close_->Id()) { Close(); return true; }
    if (action.source == reset_->Id())
    {
        for (auto &input : inputs_) if (input) input->Blur();
        draft_ = {};
        error_ = SongFilterError::None;
        RefreshValues();
        RefreshTexts();
        return true;
    }
    return false;
}

void SongFilterPanel::ProcessInput(const InputState &input)
{
    if (!IsVisible()) return;
    UpdateInputVisibility();
    SynchronizeText();
    bool focus = false, submit = false, cancel = false;
    for (auto &field : inputs_) if (field)
    {
        focus = focus || field->HasFocus();
        submit = field->TakeSubmit() || submit;
        cancel = field->TakeCancel() || cancel;
    }
    if (cancel || (!focus && input.WasKeyPressed(VK_ESCAPE))) Close();
    else if (submit || (!focus && input.WasKeyPressed(VK_RETURN))) TryApply();
}

std::optional<SongFilter> SongFilterPanel::TakeApplied()
{
    return std::exchange(applied_, std::nullopt);
}
