#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "IEditorAudioSource.h"
#include "IEditorContext.h"
#include "../EditorLayout.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <map>
#include <memory>

// Places the common view lends a mode for one build, in frame coordinates.
struct EditorModeAreas
{
    PhantomCore::Rect tools{editor_layout::ToolStripLeft, editor_layout::BodyTop, editor_layout::ToolStripWidth,
                            editor_layout::ToolRowHeight};
    PhantomCore::Rect lane{editor_layout::HeadWidth, editor_layout::LaneTop,
                           editor_layout::BaseWidth - editor_layout::HeadWidth, editor_layout::LaneHeight};
    // Current-time x of the realtime lane (the judgement line).
    float judgeX{editor_layout::HeadWidth + editor_layout::JudgeOffset};
    PhantomCore::Rect score{0, editor_layout::TrackTop, editor_layout::BaseWidth,
                            editor_layout::FooterTop - editor_layout::TrackTop};
    // The body between the tab bar and the footer (Metadata).
    PhantomCore::Rect body{0, editor_layout::BodyTop, editor_layout::BaseWidth, editor_layout::BodyHeight};
    // The chart's BG image for Preview and Metadata; empty when it has none.
    std::filesystem::path background;
    // Body of the open note panel; empty while it is closed.
    PhantomCore::Rect inspector{};
};

// Drawing primitives supplied by the common view. A mode owns its layout,
// tool palette and hit testing; it never retains this per-build drawing target.
class IEditorModeCanvas
{
  public:
    virtual ~IEditorModeCanvas() = default;
    virtual EditorModeAreas Areas() const noexcept { return {}; }
    // Device pixels per local drawing unit, including the editor's layout scale.
    virtual float PixelScale() const noexcept { return 1; }
    virtual void Box(PhantomCore::Rect, PhantomCore::Color, float radius = 0) = 0;
    virtual void Text(PhantomCore::Rect, std::wstring, float size = 20,
                      PhantomCore::Color color = {.17F, .34F, .47F, 1}) = 0;
    virtual void Image(PhantomCore::Rect, const std::filesystem::path &, PhantomCore::Color) = 0;
    virtual void Button(PhantomCore::Rect, std::wstring, std::function<void()>, bool selected = false) = 0;
    // A labelled single-line field; `commit` receives the typed text (UTF-8) and may throw to reject it.
    virtual void TextField(PhantomCore::Rect r, std::wstring label, std::wstring text,
                           std::function<void(const std::string &)> commit, bool number = false)
    {
        // Without inline editors the caption sits above and the value reads as a button.
        static_cast<void>(number);
        static_cast<void>(commit);
        if (!label.empty())
            Text({r.x, r.y - 18, r.width, 16}, std::move(label), 12);
        Button(r, std::move(text), [] {});
    }
    // A clickable area over artwork the mode drew itself.
    virtual void Region(PhantomCore::Rect r, std::function<void()> action) { Button(r, {}, std::move(action)); }
};

struct EditorSoundChoice
{
    int keyType{};
    std::wstring label;
    std::wstring effectLabel;
};

class IEditorMode : public IEditorAudioSource
{
  public:
    virtual ~IEditorMode() = default;
    virtual std::string_view Id() const noexcept = 0;
    virtual std::unique_ptr<IEditorDocument> OpenDocument(
        const GameplayLaunchRequest &) const = 0;
    virtual void SelectTool(int) = 0;
    virtual void HandleToolKeys(IEditorContext &, const PhantomCore::InputState &)
    {
    }
    virtual void PlaceNote(IEditorContext &, MusicalPosition) = 0;
    virtual bool HasPendingPlacement() const noexcept = 0;
    virtual bool CancelInteraction() noexcept = 0;
    virtual void CloseToolMenu() noexcept = 0;
    virtual bool OpenToolMenu(PhantomCore::Point) = 0;
    virtual void DrawTools(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual void DrawChart(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    // The note panel: the selected note's properties inside Areas().inspector.
    virtual void DrawNotePanel(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) {}
    virtual void EditScore(IEditorContext &, PhantomCore::Point, bool erase) = 0;
    virtual void ScrollScore(IEditorContext &, int direction) = 0;
    virtual bool BeginScoreSelection(IEditorContext &, PhantomCore::Point)
    {
        return false;
    }
    virtual bool UpdateScoreSelection(IEditorContext &, PhantomCore::Point, bool)
    {
        return false;
    }
    virtual void DrawMetadata(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) = 0;
    virtual std::vector<EditorSoundChoice> SoundChoices(const TextCatalog &) const = 0;
    virtual std::wstring_view SoundEffectHelp(const TextCatalog &) const = 0;
};
