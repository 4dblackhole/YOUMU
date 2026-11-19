#include "EditorView.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
using namespace PhantomCore;

using namespace editor_ui;

namespace
{

    constexpr std::size_t TileColumns = 128;
    constexpr std::array<Color, 16> SpectrumColors{{{0, 0, 0, 1},
                                                       {.08F, 0, .16F, 1},
                                                       {.18F, .01F, .32F, 1},
                                                       {.32F, .02F, .44F, 1},
                                                       {.48F, .02F, .46F, 1},
                                                       {.63F, .02F, .38F, 1},
                                                       {.77F, .03F, .27F, 1},
                                                       {.88F, .06F, .15F, 1},
                                                       {.97F, .14F, .05F, 1},
                                                       {1, .27F, .02F, 1},
                                                       {1, .41F, .01F, 1},
                                                       {1, .56F, .02F, 1},
                                                       {1, .71F, .07F, 1},
                                                       {1, .84F, .20F, 1},
                                                       {1, .94F, .50F, 1},
                                                       {1, 1, .86F, 1}}};

    std::wstring Decimal(const double value, const int precision)
    {
        std::wostringstream text;
        text << std::fixed << std::setprecision(precision) << value;
        return text.str();
    }

    using SourceBands = std::array<std::size_t, SpectrumBandCount>;
    SourceBands MapFrequencyBands(const AudioAnalysis &data, double minimumHz, double maximumHz)
    {
        SourceBands sourceBands{};
        if (data.frames.empty())
            return sourceBands;
        for (std::size_t band = 0; band < sourceBands.size(); ++band)
        {
            const double frequency =
                minimumHz * std::pow(maximumHz / minimumHz, (static_cast<double>(band) + .5) / sourceBands.size());
            sourceBands[band] = frequency > data.maximumFrequencyHz
                                    ? data.frames.front().bands.size()
                                    : static_cast<std::size_t>(
                                          std::clamp(std::log(frequency / data.minimumFrequencyHz) /
                                                         std::log(data.maximumFrequencyHz / data.minimumFrequencyHz) *
                                                         data.frames.front().bands.size(),
                                                     0.0, static_cast<double>(data.frames.front().bands.size() - 1)));
        }
        return sourceBands;
    }

    void AccumulateColumns(std::vector<SpectrumFrame> &columns, const AudioAnalysis &data, const double offset,
                           const double begin, const double window, const SourceBands &sourceBands)
    {
        if (data.frames.empty() || data.secondsPerFrame <= 0)
            return;
        const int firstColumn = static_cast<int>(std::clamp(std::floor((offset - begin) / window * columns.size()), 0.0,
                                                            static_cast<double>(columns.size())));
        const int lastColumn =
            static_cast<int>(std::clamp(std::ceil((offset + data.durationSeconds - begin) / window * columns.size()),
                                        0.0, static_cast<double>(columns.size())));
        for (int column = firstColumn; column < lastColumn; ++column)
        {
            const double left = std::max(0.0, begin + window * column / columns.size() - offset);
            const double right =
                std::min(data.durationSeconds, begin + window * (column + 1) / columns.size() - offset);
            if (right <= left)
                continue;
            const auto first = std::min(data.frames.size() - 1, static_cast<std::size_t>(left / data.secondsPerFrame));
            const auto last =
                std::min(data.frames.size(), static_cast<std::size_t>(std::ceil(right / data.secondsPerFrame)));
            auto &output = columns[static_cast<std::size_t>(column)];
            // Max-pooling retains short hits at every zoom level and combines
            // overlapping expected hits without hiding the music track.
            for (auto index = first; index < last; ++index)
            {
                const auto &frame = data.frames[index];
                output.minimum = std::min(output.minimum, frame.minimum);
                output.maximum = std::max(output.maximum, frame.maximum);
                for (std::size_t band = 0; band < sourceBands.size(); ++band)
                    if (sourceBands[band] < frame.bands.size())
                        output.bands[band] = std::max(output.bands[band], frame.bands[sourceBands[band]]);
            }
        }
    }

