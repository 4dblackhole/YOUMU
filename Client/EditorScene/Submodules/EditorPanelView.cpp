#include "EditorView.h"
#include <algorithm>
#include <format>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

namespace
{
    constexpr float RowHeight = 34, VisibleRows = 6, ActionHeight = 32;
    std::wstring PositionCell(MusicalPosition position)
    {
        return std::format(L"{} · {}", position.measure + 1, Wide(Fraction(position.fraction)));
    }
    std::wstring Decimal(double value)
    {
        return std::format(L"{:g}", value);
    }
} // namespace

bool EditorView::EffectInPanel(std::size_t row, EditorPanel panel) const
{
    const auto &effects = state_.Document().Effects();
    if (row < effects.commands.size())
        return ChannelOf(effects.commands[row].type) == panel;
    return panel == EditorPanel::HitSounds && row - effects.commands.size() < effects.hitSoundChanges.size();
}

std::wstring EditorView::EffectLabel(const EditorEffectSelection &selection) const
{
    if (const auto *type = std::get_if<EffectCommandType>(&selection))
    {
        if (*type == EffectCommandType::Kiai)
            return std::wstring(Texts().effectTypes[4]);
        const auto definition = std::ranges::find(EditorEffectDefinitions, *type, &EditorEffectDefinition::type);
        return definition == EditorEffectDefinitions.end() ? std::to_wstring(static_cast<int>(*type))
                                                           : std::wstring(Texts().effectTypes[definition->labelIndex]);
    }
    const int key = std::get<EditorHitSoundTarget>(selection).keyType;
    for (const auto &choice : state_.Mode().SoundChoices(texts_))
        if (choice.keyType == key)
            return choice.label;
    return std::to_wstring(key);
}

void EditorView::CycleEffectType(EditorPanel panel)
{
    std::vector<EditorEffectSelection> choices;
    if (panel == EditorPanel::HitSounds)
        for (const auto &sound : state_.Mode().SoundChoices(texts_))
            choices.emplace_back(EditorHitSoundTarget{sound.keyType});
    else
        for (const auto &definition : EditorEffectDefinitions)
            if (ChannelOf(definition.type) == panel)
                choices.emplace_back(definition.type);
    if (choices.empty())
        return;
    const auto matches = [&](const EditorEffectSelection &value) {
        if (value.index() != state_.effectForm.selection.index())
            return false;
        if (const auto *type = std::get_if<EffectCommandType>(&value))
            return *type == std::get<EffectCommandType>(state_.effectForm.selection);
        return std::get<EditorHitSoundTarget>(value).keyType ==
               std::get<EditorHitSoundTarget>(state_.effectForm.selection).keyType;
    };
    auto next = std::ranges::find_if(choices, matches);
    if (next != choices.end())
        ++next;
    state_.effectForm.selection = next == choices.end() ? choices.front() : *next;
    state_.RequestRebuild();
}

void EditorView::SelectEffectRow(std::size_t row)
{
    state_.SelectListRow(row, toggleSelection_, rangeSelection_);
    auto draft = state_.effectForm;
    draft.Load(state_.Document().Effects(), row);
    if (const auto *sound = std::get_if<EditorHitSoundTarget>(&draft.selection))
    {
        const auto choices = state_.Mode().SoundChoices(texts_);
        if (std::ranges::find(choices, sound->keyType, &EditorSoundChoice::keyType) == choices.end())
            throw std::runtime_error("This sound target is not supported by the selected editor mode.");
    }
    state_.effectForm = std::move(draft);
    state_.RequestRebuild();
}

void EditorView::OpenChannel(EditorPanel panel)
{
    // A channel's form starts on one of its own types.
    if (FocusOf(panel) == EditorFocus::Effects)
    {
        const auto &selection = state_.effectForm.selection;
        const auto *type = std::get_if<EffectCommandType>(&selection);
        const bool sound = std::holds_alternative<EditorHitSoundTarget>(selection);
        const bool fits = panel == EditorPanel::HitSounds ? sound : type && ChannelOf(*type) == panel;
        if (!fits)
        {
            state_.effectForm.selection = EffectCommandType::ScrollSpeed;
            const auto sounds = state_.Mode().SoundChoices(texts_);
            if (panel == EditorPanel::HitSounds && !sounds.empty())
                state_.effectForm.selection = EditorHitSoundTarget{sounds.front().keyType};
            CycleEffectType(panel);
            if (panel != EditorPanel::HitSounds)
                CycleEffectType(panel);
        }
    }
    state_.OpenPanel(panel);
}

