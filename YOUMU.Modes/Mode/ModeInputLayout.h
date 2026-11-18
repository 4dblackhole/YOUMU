#pragma once
#include "Common/RhythmTypes.h"
#include "Mode/PlayOptions.h"
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Stable identifiers belong to the mode. The first key is primary; the
// remaining keys are auxiliary. PhysicalKey contains a Win32 Virtual-Key.
struct ModeInputBinding
{
    std::string id, displayName;
    NoteAction action{};
    std::vector<PhysicalKey> keys;
    std::size_t laneIndex{};
};
using InputKeyOverrides = std::map<std::string, std::vector<PhysicalKey>>;
struct ModeInputLayout
{
    std::string modeId;
    std::vector<ModeInputBinding> bindings;
    // Any slot, the primary included, may hold 0 (unassigned). A binding whose
    // slots are all unassigned receives no input.
    bool allowUnassignedKeys{};
};
struct PlaySessionOptions
{
    RhythmDuration globalOffset{};
    InputKeyOverrides inputKeys;
    PlayOptions play;
};
// Resolve and validate before publishing settings or constructing a session.
[[nodiscard]] ModeInputLayout ResolveInputLayout(ModeInputLayout defaults, const InputKeyOverrides &overrides);