    std::vector<SpectrumFrame> BuildSpectrumColumns(const AnalysisBatch &data, const std::vector<AudioMarker> &markers,
                                                    std::size_t track, double start, double tileSeconds,
                                                    double minimumHz, double maximumHz, double longestSound)
    {
        const auto &sounds = data.sounds;
        std::vector<SpectrumFrame> columns(TileColumns);
        if (track == 0)
        {
            if (const auto music = sounds.find("Music"); music != sounds.end())
                AccumulateColumns(columns, music->second, 0, start, tileSeconds,
                                  MapFrequencyBands(music->second, minimumHz, maximumHz));
        }
        else
        {
            std::map<std::string, SourceBands> bands;
            for (auto marker = std::ranges::lower_bound(markers, start - longestSound, {}, &AudioMarker::seconds);
                 marker != markers.end() && marker->seconds <= start + tileSeconds; ++marker)
                if (const auto sound = sounds.find(marker->sound); sound != sounds.end())
                {
                    auto [mapping, inserted] = bands.try_emplace(marker->sound);
                    if (inserted)
                        mapping->second = MapFrequencyBands(sound->second, minimumHz, maximumHz);
                    AccumulateColumns(columns, sound->second, marker->seconds, start, tileSeconds, mapping->second);
                }
        }
        return columns;
    }

    void BuildSpectrumGeometry(EditorVisual &geometry, const SpectrumTile &tile, const Rect rect,
                               std::size_t visibleColumns)
    {
        const float columnWidth = rect.width / static_cast<float>(visibleColumns);
        const float bandHeight = rect.height / static_cast<float>(tile.bands);
        for (std::size_t band = 0; band < tile.bands; ++band)
            for (std::size_t column = 0; column < tile.columns;)
            {
                const auto colorAt = [&](std::size_t c) {
                    const std::size_t level = tile.levels[c * tile.bands + band];
                    return (level * (SpectrumColors.size() - 1) + SpectrumLevelMaximum / 2) / SpectrumLevelMaximum;
                };
                const auto color = colorAt(column);
                auto end = column + 1;
                while (end < tile.columns && colorAt(end) == color) ++end;
                if (color != 0)
                    geometry.AddRectangle({static_cast<float>(column) * columnWidth,
                                           static_cast<float>(band) * bandHeight,
                                           static_cast<float>(end - column) * columnWidth + .1F, bandHeight + .1F},
                                          SpectrumColors[color], 0);
                column = end;
            }
        geometry.Finish();
    }

    SpectrumTile OverviewTile(const std::vector<SpectrumFrame> &columns)
    {
        SpectrumTile tile{columns.size(), SpectrumBandCount, {}};
        tile.levels.reserve(tile.columns * tile.bands);
        for (const auto &column : columns)
            for (const float level : column.bands)
                tile.levels.push_back(static_cast<std::uint8_t>(std::lround(level * SpectrumLevelMaximum)));
        return tile;
    }
} // namespace

void EditorView::PrepareAudioTrack(std::size_t track, Rect rect, std::int64_t first, std::int64_t last,
                                   bool invalidate)
{
    if (!audioRoots_[track])
    {
        auto &root = node->CreateChild(track == 0 ? "Audio.MusicSpectrum" : "Audio.HitSpectrum");
        root.SetZIndex(2);
        audioRoots_[track] = &root;
        auto &overlay = root.CreateChild("Ruler");
        overlay.SetZIndex(1);
        audioOverlays_[track] = &overlay.AddComponent<EditorVisual>();
    }
    PlaceNode(*audioRoots_[track], rect);
    audioRoots_[track]->SetClipRect({0, 0, rect.width, rect.height});
    audioRoots_[track]->SetVisible(true);
    audioOverlays_[track]->Clear();
    auto &tiles = audioTiles_[track];
    for (auto it = tiles.begin(); it != tiles.end();)
    {
        // Retain one neighbouring tile on each side for short back-seeks.
        if (invalidate || it->first < first - 1 || it->first > last + 1)
        {
            static_cast<void>(audioRoots_[track]->RemoveChild(it->second.node->Id()));
            it = tiles.erase(it);
        }
        else
        {
            it->second.node->SetVisible(false);
            ++it;
        }
    }
}