void EditorView::DrawListHeader(Rect body, std::span<const std::pair<std::wstring_view, float>> columns)
{
    float x = body.x;
    for (const auto &[caption, columnWidth] : columns)
    {
        Label({x + 8, body.y, columnWidth - 8, 22}, std::wstring(caption), {13, theme::Caption, TextAlignment::Leading, 700});
        x += columnWidth;
    }
    Box({body.x, body.y + 25, body.width, 1}, theme::RuleStrong);
}

void EditorView::DrawListRow(Rect row, std::span<const std::pair<std::wstring, float>> cells, bool selected,
                             std::function<void()> select)
{
    if (selected)
    {
        Box(row, White);
        Box({row.x, row.y, 4, row.height}, theme::Sun);
    }
    Box({row.x, row.y + row.height - 1, row.width, 1}, theme::Rule);
    float x = row.x;
    for (const auto &[text, columnWidth] : cells)
    {
        Label({x + 8, row.y, columnWidth - 8, row.height}, text, {15, theme::Ink, TextAlignment::Leading, 700, true});
        x += columnWidth;
    }
    Control(row, std::move(select));
}

float EditorView::DrawPanelActions(Rect body, std::size_t selected, std::function<void()> remove)
{
    const float y = body.y + body.height - ActionHeight;
    Box({body.x - 20, y - 10, body.width + 40, 1}, theme::Rule);
    const std::wstring caption = selected > 1 ? std::format(L"{} {}", Texts().remove, selected) : std::wstring(Texts().remove);
    const Rect remover{body.x, y, std::max(72.0F, EstimateTextWidth(caption, 13) + 24), ActionHeight};
    Box(remover, White);
    Box({remover.x, remover.y, remover.width, 1}, ui_colors::Rgb(0xF1C5CF));
    Box({remover.x, remover.y + remover.height - 1, remover.width, 1}, ui_colors::Rgb(0xF1C5CF));
    Label(remover, caption, {13, selected ? theme::Danger : theme::Caption, TextAlignment::Center, 700});
    if (selected)
        Control(remover, std::move(remove));
    return y;
}

void EditorView::DrawTimingPanel(Rect body)
{
    const auto &text = Texts();
    const auto &timing = state_.Document().Timing();
    const float typeWidth = 96, positionWidth = 100;
    const std::array<std::pair<std::wstring_view, float>, 3> columns{
        std::pair{texts_.Get("Editor.position"), positionWidth}, std::pair{texts_.Get("Editor.type"), typeWidth},
        std::pair{texts_.Get("Editor.value"), body.width - positionWidth - typeWidth}};
    DrawListHeader(body, columns);
    float y = body.y + 26;
    for (std::size_t i = state_.listOffset; i < timing.size() && i < state_.listOffset + VisibleRows; ++i)
    {
        const auto &t = timing[i];
        const auto type = t.type == TimingDirectiveType::Bpm             ? std::wstring(texts_.Get("Editor.bpm"))
                          : t.type == TimingDirectiveType::MeasureLength ? std::wstring(text.measureType)
                                                                         : std::wstring(text.delayType);
        const auto value = t.type == TimingDirectiveType::MeasureLength ? Wide(Fraction(t.ratio))
                           : t.type == TimingDirectiveType::Bpm          ? Decimal(t.value)
                                                                         : Decimal(t.value) + L" ms";
        const std::array<std::pair<std::wstring, float>, 3> cells{
            std::pair{PositionCell(t.position), positionWidth}, std::pair{type, typeWidth},
            std::pair{value, body.width - positionWidth - typeWidth}};
        DrawListRow({body.x, y, body.width, RowHeight}, cells, state_.IsListRowSelected(i), [this, t, i] {
            state_.SelectListRow(i, toggleSelection_, rangeSelection_);
            state_.timingForm.measure = std::to_string(t.position.measure + 1);
            state_.timingForm.fraction = Fraction(t.position.fraction);
            if (t.type == TimingDirectiveType::Bpm)
                state_.timingForm.bpm = std::to_string(t.value);
            if (t.type == TimingDirectiveType::MeasureLength)
                state_.timingForm.measureLength = Fraction(t.ratio);
            state_.RequestRebuild();
        });
        y += RowHeight;
    }
    if (timing.empty())
        Label({body.x + 8, y, body.width, RowHeight}, L"—", {15, theme::Caption});

    const float formTop = body.y + 26 + RowHeight * VisibleRows + 32;
    const float half = (body.width - 8) * .5F;
    Field({body.x, formTop, half, 34}, text.startMeasure.data(), state_.timingForm.measure);
    Field({body.x + half + 8, formTop, half, 34}, text.changePosition.data(), state_.timingForm.fraction);
    Field({body.x, formTop + 60, half, 34}, texts_.Get("Editor.bpm").data(), state_.timingForm.bpm);
    Field({body.x + half + 8, formTop + 60, half, 34}, text.measureLength.data(), state_.timingForm.measureLength);

    std::size_t selected = 0;
    for (std::size_t i = 0; i < timing.size(); ++i)
        selected += state_.IsListRowSelected(i) ? 1 : 0;
    const float actionsTop = DrawPanelActions(body, selected, [this] { state_.DeleteSelection(); });
    const Rect length{body.x + body.width - 96, actionsTop, 96, ActionHeight};
    const Rect bpm{length.x - 8 - 96, actionsTop, 96, ActionHeight};
    Button(bpm, std::wstring(text.addOrUpdateBpm), [this] { state_.ApplyBpm(); });
    Button(length, std::wstring(text.applyMeasureLength), [this] { state_.ApplyMeasureLength(); }, true);
}

