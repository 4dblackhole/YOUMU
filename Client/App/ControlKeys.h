#pragma once

#include <cstdint>

// Gameplay control keys that are not play inputs, as Win32 Virtual-Keys.
// Holding the restart key for half a second restarts the attempt. The player may choose it in the
// options; it can be neither a play key nor a fixed game control (IsReservedGameControlKey).
inline constexpr std::uint16_t DefaultRestartKey = 0xC0; // VK_OEM_3: backquote (`)
