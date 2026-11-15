#pragma once
#include "EditorLayout.h"
#include "Model/Submodules/EffectDocument.h"
#include <array>
#include <variant>

enum class EditorTab
{
    Compose,
    Audio,
    Metadata
};

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
    case EffectCommandType::Kiai:
    case EffectCommandType::MeasureLineVisible:
        return EditorPanel::Zones;
    case EffectCommandType::Custom:
        break;
    }
    return std::nullopt;
}

struct EditorTimingForm
{
    std::string measure{"1"}, fraction{"0/4"}, bpm{"120"}, measureLength{"4/4"};
};

struct EditorEffectDefinition
{
    EffectCommandType type;
    std::size_t labelIndex;
    bool hasAudioBus;
};
inline constexpr std::array EditorEffectDefinitions{
    EditorEffectDefinition{EffectCommandType::BusVolume, 0, true},
    EditorEffectDefinition{EffectCommandType::MeasureLineVisible, 1, false},
    EditorEffectDefinition{EffectCommandType::NoteSpeed, 2, false},
    EditorEffectDefinition{EffectCommandType::ScrollSpeed, 3, false}};

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
    EditorEffectSelection selection{EffectCommandType::BusVolume};
    AutomationCurve curve{AutomationCurve::Linear};
    void Load(const EffectDocument &, std::size_t row);
    [[nodiscard]] EffectDocument Apply(const EffectDocument &) const;
};
