#include "EditorWorkspaceStore.h"
#include "EditorWorkspace.h"

EditorWorkspaceStore::EditorWorkspaceStore() = default;
EditorWorkspaceStore::~EditorWorkspaceStore() = default;
void EditorWorkspaceStore::Suspend(std::unique_ptr<EditorWorkspace> workspace)
{
    suspended_ = std::move(workspace);
}
std::unique_ptr<EditorWorkspace> EditorWorkspaceStore::Take()
{
    return std::move(suspended_);
}
