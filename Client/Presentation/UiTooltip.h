#pragma once

#include "PhantomCore.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// A control's hint (UI-02 common part): its name, an optional lighter note and its shortcut keys.
// Keys are written like L"Ctrl+O" or L"Esc / Enter" and are drawn as key chips.
struct TooltipText
{
    std::wstring label;
    std::wstring keys;
    std::wstring note;

    [[nodiscard]] bool Empty() const noexcept
    {
        return label.empty() && keys.empty() && note.empty();
    }
};

// The tooltip of one Canvas: a navy plate with white text, 8px under the pointed control (above it
// when there is no room), its arrow at the control's centre. It appears after the pointer rests on a
// registered control for 0.5 s, at once when moving straight from one hint to the next, and hides on
// a press, the wheel, a key or when its host hides it.
class UiTooltip final
{
  public:
    static constexpr double DelaySeconds = 0.5, WarmSeconds = 0.3, WarmDelaySeconds = 0.06;

    void Initialize(PhantomCore::Visual2DCanvas &canvas);
    void Shutdown() noexcept;
    // Registers or replaces a control's hint; an empty hint removes it. A pointed child shows its
    // nearest registered ancestor's hint.
    void Set(const PhantomCore::Visual2DNode &target, TooltipText text);
    // Hints a line inside a control that keeps the pointer itself (a song card's title): it shows
    // while the pointer is over the line and the pointed control contains it. Needs the pointer.
    void SetLine(const PhantomCore::Visual2DNode &line, TooltipText text);
    // hovered comes from the host's input router; enabled is false while a modal hides the controls.
    // pointer (Canvas coordinates) is needed only for lines registered with SetLine.
    void Update(double deltaSeconds, const PhantomCore::InputState &input, PhantomCore::NodeId hovered,
                bool enabled = true, std::optional<PhantomCore::Point> pointer = std::nullopt);
    void Hide() noexcept;
    // Forgets every hint, for a host that rebuilds its controls.
    void Clear() noexcept;
    [[nodiscard]] bool IsShown() const noexcept;
    [[nodiscard]] PhantomCore::NodeId ShownTarget() const noexcept
    {
        return shown_ ? armed_ : PhantomCore::NodeId{};
    }

  private:
    struct Chip
    {
        PhantomCore::Visual2DNode *plate{}, *line{}, *text{};
    };
    [[nodiscard]] PhantomCore::NodeId ResolveTarget(PhantomCore::NodeId hovered,
                                                    std::optional<PhantomCore::Point> pointer) const;
    [[nodiscard]] PhantomCore::NodeId ResolveLine(PhantomCore::NodeId hovered, PhantomCore::Point pointer) const;
    void Show();
    void BuildContent(const TooltipText &text);
    void Layout();
    [[nodiscard]] float TextWidth(PhantomCore::Visual2DNode &node, float size) const;
    [[nodiscard]] Chip &ChipAt(std::size_t index);
    [[nodiscard]] PhantomCore::Visual2DNode &SeparatorAt(std::size_t index);

    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DNode *root_{}, *arrow_{}, *plate_{}, *label_{}, *note_{};
    std::vector<Chip> chips_;
    std::vector<PhantomCore::Visual2DNode *> separators_;
    // The key chips of the shown hint in order, with the separators between them.
    std::vector<std::wstring> keyTexts_;
    std::vector<std::wstring> separatorTexts_;
    std::map<PhantomCore::NodeId, TooltipText> hints_;
    // Hinted lines that take no pointer themselves (SetLine).
    std::set<PhantomCore::NodeId> lines_;
    PhantomCore::NodeId armed_{}, dismissed_{};
    double waitSeconds_{}, clock_{}, hiddenAt_{-1.0};
    bool shown_{};
    float laidOutWidth_{};
    // Lines of the shown label once a long name is broken to the plate width.
    int labelLines_{1};
};
