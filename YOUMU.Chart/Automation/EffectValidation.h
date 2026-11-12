#pragma once
#include "Model/Submodules/EffectDocument.h"
#include "Parsing/LoadCancellation.h"

class MusicalTimeline;
// Shared semantic rules for file parsing, editing and session construction.
void ValidateEffectValue(EffectCommandType type, double value);
void ValidateEffectCommand(const EffectCommand &command, std::stop_token stop = {});
void ResolveEffectInterpolations(EffectDocument &document, std::stop_token stop = {});
void ValidateEffectDocument(const EffectDocument &document, std::stop_token stop = {});
void ValidateEffectPositions(const EffectDocument &document, const MusicalTimeline &timeline,
                             std::stop_token stop = {});
