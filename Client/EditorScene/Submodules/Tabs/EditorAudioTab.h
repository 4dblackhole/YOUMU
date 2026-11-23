#pragma once
#include "EditorTabBase.h"
#include <map>
#include <tuple>

// Audio: music waveform, music FFT spectrogram and expected-hit spectrogram on one time axis, with the
// metronome, visible range and pattern offset controls. Spectrum tiles are retained nodes above the surface.
class EditorAudioTab final : public EditorTabBase
{
  public:
    EditorAudioTab(EditorView &view, EditorWorkspace &state, TextCatalog &texts,
                   PhantomCore::ScreenVisual2DManager &manager) noexcept
        : EditorTabBase(view, state, texts), visuals(manager)
    {
    }
    [[nodiscard]] std::string_view CaptionKey() const noexcept override { return "Editor.tabs.1"; }
    void Initialize(PhantomCore::Visual2DNode &editorNode) override;
    void Shutdown() noexcept override;
    void Resize() override;
    void Show(bool current) override;
    void Build() override;
    void BuildFailed() noexcept override;
    void SyncInput(bool usable) override;
    void Wheel(int direction, std::optional<PhantomCore::Point> point, bool control) override;

  private:
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

    PhantomCore::ScreenVisual2DManager &visuals;
    PhantomCore::Visual2DNode *node{};
    std::array<PhantomCore::Visual2DNode *, 3> audioSelectionNodes_{};
    std::optional<RhythmTime> audioSelectionAnchor_;
    double audioSelectionViewBegin_{}, audioSelectionViewWindow_{};
    // Waveform, music spectrum and expected-hit spectrum.
    std::array<PhantomCore::Rect, 3> audioTracks_{};
    using AudioColumnsKey = std::tuple<double, double, double, std::uint64_t, std::size_t, std::size_t>;
    std::optional<AudioColumnsKey> audioColumnsKey_;
    const IEditorDocument *audioColumnsDocument_{};
    std::uint64_t audioColumnsDocumentRevision_{};
    std::vector<WaveformPeak> musicColumns_;
    struct AudioTile
    {
        PhantomCore::Visual2DNode *node{};
        std::shared_ptr<const SpectrumTile> detail;
        // Levels on screen (overview or detail) and the texture holding them; rectangles when it is empty.
        std::shared_ptr<const SpectrumTile> levels;
        PhantomCore::ImageHandle image;
    };
    void ShowSpectrumTile(AudioTile &, std::shared_ptr<const SpectrumTile> levels, PhantomCore::Rect rect,
                          std::size_t visibleColumns);
    void ReleaseSpectrumImage(AudioTile &) noexcept;
    [[nodiscard]] bool CanRetrySpectrumImage(const AudioTile &) const noexcept;
    // Set when the renderer has no pixel images; tiles then always draw rectangles.
    bool spectrumImagesUnsupported_{};
    // Set for the rest of a frame once every pixel image slot is taken.
    bool spectrumImagesFull_{};
    std::array<PhantomCore::Visual2DNode *, 2> audioRoots_{};
    std::array<EditorVisual *, 2> audioOverlays_{};
    std::array<std::map<std::int64_t, AudioTile>, 2> audioTiles_;
};
