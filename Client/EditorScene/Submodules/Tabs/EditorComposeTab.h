#pragma once
#include "EditorTabBase.h"
#include <span>
#include <utility>

// Compose: tool row, channel heads, ruler, the mode's chart, realtime channels and the right panel.
class EditorComposeTab final : public EditorTabBase
{
  public:
    EditorComposeTab(EditorView &view, EditorWorkspace &state, TextCatalog &texts) noexcept
        : EditorTabBase(view, state, texts)
    {
    }
    [[nodiscard]] std::string_view CaptionKey() const noexcept override { return "Editor.tabs.0"; }
    void Build() override;
    [[nodiscard]] bool HasInspector() const noexcept override { return state_.Panel().has_value(); }
    void UpdateKeys(const PhantomCore::InputState &input) override;
    void Wheel(int direction, std::optional<PhantomCore::Point> point, bool control) override;
    bool UpdatePointer(const PhantomCore::InputState &input, PhantomCore::Point point) override;
    void Press(const PhantomCore::InputState &input, PhantomCore::Point point, bool doubleClick) override;

  private:
    void DrawToolRow();
    void DrawBeatControl();
    void DrawLineBreakToggle();
    void DrawHeads();
    void DrawZoneHeads();
    void DrawChannelHead(editor_layout::Channel, std::wstring_view icon, std::wstring name, std::wstring value);
    void DrawRuler();
    void DrawPanel();
    void StepSpacing(int direction);
    [[nodiscard]] std::wstring HeadValue(editor_layout::Channel) const;

    // Realtime channels under the lane, negative-delay cuts and the dragged beat range (EditorComposeChannels).
    void DrawChannels();
    void DrawCutsAndRange();
    void Chip(float x, float middle, std::wstring text, PhantomCore::Color background, bool selected, float h = 18);
    [[nodiscard]] double AutomationValueAt(EffectCommandType, std::string_view target, double fallback) const;

    // The right panel of the open channel (EditorComposePanels).
    void DrawPanelHeader(std::wstring title);
    void DrawTimingPanel(PhantomCore::Rect);
    void DrawEffectPanel(PhantomCore::Rect, EditorPanel);
    void DrawMusicPanel(PhantomCore::Rect);
    void DrawListHeader(PhantomCore::Rect body, std::span<const std::pair<std::wstring_view, float>> columns);
    void DrawListRow(PhantomCore::Rect row, std::span<const std::pair<std::wstring, float>> cells, bool selected,
                     bool primary, std::function<void()> select, PhantomCore::Color color = editor_ui::Ink);
    // The panel's last row: Delete at the left, Add and Apply at the right; returns the row's top.
    float DrawPanelActions(PhantomCore::Rect body, std::size_t selected, std::function<void()> remove,
                           std::function<void()> add, std::function<void()> apply);
    void Segmented(PhantomCore::Rect row, std::span<const std::wstring> options, std::size_t selected,
                   std::function<void(std::size_t)> choose);
    void SelectEffectType(const EditorEffectSelection &);
    void DrawExcludeChips(PhantomCore::Rect row);
    // A clock button beside position fields: fills them with the Beat tick nearest the cursor.
    void CursorPositionButton(PhantomCore::Rect r, std::string &measure, std::string *fraction);
    void DrawCurveRow(PhantomCore::Rect row);
    std::wstring EffectLabel(const EditorEffectSelection &) const;
    void SelectEffectRow(std::size_t);
    [[nodiscard]] bool EffectInPanel(std::size_t row, EditorPanel) const;
};