void EditorView::UpdateWaveformColumns(double begin, std::size_t columns)
{
    const auto &sounds = state_.Analysis().Data().sounds;
    const auto music = sounds.find("Music");
    musicColumns_ = music != sounds.end() && music->second.pcm
                        ? BuildAudioWaveform(*music->second.pcm, begin, state_.audioWindow, columns)
                        : std::vector<WaveformPeak>(columns);
}

void EditorView::UpdateAudioTiles(double begin, double minimumHz, double maximumHz, double longestSound)
{
    const auto &musicTrack = audioTracks_[1];
    const auto columns = static_cast<std::size_t>(std::clamp(std::ceil(musicTrack.width * PixelScale()), 128.0F, 4096.0F));
    const auto bands = static_cast<std::size_t>(std::clamp(std::ceil(musicTrack.height * PixelScale()), 32.0F, 512.0F));
    const double columnSeconds = state_.audioWindow / static_cast<double>(columns);
    const double tileSeconds = columnSeconds * TileColumns;
    const auto first = static_cast<std::int64_t>(std::floor(begin / tileSeconds));
    const auto last = static_cast<std::int64_t>(std::floor((begin + state_.audioWindow) / tileSeconds));
    const AudioColumnsKey key{state_.audioWindow, minimumHz, maximumHz, state_.Analysis().Revision(), columns, bands};
    const bool invalidate = audioColumnsKey_ != key || audioColumnsDocument_ != &state_.Document() ||
                            audioColumnsDocumentRevision_ != state_.Document().Revision();
    const auto requestAt = [&](std::size_t track, std::int64_t index) {
        return EditorSpectrumRequest{track, {static_cast<double>(index) * tileSeconds, tileSeconds,
                                             TileColumns, bands, minimumHz, maximumHz}};
    };
    std::vector<EditorSpectrumRequest> requests;
    // Queue visible tiles first. Neighbours are lower priority and only warm the cache.
    for (std::size_t track = 0; track < 2; ++track)
        for (auto index = first; index <= last; ++index) requests.push_back(requestAt(track, index));
    for (std::size_t track = 0; track < 2; ++track)
    {
        requests.push_back(requestAt(track, first - 1));
        requests.push_back(requestAt(track, last + 1));
    }
    state_.Analysis().PrepareSpectrumTiles(requests);
    const auto &markers = state_.Analysis().Markers();
    const std::array rectangles{audioTracks_[1], audioTracks_[2]};
    for (std::size_t track = 0; track < rectangles.size(); ++track)
    {
        const auto rect = rectangles[track];
        PrepareAudioTrack(track, rect, first, last, invalidate);
        auto &tiles = audioTiles_[track];
        for (auto index = first; index <= last; ++index)
        {
            auto tile = tiles.find(index);
            const auto detail = state_.Analysis().Spectrum(requestAt(track, index));
            if (tile == tiles.end())
            {
                auto &tileNode = audioRoots_[track]->CreateChild("Spectrum.Tile");
                tileNode.AddComponent<EditorVisual>();
                tile = tiles.emplace(index, AudioTile{&tileNode, {}}).first;
                if (!detail)
                {
                    const auto overview = OverviewTile(BuildSpectrumColumns(state_.Analysis().Data(), markers, track,
                        static_cast<double>(index) * tileSeconds, tileSeconds, minimumHz, maximumHz, longestSound));
                    BuildSpectrumGeometry(*tileNode.GetComponent<EditorVisual>(), overview, rect, columns);
                }
            }
            if (detail && tile->second.detail != detail)
            {
                auto &geometry = *tile->second.node->GetComponent<EditorVisual>();
                geometry.Clear();
                BuildSpectrumGeometry(geometry, *detail, rect, columns);
                tile->second.detail = detail;
            }
            tile->second.node->SetVisible(true);
            tile->second.node->SetPosition(
                {static_cast<float>((index * tileSeconds - begin) / state_.audioWindow * rect.width), 0});
        }
    }
    UpdateWaveformColumns(begin, columns);
    audioColumnsKey_ = key;
    audioColumnsDocument_ = &state_.Document();
    audioColumnsDocumentRevision_ = state_.Document().Revision();
}

