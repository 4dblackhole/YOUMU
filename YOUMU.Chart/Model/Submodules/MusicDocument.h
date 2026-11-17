#pragma once
#include "Model/Submodules/ChartTypes.h"

struct MusicDocument
{
    int version{1};
    std::filesystem::path sourcePath;
    std::filesystem::path audioFile;
    // The romanized line above each block of notations. Empty when nobody filled it in; see
    // NotationText for what a screen shows then.
    std::string romanizedName;
    std::vector<std::string> names;
    std::string romanizedArtist;
    std::vector<std::string> artists;
    std::vector<std::string> tags;
};
