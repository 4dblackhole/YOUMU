#pragma once
#include "PhantomCore.h"
#include <string>

enum class ResultAction
{
    None,
    Back,
    Retry
};
struct ResultViewContext
{
    std::wstring songTitle;
    std::wstring patternCreator;
    bool canRetry{};
    std::wstring songArtist;
    std::filesystem::path recordDatabase;
    std::filesystem::path backgroundPath;
    double backgroundOffsetX{}, backgroundOffsetY{}; // Logical units at reference height 720.
};
class IResultView
{
  public:
    virtual ~IResultView() = default;
    virtual void Initialize(const PhantomCore::EngineServices &services) = 0;
    virtual void SetVisible(bool visible) noexcept = 0;
    // deltaSeconds paces hover hints; input is empty while something covers the result.
    virtual ResultAction Update(const PhantomCore::InputState &input, double deltaSeconds) = 0;
    virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void Shutdown() noexcept = 0;
};
