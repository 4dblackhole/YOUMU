#include "SongFilterPanel.h"
#include "SongSelectionText.h"
#include "Presentation/UiDropdown.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

using namespace PhantomCore;
using namespace ui_modal;

namespace
{
    // Body geometry (popups.css Song filters, a switch added before each row name).
    constexpr float Width = 560, RowTop = 80, RowPitch = 48, RowHeight = 40, SwitchLeft = 0, LabelLeft = 52;
    constexpr float MinimumLeft = 156, MinimumWidth = 164, MaximumLeft = 344, MaximumWidth = 168, HeadTop = 56;
    constexpr float BodyHeight = RowTop + RowPitch * static_cast<float>(SongFilterRowCount - 1) + RowHeight;
    constexpr std::array RowKeys{"SongFilter.notes", "SongFilter.bpm", "SongFilter.judgeLevel", "SongFilter.difficulty",
                                 "SongFilter.hp", "SongFilter.length", "SongFilter.tag"};
    static_assert(RowKeys.size() == SongFilterRowCount);
    constexpr auto FieldFill = ui_colors::Rgb(0xFFFFFF, .74F);

    [[nodiscard]] float RowY(const std::size_t row) noexcept
    {
        return RowTop + RowPitch * static_cast<float>(row);
    }
    // Lower bounds of JudgeLevel and HP show their legal range as placeholders.
    [[nodiscard]] std::wstring_view Placeholder(const std::size_t field) noexcept
    {
        const auto row = static_cast<SongFilterRow>(field / 2);
        const bool levelRow = row == SongFilterRow::JudgeLevel || row == SongFilterRow::Hp;
        if (!levelRow)
            return {};
        return field % 2 == 0 ? L"1" : L"100";
    }
    [[nodiscard]] const char *ErrorKey(const SongFilterError error) noexcept
    {
        switch (error)
        {
        case SongFilterError::Notes: return "SongFilter.invalidNotes";
        case SongFilterError::Bpm: return "SongFilter.invalidBpm";
        case SongFilterError::JudgeLevel: return "SongFilter.invalidJudgeLevel";
        case SongFilterError::Difficulty: return "SongFilter.invalidDifficulty";
        case SongFilterError::Hp: return "SongFilter.invalidHp";
        case SongFilterError::Length: return "SongFilter.invalidLength";
        case SongFilterError::ReversedRange: return "SongFilter.reversedRange";
        default: return nullptr;
        }
    }
} // namespace

void SongFilterPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window,
                                 ScreenVisual2DManager *visuals)
{
    canvas_ = &canvas;
    texts_ = &texts;
    modal_.Create(canvas, {"SongFilter", Width, true, true, visuals});
    auto &body = modal_.Body();
    modeLabel_ = &CreateText(body, "SongFilter.ModeLabel", 13, 600, Ink);
    mode_ = &CreateDropdown(body, {}, {}, "SongFilter.Mode");
    mode_->SetZIndex(20);
    VisualStyle field{};
    field.normal = field.disabled = FieldFill;
    field.hovered = field.pressed = ui_colors::White;
    mode_->GetComponent<SpriteVisualComponent>()->SetStyle(field);
    RequireDropdown(*mode_).SetValueStyle(14, 600, Ink);
    minimum_ = &CreateText(body, "SongFilter.Minimum", 13, 700, Caption);
    maximum_ = &CreateText(body, "SongFilter.Maximum", 13, 700, Caption);
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
    {
        switches_[row] = &CreateSwitch(body, "SongFilter.Switch." + std::to_string(row));
        labels_[row] = &CreateText(body, "SongFilter.Label." + std::to_string(row), 13, 600, Ink);
        if (row < SongFilterRangeCount)
        {
            dashes_[row] = &CreateText(body, "SongFilter.Dash." + std::to_string(row), 14, 400, Caption);
            SetText(*dashes_[row], L"–");
            dashes_[row]->GetComponent<TextVisualComponent>()->SetHorizontalAlignment(TextAlignment::Center);
        }
    }
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        fields_[i] = &CreateField(body, "SongFilter.Field." + std::to_string(i), IsNumberField(i));
        if (!window)
            continue;
        inputs_[i] = window->CreateTextInput();
        if (!inputs_[i])
            throw std::runtime_error("Cannot create song filter input.");
        inputs_[i]->SetColors(0x16294A, 0xFFFFFF);
        inputs_[i]->SetPlaceholder(Placeholder(i));
    }
    RefreshTexts();
    Layout();
}

