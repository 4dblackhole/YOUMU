// Option.ini [Diagnostics] graphics switches: reading, writing back with a comment line per key, rejecting bad values,
// and whether the build passes them to the engine. RunGraphicsSwitchesTests.ps1 builds this twice, with
// YOUMU_GRAPHICS_SWITCHES=1 and =0, and runs each with "allowed" or "blocked".
#include "App/OptionSettings.h"
#include "Diagnostics/GraphicsSwitches.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace PhantomCore;

namespace
{
    void Check(const bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    void WriteFile(const std::filesystem::path &path, const std::string_view text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }

    // Loads text as a whole Option.ini; false when the file is rejected.
    bool Load(const std::filesystem::path &path, const std::string_view text, OptionValues &values)
    {
        WriteFile(path, text);
        OptionSettings settings(path);
        std::string error;
        const bool loaded = settings.Load(error);
        values = settings.Values();
        return loaded;
    }

    bool SameSwitches(const GraphicsDeviceDiagnosticsConfig &left, const GraphicsDeviceDiagnosticsConfig &right)
    {
        const bool device = left.validation == right.validation && left.adapter == right.adapter;
        const bool adapter = left.adapterIndex == right.adapterIndex && left.adapterName == right.adapterName;
        const bool tearing = left.allowTearing == right.allowTearing;
        const bool swapChain = left.swapChainBuffers == right.swapChainBuffers && left.waitableSwapChain == right.waitableSwapChain;
        const bool presentation = tearing && swapChain;
        return device && adapter && presentation;
    }

    constexpr std::string_view Experiment = "[Diagnostics]\r\nDeviceRemovedData=1\r\nGraphicsValidation=2\r\n"
                                            "GraphicsAdapter=warp\r\nAllowTearing=0\r\nSwapChainBuffers=3\r\n"
                                            "WaitableSwapChain=1\r\nRenderRateOverrideHz=300\r\n";

    GraphicsDeviceDiagnosticsConfig ExperimentSwitches()
    {
        GraphicsDeviceDiagnosticsConfig graphics;
        graphics.validation = GraphicsValidation::GpuBased;
        graphics.adapter = GraphicsAdapterRequest::Warp;
        graphics.allowTearing = false;
        graphics.swapChainBuffers = 3;
        graphics.waitableSwapChain = true;
        return graphics;
    }

    void TestReading(const std::filesystem::path &ini)
    {
        OptionValues values;
        Check(Load(ini, "[General]\r\nLanguage=en\r\n", values), "A file without [Diagnostics] loads.");
        Check(SameSwitches(values.graphics, {}) && values.renderRateOverrideHz == 0,
              "Missing graphics switches keep the engine defaults.");
        Check(Load(ini, Experiment, values), "The experiment switches load.");
        Check(SameSwitches(values.graphics, ExperimentSwitches()) && values.deviceRemovedData,
              "Every graphics switch is read, WARP in any case.");
        Check(values.renderRateOverrideHz == 300, "The render rate override is read.");

        Check(Load(ini, "[Diagnostics]\r\nGraphicsAdapter=1\r\n", values), "An adapter number loads.");
        const bool index = values.graphics.adapter == GraphicsAdapterRequest::Index && values.graphics.adapterIndex == 1;
        Check(index, "A number names the adapter by its place in the list.");
        Check(Load(ini, "[Diagnostics]\r\nGraphicsAdapter=GeForce\r\n", values), "An adapter name loads.");
        const bool named = values.graphics.adapter == GraphicsAdapterRequest::Name && values.graphics.adapterName == "GeForce";
        Check(named, "Other text names the adapter by part of its name.");

        for (const char *bad : {"[Diagnostics]\r\nGraphicsValidation=3\r\n", "[Diagnostics]\r\nSwapChainBuffers=4\r\n",
                                "[Diagnostics]\r\nAllowTearing=2\r\n", "[Diagnostics]\r\nWaitableSwapChain=-1\r\n",
                                "[Diagnostics]\r\nRenderRateOverrideHz=5\r\n"})
            Check(!Load(ini, bad, values), "An out-of-range graphics switch rejects the file like DeviceRemovedData does.");
    }

    void TestWriting(const std::filesystem::path &ini)
    {
        OptionValues values;
        Check(Load(ini, Experiment, values), "The experiment switches load.");
        values.graphics.adapter = GraphicsAdapterRequest::Name;
        values.graphics.adapterName = "Intel(R) UHD";
        OptionSettings settings(ini);
        std::string error;
        Check(settings.Save(values, error), "The switches are saved with the rest of Option.ini.");
        OptionSettings reread(ini);
        Check(reread.Load(error) && SameSwitches(reread.Values().graphics, values.graphics),
              "Saved switches read back the same.");
        Check(reread.Values().renderRateOverrideHz == 300, "The saved render rate override reads back the same.");
        values.graphics.adapterName = "123";
        Check(!settings.Save(values, error), "An adapter name that would read back as a number is not saved.");
    }

    std::string ReadFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    // The saved [Diagnostics] section, up to the next section.
    std::string DiagnosticsText(const std::string &text)
    {
        const auto section = text.find("[Diagnostics]");
        Check(section != std::string::npos, "The saved file has [Diagnostics].");
        const auto next = text.find("\r\n[", section);
        return text.substr(section, next == std::string::npos ? std::string::npos : next - section);
    }

    // Save puts a comment line, in ASCII, directly before each [Diagnostics] key; comments change nothing that loads.
    void TestComments(const std::filesystem::path &ini)
    {
        OptionValues values;
        Check(Load(ini, Experiment, values), "The experiment switches load.");
        OptionSettings settings(ini);
        std::string error;
        Check(settings.Save(values, error), "Option.ini with the experiment switches is saved.");
        const std::string diagnostics = DiagnosticsText(ReadFile(ini));
        for (const char *key : {"DeviceRemovedData", "GraphicsValidation", "GraphicsAdapter", "AllowTearing",
                                "SwapChainBuffers", "WaitableSwapChain", "RenderRateOverrideHz"})
        {
            const auto comment = diagnostics.find(std::string("\r\n; ") + key + ": ");
            const auto entry = diagnostics.find(std::string("\r\n") + key + "=");
            const bool directlyBefore = comment != std::string::npos && diagnostics.find("\r\n", comment + 2) == entry;
            Check(entry != std::string::npos && directlyBefore, "Each [Diagnostics] key has its comment just before it.");
        }
        const auto isAscii = [](const char c) { return static_cast<unsigned char>(c) < 0x80; };
        const bool ascii = std::ranges::all_of(diagnostics, isAscii);
        Check(ascii, "The [Diagnostics] comments are ASCII.");

        OptionSettings reread(ini);
        Check(reread.Load(error) && reread.LoadWarning().empty(), "A saved file with its comments loads without a warning.");
        const bool same = SameSwitches(reread.Values().graphics, values.graphics) && reread.Values().deviceRemovedData;
        Check(same && reread.Values().renderRateOverrideHz == 300, "The saved values read back the same past the comments.");

        Check(Load(ini, "[Diagnostics]\r\n; a note\r\n# another\r\n   ; indented\r\nGraphicsValidation=1\r\n", values),
              "Comment lines typed by hand load.");
        Check(values.graphics.validation == GraphicsValidation::DebugLayer, "The value after the comment lines is read.");
        Check(!Load(ini, "[Diagnostics]\r\nGraphicsValidation=1 ; a note\r\n", values),
              "Text after a value is part of the value, so the file is rejected, as the saved comment says.");
    }

    void TestBuildSwitch(const bool expectAllowed, const std::filesystem::path &ini)
    {
        OptionValues values;
        Check(Load(ini, Experiment, values), "The experiment switches load.");
        Check(GraphicsSwitchesAllowed() == expectAllowed, "The build's switch setting is the one the runner built.");
        const auto forRun = GraphicsSwitchesForRun(values);
        const std::string line = DescribeGraphicsSwitches(values);
        const bool saysIgnored = line.find("ignored by this build") != std::string::npos;
        Check(line.find("validation 2, adapter WARP, tearing 0, buffers 3, waitable 1, render rate override 300") !=
                  std::string::npos,
              "The log line shows what Option.ini asks for.");
        const double rate = RenderRateOverrideForRun(values);
        if (expectAllowed)
        {
            Check(SameSwitches(forRun, ExperimentSwitches()) && !saysIgnored, "A normal build passes the switches on.");
            Check(rate == 300.0, "A normal build passes the render rate override on.");
            return;
        }
        Check(SameSwitches(forRun, {}), "A build without the switches gives the engine its defaults whatever Option.ini says.");
        Check(rate == 0.0, "A build without the switches keeps the display rate whatever Option.ini says.");
        Check(saysIgnored, "The log line says the build ignores the switches.");
    }
} // namespace

int wmain(const int argc, wchar_t **argv)
{
    try
    {
        if (argc != 3)
            throw std::runtime_error("Usage: GraphicsSwitchesTests allowed|blocked <folder>");
        const bool expectAllowed = std::wstring_view(argv[1]) == L"allowed";
        const std::filesystem::path folder = argv[2];
        std::filesystem::create_directories(folder);
        const auto ini = folder / L"Option.ini";
        TestReading(ini);
        TestWriting(ini);
        TestComments(ini);
        TestBuildSwitch(expectAllowed, ini);
        std::cout << "Graphics switches passed (" << (expectAllowed ? "allowed" : "blocked") << " build).\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
