#pragma once
#include "PhantomCore.h"
#include <optional>
#include <string_view>

// One page of the editor under the tab bar. EditorView keeps one per EditorTab, draws the current
// one into each build and hands it the input the shared shell (menu, footer, modals) leaves.
class IEditorTab
{
  public:
    virtual ~IEditorTab() = default;
    // Text key of the tab bar caption.
    [[nodiscard]] virtual std::string_view CaptionKey() const noexcept = 0;
    // Retained nodes under the editor node, made once before the first build.
    virtual void Initialize(PhantomCore::Visual2DNode &editorNode) { static_cast<void>(editorNode); }
    virtual void Shutdown() noexcept {}
    // The frame was laid out again (window size).
    virtual void Resize() {}
    // At the start of each build: whether this tab is the current one.
    virtual void Show(bool current) { static_cast<void>(current); }
    // Draws the tab body for one build. Only the current tab draws.
    virtual void Build() = 0;
    // A build failed: hide retained nodes until the next successful build.
    virtual void BuildFailed() noexcept {}
    // After the body is drawn: whether the tab's input nodes take the pointer.
    virtual void SyncInput(bool usable) { static_cast<void>(usable); }
    // Whether the right panel narrows the content of this build.
    [[nodiscard]] virtual bool HasInspector() const noexcept { return false; }
    // Keys after the shared shortcuts, without Ctrl or Alt, while no field types and no modal is open.
    virtual void UpdateKeys(const PhantomCore::InputState &input) { static_cast<void>(input); }
    // One wheel step (+1 later) over the frame; the shell rebuilds afterwards.
    virtual void Wheel(int direction, std::optional<PhantomCore::Point> point, bool control)
    {
        static_cast<void>(direction);
        static_cast<void>(point);
        static_cast<void>(control);
    }
    // The pointer before shared controls take a press; true when the tab used it (a drag, a right click).
    virtual bool UpdatePointer(const PhantomCore::InputState &input, PhantomCore::Point point)
    {
        static_cast<void>(input);
        static_cast<void>(point);
        return false;
    }
    // A left press that no shared control took.
    virtual void Press(const PhantomCore::InputState &input, PhantomCore::Point point, bool doubleClick)
    {
        static_cast<void>(input);
        static_cast<void>(point);
        static_cast<void>(doubleClick);
    }
};
