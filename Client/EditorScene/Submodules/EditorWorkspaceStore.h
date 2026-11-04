#pragma once
#include <memory>

class EditorWorkspace;

// Only the editor document/workspace survives a Test round trip. Scene visuals,
// audio resources and gameplay sessions are still destroyed on exit.
class EditorWorkspaceStore final
{
  public:
    EditorWorkspaceStore();
    ~EditorWorkspaceStore();
    void Suspend(std::unique_ptr<EditorWorkspace> workspace);
    std::unique_ptr<EditorWorkspace> Take();

  private:
    std::unique_ptr<EditorWorkspace> suspended_;
};
