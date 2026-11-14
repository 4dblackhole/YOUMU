#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Submodules/SongSelectPurpose.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Submodules/EditorOsuImport.h"
#include "GameFlow/PlaySessionLoader.h"
#include <memory>

class MusicSelectView;
class SongLibrary;
class SongSelectionState;

class MusicSelectScene final : public PhantomCore::GameScene
{
  public:
    explicit MusicSelectScene(PhantomCore::ScreenVisual2DManager &screenVisuals,
                              std::shared_ptr<SongLibrary> library,
                              std::shared_ptr<GameplayLaunchStore> launchRequest,
                              SongSelectPurpose purpose,
                              TextCatalog &texts,
                              OptionSettings &options);

    ~MusicSelectScene() override;

    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    bool StartSelectedPattern(PhantomCore::SceneManager &scenes);
    void CreateDifficulty(PhantomCore::SceneManager &scenes);
    void ConvertOsu();
    void PollOsuImport();
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
    PlaySessionLoader patternLoader_;
    std::optional<GameplayLaunchRequest> pendingLaunch_;
};
