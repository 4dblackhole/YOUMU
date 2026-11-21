#include "../../EditorSupport.h"
#include "App/AssetPaths.h"
#include "App/SkinSetSelection.h"
#include "TaikoEditorMode.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
using namespace PhantomCore;

using namespace editor_ui;

namespace
{
    constexpr std::array<std::uint16_t, 6> ToolKeys{'W', 'E', 'R', 'T', 'Y', 'U'};

}

void TaikoEditorMode::ActivateGroup(IEditorContext &state, std::size_t group)
{
    int next = groupTools_.at(group);
    if (tool_ == next)
    {
        // Cycle only within the active set; returning to another set keeps its variant.
        next = NextInGroup(next);
    }
    state.SelectTool(next);
}

void TaikoEditorMode::HandleToolKeys(IEditorContext &state, const InputState &input)
{
    if (input.WasKeyPressed('Q'))
    {
        HandleToolKey(state, 'Q');
        return;
    }
    for (const auto key : ToolKeys)
        if (input.WasKeyPressed(key))
            HandleToolKey(state, key);
}

bool TaikoEditorMode::HandleToolKey(IEditorContext &state, std::uint16_t key)
{
    if (key == 'Q')
    {
        state.SelectTool(0);
        return true;
    }
    const auto found = std::ranges::find(ToolKeys, key);
    if (found == ToolKeys.end())
        return false;
    ActivateGroup(state, static_cast<std::size_t>(found - ToolKeys.begin()));
    return true;
}

namespace
{
    constexpr float ButtonWidth = editor_layout::ToolButtonWidth;
    PhantomCore::Rect ToolButton(PhantomCore::Rect strip, std::size_t index) noexcept
    {
        return {strip.x + editor_layout::ToolStripPadding + ButtonWidth * static_cast<float>(index), strip.y,
                ButtonWidth, strip.height};
    }
    std::size_t VariantCount(Group group) noexcept
    {
        return static_cast<std::size_t>(std::ranges::count(Tools, group, &Tool::group));
    }
} // namespace

void TaikoEditorMode::DrawTools(IEditorModeCanvas &canvas, IEditorContext &state,
                                const TextCatalog &)
{
    areas_ = canvas.Areas();
    const auto strip = areas_.tools;
    canvas.Box(strip, ui_colors::theme::BrandDeep);
    canvas.Image(strip, EditorSound(L"ToolStrip.png"), White);
    // Q selects; W..U pick a note group, and pressing the current one cycles its variants.
    const auto drawButton = [&](std::size_t index, bool on, wchar_t key) {
        const auto r = ToolButton(strip, index);
        if (on)
        {
            canvas.Box(r, {1, 1, 1, .14F});
            canvas.Box({r.x, r.y + r.height - 3, r.width, 3}, ui_colors::theme::Sun);
        }
        canvas.Text({r.x + r.width - 13, r.y + 3, 10, 12}, std::wstring(1, key), 10,
                    on ? ui_colors::theme::Sun : Color{1, 1, 1, .55F});
        return r;
    };
    const auto select = drawButton(0, tool_ == 0, L'Q');
    canvas.Image({select.x + 12, select.y + 10, 24, 24}, SkinSetSelection::Instance().Resolve(L"Editor/SelectionTool.png"),
                 White);
    canvas.Region(select, [&state] { state.SelectTool(0); });
    for (std::size_t i = 0; i < groupTools_.size(); ++i)
    {
        const int id = groupTools_[i];
        const auto r = drawButton(i + 1, tool_ == id, static_cast<wchar_t>(ToolKeys[i]));
        const auto *tool = Find(id);
        const float radius = i == 1 ? 9.5F : 7.0F;
        if (tool && FindTaikoNote(static_cast<int>(tool->note))->longNote)
            LongNoteIcon(canvas, r, id, radius);
        else
            Circle(canvas, r.x + r.width * .5F, r.y + 22, id, radius);
        if (tool && VariantCount(tool->group) > 1)
        {
            // One dot per variant; the current variant is gold.
            const auto count = VariantCount(tool->group);
            float x = r.x + (r.width - (4.0F * count + 3.0F * (count - 1))) * .5F;
            for (const auto &variant : Tools)
            {
                if (variant.group != tool->group)
                    continue;
                canvas.Box({x, r.y + r.height - 10, 4, 4}, variant.id == id ? ui_colors::theme::Sun : Color{1, 1, 1, .28F});
                x += 7;
            }
        }
        canvas.Region(r, [this, &state, i] { ActivateGroup(state, i); });
    }
}

bool TaikoEditorMode::OpenToolMenu(Point point)
{
    const auto strip = areas_.tools;
    if (!strip.Contains(point))
        return false;
    const auto index = static_cast<std::size_t>((point.x - strip.x - editor_layout::ToolStripPadding) / ButtonWidth);
    if (index < 1 || index > groupTools_.size())
        return false;
    const auto *tool = Find(groupTools_[index - 1]);
    if (!tool || VariantCount(tool->group) < 2)
        return false;
    popup_ = static_cast<int>(index - 1);
    pending_.reset();
    return true;
}

void TaikoEditorMode::DrawToolMenu(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const TextCatalog &catalog)
{
    if (popup_ < 0)
        return;
    const auto anchor = ToolButton(areas_.tools, static_cast<std::size_t>(popup_) + 1);
    const int current = groupTools_.at(static_cast<std::size_t>(popup_));
    std::size_t row = 0;
    for (const auto &tool : Tools)
        if (static_cast<int>(tool.group) == popup_)
        {
            const PhantomCore::Rect r{anchor.x, anchor.y + anchor.height + 34 * static_cast<float>(row++), 208, 34};
            canvas.Button(r, std::wstring(Name(tool.id, TaikoEditor(catalog).toolVariants)),
                          [&state, id = tool.id] { state.SelectTool(id); }, tool_ == tool.id && current == tool.id);
        }
}
