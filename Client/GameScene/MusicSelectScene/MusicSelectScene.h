#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Submodules/SongSelectPurpose.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Submodules/EditorOsuImport.h"
#include "Submodules/OsuImportPanel.h"
#include "GameFlow/PlaySessionLoader.h"
#include "Mode/Results/IPlayResult.h"
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

class MusicSelectView;
class SongLibrary;
class SongSelectionState;

class MusicSelectScene final : public PhantomCore::GameScene
{
  public:
    // Hands a record's restored result to the result screen (shown again, never saved again).
    using RecordedResultHandler = std::function<void(std::unique_ptr<IPlayResult>)>;
    explicit MusicSelectScene(PhantomCore::ScreenVisual2DManager &screenVisuals,
                              std::shared_ptr<SongLibrary> library,
                              std::shared_ptr<GameplayLaunchStore> launchRequest,
                              SongSelectPurpose purpose,
                              TextCatalog &texts,
                              OptionSettings &options,
                              RecordedResultHandler openRecordedResult = {});

    ~MusicSelectScene() override;

    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    // The launch request for the selected pattern (and its chart name), or none when nothing
    // playable is selected; an unsupported mode shows its error.
    std::optional<GameplayLaunchRequest> SelectedLaunchRequest(std::string *patternName = nullptr);
    bool StartSelectedPattern(PhantomCore::SceneManager &scenes);
    // A clicked record: its detail loads on a worker, then the result screen opens with Back and
    // Retry (the selected pattern), without saving the record again.
    void OpenRecord();
    void PollRecordOpen(PhantomCore::SceneManager &scenes);
    void CreateDifficulty(PhantomCore::SceneManager &scenes);
    // Right-click menu: Edit opens the selected difficulty in the editor (game song select too),
    // a confirmed delete goes to the Recycle Bin and Open folder shows the song folder.
    void OpenSelectedInEditor(PhantomCore::SceneManager &scenes);
    void DeleteApproved();
    void OpenSongFolder();
    void ConvertOsu();
    void PollOsuImport();
    void ShowOsuScan(OsuImportScan scan);
    void AnswerOsuImport(const OsuImportChoice &choice);
    void FinishOsuImport(const EditorOsuImportResult &result);
    void ShowImportAlert(const char *messageKey, const std::string &detail);
    void RefreshCatalog();
    void PollPatternLoad(PhantomCore::SceneManager &scenes);
    std::shared_ptr<GameplayLaunchStore> launchRequest_;
    SongSelectPurpose purpose_;
    OptionSettings &options_;
    TextCatalog &texts_;
    // Game song select borrows the library the title shares; editor song select owns its own.
    std::shared_ptr<SongLibrary> library_;
    SongSelectionState &selection_;
    std::unique_ptr<MusicSelectView> view_;
    bool active_{};
    bool firstEntry_{true};
    // Returning to the title keeps the song playing; every other exit silences it.
    bool keepPlaybackOnExit_{};
    EditorOsuImport osuImport_;
    std::wstring osuImportFolder_; // Name of the osu! song folder being imported, for the list's heading.
    PlaySessionLoader patternLoader_;
    std::optional<GameplayLaunchRequest> pendingLaunch_;
    RecordedResultHandler openRecordedResult_;
    std::future<std::unique_ptr<IPlayResult>> recordOpen_;
    std::optional<GameplayLaunchRequest> pendingRecordLaunch_;
};
