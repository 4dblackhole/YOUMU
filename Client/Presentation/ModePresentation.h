#pragma once

#include "PhantomCore.h"

#include <span>
#include <string_view>

// Client-only presentation registered per game mode id. Judgement, notes and
// input layouts stay in YOUMU.Modes; an entry only names the skin files, key
// editor and captions the Client shows for that mode. A supported mode without
// an entry plays without built-in hit sounds, edits its keys as inline rows in
// the Input tab and shows the binding display names declared by its layout.
// Entries are defined in ModePresentation.cpp.

struct ModeBindingCaption
{
    std::string_view id;
    std::string_view textKey;
};

// One decoded sample shared by every sound id in the list.
struct ModeHitSoundFile
{
    std::span<const char *const> soundIds;
    std::wstring_view file;
};

// One binding column of a key overlay, in the layout's binding order. The face
// image is placed on the layout image using bounds in that image's pixels.
struct ModeKeyOverlayFace
{
    std::string_view skinImage;
    PhantomCore::Color tint;
    PhantomCore::Rect sourceBounds;
    std::wstring_view heading;
};

// A centered key editor opened from one entry in the Input tab.
struct ModeKeyOverlay
{
    std::string_view nodePrefix;
    std::string_view titleTextKey;
    std::string_view layoutImage;
    float layoutImageSourceSize;
    std::span<const ModeKeyOverlayFace> faces;
    float settingsEntryHeight;
};

struct ModePresentation
{
    std::string_view modeId;
    std::wstring_view hitSoundFolder;
    std::span<const ModeHitSoundFile> hitSounds;
    const ModeKeyOverlay *keyOverlay{};
    std::span<const ModeBindingCaption> bindingCaptions;
};

extern const ModePresentation TaikoPresentation;

[[nodiscard]] std::span<const ModePresentation *const> ModePresentations() noexcept;
[[nodiscard]] const ModePresentation *FindModePresentation(std::string_view modeId) noexcept;
