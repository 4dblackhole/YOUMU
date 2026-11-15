#pragma once
#include "EditorLayout.h"
#include "EditorMenuBar.h"
#include "EditorSupport.h"
#include "EditorTextFields.h"
#include "EditorVisual.h"
#include "EditorWorkspace.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <tuple>
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

class EditorView final : public IEditorModeCanvas
{
  public:
    EditorView(PhantomCore::ScreenVisual2DManager &manager, EditorWorkspace &state, TextCatalog &texts)
        : visuals(manager), state_(state), texts_(texts)
    {
    }
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
    bool TakeTestRequest() noexcept { return std::exchange(testRequested_, false); }

  private:
    // Frame and drawing primitives.
    [[nodiscard]] PhantomCore::Rect ToNode(PhantomCore::Rect) const noexcept;
    [[nodiscard]] std::optional<PhantomCore::Point> DesignPoint(const PhantomCore::InputState &) const;
    [[nodiscard]] float ContentRight() const noexcept;
    void PlaceNode(PhantomCore::Visual2DNode &, PhantomCore::Rect) const;
    void DrawLetterbox();
    void Label(PhantomCore::Rect, std::wstring, EditorTextStyle = {});
    void Icon(PhantomCore::Rect, std::wstring_view name, PhantomCore::Color);
    // A block whose left (positive) or right (negative) edge leans by the slant towards the bottom.
    void Slanted(PhantomCore::Rect, float leftLean, float rightLean, PhantomCore::Color);
    void Control(PhantomCore::Rect, std::function<void()> action);
    void FlatButton(PhantomCore::Rect, std::wstring, std::function<void()>, bool primary = false);
    void Field(PhantomCore::Rect r, const wchar_t *label, std::string &value);
    // A labelled inline field over `r`; without a window it falls back to the popup editor.
    void InputField(PhantomCore::Rect r, std::wstring label, std::wstring text, EditorTextFields::Commit commit,
                    bool number = false, bool showLabel = true);
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

    // Compose: tool row, channel heads, ruler, chart and the right panel.
    void DrawCompose();
    void DrawToolRow();
    void DrawBeatControl();
    void DrawHeads();
    void DrawZoneHeads();
    void DrawChannelHead(editor_layout::Channel, std::wstring_view icon, std::wstring name, std::wstring value);
    void DrawRuler();
    void DrawPanel();
    // Realtime channels under the lane, negative-delay cuts and the dragged beat range.
    void DrawChannels();
    void DrawCutsAndRange();
    void Chip(float x, float middle, std::wstring text, PhantomCore::Color background, bool selected, float h = 18);
    [[nodiscard]] double AutomationValueAt(EffectCommandType, std::string_view target, double fallback) const;
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
    void DrawCurveRow(PhantomCore::Rect row);
    // Curves dialog (YME [Interpolation]): list, name/expression with checking, preview.
    void OpenCurves();
    void CloseCurves();
    void ApplyCurves();
    void RenameCurve(const std::string &name);
    void DrawCurvesDialog();
    void DrawSwitch(PhantomCore::Rect row, std::wstring caption, bool on, std::function<void()> toggle);
    [[nodiscard]] PhantomCore::Rect PanelBody() const noexcept;
    [[nodiscard]] std::wstring HeadValue(editor_layout::Channel) const;
    std::wstring EffectLabel(const EditorEffectSelection &) const;
    void SelectEffectRow(std::size_t);
    [[nodiscard]] bool EffectInPanel(std::size_t row, EditorPanel) const;

    // Audio tab.
    void DrawAudio();
    void DrawAudioTools();
    void LayoutAudioTracks();
    void UpdateAudioTiles(double begin, double minimumHz, double maximumHz, double longestSound);
    void PrepareAudioTrack(std::size_t track, PhantomCore::Rect rect, std::int64_t first, std::int64_t last,
                           bool invalidate);
    void UpdateWaveformColumns(double begin, std::size_t columns);
    void DrawAudioWaveform(const std::vector<WaveformPeak> &columns);
    void DrawAudioTimeRuler(double begin);
    void DrawAudioAlignment(double begin);
    void InitializeAudioSelection();
    void UpdateAudioSelection(PhantomCore::Visual2DNode &, const PhantomCore::PointerEvent &);
    void DrawAudioSelection(double begin);
    void DrawAudioSpectrum(std::size_t track, double minimumHz, double maximumHz);

