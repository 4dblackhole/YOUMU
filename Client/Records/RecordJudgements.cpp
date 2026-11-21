#include "Records/RecordJudgements.h"
#include "Taiko/Results/TaikoPlayResult.h"

std::vector<std::pair<std::string_view, std::size_t>> RecordJudgementCounts(const PlayRecord &record) noexcept
{
    if (record.detail.empty())
        return {};
    try
    {
        const auto result = record.LoadResult();
        const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
        if (taiko == nullptr)
            return {};
        const auto &counts = taiko->judgements;
        return {{"Judgements.MAX", counts[0]},  {"Judgements.PERFECT", counts[1]}, {"Judgements.GREAT", counts[2]},
                {"Judgements.GOOD", counts[3]}, {"Judgements.BAD", counts[4]},     {"Judgements.OK", taiko->okCount},
                {"Judgements.MISS", counts[5]}, {"Judgements.POOR", taiko->poorCount}};
    }
    catch (...)
    {
        // A damaged detail only loses the counts; the row itself still shows.
        return {};
    }
}
