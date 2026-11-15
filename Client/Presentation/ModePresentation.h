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

// One binding column of a key overlay, in the layout's binding order: the gameplay
// input panel's key face, its tint and its bounds in that panel's pixels. The column
// head draws the face at a common height, keeping the faces' vertical offsets.
struct ModeKeyOverlayFace
{
    std::string_view skinImage;
    PhantomCore::Color tint;
    PhantomCore::Rect sourceBounds;
};

// A centred key settings modal opened from one card in the options Input section.
struct ModeKeyOverlay
{
    std::string_view nodePrefix;
    std::string_view titleTextKey;
    std::span<const ModeKeyOverlayFace> faces;
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