bool SongFilterPanel::RowAvailable(const std::size_t row) const noexcept
{
    return row != static_cast<std::size_t>(SongFilterRow::Difficulty) || difficultyAvailable_;
}

void SongFilterPanel::Open(const SongFilter &filter, const SongCatalogLoadResult &catalog)
{
    draft_ = FormatSongFilter(filter);
    error_ = SongFilterError::None;
    applied_.reset();
    difficultyAvailable_ = false;
    std::set<std::string> modes;
    if (!filter.mode.empty())
        modes.insert(filter.mode);
    for (const auto &song : catalog.songs)
        for (const auto &entry : song.patterns)
        {
            if (!entry.pattern.mode.empty())
                modes.insert(entry.pattern.mode);
            if (entry.calculatedDifficulty && std::isfinite(*entry.calculatedDifficulty))
                difficultyAvailable_ = true;
        }
    modes_ = {""};
    modes_.insert(modes_.end(), modes.begin(), modes.end());
    modal_.SetVisible(true);
    RefreshTexts();
    RefreshValues();
    Layout();
}

void SongFilterPanel::Close() noexcept
{
    for (auto &input : inputs_)
        if (input)
        {
            input->Blur();
            input->SetVisible(false);
        }
    if (mode_)
        RequireDropdown(*mode_).Collapse();
    modal_.SetVisible(false);
}

void SongFilterPanel::Shutdown() noexcept
{
    Close();
    for (auto &input : inputs_)
        input.reset();
    modal_.Forget();
    canvas_ = nullptr;
    texts_ = nullptr;
    mode_ = nullptr;
}

void SongFilterPanel::SetViewport(std::uint32_t, const std::uint32_t height)
{
    height_ = height;
    if (canvas_)
        Layout();
}

void SongFilterPanel::Layout()
{
    if (!canvas_ || height_ == 0)
        return;
    modal_.Layout(BodyHeight);
    const float h = BodyHeight;
    Place(*modeLabel_, {0, 11, 140, 18}, h);
    Place(*mode_, {MinimumLeft, 0, MaximumLeft + MaximumWidth - MinimumLeft, RowHeight}, h);
    Place(*minimum_, {MinimumLeft, HeadTop, MinimumWidth, 16}, h);
    Place(*maximum_, {MaximumLeft, HeadTop, MaximumWidth, 16}, h);
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
    {
        const float top = RowY(row);
        Place(*switches_[row], {SwitchLeft, top + 8, 40, 24}, h);
        Place(*labels_[row], {LabelLeft, top, MinimumLeft - LabelLeft - 8, RowHeight}, h);
        if (row == static_cast<std::size_t>(SongFilterRow::Tag))
        {
            PlaceField(*fields_.back(), {MinimumLeft, top, MaximumLeft + MaximumWidth - MinimumLeft, RowHeight}, h);
            continue;
        }
        PlaceField(*fields_[row * 2], {MinimumLeft, top, MinimumWidth, RowHeight}, h);
        Place(*dashes_[row], {MinimumLeft + MinimumWidth, top, MaximumLeft - MinimumLeft - MinimumWidth, RowHeight}, h);
        PlaceField(*fields_[row * 2 + 1], {MaximumLeft, top, MaximumWidth, RowHeight}, h);
    }
    RefreshRows();
    UpdateInputs();
}

void SongFilterPanel::UpdateInputs()
{
    const bool visible = IsVisible() && mode_ && !RequireDropdown(*mode_).IsExpanded();
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        if (!inputs_[i])
            continue;
        PlaceTextInput(*inputs_[i], *fields_[i], *canvas_, height_, modal_.Scale(), IsNumberField(i));
        inputs_[i]->SetVisible(visible && RowAvailable(i / 2));
    }
}