void EditorView::LayoutAudioTracks()
{
    // Waveform, music and expected hits share the width; heights follow 1.2 : 1 : .8.
    const float left = 24, trackWidth = std::max(1.0F, frame_.width - 48);
    const float top = editor_layout::BodyTop + 58, bottom = editor_layout::FooterTop - 14;
    const float unit = (bottom - top - 20 - 24) / 3;
    audioTracks_[0] = {left, top, trackWidth, unit * 1.2F};
    audioTracks_[1] = {left, top + unit * 1.2F + 36, trackWidth, unit};
    audioTracks_[2] = {left, audioTracks_[1].y + unit + 8, trackWidth, unit * .8F};
}

void EditorView::DrawAudioSpectrum(std::size_t track, const double minimumHz, const double maximumHz)
{
    const auto rect = audioTracks_[track];
    Box(rect, {0, 0, 0, 1});
    auto &overlay = *audioOverlays_[track - 1];
    for (const double frequency : {100.0, 1000.0, 5000.0})
    {
        if (frequency < minimumHz || frequency > maximumHz)
            continue;
        const float fraction = static_cast<float>(1 - std::log(frequency / minimumHz) / std::log(maximumHz / minimumHz));
        const float y = rect.y + rect.height * fraction;
        overlay.AddRectangle({0, rect.height - (y - rect.y), rect.width, 1}, {1, 1, 1, .10F}, 0);
        // Labels live in the overlay so the spectrum tiles cannot cover them (local origin bottom-left).
        const auto label = frequency >= 1000
                               ? Decimal(frequency / 1000, 0) + std::wstring(texts_.Get("Common.kilohertzSuffix"))
                               : Decimal(frequency, 0) + std::wstring(texts_.Get("Common.hertzSuffix"));
        const float top = std::clamp(y - 8, rect.y, rect.y + rect.height - 16) - rect.y;
        auto face = ui_fonts::Number(700);
        DrawPacket packet{};
        packet.type = DrawPacketType::Text;
        packet.bounds = {rect.width - 52, rect.height - top - 15, 46, 15};
        packet.color = {1, 1, 1, .7F};
        packet.text = std::make_shared<TextContent>(
            TextContent{label, std::move(face.font), 11, TextAlignment::Trailing, {}, face.weight});
        overlay.AddPacket(std::move(packet));
    }
}

void EditorView::DrawAudioWaveform(const std::vector<WaveformPeak> &columns)
{
    const auto waveform = audioTracks_[0];
    Box(waveform, {.035F, .045F, .05F, 1});
    const float centerY = waveform.y + waveform.height * .5F;
    Box({waveform.x, centerY, waveform.width, 1}, {.25F, .35F, .3F, 1});
    for (std::size_t column = 0; column < columns.size(); ++column)
    {
        const float top = centerY - std::clamp(columns[column].maximum, -1.0F, 1.0F) * waveform.height * .48F;
        const float bottom = centerY - std::clamp(columns[column].minimum, -1.0F, 1.0F) * waveform.height * .48F;
        if (bottom > top || top != centerY)
            Box({waveform.x + waveform.width * static_cast<float>(column) / static_cast<float>(columns.size()), top,
                 waveform.width / static_cast<float>(columns.size()) + .1F, std::max(1.0F / PixelScale(), bottom - top)},
                {.24F, .67F, .46F, 1});
    }
}

void EditorView::DrawAudioTimeRuler(const double begin)
{
    // One time ruler and cursor align all three tracks to the editor time.
    const auto waveform = audioTracks_[0];
    double step = std::pow(10.0, std::floor(std::log10(state_.audioWindow / 8)));
    const double ratio = state_.audioWindow / (8 * step);
    step *= ratio > 5 ? 10 : ratio > 2 ? 5 : ratio > 1 ? 2 : 1;
    for (double seconds = std::ceil(begin / step) * step; seconds <= begin + state_.audioWindow; seconds += step)
    {
        const float x = waveform.x + static_cast<float>((seconds - begin) / state_.audioWindow) * waveform.width;
        Label({std::clamp(x + 4, waveform.x, waveform.x + waveform.width - 70), waveform.y + waveform.height - 18, 66, 16},
              Decimal(seconds, step < 1 ? 2 : 0) + std::wstring(texts_.Get("Common.secondsSuffix")),
              {11, {1, 1, 1, .6F}, TextAlignment::Leading, 700, true});
        Box({x, waveform.y, 1, waveform.height}, {1, 1, 1, .08F});
        for (std::size_t track = 0; track < audioOverlays_.size(); ++track)
            audioOverlays_[track]->AddRectangle({x - waveform.x, 0, 1, audioTracks_[track + 1].height},
                                                {1, 1, 1, .08F}, 0);
    }
    Box({waveform.x + waveform.width * .25F - 1, waveform.y, 2, waveform.height}, ui_colors::theme::Sun);
    for (std::size_t track = 0; track < audioOverlays_.size(); ++track)
    {
        const auto &rect = audioTracks_[track + 1];
        audioOverlays_[track]->AddRectangle({rect.width * .25F - 1, 0, 2, rect.height}, ui_colors::theme::Sun, 0);
        audioOverlays_[track]->Finish();
    }
}

