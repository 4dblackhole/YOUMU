#include "EditorView.h"
#include <algorithm>
#include <format>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

namespace
{
    constexpr float RowHeight = 34, ActionHeight = 32;
    std::wstring PositionCell(MusicalPosition position)
    {
        return std::format(L"{} · {}", position.measure + 1, Wide(Fraction(position.fraction)));
    }
    std::wstring Decimal(double value)
    {
        return std::format(L"{:g}", value);
    }
    std::wstring ExcludeLabel(const std::vector<std::size_t> &divisions)
    {
        std::wstring text;
        for (const auto division : divisions)
            text += L" 1/" + std::to_wstring(division * 4);
        return text;
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
            return choice.effectLabel;
    return std::to_wstring(key);
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

void EditorView::DrawListRow(Rect row, std::span<const std::pair<std::wstring, float>> cells, bool selected, bool primary,
                             std::function<void()> select, Color color)
{
    if (selected)
    {
        Box(row, primary ? White : Color{1, 1, 1, .78F});
        Box({row.x, row.y, 4, row.height}, primary ? theme::Sun : ui_colors::Rgb(0xF2B23A, .45F));
    }
    Box({row.x, row.y + row.height - 1, row.width, 1}, theme::Rule);
    float x = row.x;
    for (const auto &[text, columnWidth] : cells)
    {
        Label({x + 8, row.y, columnWidth - 8, row.height}, text, {15, color, TextAlignment::Leading, 700, true});
        x += columnWidth;
    }
    Control(row, std::move(select));
}

float EditorView::DrawPanelActions(Rect body, std::size_t selected, std::function<void()> remove,
                                   std::function<void()> add, std::function<void()> apply)
{
    const float y = body.y + body.height - ActionHeight;
    Box({body.x - 20, y - 10, body.width + 40, 1}, theme::Rule);
    const std::wstring caption =
        selected > 1 ? std::format(L"{} {}", Texts().remove, selected) : std::wstring(Texts().remove);
    const Rect remover{body.x, y, std::max(72.0F, EstimateTextWidth(caption, 13) + 24), ActionHeight};
    Box(remover, White);
    Box({remover.x, remover.y, remover.width, 1}, ui_colors::Rgb(0xF1C5CF));
    Box({remover.x, remover.y + remover.height - 1, remover.width, 1}, ui_colors::Rgb(0xF1C5CF));
    Box({remover.x, remover.y, 1, remover.height}, ui_colors::Rgb(0xF1C5CF));
    Box({remover.x + remover.width - 1, remover.y, 1, remover.height}, ui_colors::Rgb(0xF1C5CF));
    Label(remover, caption, {13, selected ? theme::Danger : theme::Caption, TextAlignment::Center, 700});
    if (selected)
        Control(remover, std::move(remove));
    const Rect applier{body.x + body.width - 72, y, 72, ActionHeight};
    const Rect adder{applier.x - 8 - 64, y, 64, ActionHeight};
    if (selected > 1)
        Label({remover.x + remover.width + 6, y, adder.x - remover.x - remover.width - 12, ActionHeight},
              texts_.Format("Editor.selectedCountFormat", selected), {11, theme::Caption, TextAlignment::Center, 600});
    Button(adder, std::wstring(texts_.Get("Editor.add")), std::move(add));
    Button(applier, std::wstring(texts_.Get("Editor.applyRow")), std::move(apply), true);
    return y;
}

void EditorView::Segmented(Rect row, std::span<const std::wstring> options, std::size_t selected,
                           std::function<void(std::size_t)> choose)
{
    const float gap = 4, w = (row.width - gap * static_cast<float>(options.size() - 1)) / static_cast<float>(options.size());
    for (std::size_t i = 0; i < options.size(); ++i)
        Button({row.x + (w + gap) * static_cast<float>(i), row.y, w, row.height}, options[i],
               [choose, i] { choose(i); }, i == selected);
}

void EditorView::DrawTimingPanel(Rect body)
{
    const auto &text = Texts();
    const auto &timing = state_.Document().Timing();
    auto &form = state_.timingForm;
    const bool cut = form.kind == EditorTimingKind::Delay && !form.delay.empty() && form.delay.front() == '-';
    // The form keeps its height; the list takes what remains above it.
    const float formHeight = 36 + 54 * 2 + (cut ? 34 : 0) + ActionHeight + 18;
    const auto visibleRows = static_cast<std::size_t>(std::max(3.0F, (body.height - formHeight - 26) / RowHeight));
    const float typeWidth = 84, positionWidth = 96;
    const std::array<std::pair<std::wstring_view, float>, 3> columns{
        std::pair{texts_.Get("Editor.position"), positionWidth}, std::pair{texts_.Get("Editor.type"), typeWidth},
        std::pair{texts_.Get("Editor.value"), body.width - positionWidth - typeWidth}};
    DrawListHeader(body, columns);
    float y = body.y + 26;
    std::size_t selected = 0;
    for (std::size_t i = 0; i < timing.size(); ++i)
    {
        selected += state_.IsListRowSelected(i) ? 1 : 0;
        if (i < state_.listOffset || i >= state_.listOffset + visibleRows)
            continue;
        const auto &t = timing[i];
        const auto type = t.type == TimingDirectiveType::Bpm             ? std::wstring(texts_.Get("Editor.bpm"))
                          : t.type == TimingDirectiveType::MeasureLength ? std::wstring(text.measureType)
                                                                         : std::wstring(text.delayType);
        const bool removes = t.type == TimingDirectiveType::DelayMilliseconds && t.value < 0;
        const auto value = t.type == TimingDirectiveType::MeasureLength ? Wide(MeasureLengthText(t.ratio))
                           : t.type == TimingDirectiveType::Bpm
                               ? Decimal(t.value)
                               : std::format(L"{}{:g} ms", t.value > 0 ? L"+" : L"−", std::abs(t.value));
        const std::array<std::pair<std::wstring, float>, 3> cells{
            std::pair{PositionCell(t.position), positionWidth}, std::pair{type, typeWidth},
            std::pair{value, body.width - positionWidth - typeWidth}};
        DrawListRow({body.x, y, body.width, RowHeight}, cells, state_.IsListRowSelected(i), state_.ListAnchor() == i,
                    [this, t, i] {
                        state_.SelectListRow(i, toggleSelection_, rangeSelection_);
                        auto &edited = state_.timingForm;
                        edited.measure = std::to_string(t.position.measure + 1);
                        edited.fraction = Fraction(t.position.fraction);
                        if (t.type == TimingDirectiveType::Bpm)
                        {
                            edited.kind = EditorTimingKind::Bpm;
                            edited.bpm = std::format("{:g}", t.value);
                        }
                        else if (t.type == TimingDirectiveType::MeasureLength)
                        {
                            edited.kind = EditorTimingKind::MeasureLength;
                            edited.measureLength = MeasureLengthText(t.ratio);
                        }
                        else
                        {
                            edited.kind = EditorTimingKind::Delay;
                            edited.delay = std::format("{:g}", t.value);
                        }
                        state_.RequestRebuild();
                    },
                    removes ? theme::Danger : theme::Ink);
        y += RowHeight;
    }
    if (timing.empty())
        Label({body.x + 8, y, body.width, RowHeight}, L"—", {15, theme::Caption});

    float formTop = body.y + 26 + RowHeight * static_cast<float>(visibleRows) + 12;
    const std::array<std::wstring, 3> kinds{std::wstring(texts_.Get("Editor.bpm")), std::wstring(text.measureType),
                                            std::wstring(text.delayType)};
    Segmented({body.x, formTop, body.width, 30}, kinds, static_cast<std::size_t>(form.kind), [this](std::size_t i) {
        state_.timingForm.kind = static_cast<EditorTimingKind>(i);
        state_.RequestRebuild();
    });
    formTop += 36 + 18;
    const float half = (body.width - 8) * .5F;
    Field({body.x, formTop, half, 32}, text.startMeasure.data(), form.measure);
    if (form.kind != EditorTimingKind::MeasureLength)
        Field({body.x + half + 8, formTop, half, 32}, text.changePosition.data(), form.fraction);
    formTop += 54;
    if (form.kind == EditorTimingKind::Bpm)
        Field({body.x, formTop, half, 32}, texts_.Get("Editor.bpm").data(), form.bpm);
    else if (form.kind == EditorTimingKind::MeasureLength)
        Field({body.x, formTop, half, 32}, text.measureLength.data(), form.measureLength);
    else
        Field({body.x, formTop, half, 32}, texts_.Get("Editor.delayMilliseconds").data(), form.delay);
    if (cut)
    {
        double removed = 0;
        try
        {
            removed = -Number(form.delay);
        }
        catch (const std::exception &)
        {
        }
        Icon({body.x, formTop + 42, 15, 15}, L"alert", theme::Danger);
        Text({body.x + 20, formTop + 40, body.width - 20, 34},
             WrapCaption(texts_.Format("Editor.delayCutFormat", removed), 12, body.width - 20), 12, theme::Danger);
    }
    DrawPanelActions(body, selected, [this] { state_.DeleteSelection(); }, [this] { state_.AddTiming(); },
                     [this] { state_.ApplyTimingRow(); });
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

    auto &draft = state_.effectForm;
    std::vector<EditorEffectSelection> types;
    if (panel == EditorPanel::HitSounds)
        for (const auto &choice : state_.Mode().SoundChoices(texts_))
            types.emplace_back(EditorHitSoundTarget{choice.keyType});
    else
        for (const auto &candidate : EditorEffectDefinitions)
            if (ChannelOf(candidate.type) == panel)
                types.emplace_back(candidate.type);
    {
        // A form left on another channel's type starts on this channel's first type.
        const auto *current = std::get_if<EffectCommandType>(&draft.selection);
        const bool fits = panel == EditorPanel::HitSounds ? std::holds_alternative<EditorHitSoundTarget>(draft.selection)
                                                          : current && ChannelOf(*current) == panel;
        if (!fits && !types.empty())
            draft.selection = types.front();
    }
    const auto *type = std::get_if<EffectCommandType>(&draft.selection);
    const bool sound = std::holds_alternative<EditorHitSoundTarget>(draft.selection);
    const auto *definition = type ? FindEffectDefinition(*type) : nullptr;
    const bool editable = panel == EditorPanel::HitSounds ? sound : definition && ChannelOf(*type) == panel;
    const bool toggle = definition && definition->value == EditorEffectValue::Toggle;
    const bool area = !draft.endMeasure.empty() || !draft.endFraction.empty();
    const bool exclude = toggle && *type == EffectCommandType::SyncopationZone && draft.beginValue == "1";
    const bool targets = panel == EditorPanel::Volume || panel == EditorPanel::AudioEffects;
    float formHeight = ActionHeight + 18 + 54;
    // Hit sounds always name the target, even for a mode with a single sound choice.
    const bool typeSelector = types.size() > 1 || sound;
    formHeight += typeSelector ? 36 : 0;
    formHeight += targets ? 36 : 0;
    formHeight += definition && definition->area ? 32 : 0;
    formHeight += area ? 54 + 36 : 0;
    formHeight += exclude ? 58 : 0;
    const auto visibleRows = static_cast<std::size_t>(std::max(3.0F, (body.height - formHeight - 26) / RowHeight));

    const float positionWidth = 96, typeWidth = panel == EditorPanel::Volume ? 120.0F : 104.0F;
    const std::array<std::pair<std::wstring_view, float>, 3> columns{
        std::pair{texts_.Get("Editor.position"), positionWidth},
        std::pair{texts_.Get(panel == EditorPanel::Volume ? "Editor.target" : "Editor.type"), typeWidth},
        std::pair{texts_.Get("Editor.value"), body.width - positionWidth - typeWidth}};
    DrawListHeader(body, columns);
    float y = body.y + 26;
    const auto choices = state_.Mode().SoundChoices(texts_);
    std::size_t selected = 0;
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const auto row = rows[index];
        selected += state_.IsListRowSelected(row) ? 1 : 0;
        if (index < state_.listOffset || index >= state_.listOffset + visibleRows)
            continue;
        std::wstring position, label, value;
        if (row < effects.commands.size())
        {
            const auto &command = effects.commands[row];
            const auto *rowDefinition = FindEffectDefinition(command.type);
            position = PositionCell(command.position);
            label = EffectLabel(command.type);
            if (rowDefinition && rowDefinition->value == EditorEffectValue::Toggle)
                value = std::wstring(texts_.Get(command.endValue >= .5 ? "Editor.toggleOn" : "Editor.toggleOff"));
            else
                value = Decimal(command.beginValue);
            if (command.endPosition)
                value += L" → " + Decimal(command.endValue);
            if (!command.excludedDivisions.empty())
                value += L" −" + ExcludeLabel(command.excludedDivisions);
            // Volume has one type, so its column names the target instead.
            if (panel == EditorPanel::Volume)
                label = Wide(command.target);
        }
        else
        {
            const auto &change = effects.hitSoundChanges[row - effects.commands.size()];
            position = PositionCell(change.position);
            const auto found = std::ranges::find(choices, change.keyType, &EditorSoundChoice::keyType);
            label = found == choices.end() ? std::to_wstring(change.keyType) : found->effectLabel;
            value = Wide(change.soundIndex);
        }
        const std::array<std::pair<std::wstring, float>, 3> cells{
            std::pair{position, positionWidth}, std::pair{label, typeWidth},
            std::pair{value, body.width - positionWidth - typeWidth}};
        DrawListRow({body.x, y, body.width, RowHeight}, cells, state_.IsListRowSelected(row),
                    state_.ListAnchor() == row, [this, row] { SelectEffectRow(row); });
        y += RowHeight;
    }
    if (rows.empty())
        Label({body.x + 8, y, body.width, RowHeight}, L"—", {15, theme::Caption});

    float formTop = body.y + 26 + RowHeight * static_cast<float>(visibleRows) + 12;
    if (!editable)
    {
        // A mode without sound choices has nothing to edit here; rows can still be deleted.
        DrawPanelActions(body, selected, [this] { state_.DeleteSelection(); }, [] {}, [] {});
        return;
    }
    if (typeSelector)
    {
        std::vector<std::wstring> names;
        std::size_t current = 0;
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            names.push_back(EffectLabel(types[i]));
            if (types[i].index() == draft.selection.index())
            {
                const auto *candidate = std::get_if<EffectCommandType>(&types[i]);
                const bool same = candidate ? *candidate == *type
                                            : std::get<EditorHitSoundTarget>(types[i]).keyType ==
                                                  std::get<EditorHitSoundTarget>(draft.selection).keyType;
                current = same ? i : current;
            }
        }
        Segmented({body.x, formTop, body.width, 30}, names, current, [this, types](std::size_t i) {
            SelectEffectType(types[i]);
        });
        formTop += 36;
    }
    if (panel == EditorPanel::Volume)
    {
        const std::array<std::wstring, 3> names{L"HitSound", L"TickSound", L"UserInputFeedback"};
        const auto found = std::ranges::find(names, Wide(draft.audioBus));
        Segmented({body.x, formTop, body.width, 30}, names, static_cast<std::size_t>(found - names.begin()),
                  [this, names](std::size_t i) {
                      state_.effectForm.audioBus = Utf8(names[i]);
                      state_.RequestRebuild();
                  });
        formTop += 36;
    }
    else if (panel == EditorPanel::AudioEffects)
    {
        Button({body.x, formTop, body.width, 30},
               std::wstring(texts_.Get("Editor.audioBus")) + L"  " + Wide(draft.audioBus), [this] {
                   constexpr std::array<std::string_view, 4> buses{"Music", "HitSound", "TickSound", "UserInputFeedback"};
                   auto next = std::ranges::find(buses, state_.effectForm.audioBus);
                   next = next == buses.end() || next + 1 == buses.end() ? buses.begin() : next + 1;
                   state_.effectForm.audioBus = std::string(*next);
                   state_.RequestRebuild();
               });
        formTop += 36;
    }
    formTop += 18;
    const float third = (body.width - 16) / 3;
    Field({body.x, formTop, third, 32}, text.startMeasure.data(), draft.startMeasure);
    Field({body.x + third + 8, formTop, third, 32}, text.startBeat.data(), draft.startFraction);
    if (toggle)
    {
        const std::array<std::wstring, 2> states{std::wstring(texts_.Get("Editor.toggleOn")),
                                                 std::wstring(texts_.Get("Editor.toggleOff"))};
        Segmented({body.x + (third + 8) * 2, formTop, third, 32}, states, draft.beginValue == "1" ? 0 : 1,
                  [this](std::size_t i) {
                      state_.effectForm.beginValue = state_.effectForm.endValue = i == 0 ? "1" : "0";
                      state_.RequestRebuild();
                  });
    }
    else
        Field({body.x + (third + 8) * 2, formTop, third, 32}, sound ? text.hitSoundIndex.data() : text.startValue.data(),
              draft.beginValue);
    formTop += 54;
    if (exclude)
        DrawExcludeChips({body.x, formTop, body.width, 52});
    if (definition && definition->area)
    {
        DrawSwitch({body.x, formTop, body.width, 28}, std::wstring(texts_.Get("Editor.area")), area, [this, area] {
            auto &edited = state_.effectForm;
            if (area)
            {
                edited.endMeasure.clear();
                edited.endFraction.clear();
            }
            else
            {
                edited.endMeasure = std::to_string(Integer(edited.startMeasure) + 1);
                edited.endFraction = "0/1";
            }
            state_.RequestRebuild();
        });
        formTop += 32;
    }
    if (area)
    {
        formTop += 18;
        Field({body.x, formTop, third, 32}, text.endMeasureOptional.data(), draft.endMeasure);
        Field({body.x + third + 8, formTop, third, 32}, text.endBeatOptional.data(), draft.endFraction);
        Field({body.x + (third + 8) * 2, formTop, third, 32}, text.endValue.data(), draft.endValue);
        formTop += 40;
        DrawCurveRow({body.x, formTop, body.width, 30});
    }
    DrawPanelActions(body, selected, [this] { state_.DeleteSelection(); }, [this] { state_.ApplyEffect(); },
                     [this] { state_.ApplyEffectRow(); });
}

