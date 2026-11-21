#include "Mode/ModeInputLayout.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace
{
// Overrides may leave slots unassigned only when the layout allows it.
[[nodiscard]] bool HasValidKeyCount(std::size_t slots, std::size_t keyCount, bool allowUnassignedKeys)
{
    if (keyCount == 0 || keyCount > slots)
        return false;
    return allowUnassignedKeys || keyCount == slots;
}
} // namespace

bool IsReservedGameControlKey(PhysicalKey key) noexcept
{
    if (key < 8 || key > 254)
        return true;
    switch (key)
    {
    case 27:   // ESC: pause
    case 32:   // Space
    case 0x76: // F7: performance display
        return true;
    default:
        return false;
    }
}

ModeInputLayout ResolveInputLayout(ModeInputLayout layout, const InputKeyOverrides &overrides)
{
    for (const auto &[id, keys] : overrides)
    {
        auto found = std::ranges::find(layout.bindings, id, &ModeInputBinding::id);
        if (found == layout.bindings.end() ||
            !HasValidKeyCount(found->keys.size(), keys.size(), layout.allowUnassignedKeys))
            throw std::invalid_argument("Unknown input binding or incorrect key count: " + id);
        const auto slots = found->keys.size();
        found->keys = keys;
        found->keys.resize(slots, 0);
    }
    std::set<PhysicalKey> used;
    for (const auto &binding : layout.bindings)
    {
        if (binding.keys.empty())
            throw std::invalid_argument("An input binding needs at least one key slot.");
        if (binding.keys.front() == 0 && !layout.allowUnassignedKeys)
            throw std::invalid_argument("The primary key cannot be empty.");
        for (const auto key : binding.keys)
        {
            if (key == 0 && layout.allowUnassignedKeys) continue;
            // Preserve the pause control and the global performance-display shortcut. The Client keeps its
            // configurable restart key apart from every play key itself.
            if (IsReservedGameControlKey(key))
                throw std::invalid_argument("This key is reserved for game controls.");
            if (!used.insert(key).second)
                throw std::invalid_argument("Each input in a mode must use a different key.");
        }
    }
    return layout;
}

ModeInputLayout LimitKeysPerBinding(ModeInputLayout layout, const std::size_t keys)
{
    if (keys == 0)
        throw std::invalid_argument("A binding must keep at least one key.");
    for (auto &binding : layout.bindings)
    {
        std::size_t kept = 0;
        for (auto &key : binding.keys)
        {
            if (key == 0)
                continue;
            if (kept < keys)
            {
                ++kept;
                continue;
            }
            // Only a layout with empty slots can lose a key; elsewhere every slot must hold one.
            if (!layout.allowUnassignedKeys)
                throw std::invalid_argument("This input layout cannot leave key slots unassigned.");
            key = 0;
        }
    }
    return layout;
}
