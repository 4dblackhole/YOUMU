#pragma once
#include "EditorAnalysisController.h"
#include "EditorForms.h"
#include "EditorMetronome.h"
#include "EditorPlayback.h"
#include "Modes/IEditorMode.h"
#include <algorithm>
#include <optional>

class EditorWorkspace final : public IEditorContext
{
  public:
    struct AudioSelection
    {
        RhythmTime begin{}, end{};
    };
    explicit EditorWorkspace(GameplayLaunchRequest selected, std::unique_ptr<IEditorMode> mode = {})
        : request_(std::move(selected)), mode_(std::move(mode))
    {
    }
    // Injected modes/documents use the same owner and editing boundary.
    EditorWorkspace(std::unique_ptr<IEditorMode> mode, std::unique_ptr<IEditorDocument> document)
        : editor_(std::move(document)), mode_(std::move(mode))
    {
    }
    void Initialize();
    void UpdateAnalysis();
    void InitializePlayback(PhantomCore::AudioSystem &audio) noexcept
    {
        playback_.Initialize(audio);
        playback_.SetVolumes(request_.volumes.Music(), request_.volumes.Effects());
    }
    void ShutdownPlayback() noexcept { playback_.Shutdown(); }
    void UpdatePlayback();
    void TogglePlayback();
    bool Playing() const noexcept
    {
        return playback_.Active();
    }
    void SetPlaybackRate(double rate);
    double PlaybackRate() const noexcept { return playback_.Rate(); }
    void SelectAudioRange(RhythmTime anchor, RhythmTime end)
    {
        audioSelection_ = AudioSelection{std::min(anchor, end), std::max(anchor, end)};
        RequestRebuild();
    }
    const std::optional<AudioSelection> &SelectedAudioRange() const noexcept { return audioSelection_; }
    void ToggleMetronome()
    {
        metronome_.SetEnabled(!metronome_.Enabled());
        RequestRebuild();
    }
    void PrepareAudioPreview();
    const EditorMetronome &Metronome() const noexcept
    {
        return metronome_;
    }
    void SetPatternOffset(double milliseconds);
    void SeekFirstNote();
    void SeekLastNote();
    void SetRealtimeZoom(double factor);
    void SelectTool(int value) override;
    void Save();
    // New difficulty: Metadata only until Create writes the files.
    bool Creating() const noexcept
    {
        return creating_;
    }
    std::filesystem::path MusicMetadataPath() const;
    const GameplayLaunchRequest &Request() const noexcept
    {
        return request_;
    }
    void PlaceNote(MusicalPosition position);
    [[nodiscard]] std::pair<double, double> TimelineRangeMilliseconds() const;
    void Seek(double milliseconds) override;
    double TimeMilliseconds() const noexcept override
    {
        return timeMs_;
    }
    const IEditorDocument &Document() const noexcept override
    {
        return *editor_;
    }
    IEditorMode &Mode() noexcept
    {
        return *mode_;
    }
    EditorAnalysisController &Analysis() noexcept
    {
        return analysis_;
    }
    const EditorScoreState &Score() const noexcept override
    {
        return score_;
    }
    void SetFirstMeasure(std::int64_t) override;
    void SetDivision(std::int64_t);
    void SetRealtime(bool);
    // Compose views: Realtime, Score or Preview (read-only).
    void SetPreview();
    void RequestRebuild() noexcept override
    {
        rebuild_ = true;
    }
    bool NeedsRebuild() const noexcept
    {
        return rebuild_;
    }
    void FinishBuild() noexcept
    {
        rebuild_ = false;
    }
    EditorTab Tab() const noexcept
    {
        return tab_;
    }
    const std::string &Status() const noexcept
    {
        return status_;
    }
    const std::filesystem::path &SavedPath() const noexcept
    {
        return savedPath_;
    }
    void SetStatus(std::string value)
    {
        savedPath_.clear();
        status_ = std::move(value);
        RequestRebuild();
    }
    void SelectTab(EditorTab value);
    // The right panel of Compose. Opening one moves Copy/Paste/Delete to its list.
    const std::optional<EditorPanel> &Panel() const noexcept
    {
        return panel_;
    }
    void OpenPanel(EditorPanel panel);
    void ClosePanel();
    EditorFocus Focus() const noexcept
    {
        return panel_ ? FocusOf(*panel_) : EditorFocus::Notes;
    }
    void SelectTimingPosition(MusicalPosition) override;
    void Replace(PatternDocument, EffectDocument) override;
    void AddNote(MusicalPosition, int keyType,
                 std::optional<MusicalPosition> end = {},
                 std::vector<std::string> extra = {}) override;
    void UpdateNote(std::size_t, PatternNote,
                    std::optional<MusicalPosition>) override;
    void DeleteNote(std::size_t sourceOrder) override;
    const std::set<std::size_t> &SelectedNoteIds() const noexcept override
    {
        return selectedNotes_;
    }
    void SelectNotes(std::span<const std::size_t>) override;
    void SelectNoteRange(MusicalPosition, MusicalPosition) override;
    const std::optional<std::pair<MusicalPosition, MusicalPosition>> &
    NoteSelectionRange() const noexcept override
    {
        return noteSelectionRange_;
    }
    void SelectListRow(std::size_t row, bool toggle, bool range);
    bool IsListRowSelected(std::size_t) const noexcept;
    void CopySelection();
    void PasteSelection(bool includeBlanks);
    void DeleteSelection();
    void Undo();
    void Redo();
    void SetMeasureLength(std::int64_t, Rational);
    void RemoveTiming(std::size_t);
    void ApplyBpm();
    void ApplyMeasureLength();
    void ApplyDelay();
    // Add writes the form at its position; Apply replaces the selected anchor row with it.
    void AddTiming();
    void ApplyTimingRow();
    void RemoveEffect(std::size_t);
    void ApplyEffect();
    void ApplyEffectRow();
    // The row that Shift+click extends from: the one the form below the list edits.
    [[nodiscard]] std::optional<std::size_t> ListAnchor() const noexcept;
    double audioWindow{8.0};
    EditorTimingForm timingForm;
    EditorEffectForm effectForm;
    std::size_t listOffset{};

