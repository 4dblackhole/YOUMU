#pragma once

#include "App/OptionSettings.h"
#include "PhantomCore.h"

#include <string>

// The Option.ini [Diagnostics] graphics switches (validation, adapter, tearing, swap-chain buffers, waitable
// swap chain, render rate override) are for chasing a lost or hung graphics device. A build made with the MSBuild property
// YoumuGraphicsSwitches=false (YOUMU_GRAPHICS_SWITCHES=0) ignores them, so an edited Option.ini cannot switch a
// build handed to players into validation or a software adapter. DeviceRemovedData is not one of them.
[[nodiscard]] bool GraphicsSwitchesAllowed() noexcept;

// The engine's graphics settings for this run: Option.ini's, or the engine defaults where the build ignores them.
[[nodiscard]] PhantomCore::GraphicsDeviceDiagnosticsConfig GraphicsSwitchesForRun(const OptionValues &values);

// EngineConfig::renderRateOverrideHz for this run: Option.ini's RenderRateOverrideHz, or 0 (the display rate)
// where the build ignores the switches.
[[nodiscard]] double RenderRateOverrideForRun(const OptionValues &values) noexcept;

// One log line: "graphics switches: validation 0, adapter Default, tearing 1, buffers 2, waitable 0, render rate
// override 0", with "ignored by this build" before the values when they are not used.
[[nodiscard]] std::string DescribeGraphicsSwitches(const OptionValues &values);
