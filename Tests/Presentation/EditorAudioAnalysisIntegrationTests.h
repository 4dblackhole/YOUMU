#pragma once
#include <filesystem>

// Actual shared PCM decoders are a Client integration concern.
// shippedAudio is the tracked built-in song's audio; defaultSkin is the built-in Default Skin folder.
void RunEditorAudioAnalysisIntegrationTests(const std::filesystem::path &shippedAudio,
                                            const std::filesystem::path &defaultSkin);