  private:
    template <class Event> struct ClipboardEvent
    {
        Event value;
        Rational offset;
        std::optional<Rational> endOffset;
    };
    struct ClipboardNote
    {
        PatternNote head;
        std::optional<PatternNote> tail;
        Rational headOffset, tailOffset;
    };
    struct Clipboard
    {
        EditorFocus focus{EditorFocus::Notes};
        Rational length;
        std::vector<ClipboardNote> notes;
        std::vector<ClipboardEvent<TimingDirective>> timing;
        std::vector<ClipboardEvent<EffectCommand>> effects;
        std::vector<ClipboardEvent<HitSoundChange>> sounds;
    };
    void PasteNotes(const Clipboard &, MusicalPosition, bool);
    void PasteTiming(const Clipboard &, MusicalPosition, bool);
    void PasteEffects(const Clipboard &, MusicalPosition, bool);
    void ClearEditSelection();
    std::set<std::size_t> selectedNotes_, selectedTiming_, selectedEffects_;
    std::optional<std::pair<MusicalPosition, MusicalPosition>>
        noteSelectionRange_;
    std::optional<std::size_t> timingSelectionAnchor_, effectSelectionAnchor_;
    std::optional<Clipboard> clipboard_;
    void DocumentChanged();
    EditorTab tab_{EditorTab::Compose};
    std::optional<EditorPanel> panel_;
    bool rebuild_{true};
    std::string status_;
    std::filesystem::path savedPath_;
    GameplayLaunchRequest request_;
    std::unique_ptr<IEditorDocument> editor_;
    std::unique_ptr<IEditorMode> mode_;
    EditorAnalysisController analysis_;
    EditorPlayback playback_;
    std::optional<AudioSelection> audioSelection_;
    EditorMetronome metronome_;
    EditorScoreState score_;
    double timeMs_{};
    RhythmTime lastPlaybackPresentation_{};
    bool nextZToZero_{};
    bool nextXToMusicEnd_{};
    std::uint64_t audioSourceRevision_{};
    bool creating_{};
};
