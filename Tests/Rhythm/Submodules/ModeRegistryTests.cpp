#include "Mode/PlayGameModeFactory.h"
#include "TestSupport.h"

#include <set>

namespace tests
{
    void TestPlayGameModeRegistry()
    {
        // Every registered entry must construct the mode it names, and the
        // settings screen must receive exactly one valid default layout per mode.
        const auto layouts = AvailableInputLayouts();
        Require(!layouts.empty(), "At least one playable mode must be registered.");
        std::set<std::string> ids;
        for (const auto &layout : layouts)
        {
            Require(ids.insert(layout.modeId).second, "Each registered mode must be listed once: " + layout.modeId);
            Require(IsSupportedPlayGameMode(layout.modeId), "A listed layout must name a supported mode.");
            const auto mode = CreatePlayGameMode(layout.modeId);
            Require(mode->Id() == layout.modeId && mode->InputLayout().modeId == layout.modeId,
                    "The registry id, the mode id and its layout id must agree: " + layout.modeId);
            Require(!layout.bindings.empty(), "A playable mode must declare at least one input binding.");
            static_cast<void>(ResolveInputLayout(layout, {}));
        }

        for (const std::string_view unknown : {"", "Unknown", "taiko"})
        {
            Require(!IsSupportedPlayGameMode(unknown), "Mode ids are exact registry keys.");
            bool rejected = false;
            try { static_cast<void>(CreatePlayGameMode(unknown)); }
            catch (const std::invalid_argument &) { rejected = true; }
            Require(rejected, "Creating an unregistered mode must fail before a session exists.");
        }

        PatternDocument unregistered;
        unregistered.mode = "Unknown";
        Require(!CountPlayableNotes(unregistered).has_value(),
                "Song selection must not count notes for a mode it cannot play.");
    }
}
