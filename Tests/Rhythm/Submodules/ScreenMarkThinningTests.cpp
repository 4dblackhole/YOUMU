#include "GameScene/RhythmTestScene/Submodules/ScreenMarkThinning.h"
#include "TestSupport.h"

#include <cmath>
#include <limits>
#include <set>
#include <vector>

namespace tests
{
    namespace
    {
        // Admitted indexes of evenly spaced marks, visiting them from index `first`.
        std::vector<std::size_t> Admitted(float spacing, std::size_t count, float minimum, float screenPerLocal,
                                          std::size_t first = 0)
        {
            ScreenMarkThinning thinning(minimum, screenPerLocal);
            std::vector<std::size_t> result;
            for (std::size_t index = first; index < count; ++index)
                if (thinning.Admit(spacing * static_cast<float>(index)))
                    result.push_back(index);
            return result;
        }
    }

    void TestScreenMarkThinning()
    {
        Require(Admitted(5.0F, 64, 2.0F, 1.0F).size() == 64, "Marks already apart on screen are all drawn.");
        Require(Admitted(0.5F, 64, 0.0F, 1.0F).size() == 64, "A zero minimum distance draws every mark.");

        // A 1024-division half-measure Buzz at scroll 1x: 512 ticks 1.133 canvas pixels apart.
        const float spacing = 1160.53F / 1024.0F;
        const auto dense = Admitted(spacing, 512, 2.0F, 1.0F);
        std::set<long long> bands;
        for (std::size_t index = 0; index < 512; ++index)
            bands.insert(static_cast<long long>(std::floor(spacing * static_cast<float>(index) / 2.0F)));
        Require(dense.size() == bands.size(), "One mark is drawn for each 2-pixel band the marks cover.");
        Require(dense.size() > 280 && dense.size() < 300, "512 overlapping ticks become about 290 sprites.");
        for (std::size_t i = 1; i < dense.size(); ++i)
            Require(dense[i] - dense[i - 1] <= 2, "No gap wider than one band appears inside the dense run.");

        // The distance is in screen pixels: a 1080p window (1.5 screen pixels per canvas pixel)
        // keeps more marks, a slower scroll keeps fewer.
        Require(Admitted(spacing, 512, 2.0F, 1.5F).size() > dense.size(), "Larger windows keep more marks.");
        Require(Admitted(spacing * 0.5F, 512, 2.0F, 1.0F).size() < dense.size(), "Slower scroll keeps fewer marks.");

        // Bands start at the object's origin, so once the visible run starts later (ticks processed
        // or scrolled out) every later band keeps the same mark: nothing shimmers while scrolling.
        for (const std::size_t first : {1U, 7U, 100U, 333U})
        {
            const auto later = Admitted(spacing, 512, 2.0F, 1.0F, first);
            std::set<std::size_t> full(dense.begin(), dense.end());
            Require(later.front() == first, "The first visible mark is always drawn.");
            for (std::size_t i = 1; i < later.size(); ++i)
                Require(full.contains(later[i]), "Later bands choose the same mark as before.");
        }

        ScreenMarkThinning unknown(2.0F, 1.0F);
        Require(unknown.Admit(std::numeric_limits<float>::quiet_NaN()), "A mark without a finite position is drawn.");
    }
}