void EditorView::DrawAudio()
{
    const auto &text = Texts();
    try
    {
        state_.PrepareAudioPreview();
    }
    catch (const std::exception &error)
    {
        state_.Analysis().ClearMarkers(state_.Document().Revision());
        state_.SetStatus(error.what());
    }

    // Pool each source into the same visible time and frequency grid.
    const double begin = state_.TimeMilliseconds() / 1000.0 - state_.audioWindow * .25;
    double minimumHz = 20, maximumHz = 20'000, longestSound = 0;
    const auto &sounds = state_.Analysis().Data().sounds;
    if (!sounds.empty())
    {
        maximumHz = 0;
        for (const auto &[id, sound] : sounds)
        {
            maximumHz = std::max(maximumHz, sound.maximumFrequencyHz);
            minimumHz = std::min(minimumHz, sound.minimumFrequencyHz);
            if (id != "Music")
                longestSound = std::max(longestSound, sound.durationSeconds);
        }
        maximumHz = std::max(maximumHz, minimumHz * 2);
    }
    UpdateAudioTiles(begin, minimumHz, maximumHz, longestSound);

    // Draw distinct tracks before their shared ruler/cursor and controls.
    DrawAudioWaveform(musicColumns_);
    DrawAudioSpectrum(1, minimumHz, maximumHz);
    DrawAudioSpectrum(2, minimumHz, maximumHz);
    DrawAudioAlignment(begin);
    DrawAudioSelection(begin);
    DrawAudioTimeRuler(begin);
    // Track names: the waveform's lies in this visual; the spectra's above their tile nodes.
    const std::array captions{text.waveform, text.musicSpectrum, text.hitSoundSpectrum};
    for (std::size_t track = 0; track < captions.size(); ++track)
    {
        const std::wstring caption(captions[track]);
        const float captionWidth = EstimateTextWidth(caption, 12) + 12;
        if (track == 0)
        {
            const auto rect = audioTracks_[0];
            Box({rect.x + 8, rect.y + 6, captionWidth, 20}, {8 / 255.0F, 8 / 255.0F, 24 / 255.0F, .55F});
            Label({rect.x + 14, rect.y + 6, captionWidth, 20}, caption, {12, {1, 1, 1, .88F}, TextAlignment::Leading, 700});
            continue;
        }
        const auto &rect = audioTracks_[track];
        auto &overlay = *audioOverlays_[track - 1];
        overlay.AddRectangle({8, rect.height - 26, captionWidth, 20}, {8 / 255.0F, 8 / 255.0F, 24 / 255.0F, .55F}, 0);
        DrawPacket packet{};
        packet.type = DrawPacketType::Text;
        packet.bounds = {14, rect.height - 26, captionWidth, 20};
        packet.color = {1, 1, 1, .88F};
        packet.text = std::make_shared<TextContent>(
            TextContent{caption, ui_fonts::Text(caption), 12, TextAlignment::Leading, {}, 700});
        overlay.AddPacket(std::move(packet));
        overlay.Finish();
    }
    if (state_.Analysis().Running() || state_.Analysis().SpectrumRunning())
        Label({frame_.width - 324, editor_layout::BodyTop + 10, 300, 40}, std::wstring(text.analyzingAudio),
              {13, ui_colors::theme::Brand, TextAlignment::Trailing, 700});
    DrawAudioTools();
}
