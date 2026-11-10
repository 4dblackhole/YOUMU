#pragma once

#include "GameFlow/PlaySessionLoader.h"

class GameplaySessionFactory final
{
  public:
    static void PrepareDebugLaunchRequest(GameplayLaunchRequest &request, bool debugMode);
    [[nodiscard]] static std::unique_ptr<PlaySession> Create(const GameplayLaunchRequest &request, bool debugMode);
    // A null result means that the asynchronous session load is still running.
    [[nodiscard]] std::unique_ptr<PlaySession> Poll(const GameplayLaunchRequest &request);

  private:
    [[nodiscard]] static std::unique_ptr<PlaySession> CreateDemoSession();
    [[nodiscard]] static std::unique_ptr<PlaySession> CreateLongNoteDebugSession();

    PlaySessionLoader loader_;
};
