#include "EditorScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Submodules/EditorView.h"
#include "Submodules/EditorWorkspace.h"
#include "Submodules/EditorWorkspaceStore.h"
#include <stdexcept>

using namespace PhantomCore;

EditorScene::EditorScene(ScreenVisual2DManager &visuals,
                         std::shared_ptr<GameplayLaunchStore> request,
                         TextCatalog &texts, std::shared_ptr<EditorWorkspaceStore> workspaceStore)
    : launchStore_(std::move(request)), workspaceStore_(std::move(workspaceStore))
{
    if (!launchStore_ || !workspaceStore_)
        throw std::invalid_argument("Editor requires launch and workspace stores.");
    auto launch = launchStore_->Snapshot();
    auto suspended = workspaceStore_->Take();
    workspace_ = launch.editorTestStart && suspended ? std::move(suspended)
                                                    : std::make_unique<EditorWorkspace>(launch);
    launch.editorTestStart.reset();
    launchStore_->Set(std::move(launch));
    if (auto error = launchStore_->TakeEditorTestError(); !error.empty())
        workspace_->SetStatus(std::move(error));
    view_ = std::make_unique<EditorView>(visuals, *workspace_, texts);
}
EditorScene::~EditorScene() = default;
void EditorScene::Initialize(const EngineServices &services)
{
    workspace_->Initialize();
    workspace_->InitializePlayback(services.audio);
    view_->Initialize(services);
}
void EditorScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    try
    {
        workspace_->UpdateAnalysis();
    }
    catch (const std::exception &error)
    {
        workspace_->SetStatus(error.what());
    }
    if (view_->Update(context))
    {
        static_cast<void>(scenes.ChangeScene(EditorSongSelectSceneId));
        return;
    }
    if (view_->TakeTestRequest())
    {
        try
        {
            launchStore_->Set(workspace_->TestLaunchRequest());
            if (!scenes.ChangeScene(RhythmTestSceneId))
                throw std::runtime_error("Failed to enter the editor Test.");
            testStarting_ = true;
            workspace_->ShutdownPlayback();
        }
        catch (const std::exception &error)
        {
            workspace_->SetStatus(error.what());
        }
        return;
    }
    workspace_->UpdatePlayback();
}
void EditorScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->Resize(width, height);
}
void EditorScene::Shutdown() noexcept
{
    if (!workspace_) return;
    workspace_->Analysis().Stop();
    workspace_->ShutdownPlayback();
    view_->Shutdown();
    if (testStarting_)
        workspaceStore_->Suspend(std::move(workspace_));
}
void EditorScene::Render(const RenderContext &)
{
}
