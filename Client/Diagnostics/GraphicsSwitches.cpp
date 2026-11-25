#include "GraphicsSwitches.h"

// YOUMU.Client.vcxproj defines it from the MSBuild property YoumuGraphicsSwitches (default true). A build of this
// file that does not define it, such as a test runner's, honours the switches.
#if !defined(YOUMU_GRAPHICS_SWITCHES)
#define YOUMU_GRAPHICS_SWITCHES 1
#endif

bool GraphicsSwitchesAllowed() noexcept
{
    return YOUMU_GRAPHICS_SWITCHES != 0;
}

PhantomCore::GraphicsDeviceDiagnosticsConfig GraphicsSwitchesForRun(const OptionValues &values)
{
    return GraphicsSwitchesAllowed() ? values.graphics : PhantomCore::GraphicsDeviceDiagnosticsConfig{};
}

double RenderRateOverrideForRun(const OptionValues &values) noexcept
{
    return GraphicsSwitchesAllowed() ? static_cast<double>(values.renderRateOverrideHz) : 0.0;
}

std::string DescribeGraphicsSwitches(const OptionValues &values)
{
    const auto &graphics = values.graphics;
    // The Option.ini values, so a line can be copied back into the file.
    return std::string("graphics switches: ") + (GraphicsSwitchesAllowed() ? "" : "ignored by this build, ") +
           "validation " + std::to_string(static_cast<int>(graphics.validation)) + ", adapter " +
           GraphicsAdapterSetting(graphics) + ", tearing " + (graphics.allowTearing ? "1" : "0") + ", buffers " +
           std::to_string(graphics.swapChainBuffers) + ", waitable " + (graphics.waitableSwapChain ? "1" : "0") +
           ", render rate override " + std::to_string(values.renderRateOverrideHz);
}
