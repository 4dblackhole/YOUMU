#pragma once

#include "PhantomCore.h"

// The work that runs outside the game loop (reading records, the song list, sounds, images, a chart; converting
// an osu! folder), in the log. The engine keeps the table of what runs now (PhantomCore::BackgroundWork); this
// writes it.
//
// While one exists (the watch mode makes one), every piece of work is a line when it starts and a line when it
// ends: its name, its thread, how long it took and how it ended. The lines are written by the worker itself
// and, like the watched statistics, are not forced to the disk. A normal run makes none and writes neither.
class BackgroundWorkLog final : public PhantomCore::IBackgroundWorkObserver
{
  public:
    // Throws std::logic_error when another one exists.
    BackgroundWorkLog() : observation_(*this) {}

    void OnBackgroundWork(const PhantomCore::BackgroundWorkEvent &event) noexcept override;

    // One line, in every mode: "work in progress: <name> (thread <id>, <ms> ms); ..." or "work in progress:
    // none". For the lines that explain an ending. It allocates nothing and takes no lock, so a handler of a
    // stalled loop may call it.
    static void WriteRunning() noexcept;

  private:
    PhantomCore::BackgroundWorkObservation observation_;
};
