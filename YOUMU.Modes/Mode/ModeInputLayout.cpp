#include "Mode/ModeInputLayout.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace
{
// Overrides may leave auxiliary slots unassigned only when the layout allows it.
[[nodiscard]] bool HasValidKeyCount(std::size_t slots, std::size_t keyCount, bool allowUnassignedAuxiliaryKeys)
{
    if (keyCount == 0 || keyCount > slots)
        return false;
    return allowUnassignedAuxiliaryKeys || keyCount == slots;
}

[[nodiscard]] bool IsReservedKey(PhysicalKey key)
{
    if (key < 8 || key > 254)
        return true;
    switch (key)
    {
    case 27:
    case 32:
    case 0xC0: // VK_OEM_3 (backquote): hold to restart
    case 0x76:
        return true;
    default:
        return false;
    }
}
} // namespace

ModeInputLayout ResolveInputLayout(ModeInputLayout layout, const InputKeyOverrides &overrides)
{
    for (const auto &[id, keys] : overrides)
    {
        auto found = std::ranges::find(layout.bindings, id, &ModeInputBinding::id);
        if (found == layout.bindings.end() ||
            !HasValidKeyCount(found->keys.size(), keys.size(), layout.allowUnassignedAuxiliaryKeys))
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
            if (key == 0 && layout.allowUnassignedAuxiliaryKeys) continue;
            // Preserve pause/reset controls and the global performance-display shortcut.
            if (IsReservedKey(key))
                throw std::invalid_argument("This key is reserved for game controls.");
            if (!used.insert(key).second)
                throw std::invalid_argument("Each input in a mode must use a different key.");
        }
    }
    return layout;
}
