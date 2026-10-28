#include "Mode/ModeInputLayout.h"
#include <algorithm>
#include <set>
#include <stdexcept>

ModeInputLayout ResolveInputLayout(ModeInputLayout layout, const InputKeyOverrides &overrides)
{
    for (const auto &[id, keys] : overrides)
    {
        auto found = std::ranges::find(layout.bindings, id, &ModeInputBinding::id);
        if (found == layout.bindings.end() || keys.empty() || keys.size() > found->keys.size() ||
            (layout.modeId != "Taiko" && found->keys.size() != keys.size()))
            throw std::invalid_argument("Unknown input binding or incorrect key count: " + id);
        const auto slots = found->keys.size();
        found->keys = keys;
        found->keys.resize(slots, 0);
    }
    std::set<PhysicalKey> used;
    for (const auto &binding : layout.bindings)
    {
        if (binding.keys.empty() || binding.keys.front() == 0)
            throw std::invalid_argument("The primary key cannot be empty.");
        for (const auto key : binding.keys)
        {
            if (key == 0 && layout.modeId == "Taiko") continue;
            // Preserve pause/reset controls and the global performance-display shortcut.
            if (key < 8 || key > 254 || key == 27 || key == 32 || key == 'R' || key == 0x76)
                throw std::invalid_argument("This key is reserved for game controls.");
            if (!used.insert(key).second)
                throw std::invalid_argument("Each input in a mode must use a different key.");
        }
    }
    return layout;
}