void EditorView::DrawEffectPanel(Rect body, EditorPanel panel)
{
    const auto &text = Texts();
    const auto &effects = state_.Document().Effects();
    const auto total = effects.commands.size() + effects.hitSoundChanges.size();
    std::vector<std::size_t> rows;
    for (std::size_t row = 0; row < total; ++row)
        if (EffectInPanel(row, panel))
            rows.push_back(row);
    const float positionWidth = 100, typeWidth = 92;
    const std::array<std::pair<std::wstring_view, float>, 3> columns{
        std::pair{texts_.Get("Editor.position"), positionWidth}, std::pair{texts_.Get("Editor.type"), typeWidth},
        std::pair{texts_.Get("Editor.value"), body.width - positionWidth - typeWidth}};
    DrawListHeader(body, columns);
    float y = body.y + 26;
    const auto choices = state_.Mode().SoundChoices(texts_);
    std::size_t selected = 0;
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const auto row = rows[index];
        selected += state_.IsListRowSelected(row) ? 1 : 0;
        if (index < state_.listOffset || index >= state_.listOffset + VisibleRows)
            continue;
        std::wstring position, type, value;
        if (row < effects.commands.size())
        {
            const auto &command = effects.commands[row];
            position = PositionCell(command.position);
            type = EffectLabel(command.type);
            value = command.type == EffectCommandType::Kiai || command.type == EffectCommandType::SyncopationZone
                        ? (command.beginValue >= .5 ? L"On" : L"Off")
                        : Decimal(command.beginValue);
            if (command.endPosition)
                value += L" → " + Decimal(command.endValue);
        }
        else
        {
            const auto &change = effects.hitSoundChanges[row - effects.commands.size()];
            position = PositionCell(change.position);
            const auto found = std::ranges::find(choices, change.keyType, &EditorSoundChoice::keyType);
            type = found == choices.end() ? std::to_wstring(change.keyType) : found->label;
            value = Wide(change.soundIndex);
        }
        const std::array<std::pair<std::wstring, float>, 3> cells{
            std::pair{position, positionWidth}, std::pair{type, typeWidth},
            std::pair{value, body.width - positionWidth - typeWidth}};
        DrawListRow({body.x, y, body.width, RowHeight}, cells, state_.IsListRowSelected(row),
                    [this, row] { SelectEffectRow(row); });
        y += RowHeight;
    }
    if (rows.empty())
        Label({body.x + 8, y, body.width, RowHeight}, L"—", {15, theme::Caption});

    const auto &form = state_.effectForm;
    const auto *type = std::get_if<EffectCommandType>(&form.selection);
    const bool sound = std::holds_alternative<EditorHitSoundTarget>(form.selection);
    const bool editable = panel == EditorPanel::HitSounds ? sound : type && ChannelOf(*type) == panel;
    const float formTop = body.y + 26 + RowHeight * VisibleRows + 32;
    const float actionsTop = DrawPanelActions(body, selected, [this] { state_.DeleteSelection(); });
    if (!editable)
    {
        // Commands without an editor here are listed and preserved on save.
        Label({body.x, formTop - 18, body.width, 34}, std::wstring(texts_.Get("Editor.effectNotEditable")),
              {13, theme::Caption, TextAlignment::Leading, 600});
        return;
    }
    auto &draft = state_.effectForm;
    const float third = (body.width - 16) / 3;
    Button({body.x, formTop - 18, body.width, 30}, EffectLabel(draft.selection), [this, panel] { CycleEffectType(panel); });
    Field({body.x, formTop + 36, third, 34}, text.startMeasure.data(), draft.startMeasure);
    Field({body.x + third + 8, formTop + 36, third, 34}, text.startBeat.data(), draft.startFraction);
    Field({body.x + (third + 8) * 2, formTop + 36, third, 34},
          sound ? text.hitSoundIndex.data() : text.startValue.data(), draft.beginValue);
    if (!sound)
    {
        Field({body.x, formTop + 96, third, 34}, text.endMeasureOptional.data(), draft.endMeasure);
        Field({body.x + third + 8, formTop + 96, third, 34}, text.endBeatOptional.data(), draft.endFraction);
        Field({body.x + (third + 8) * 2, formTop + 96, third, 34}, text.endValue.data(), draft.endValue);
        const std::array curves{AutomationCurve::Linear, AutomationCurve::Exponential, AutomationCurve::Harmonic};
        const auto found = std::ranges::find(curves, draft.curve);
        const auto label = found == curves.end() ? Wide(draft.curveName)
                                                 : std::wstring(text.curves[static_cast<std::size_t>(found - curves.begin())]);
        Button({body.x, formTop + 140, body.width, 30}, label, [this, curves] {
            auto &edited = state_.effectForm;
            auto next = std::ranges::find(curves, edited.curve);
            if (next != curves.end())
                ++next;
            edited.curve = next == curves.end() ? curves.front() : *next;
            edited.curveName.clear();
            state_.RequestRebuild();
        });
    }
    Button({body.x + body.width - 96, actionsTop, 96, ActionHeight}, std::wstring(text.addOrUpdateEffect),
           [this] { state_.ApplyEffect(); }, true);
}

