#pragma once
#include "EditorLayout.h"
#include "Model/Submodules/EffectDocument.h"
#include <array>
#include <variant>

// Tab bar order; EditorView keeps one IEditorTab per value.
enum class EditorTab
{
    Compose,
    Audio,
    Metadata
};
inline constexpr std::size_t EditorTabCount = 3;

// The right panel edits one Compose channel; the channel decides which
// document list Copy, Paste and Delete act on.
using EditorPanel = editor_layout::Channel;
enum class EditorFocus
{
    Notes,
    Timing,
    Effects
};
[[nodiscard]] constexpr EditorFocus FocusOf(EditorPanel panel) noexcept
{
    if (panel == EditorPanel::Timing)
        return EditorFocus::Timing;
    if (panel == EditorPanel::Notes || panel == EditorPanel::Music)
        return EditorFocus::Notes;
    return EditorFocus::Effects;
}
// The Compose channel that draws and edits an automation type; Custom commands have none.
[[nodiscard]] constexpr std::optional<EditorPanel> ChannelOf(EffectCommandType type) noexcept
{
    switch (type)
    {
    case EffectCommandType::ScrollSpeed:
    case EffectCommandType::NoteSpeed:
        return EditorPanel::ScrollSpeed;
    case EffectCommandType::BusVolume:
        return EditorPanel::Volume;
    case EffectCommandType::ReverbSend:
    case EffectCommandType::LowPassCutoff:
    case EffectCommandType::HighPassCutoff:
        return EditorPanel::AudioEffects;
    case EffectCommandType::SyncopationZone:
    case EffectCommandType::Highlight:
    case EffectCommandType::MeasureLineVisible:
        return EditorPanel::Zones;
    case EffectCommandType::Custom:
        break;
    }
    return std::nullopt;
}

// The Timing panel edits one directive kind at a time.
enum class EditorTimingKind
{
    Bpm,
    MeasureLength,
    Delay
};
struct EditorTimingForm
{
    EditorTimingKind kind{EditorTimingKind::Bpm};
    std::string measure{"1"}, fraction{"0/4"}, bpm{"120"}, measureLength{"4/4"}, delay{"0"};
};

// How a command is edited: a number (Value or From/To over an Area) or an On/Off state.
enum class EditorEffectValue
{
    Number,
    Toggle
};
struct EditorEffectDefinition
{
    EffectCommandType type;
    std::size_t labelIndex;
    bool hasAudioBus;
    bool area;
    EditorEffectValue value;
};
// Order within a channel is the order of its type buttons.
inline constexpr std::array EditorEffectDefinitions{
    EditorEffectDefinition{EffectCommandType::ScrollSpeed, 3, false, true, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::NoteSpeed, 2, false, false, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::BusVolume, 0, true, true, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::ReverbSend, 5, true, true, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::LowPassCutoff, 6, true, true, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::HighPassCutoff, 7, true, true, EditorEffectValue::Number},
    EditorEffectDefinition{EffectCommandType::SyncopationZone, 8, false, false, EditorEffectValue::Toggle},
    EditorEffectDefinition{EffectCommandType::Highlight, 4, false, false, EditorEffectValue::Toggle},
    EditorEffectDefinition{EffectCommandType::MeasureLineVisible, 1, false, false, EditorEffectValue::Toggle}};
[[nodiscard]] constexpr const EditorEffectDefinition *FindEffectDefinition(EffectCommandType type) noexcept
{
    for (const auto &definition : EditorEffectDefinitions)
        if (definition.type == type)
            return &definition;
    return nullptr;
}
// Syncopation Exclude presets: quarter-note divisors (1 = quarter, 2 = eighth, 4 = sixteenth ...).
inline constexpr std::array<std::size_t, 6> ExcludePresets{1, 2, 4, 3, 6, 8};

struct EditorHitSoundTarget
{
    int keyType{};
};
using EditorEffectSelection = std::variant<EffectCommandType, EditorHitSoundTarget>;

// Text fields are drafts. Apply builds a candidate; document validation publishes it.
class EditorEffectForm
{
  public:
    std::string startMeasure{"1"}, startFraction{"0/4"}, endMeasure, endFraction;
    std::string beginValue{"1"}, endValue{"1"}, audioBus{"HitSound"}, curveName;
    // Syncopation Exclude divisors, separated by |.
    std::string excluded;
    EditorEffectSelection selection{EffectCommandType::BusVolume};
    AutomationCurve curve{AutomationCurve::Linear};
    void Load(const EffectDocument &, std::size_t row);
    [[nodiscard]] EffectDocument Apply(const EffectDocument &) const;

  private:
    [[nodiscard]] EffectDocument ApplyToggle(EffectDocument, EffectCommand) const;
};
