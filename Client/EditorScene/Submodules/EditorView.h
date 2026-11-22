#pragma once
#include "EditorFindReplace.h"
#include "EditorLayout.h"
#include "EditorMenuBar.h"
#include "EditorSupport.h"
#include "EditorTextFields.h"
#include "EditorVisual.h"
#include "EditorWorkspace.h"
#include "Presentation/UiAlert.h"
#include "Tabs/IEditorTab.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// Text placement for the editor's own captions. Rectangles are frame
// coordinates; the text is centred vertically in its rectangle.
struct EditorTextStyle
{
    float size{14};
    PhantomCore::Color color{editor_ui::Ink};
    PhantomCore::TextAlignment alignment{PhantomCore::TextAlignment::Leading};
    std::uint16_t weight{700};
    bool number{};
};

// The editor shell: menu bar, tab bar, footer with the shared time controls, and the modals
// (alert, Curves, Find/Replace). The page under the tab bar belongs to the current IEditorTab.
class EditorView final : public IEditorModeCanvas
{
  public:
    EditorView(PhantomCore::ScreenVisual2DManager &manager, EditorWorkspace &state, TextCatalog &texts);
    ~EditorView() override;
    EditorView(const EditorView &) = delete;
    EditorView &operator=(const EditorView &) = delete;
    void Initialize(const PhantomCore::EngineServices &services);
    void Resize(std::uint32_t w, std::uint32_t h)
    {
        width = w;
        height = h;
        if (node)
            ReloadSize();
    }
    void Shutdown() noexcept;
    void Build();
    void ReloadSize();
    bool Update(const PhantomCore::UpdateContext &context);

    // Frame and drawing primitives shared with the tabs (frame coordinates).
    [[nodiscard]] PhantomCore::Rect ToNode(PhantomCore::Rect) const noexcept;
    [[nodiscard]] const editor_layout::Frame &FrameLayout() const noexcept { return frame_; }
    [[nodiscard]] float ContentRight() const noexcept;
    // Body of the right panel under its header.
    [[nodiscard]] PhantomCore::Rect PanelBody() const noexcept;
    void PlaceNode(PhantomCore::Visual2DNode &, PhantomCore::Rect) const;
    void Label(PhantomCore::Rect, std::wstring, EditorTextStyle = {});
    void Icon(PhantomCore::Rect, std::wstring_view name, PhantomCore::Color);
    // A block whose left (positive) or right (negative) edge leans by the slant towards the bottom.
    void Slanted(PhantomCore::Rect, float leftLean, float rightLean, PhantomCore::Color);
    void Control(PhantomCore::Rect, std::function<void()> action);
    void FlatButton(PhantomCore::Rect, std::wstring, std::function<void()>, bool primary = false);
    void Field(PhantomCore::Rect r, const wchar_t *label, std::string &value);
    // A labelled inline field over `r`; without a window it falls back to the popup editor.
    // Under an open modal layer the value is drawn only, since native editors would sit above it.
    void InputField(PhantomCore::Rect r, std::wstring label, std::wstring text, EditorTextFields::Commit commit,
                    bool number = false, bool showLabel = true, EditorTextFields::Commit typing = {});
    void DrawSwitch(PhantomCore::Rect row, std::wstring caption, bool on, std::function<void()> toggle);
    [[nodiscard]] EditorTextSet Texts() const noexcept;
    // The beat division slider sits over the rectangle a tab drew for it in this build; hidden otherwise.
    void PlaceDivisionSlider(PhantomCore::Rect rect) noexcept { divisionRect_ = rect; }
    // Curves dialog (YME [Interpolation]), opened from the Compose effect panel.
    void OpenCurves();
    // The pointer of the click being dispatched, for controls that read a position.
    [[nodiscard]] const std::optional<PhantomCore::Point> &LastPointer() const noexcept { return lastPointer_; }
    // Ctrl and Shift of the last pointer update: list rows add to or extend the selection.
    [[nodiscard]] bool ToggleSelection() const noexcept { return toggleSelection_; }
    [[nodiscard]] bool RangeSelection() const noexcept { return rangeSelection_; }
    // True while the last build failed and only the error, tab bar and footer are shown.
    [[nodiscard]] bool BuildFailed() const noexcept { return buildFailed_; }

    // IEditorModeCanvas
    EditorModeAreas Areas() const noexcept override;
    void Image(PhantomCore::Rect, const std::filesystem::path &, PhantomCore::Color) override;
    void ChartBackground(PhantomCore::Rect r, const std::filesystem::path &image, PhantomCore::Point offset,
                         PhantomCore::Size screen) override;
    float PixelScale() const noexcept override;
    void Box(PhantomCore::Rect r, PhantomCore::Color color, float radius = 0) override;
    void Text(PhantomCore::Rect r, std::wstring text, float size = 20, PhantomCore::Color color = editor_ui::Ink) override;
    void Button(PhantomCore::Rect r, std::wstring text, std::function<void()> action, bool selected = false) override;
    void Region(PhantomCore::Rect r, std::function<void()> action) override { Control(r, std::move(action)); }
    void TextField(PhantomCore::Rect r, std::wstring label, std::wstring text,
                   std::function<void(const std::string &)> commit, bool number) override
    {
        InputField(r, std::move(label), std::move(text), std::move(commit), number);
    }

