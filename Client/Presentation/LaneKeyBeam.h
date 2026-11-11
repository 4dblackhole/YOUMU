#pragma once

#include "PhantomCore.h"
#include "Note/Submodules/NoteTypes.h"

// Owns presentation state; the Lane/Canvas owns the sprite node.
class LaneKeyBeam final
{
  public:
    void Initialize(PhantomCore::Visual2DNode &lane, PhantomCore::ImageHandle image,
                    PhantomCore::Size imageSize);
    void SetLayout(float laneWidth, float laneLength);
    void OnKeyPressed(const NoteProcessResult &result);
    void Update(double deltaSeconds);
    void Reset() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] static PhantomCore::Color InputColor(
        const NoteProcessResult &result) noexcept;
    [[nodiscard]] static PhantomCore::Color GradeColor(
        JudgementGrade grade) noexcept;
    [[nodiscard]] static bool IsWrongInput(const NoteEvent &event) noexcept;
    // Returned observer is valid only while result is alive and unchanged.
    [[nodiscard]] static const NoteEvent *InputResponse(
        const NoteProcessResult &result) noexcept;

  private:
    PhantomCore::Visual2DNode *node_{};
    PhantomCore::Color color_{1, 1, 1, 1};
    float aspectRatio_{1};
    double elapsedSeconds_{0.2};
};