void EditorView::SelectEffectType(const EditorEffectSelection &selection)
{
    auto &draft = state_.effectForm;
    draft.selection = selection;
    // Each kind keeps its own target convention.
    if (const auto *type = std::get_if<EffectCommandType>(&selection))
    {
        if (*type == EffectCommandType::BusVolume)
            draft.audioBus = "HitSound";
        else if (const auto *definition = FindEffectDefinition(*type); definition && definition->hasAudioBus)
            draft.audioBus = "Music";
        if (const auto *definition = FindEffectDefinition(*type); definition && !definition->area)
        {
            draft.endMeasure.clear();
            draft.endFraction.clear();
        }
        if (const auto *definition = FindEffectDefinition(*type); definition && definition->value == EditorEffectValue::Toggle)
            draft.beginValue = draft.endValue = draft.beginValue == "0" ? "0" : "1";
    }
    state_.RequestRebuild();
}

void EditorView::DrawExcludeChips(Rect row)
{
    // Syncopation On: notes exactly on a ticked grid keep their normal windows.
    auto &draft = state_.effectForm;
    std::vector<std::size_t> divisions;
    for (std::size_t start = 0; start < draft.excluded.size();)
    {
        const auto end = std::min(draft.excluded.find('|', start), draft.excluded.size());
        try
        {
            divisions.push_back(static_cast<std::size_t>(Integer(draft.excluded.substr(start, end - start))));
        }
        catch (const std::exception &)
        {
        }
        start = end + 1;
    }
    Label({row.x, row.y - 2, row.width, 16}, std::wstring(texts_.Get("Editor.excludeHelp")),
          {11, theme::Caption, TextAlignment::Leading, 600});
    float x = row.x;
    for (const auto division : ExcludePresets)
    {
        const bool on = std::ranges::find(divisions, division) != divisions.end();
        const Rect chip{x, row.y + 18, 44, 28};
        Box(chip, on ? ui_colors::Rgb(0xA640E6) : Color{1, 1, 1, .8F});
        Label(chip, L"1/" + std::to_wstring(division * 4), {13, on ? White : theme::InkSoft, TextAlignment::Center, 800, true});
        Control(chip, [this, division, divisions] {
            auto next = divisions;
            if (const auto found = std::ranges::find(next, division); found != next.end())
                next.erase(found);
            else
                next.push_back(division);
            std::string joined;
            for (const auto value : next)
                joined += (joined.empty() ? "" : "|") + std::to_string(value);
            state_.effectForm.excluded = std::move(joined);
            state_.RequestRebuild();
        });
        x += 48;
    }
    // Other divisors as quarter note ÷ n, separated by |.
    const Rect custom{x, row.y + 18, row.x + row.width - x, 28};
    InputField(custom, std::wstring(texts_.Get("Editor.exclude")), Wide(state_.effectForm.excluded),
               [this](const std::string &typed) {
                   state_.effectForm.excluded = typed;
                   state_.RequestRebuild();
               },
               true, false);
}

