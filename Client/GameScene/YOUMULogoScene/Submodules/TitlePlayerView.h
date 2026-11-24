#pragma once
#include "PhantomCore.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

// What the title player shows for the shared song library's current song.
struct TitlePlayerDisplay
{
    bool hasSong{};
    std::wstring title;
    std::wstring artist;
    std::filesystem::path artwork;
    bool playing{};
    std::uint64_t positionMicroseconds{};
    std::optional<std::uint64_t> durationMicroseconds;
};

enum class TitlePlayerAction : std::uint8_t
{
    TogglePlay,
    Stop,
    Next,
    Previous,
    Seek,
};

// An intent the title Scene carries out on the song library; seekRatio is 0..1 of the song length.
struct TitlePlayerRequest
{
    TitlePlayerAction action{};
    float seekRatio{};
};

// The music player above the title menu: album art, artist and title, a seek bar that can be
// clicked or dragged, and play/pause, stop and next buttons. It only presents and reports intents.
class TitlePlayerView final
{
  public:
    explicit TitlePlayerView(PhantomCore::ScreenVisual2DManager &visuals) noexcept : visuals_(visuals) {}

    void Create(PhantomCore::Visual2DNode &board);
    // Places the player above the menu, centred like it on boards wider than 16:9.
    void Layout(PhantomCore::Size board);
    void SetDisplay(const TitlePlayerDisplay &display);
    // Clicks on the player's buttons.
    [[nodiscard]] std::optional<TitlePlayerRequest> ProcessAction(const PhantomCore::Action &action) const;
    // Press on the seek bar starts a drag; the release seeks to where it ends.
    [[nodiscard]] std::optional<TitlePlayerRequest> ProcessSeekPointer(const PhantomCore::InputState &input,
                                                                       std::optional<PhantomCore::Point> canvasPointer,
                                                                       PhantomCore::NodeId hovered);
    void CancelSeek() noexcept;
    [[nodiscard]] bool HasSong() const noexcept { return display_.hasSong; }
    // The transport buttons, for their tooltips.
    [[nodiscard]] const PhantomCore::Visual2DNode *PlayButton() const noexcept { return playButton_; }
    [[nodiscard]] const PhantomCore::Visual2DNode *StopButton() const noexcept { return stopButton_; }
    [[nodiscard]] const PhantomCore::Visual2DNode *NextButton() const noexcept { return nextButton_; }
    [[nodiscard]] bool SeekDragging() const noexcept { return seekDrag_.has_value(); }
    void Reset() noexcept;

  private:
    void CreateArtwork(PhantomCore::Visual2DNode &board);
    void CreateInformation(PhantomCore::Visual2DNode &board);
    void CreateControls(PhantomCore::Visual2DNode &board);
    void ApplySong();
    void ApplyArtwork(const std::filesystem::path &path);
    [[nodiscard]] bool ArtworkReleased() const noexcept;
    void LayoutArtworkImage();
    void ApplyProgress();
    [[nodiscard]] float SeekRatioAt(PhantomCore::Point canvasPointer) const;

    PhantomCore::ScreenVisual2DManager &visuals_;
    TitlePlayerDisplay display_;
    std::optional<float> seekDrag_;
    PhantomCore::Size artworkImageSize_{};
    std::filesystem::path artworkPath_;
    PhantomCore::ImageHandle artworkHandle_;
    std::wstring elapsedText_;
    std::wstring lengthText_;
    float boardHeight_{720.0F};
    float originX_{};
    PhantomCore::Visual2DNode *artworkBox_{};
    PhantomCore::Visual2DNode *artworkImage_{};
    PhantomCore::Visual2DNode *artworkFrame_{};
    PhantomCore::Visual2DNode *musicIcon_{};
    PhantomCore::Visual2DNode *artistLabel_{};
    PhantomCore::Visual2DNode *titleLabel_{};
    PhantomCore::Visual2DNode *elapsedLabel_{};
    PhantomCore::Visual2DNode *lengthLabel_{};
    PhantomCore::Visual2DNode *seekTrack_{};
    PhantomCore::Visual2DNode *seekFill_{};
    PhantomCore::Visual2DNode *seekKnob_{};
    PhantomCore::Visual2DNode *playButton_{};
    PhantomCore::Visual2DNode *playIcon_{};
    PhantomCore::Visual2DNode *stopButton_{};
    PhantomCore::Visual2DNode *stopIcon_{};
    PhantomCore::Visual2DNode *nextButton_{};
    PhantomCore::Visual2DNode *nextIcon_{};
    PhantomCore::ImageHandle playImage_{};
    PhantomCore::ImageHandle pauseImage_{};
};
