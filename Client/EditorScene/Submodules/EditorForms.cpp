#include "EditorForms.h"
#include "EditorValueParsing.h"
#include <algorithm>

void EditorEffectForm::Load(const EffectDocument &effects, std::size_t row)
{
    auto draft = *this;
    draft.endMeasure.clear();
    draft.endFraction.clear();
    if (row < effects.commands.size())
    {
        const auto &command = effects.commands[row];
        const auto *definition = FindEffectDefinition(command.type);
        if (!definition)
            throw std::runtime_error("This effect is preserved; its editor is not supported.");
        draft.selection = command.type;
        draft.startMeasure = std::to_string(command.position.measure + 1);
        draft.startFraction = Fraction(command.position.fraction);
        draft.beginValue = std::to_string(command.beginValue);
        draft.endValue = std::to_string(command.endValue);
        if (definition->value == EditorEffectValue::Toggle)
            draft.beginValue = draft.endValue = command.endValue >= .5 ? "1" : "0";
        if (definition->hasAudioBus)
            draft.audioBus = command.target;
        draft.curve = command.curve;
        draft.curveName = command.curveName;
        draft.excluded.clear();
        for (const auto division : command.excludedDivisions)
            draft.excluded += (draft.excluded.empty() ? "" : "|") + std::to_string(division);
        if (command.endPosition)
        {
            draft.endMeasure = std::to_string(command.endPosition->measure + 1);
            draft.endFraction = Fraction(command.endPosition->fraction);
        }
    }
    else
    {
        const auto &change = effects.hitSoundChanges.at(row - effects.commands.size());
        draft.selection = EditorHitSoundTarget{change.keyType};
        draft.startMeasure = std::to_string(change.position.measure + 1);
        draft.startFraction = Fraction(change.position.fraction);
        draft.beginValue = change.soundIndex;
    }
    *this = std::move(draft);
}
EffectDocument EditorEffectForm::Apply(const EffectDocument &source) const
{
    auto effects = source;
    const auto position = Position(startMeasure, startFraction);
    if (const auto *sound = std::get_if<EditorHitSoundTarget>(&selection))
    {
        std::erase_if(effects.hitSoundChanges,
                      [&](const auto &c) { return c.position == position && c.keyType == sound->keyType; });
        effects.hitSoundChanges.push_back({position, beginValue, sound->keyType});
    }
    else
    {
        EffectCommand command;
        command.position = position;
        command.type = std::get<EffectCommandType>(selection);
        const auto *definition = FindEffectDefinition(command.type);
        if (!definition)
            throw std::invalid_argument("Unsupported automation type.");
        command.target = definition->hasAudioBus ? audioBus : "";
        if (definition->value == EditorEffectValue::Toggle)
            return ApplyToggle(std::move(effects), std::move(command));
        command.beginValue = Number(beginValue);
        command.endValue = Number(endValue);
        command.curve = curve;
        command.curveName = curveName;
        if (command.curve == AutomationCurve::Expression)
        {
            const auto current = std::ranges::find_if(source.commands, [&](const auto &c) { return c.curveName == curveName; });
            if (current != source.commands.end()) command.interpolation = current->interpolation;
        }
        if (!endMeasure.empty() || !endFraction.empty())
            command.endPosition = Position(endMeasure, endFraction);
        else
            command.endValue = command.beginValue;
        std::erase_if(effects.commands, [&](const auto &c) {
            return c.position == position && c.type == command.type && c.target == command.target;
        });
        effects.commands.push_back(std::move(command));
    }
    return effects;
}

EffectDocument EditorEffectForm::ApplyToggle(EffectDocument effects, EffectCommand command) const
{
    // On/Off states are points; Syncopation On may exclude notes on given grids.
    const bool on = Number(beginValue) >= .5;
    command.beginValue = command.endValue = on ? 1 : 0;
    command.curve = AutomationCurve::Step;
    if (command.type == EffectCommandType::SyncopationZone && on && !excluded.empty())
    {
        std::size_t start = 0;
        while (start <= excluded.size())
        {
            const auto end = std::min(excluded.find('|', start), excluded.size());
            const auto division = Integer(excluded.substr(start, end - start));
            if (division < 1 || division > 1024)
                throw std::invalid_argument("Exclude requires beat divisors 1..1024 separated by |.");
            command.excludedDivisions.push_back(static_cast<std::size_t>(division));
            start = end + 1;
        }
    }
    std::erase_if(effects.commands,
                  [&](const auto &c) { return c.position == command.position && c.type == command.type; });
    effects.commands.push_back(std::move(command));
    return effects;
}