void EditorView::DrawMusicPanel(Rect body)
{
    const auto offset = state_.Document().Pattern().patternOffsetMilliseconds;
    Label({body.x, body.y, 60, 32}, std::wstring(texts_.Get("Editor.offset")), {13, theme::InkSoft, TextAlignment::Leading, 700});
    const Rect minus{body.x + 64, body.y, 40, 32}, value{body.x + 108, body.y, 92, 32}, plus{body.x + 204, body.y, 40, 32};
    Button(minus, std::wstring(texts_.Get("Editor.offsetDecrease")), [this] {
        state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds - 1);
    });
    Box(value, {1, 1, 1, .74F});
    Box({value.x, value.y + value.height - 2, value.width, 2}, ui_colors::Rgb(0xB5CFF5));
    Label(value, std::format(L"{:g} ms", offset), {15, theme::Ink, TextAlignment::Center, 800, true});
    Control(value, [this] {
        if (auto edited = EditText(std::wstring(Texts().offsetMilliseconds),
                                   std::to_string(state_.Document().Pattern().patternOffsetMilliseconds), Texts()))
            state_.SetPatternOffset(Number(*edited));
    });
    Button(plus, std::wstring(texts_.Get("Editor.offsetIncrease")), [this] {
        state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds + 1);
    });
    DrawSwitch({body.x, body.y + 48, body.width, 32}, std::wstring(texts_.Get("Editor.metronome")),
               state_.Metronome().Enabled(), [this] { state_.ToggleMetronome(); });
    Button({body.x, body.y + 96, 96, 32}, std::wstring(texts_.Get("Editor.tabs.1")),
           [this] { state_.SelectTab(EditorTab::Audio); });
}

void EditorView::DrawSwitch(Rect row, std::wstring caption, bool on, std::function<void()> toggle)
{
    Label({row.x, row.y, row.width - 56, row.height}, std::move(caption), {14, theme::Ink, TextAlignment::Leading, 600});
    const Rect track{row.x + row.width - 44, row.y + (row.height - 24) * .5F, 44, 24};
    Box(track, on ? theme::Brand : ui_colors::Rgb(0x0F2A47, .2F), 12);
    Box({on ? track.x + 22 : track.x + 2, track.y + 2, 20, 20}, White, 10);
    Control(row, std::move(toggle));
}