    // Input.
    void UpdateSliders(const PhantomCore::UpdateContext &context);
    EditorMenuInputResult UpdateMenu(const PhantomCore::InputState &input);
    void ExecuteMenuAction(EditorMenuAction action);
    void OpenDocumentFolder(const std::filesystem::path &path);
    void UpdateTimelineNavigation(const PhantomCore::InputState &, bool keyboardAllowed, bool pointerAllowed);
    bool UpdateKeyboard(const PhantomCore::InputState &);
    void UpdatePointer(const PhantomCore::UpdateContext &);
    void StepPlaybackRate(int direction, bool coarse);
    void StepSpacing(int direction);

    // IEditorModeCanvas
    EditorModeAreas Areas() const noexcept override;
    void Image(PhantomCore::Rect, const std::filesystem::path &, PhantomCore::Color) override;
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
    [[nodiscard]] EditorTextSet Texts() const noexcept;

    PhantomCore::ScreenVisual2DManager &visuals;
    EditorWorkspace &state_;
    TextCatalog &texts_;
    std::uint64_t textRevision_{};
    PhantomCore::ScreenCanvasHandle canvas;
    PhantomCore::Visual2DNode *node{};
    PhantomCore::Visual2DNode *divisionSliderNode_{};
    PhantomCore::Visual2DNode *timelineSliderNode_{};
    std::array<PhantomCore::Visual2DNode *, 3> audioSelectionNodes_{};
    std::optional<RhythmTime> audioSelectionAnchor_;
    double audioSelectionViewBegin_{}, audioSelectionViewWindow_{};
    PhantomCore::Visual2DInputRouter sliderInput_;
    EditorMenuBar menuBar_;
    EditorTextFields fields_;
    // Rectangles of this build's fields: a click elsewhere ends typing.
    std::vector<PhantomCore::Rect> fieldRects_;
    EditorVisual *visual{};
    editor_layout::Frame frame_{};
    PhantomCore::Rect timelineRect_{};
    PhantomCore::Rect divisionRect_{};
    // Waveform, music spectrum and expected-hit spectrum.
    std::array<PhantomCore::Rect, 3> audioTracks_{};
    bool buildFailed_{};
    bool testRequested_{};
    bool leaveRequested_{};
    bool curvesOpen_{};
    // Draft of the YME [Interpolation] table while the Curves dialog is open.
    struct CurvesDraft
    {
        std::map<std::string, std::string, std::less<>> table;
        // Current name -> name in the document, so renamed curves keep their effects.
        std::map<std::string, std::string, std::less<>> origins;
        std::string selected;
    } curves_;
    // The pointer of the click being dispatched, for controls that read a position.
    std::optional<PhantomCore::Point> lastPointer_;
    bool toggleSelection_{}, rangeSelection_{}, shiftDown_{};
    std::vector<::Control> controls;
    using AudioColumnsKey = std::tuple<double, double, double, std::uint64_t, std::size_t, std::size_t>;
    std::optional<AudioColumnsKey> audioColumnsKey_;
    const IEditorDocument *audioColumnsDocument_{};
    std::uint64_t audioColumnsDocumentRevision_{};
    std::vector<WaveformPeak> musicColumns_;
    struct AudioTile
    {
        PhantomCore::Visual2DNode *node{};
        std::shared_ptr<const SpectrumTile> detail;
    };
    std::array<PhantomCore::Visual2DNode *, 2> audioRoots_{};
    std::array<EditorVisual *, 2> audioOverlays_{};
    std::array<std::map<std::int64_t, AudioTile>, 2> audioTiles_;
    std::uint32_t width{1280}, height{720};
};
