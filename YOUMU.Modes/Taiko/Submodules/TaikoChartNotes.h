#pragma once
#include "Model/Submodules/CompiledChart.h"
#include "Parsing/LoadCancellation.h"
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

// Pair in authored order, before gameplay sorts by timestamp. A negative
// delay may move a tail before its head without changing either source ID.
inline std::vector<CompiledPatternNote> PlayableInSourceOrder(
    std::vector<CompiledPatternNote> notes, std::stop_token stop = {})
{
    CheckChartLoadCancellation(stop);
    std::size_t comparisons{};
    std::ranges::stable_sort(notes,
                             [&stop, &comparisons](const auto &a, const auto &b)
                             {
                                 if ((comparisons++ & 63U) == 0) CheckChartLoadCancellation(stop);
                                 return std::pair{a.note.position, a.note.sourceOrder} <
                                        std::pair{b.note.position, b.note.sourceOrder};
                             });
    std::vector<bool> retained(notes.size(), true);
    std::optional<std::size_t> head;
    std::vector<std::size_t> overlappingHeads;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        CheckChartLoadCancellation(stop);
        const auto &note = notes[i];
        if (note.note.actionType == 0)
            retained[i] = note.positionActive;
        else if (note.note.actionType == 1)
        {
            if (!head)
                head = i;
            else
                overlappingHeads.push_back(i);
        }
        else if (note.note.actionType == 2 && head && notes[*head].note.keyType == note.note.keyType)
        {
            if (note.timing < notes[*head].timing)
            {
                retained[*head] = retained[i] = false;
                for (const auto duplicate : overlappingHeads)
                {
                    CheckChartLoadCancellation(stop);
                    retained[duplicate] = false;
                }
            }
            head.reset();
            overlappingHeads.clear();
        }
    }
    std::size_t output = 0;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        CheckChartLoadCancellation(stop);
        if (retained[i])
        {
            if (output != i)
                notes[output] = std::move(notes[i]);
            ++output;
        }
    }
    notes.resize(output);
    return notes;
}