void EditorView::DrawCurveRow(Rect row)
{
    // Built-in curves and the YME's own [Interpolation] names; Curves… edits the table.
    auto &draft = state_.effectForm;
    std::vector<std::pair<AutomationCurve, std::string>> curves{
        {AutomationCurve::Linear, {}}, {AutomationCurve::Exponential, {}}, {AutomationCurve::Harmonic, {}}};
    for (const auto &[name, expression] : state_.Document().Effects().interpolations)
        curves.emplace_back(AutomationCurve::Expression, name);
    const auto current = std::ranges::find_if(curves, [&](const auto &curve) {
        return curve.first == draft.curve && (curve.first != AutomationCurve::Expression || curve.second == draft.curveName);
    });
    const auto &text = Texts();
    std::wstring name = current == curves.end() || current->first == AutomationCurve::Expression
                            ? Wide(draft.curveName)
                            : std::wstring(text.curves[static_cast<std::size_t>(current - curves.begin())]);
    const float buttonWidth = 84;
    Label({row.x, row.y, 44, row.height}, std::wstring(texts_.Get("Editor.curve")), {12, theme::Caption, TextAlignment::Leading, 700});
    Button({row.x + 48, row.y, row.width - 48 - buttonWidth - 8, row.height}, name, [this, curves, current] {
        auto next = current == curves.end() ? curves.begin() : current + 1;
        if (next == curves.end())
            next = curves.begin();
        state_.effectForm.curve = next->first;
        state_.effectForm.curveName = next->second;
        state_.RequestRebuild();
    });
    Button({row.x + row.width - buttonWidth, row.y, buttonWidth, row.height}, std::wstring(texts_.Get("Editor.curvesButton")),
           [this] { OpenCurves(); });
}

void EditorView::DrawMusicPanel(Rect body)
{
    const auto offset = state_.Document().Pattern().patternOffsetMilliseconds;
    Label({body.x, body.y, 60, 32}, std::wstring(texts_.Get("Editor.offset")), {13, theme::InkSoft, TextAlignment::Leading, 700});
    const Rect minus{body.x + 64, body.y, 40, 32}, value{body.x + 108, body.y, 92, 32}, plus{body.x + 204, body.y, 40, 32};
    Button(minus, std::wstring(texts_.Get("Editor.offsetDecrease")), [this] {
        state_.SetPatternOffset(state_.Document().Pattern().patternOffsetMilliseconds - 1);
    });
    InputField(value, std::wstring(Texts().offsetMilliseconds), std::format(L"{:g}", offset),
               [this](const std::string &typed) { state_.SetPatternOffset(Number(typed)); }, true, false);
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