  private:
    [[nodiscard]] IEditorTab &CurrentTab() const noexcept;
    [[nodiscard]] std::optional<PhantomCore::Point> DesignPoint(const PhantomCore::InputState &) const;
    void DrawLetterbox();
    [[nodiscard]] PhantomCore::TextInputBounds ScreenBounds(PhantomCore::Rect) const noexcept;

    // Shell: tabs, footer and the shared time controls.
    void BuildContent();
    void DrawTabs();
    void DrawFooter();
    void DrawTransport(PhantomCore::Rect);
    void DrawPlaybackRateControl(float x);
    void DrawTimeline(PhantomCore::Rect);
    void DrawTimeInput(PhantomCore::Rect);
    void SyncInputControls();
    void SyncTimelineSlider();
    void SyncDivisionSlider() noexcept;

    void CloseCurves();
    void ApplyCurves();
    void RenameCurve(const std::string &name);
    void DrawCurvesDialog();
    // Find/Replace dialog: two pattern lanes, matches in the range selected when it opened.
    void OpenFind();
    void CloseFind();
    void DrawFindDialog();
    void DrawPatternLane(int index, PhantomCore::Rect lane, std::wstring caption);
    // Handles the dialog's lane clicks and edge drags; true when the pointer belonged to a lane.
    bool UpdateFindPointer(const PhantomCore::InputState &, PhantomCore::Point);
    void RefreshFindMatches();
    void ShowFindMatch(std::size_t index);
    void ReplaceFindMatches(bool all);

    // Input.
    void UpdateSliders(const PhantomCore::UpdateContext &context);
    EditorMenuInputResult UpdateMenu(const PhantomCore::InputState &input);
    void ExecuteMenuAction(EditorMenuAction action);
    void OpenDocumentFolder(const std::filesystem::path &path);
    void UpdateTimelineNavigation(const PhantomCore::InputState &, bool keyboardAllowed, bool pointerAllowed);
    bool UpdateKeyboard(const PhantomCore::InputState &);
    void UpdatePointer(const PhantomCore::UpdateContext &);
    void StepPlaybackRate(int direction, bool coarse);
    // Save, or Create for a new difficulty. A failure opens the alert (a rejected name or an existing
    // folder goes to the status line); returns whether the document was written.
    bool SaveDocument();
    void Paste(bool includeBlanks);
    // Shared alert (UI-02): unsaved changes on leaving, save and paste failures. While it is open it
    // takes the pointer and keys; returns true when the editor should leave.
    enum class AlertPurpose
    {
        Notice,
        Leave
    };
    void ShowAlert(AlertContent content, AlertPurpose purpose);
    void ShowFailure(AlertKind kind, std::string_view titleKey, std::string_view messageKey, std::string detail);
    void ConfirmLeave();
    bool UpdateAlert(const PhantomCore::InputState &input);

    PhantomCore::ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    TextCatalog &texts_;
    // One page per EditorTab, in tab bar order.
    std::array<std::unique_ptr<IEditorTab>, EditorTabCount> tabs_;
    std::uint64_t textRevision_{};
    PhantomCore::ScreenCanvasHandle canvas;
    PhantomCore::Visual2DNode *node{};
    PhantomCore::Visual2DNode *divisionSliderNode_{};
    PhantomCore::Visual2DNode *timelineSliderNode_{};
    PhantomCore::Visual2DInputRouter sliderInput_;
    UiAlert alert_;
    PhantomCore::Visual2DInputRouter alertInput_;
    AlertPurpose alertPurpose_{};
    EditorMenuBar menuBar_;
    EditorTextFields fields_;
    // Rectangles of this build's fields: a click elsewhere ends typing.
    std::vector<PhantomCore::Rect> fieldRects_;
    EditorVisual *visual{};
    editor_layout::Frame frame_{};
    PhantomCore::Rect timelineRect_{};
    std::optional<PhantomCore::Rect> divisionRect_;
    bool buildFailed_{};
    bool leaveRequested_{};
    bool curvesOpen_{};
    bool findOpen_{};
    struct FindDraft
    {
        NotePattern find, replace;
        PatternScope scope;
        std::vector<PatternMatch> matches;
        std::size_t current{};
        std::uint64_t revision{};
        bool dirty{true};
        // Lane rectangles of this build and the window edge being dragged (lane 0 find, 1 replace).
        std::array<PhantomCore::Rect, 2> lanes{};
        std::optional<std::pair<int, bool>> dragging;
    } find_;
    // True while building layers under a modal (the Curves dialog or the alert).
    bool fieldsCovered_{};
    // Draft of the YME [Interpolation] table while the Curves dialog is open.
    struct CurvesDraft
    {
        std::map<std::string, std::string, std::less<>> table;
        // Current name -> name in the document, so renamed curves keep their effects.
        std::map<std::string, std::string, std::less<>> origins;
        std::string selected;
    } curves_;
    std::optional<PhantomCore::Point> lastPointer_;
    // The previous left press (QPC ticks, design point) for double-click detection.
    struct PointerPress
    {
        std::int64_t ticks;
        PhantomCore::Point point;
    };
    std::optional<PointerPress> lastPress_;
    [[nodiscard]] bool IsDoubleClick(const PhantomCore::InputState &, PhantomCore::Point);
    bool toggleSelection_{}, rangeSelection_{}, shiftDown_{};
    std::vector<::Control> controls;
    std::uint32_t width{1280}, height{720};
};
