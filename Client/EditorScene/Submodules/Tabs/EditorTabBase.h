#pragma once
#include "../EditorView.h"
#include "IEditorTab.h"

// What every tab works with: the workspace, the texts and the shell's drawing on the editor
// surface. The forwards keep tab code in the same terms as the shell's (frame coordinates).
class EditorTabBase : public IEditorTab
{
  protected:
    EditorTabBase(EditorView &view, EditorWorkspace &state, TextCatalog &texts) noexcept
        : view_(view), state_(state), texts_(texts), frame_(view.FrameLayout())
    {
    }

    void Box(PhantomCore::Rect r, PhantomCore::Color color, float radius = 0) { view_.Box(r, color, radius); }
    void Label(PhantomCore::Rect r, std::wstring text, EditorTextStyle style = {})
    {
        view_.Label(r, std::move(text), style);
    }
    void Text(PhantomCore::Rect r, std::wstring text, float size = 20, PhantomCore::Color color = editor_ui::Ink)
    {
        view_.Text(r, std::move(text), size, color);
    }
    void Icon(PhantomCore::Rect r, std::wstring_view name, PhantomCore::Color tint) { view_.Icon(r, name, tint); }
    void Image(PhantomCore::Rect r, const std::filesystem::path &path, PhantomCore::Color tint)
    {
        view_.Image(r, path, tint);
    }
    void Button(PhantomCore::Rect r, std::wstring text, std::function<void()> action, bool selected = false)
    {
        view_.Button(r, std::move(text), std::move(action), selected);
    }
    void Control(PhantomCore::Rect r, std::function<void()> action) { view_.Control(r, std::move(action)); }
    void Field(PhantomCore::Rect r, const wchar_t *label, std::string &value) { view_.Field(r, label, value); }
    void InputField(PhantomCore::Rect r, std::wstring label, std::wstring text, EditorTextFields::Commit commit,
                    bool number = false, bool showLabel = true, EditorTextFields::Commit typing = {})
    {
        view_.InputField(r, std::move(label), std::move(text), std::move(commit), number, showLabel,
                         std::move(typing));
    }
    void DrawSwitch(PhantomCore::Rect row, std::wstring caption, bool on, std::function<void()> toggle)
    {
        view_.DrawSwitch(row, std::move(caption), on, std::move(toggle));
    }
    void PlaceNode(PhantomCore::Visual2DNode &target, PhantomCore::Rect r) const { view_.PlaceNode(target, r); }
    [[nodiscard]] float PixelScale() const noexcept { return view_.PixelScale(); }
    [[nodiscard]] EditorTextSet Texts() const noexcept { return view_.Texts(); }
    [[nodiscard]] EditorModeAreas Areas() const noexcept { return view_.Areas(); }
    [[nodiscard]] float ContentRight() const noexcept { return view_.ContentRight(); }
    [[nodiscard]] PhantomCore::Rect PanelBody() const noexcept { return view_.PanelBody(); }

    EditorView &view_;
    EditorWorkspace &state_;
    TextCatalog &texts_;
    // The view's current frame; it changes with the window size.
    const editor_layout::Frame &frame_;
};