void SongFilterPanel::RefreshTexts()
{
    if (!canvas_ || !modeLabel_)
        return;
    modal_.SetTexts(texts_->Get("SongFilter.title"), texts_->Get("SongFilter.reset"), texts_->Get("SongFilter.cancel"),
                    texts_->Get("SongFilter.apply"));
    SetText(*modeLabel_, texts_->Get("SongFilter.mode"));
    SetText(*minimum_, texts_->Get("SongFilter.minimum"));
    SetText(*maximum_, texts_->Get("SongFilter.maximum"));
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
        SetText(*labels_[row], texts_->Get(RowKeys[row]));
    std::vector<std::wstring> items;
    items.push_back(std::wstring{texts_->Get("SongFilter.allModes")});
    for (std::size_t i = 1; i < modes_.size(); ++i)
        items.push_back(song_select::DecodeDisplayText(modes_[i]));
    auto &combo = RequireDropdown(*mode_);
    combo.SetItems(std::move(items));
    const auto selected = std::ranges::find(modes_, draft_.mode);
    combo.SetSelectedIndex(selected == modes_.end() ? 0 : static_cast<std::size_t>(selected - modes_.begin()));
    const char *error = ErrorKey(error_);
    modal_.SetStatus(error ? texts_->Get(error) : std::wstring_view{});
    if (IsVisible())
        Layout();
}

void SongFilterPanel::RefreshValues()
{
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        const auto &value = i + 1 == fields_.size() ? draft_.tag : draft_.values[i];
        const bool available = RowAvailable(i / 2);
        SetFieldText(*fields_[i], inputs_[i] ? std::wstring_view{} : (available ? std::wstring_view{value} : L"—"),
                     IsNumberField(i));
        if (inputs_[i])
            inputs_[i]->SetText(value);
    }
    RefreshRows();
}

void SongFilterPanel::RefreshRows()
{
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
    {
        const auto filterRow = static_cast<SongFilterRow>(row);
        const bool available = RowAvailable(row);
        SetSwitch(*switches_[row], draft_.IsRowOn(filterRow), available && draft_.HasValue(filterRow));
        labels_[row]->GetComponent<TextVisualComponent>()->SetTextColor(available ? Ink : Caption);
    }
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        const bool focused = inputs_[i] && inputs_[i]->HasFocus();
        StyleField(*fields_[i], focused, RowAvailable(i / 2));
    }
}

void SongFilterPanel::SynchronizeText()
{
    bool changed = false;
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (!inputs_[i] || !inputs_[i]->TakeText(text))
            continue;
        if (i + 1 == inputs_.size())
            draft_.SetTag(std::move(text));
        else
            draft_.SetValue(i, std::move(text));
        changed = true;
    }
    if (changed)
        RefreshRows();
}

void SongFilterPanel::TryApply()
{
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    SynchronizeText();
    SongFilter filter;
    error_ = ParseSongFilter(draft_, filter);
    if (error_ == SongFilterError::None)
    {
        applied_ = std::move(filter);
        Close();
        return;
    }
    RefreshTexts();
}

void SongFilterPanel::Reset()
{
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    draft_ = {};
    error_ = SongFilterError::None;
    RefreshValues();
    RefreshTexts();
}

bool SongFilterPanel::ProcessAction(const Action &action)
{
    if (!IsVisible())
        return false;
    if (action.type == ActionType::SelectionChanged && action.source == mode_->Id())
    {
        if (action.selectedIndex < modes_.size())
            draft_.mode = modes_[action.selectedIndex];
        return true;
    }
    if (action.type != ActionType::Clicked)
        return false;
    if (modal_.IsPrimary(action.source))
    {
        TryApply();
        return true;
    }
    if (modal_.IsClose(action.source) || modal_.IsCancel(action.source))
    {
        Close();
        return true;
    }
    if (modal_.IsReset(action.source))
    {
        Reset();
        return true;
    }
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
        if (action.source == switches_[row]->Id())
        {
            SynchronizeText();
            if (draft_.ToggleRow(static_cast<SongFilterRow>(row)))
                RefreshRows();
            return true;
        }
    return false;
}

void SongFilterPanel::ProcessInput(const InputState &input)
{
    if (!IsVisible())
        return;
    UpdateInputs();
    SynchronizeText();
    bool focus = false, submit = false, cancel = false;
    for (auto &field : inputs_)
        if (field)
        {
            focus = focus || field->HasFocus();
            submit = field->TakeSubmit() || submit;
            cancel = field->TakeCancel() || cancel;
        }
    RefreshRows();
    if (cancel || (!focus && input.WasKeyPressed(VK_ESCAPE)))
        Close();
    else if (submit || (!focus && input.WasKeyPressed(VK_RETURN)))
        TryApply();
}

std::optional<SongFilter> SongFilterPanel::TakeApplied()
{
    return std::exchange(applied_, std::nullopt);
}
