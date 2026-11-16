#include <Windows.h>
#include "MusicSelectTestInput.h"
#include "App/AssetPaths.h"
#include "SongSearchInputTests.h"
#include "GameScene/MusicSelectScene/Submodules/EditorOsuImport.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
#include "GameFlow/PlaySessionLoader.h"
#include "Judgement/AccuracyRange.h"
#include "Presentation/MarqueeTextComponent.h"
#include "Presentation/PerformanceBand.h"
#include <chrono>
#include <cmath>
#include <map>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
using namespace PhantomCore;

void TestPlayOptionsPanel();

namespace
{

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class Renderer final : public Visual2DRenderSystem
    {
      public:
        std::vector<std::filesystem::path> loaded;
        ImageHandle LoadImageFile(const std::filesystem::path &path) override
        {
            if (path.filename() == "preview-cpu-fail.jpg" || path.filename() == "preview-gpu-fail.jpg")
            {
                loaded.push_back(path);
                throw std::runtime_error("Cannot load image");
            }
            const auto id = ++id_;
            if (path.filename().wstring().starts_with(L"preview-"))
            {
                loaded.push_back(path);
                imageSizes_[id] = path.filename() == "preview-portrait.jpg" ? Size{900, 1600} : Size{1600, 900};
            }
            return {id};
        }
        Size GetImageSize(ImageHandle image) const noexcept override
        {
            const auto found = imageSizes_.find(image.value);
            return found == imageSizes_.end() ? Size{96, 96} : found->second;
        }
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point,
                          std::uint32_t) override
        {
        }
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const Float4x4 &,
                         Size, const Float4x4 &) override
        {
        }
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &,
                             const RenderContext &) override
        {
        }

      private:
        std::uint64_t id_{};
        std::map<std::uint64_t, Size> imageSizes_;
    };
    std::size_t Cards(const Visual2DNode &node)
    {
        std::size_t count = node.Name().starts_with("Song.") ? 1 : 0;
        for (const auto &child : node.Children())
            count += Cards(*child);
        return count;
    }
    bool HasCard(const Visual2DNode &node, std::size_t index)
    {
        if (node.Name() == "Song." + std::to_string(index))
            return true;
        for (const auto &child : node.Children())
            if (HasCard(*child, index))
                return true;
        return false;
    }
    Visual2DNode *Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    void Click(Visual2DCanvas &canvas, Point position)
    {
        Visual2DInputRouter router;
        PointerInput pointer{};
        pointer.available = true;
        pointer.position = position;
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
    }
    void Click(Visual2DCanvas &canvas, const Visual2DNode &node)
    {
        const auto bounds = node.BoundsInCanvas();
        Click(canvas, Point{bounds.x + bounds.width * .5F, bounds.y + bounds.height * .5F});
    }
    void TestBaseBpmSortView()
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            Renderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            OptionSettings options("build/bpm-sort-unused.ini");
            SongSelectionState state;
            for (const auto bpm : {200.0, 80.0, 80.0})
            {
                SongCatalogEntry song;
                song.music.names = {bpm == 200 ? "Fast" : "Slow"};
                song.patterns.resize(1);
                song.patterns[0].pattern.baseBpm = bpm;
                state.catalog_.songs.push_back(std::move(song));
            }
            state.RebuildVisibleSongs();
            if (purpose == SongSelectPurpose::Editor) state.sortMode_ = SongSelectionState::SortMode::BaseBpm;
            MusicSelectView view(visuals, state, purpose, texts, options);
            view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("BPM sort Canvas missing.");
            auto *sort = Find(canvas->Root(), "SortSelector");
            if (!sort) throw std::runtime_error("BPM sort selector missing.");
            auto *behavior = sort->GetComponent<UiDropdownComponent>();
            if (!behavior) throw std::runtime_error("BPM sort behavior missing.");
            auto &combo = *behavior;
            Check(combo.Items().size() == 4 && combo.Items()[3] == L"Base BPM",
                  "The Korean sort menu exposes Base BPM alongside existing modes.");
            Check(combo.SelectedIndex() == static_cast<std::size_t>(state.sortMode_),
                  "A newly initialized view reflects its retained sort mode.");
            Click(*canvas, *sort);
            Check(combo.IsExpanded(), "The actual sort menu must open.");
            const auto bounds = sort->BoundsInCanvas();
            // The list hangs under the box: 4px of padding, then 34px rows.
            Click(*canvas, Point{bounds.x + bounds.width / 2, bounds.y - 4.0F - 34.0F * 3.5F});
            InputState input;
            Check(view.Update(input, 0) == SongSelectCommand::None &&
                      state.sortMode_ == SongSelectionState::SortMode::BaseBpm &&
                      state.visibleSongIndices_ == std::vector<std::size_t>{1, 2, 0} &&
                      state.focusedSongPosition_ == 2 && HasCard(canvas->Root(), 2),
                  "Clicking Base BPM must reorder real cards and retain the focused song.");
            texts.SetLanguage(Language::English);
            static_cast<void>(view.Update(input, 0));
            Check(combo.Items()[3] == L"Base BPM" && combo.SelectedIndex() == 3,
                  "Language changes must retain the selected BPM mode.");
            state.searchText_ = L"slow";
            view.RebuildVisibleSongs();
            Check(state.visibleSongIndices_ == std::vector<std::size_t>{1, 2} && combo.SelectedIndex() == 3,
                  "Search results must keep BPM ordering and stable ties.");
            view.Shutdown();
            Check(visuals.CanvasCount() == 0, "BPM sort test releases its Canvas.");
        }
    }
    Visual2DNode &Need(Visual2DNode *node, const char *message)
    {
        if (node == nullptr)
            throw std::runtime_error(message);
        return *node;
    }
    // The options drawer keeps its own tooltip on the same Canvas, so look for the one that is shown.
    const TextVisualComponent *ShownTooltip(Visual2DNode &node)
    {
        if (node.Name() == "Tooltip" && node.IsVisible())
            if (auto *label = Find(node, "Label"))
                return label->GetComponent<TextVisualComponent>();
        for (const auto &child : node.Children())
            if (const auto *shown = ShownTooltip(*child))
                return shown;
        return nullptr;
    }
    Point Centre(const Visual2DNode &node)
    {
        const auto bounds = node.BoundsInCanvas();
        return {bounds.x + bounds.width * .5F, bounds.y + bounds.height * .5F};
    }
    void Drag(MusicSelectView &view, InputState &input, Point from, Point to)
    {
        InputStateTestAccess::Begin(input);
        InputStateTestAccess::Mouse(input, from, 0);
        InputStateTestAccess::Button(input, true);
        static_cast<void>(view.Update(input, 0));
        InputStateTestAccess::Begin(input);
        InputStateTestAccess::Mouse(input, to, 0);
        static_cast<void>(view.Update(input, 0));
        InputStateTestAccess::Begin(input);
        InputStateTestAccess::Mouse(input, to, 0);
        InputStateTestAccess::Button(input, false);
        static_cast<void>(view.Update(input, 0));
    }
    // Each frame reports the pointer again: BeginUpdate forgets that it is inside the window.
    void Rest(MusicSelectView &view, InputState &input, Point at)
    {
        for (int frame = 0; frame < 2; ++frame)
        {
            InputStateTestAccess::Begin(input);
            InputStateTestAccess::Mouse(input, at, 0);
            static_cast<void>(view.Update(input, .3));
        }
    }
    // Common parts in the song list: the shared scrollbar, the now-playing mark and card name hints.
    void TestSongListCommonParts(SongSelectPurpose purpose)
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts("Client/Texts/Languages");
        OptionSettings options("build/common-parts-unused.ini");
        SongSelectionState state;
        for (int i = 0; i < 20; ++i)
        {
            SongCatalogEntry song;
            song.music.names = {"Song " + std::to_string(i)};
            song.music.artists = {"Artist"};
            song.patterns.resize(1);
            state.catalog_.songs.push_back(std::move(song));
        }
        state.catalog_.songs[0].music.names = {"Alpha", "Alpha Alias"};
        state.RebuildVisibleSongs();
        MusicSelectView view(visuals, state, purpose, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        Check(canvas != nullptr, "Common parts Canvas missing.");
        InputState input;
        static_cast<void>(view.Update(input, 0));

        auto &viewport = Need(Find(canvas->Root(), "SongViewport"), "The song viewport must exist.");
        auto &scrollbar = Need(Find(viewport, "Scrollbar"), "The song list must own the shared scrollbar.");
        Check(scrollbar.IsVisible(), "Twenty songs overflow the list and show its scrollbar.");
        auto &thumb = Need(Find(scrollbar, "Thumb"), "The scrollbar must have its bar.");
        Check(scrollbar.BoundsInCanvas().width == 12 && thumb.BoundsInCanvas().width == 6,
              "The song list uses the shared 12px strip with a 6px bar.");
        // Drag the bar down: the list scrolls and the first card leaves the virtualized range.
        const auto grab = Centre(thumb);
        Drag(view, input, grab, {grab.x, grab.y - 300});
        Check(!HasCard(canvas->Root(), 0) && thumb.BoundsInCanvas().y < grab.y - 200,
              "Dragging the song list's bar scrolls the cards.");
        // Above the rail's top but still inside the window, where the pointer stays known.
        Drag(view, input, Centre(thumb), {grab.x, grab.y + 100});
        Check(HasCard(canvas->Root(), 0), "Dragging back to the top shows the first card again.");

        // The focused card's title gives way to the now-playing mark only while its song plays.
        auto &card = Need(Find(canvas->Root(), "Song.0"), "The focused card must exist.");
        auto &mark = Need(Find(card, "NowPlaying"), "The focused card must hold the now-playing mark.");
        auto &title = Need(Find(card, "Title"), "The focused card must have a title.");
        Check(!mark.IsVisible(), "A silent song shows no now-playing mark.");
        const float titleX = title.BoundsInCanvas().x;
        view.SetNowPlaying(true);
        Check(mark.IsVisible() && std::abs(title.BoundsInCanvas().x - titleX - 21) < .01F,
              "While the song plays, the mark stands 21px before its title.");
        Check(mark.Children().size() == 3 && mark.BoundsInCanvas().width == 13, "The mark is three bars in a 13px box.");
        view.SetNowPlaying(false);
        Check(!mark.IsVisible() && title.BoundsInCanvas().x == titleX, "Pausing removes the mark.");

        // A card title with several names hints them all; a single uncut name has no hint.
        Rest(view, input, Centre(title));
        const auto *tipText = ShownTooltip(canvas->Root());
        Check(tipText != nullptr && tipText->Text() == L"Alpha · Alpha Alias",
              "Resting on a card title lists every name of the song.");
        Rest(view, input, Centre(Need(Find(card, "Artist"), "The card must have an artist line.")));
        Check(ShownTooltip(canvas->Root()) == nullptr, "A single artist that fits its line has no hint.");
        view.Shutdown();
    }

    // The F7 band: FPS and UPS in a navy band on the bottom-right corner; draw calls in Debug only.
    void TestPerformanceBand()
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        TextCatalog texts("Client/Texts/Languages");
        PerformanceBand band;
        band.Initialize(visuals, texts);
        const auto &segments = band.Segments();
        Check(!band.IsVisible() && segments.size() == (PerformanceBand::ShowsDrawCalls ? 4U : 2U),
              "The hidden band holds FPS and UPS, and draw calls only in Debug builds.");
        Check(segments[0].name == L"FPS" && segments[0].value == L"—" && !segments[0].separated,
              "Values read a dash until the first measurement.");
        Check(segments[1].name == L"UPS" && segments[1].separated, "A rule separates UPS from FPS.");
        PerformanceStatistics measured;
        measured.framesPerSecond = 144;
        measured.updatesPerSecond = 1000;
        measured.drawCallsPerFrame = 87;
        measured.peakDrawCallsPerFrame = 120;
        measured.hasMeasurement = true;
        band.Refresh(measured);
        band.SetVisible(true);
        band.Update();
        Check(segments[0].value == L"144" && segments[1].value == L"1000", "The band reads FPS 144 | UPS 1000.");
        if constexpr (PerformanceBand::ShowsDrawCalls)
            Check(segments[2].name == L"Draws" && segments[2].value == L"87" && segments[3].value == L"120",
                  "Debug builds add the draw calls and the busiest frame.");
        const auto bounds = band.Band()->BoundsInCanvas();
        Check(std::abs(bounds.x + bounds.width - 640) < .01F && std::abs(bounds.y + 360) < .01F &&
                  bounds.height == PerformanceBand::Height,
              "The band is fixed to the bottom-right corner.");
        Check(Find(*band.Band(), "Slant") != nullptr && Find(*band.Band(), "Body") != nullptr,
              "The band's left edge is slanted.");
        band.Shutdown();
        Check(visuals.CanvasCount() == 0, "The band releases its Canvas.");
    }

    void TestSongPanelTransparency(SongSelectPurpose purpose)
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280,720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        OptionSettings options("build/panel-opacity-unused.ini");
        SongSelectionState selection;
        for (const auto *image : {"preview-wide.jpg", "preview-unselected.jpg"})
        {
            SongCatalogEntry song;
            song.music.names = {"Panel regression"};
            song.patterns.resize(1);
            song.patterns[0].pattern.backgroundFile = image;
            song.patterns[0].patternPath = options.SongsRoot() / L"Panel regression/Chart/Set/panel.ymp";
            selection.catalog_.songs.push_back(std::move(song));
        }
        MusicSelectView view(visuals,selection,purpose,texts,options);
        view.Initialize({meshes,textRendering,renderer,audio,1280,720});
        view.SetVisible(true);
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas) throw std::runtime_error("Panel regression Canvas missing.");
        Check(renderer.loaded.size()==1, "Collapsed song cards must not request chart images.");
        for (const auto width : {405U,1280U,1920U})
        {
            visuals.OnResize(width,720);
            view.OnResize(width,720);
            static_cast<void>(canvas->BuildDrawList());
            for (const auto *name : {"RecordPanel","SongInformation","SongBrowser"})
            {
                auto *panel = Find(canvas->Root(),name);
                if (!panel) { Check(purpose==SongSelectPurpose::Editor && std::string_view(name)=="RecordPanel", "Gameplay keeps its assigned record area."); continue; }
                Check(!panel->GetComponent<SpriteVisualComponent>(),
                      "Columns are not framed panels; one sheet lies under the details and the list.");
            }
            Check(!Find(canvas->Root(),"RecordVeil") && !Find(canvas->Root(),"ListVeil") && !Find(canvas->Root(),"Categories"),
                  "The side veils and the ALL bar no longer exist.");
            {
                // Game and editor share the sheet: it starts right of the record column and
                // reaches the right screen edge, with its shadow band 10 to the left.
                const auto *sheet=Find(canvas->Root(),"Sheet"), *edge=Find(canvas->Root(),"SheetEdge");
                const auto *extension=Find(canvas->Root(),"SheetExtension"), *rule=Find(canvas->Root(),"ColumnRule");
                if (!sheet || !edge) throw std::runtime_error("Song-select sheet parts missing.");
                if (!extension || !rule) throw std::runtime_error("Song-select sheet extension or column rule missing.");
                const auto logical=canvas->LogicalSize();
                const float scale=std::min(1.0F,logical.width/1280.0F), boardLeft=-logical.width/2;
                const auto area=sheet->BoundsInCanvas(), band=edge->BoundsInCanvas(), line=rule->BoundsInCanvas();
                Check(std::abs(area.x-(boardLeft+362*scale))<.01F && std::abs(area.width-918*scale)<.01F &&
                          std::abs(area.height-720*scale)<.01F,
                      "The sheet image starts at x 362 and spans the height of the 16:9 arrangement.");
                Check(std::abs(band.x-(boardLeft+352*scale))<.01F && std::abs(band.width-36*scale)<.01F,
                      "The shadow band follows the sheet's slanted edge from x 352.");
                const bool wide=logical.width>1280.01F;
                const float right=wide?extension->BoundsInCanvas().x+extension->BoundsInCanvas().width:area.x+area.width;
                Check(extension->IsVisible()==wide && std::abs(right-logical.width/2)<.01F,
                      "The sheet reaches the right screen edge; only a wider screen needs the extension.");
                const auto list=Find(canvas->Root(),"SongBrowser")->BoundsInCanvas();
                Check(std::abs(line.x+line.width-list.x)<.01F && std::abs(line.width-scale)<.01F && line.height<list.height,
                      "A thin rule separates the song details from the song list.");
            }
            {
                const auto *board=Find(canvas->Root(),purpose==SongSelectPurpose::Editor?"YOUMU.EditorSongSelect.Board":"YOUMU.SongSelect.Board");
                if (!board) throw std::runtime_error("Song-select board missing.");
                const auto bounds=board->BoundsInCanvas();
                const auto logical=canvas->LogicalSize();
                Check(std::abs(bounds.width-logical.width)<.01F &&
                          std::abs(bounds.width/bounds.height-std::max(logical.width,1280.0F)/720.0F)<.01F,
                      "A screen narrower than 16:9 scales the whole 16:9 arrangement instead of squeezing columns.");
                const auto *browser=Find(canvas->Root(),"SongBrowser");
                const auto list=browser->BoundsInCanvas();
                Check(std::abs(list.x+list.width-logical.width/2)<.01F,
                      "The song list stays attached to the right screen edge.");
            }
            for (const auto &card : Find(canvas->Root(),"SongContent")->Children())
            {
                if (!card->Name().starts_with("Song.")) continue;
                auto *title=Find(*card,"Title"), *artist=Find(*card,"Artist");
                if (!title || !artist) throw std::runtime_error("Card metadata fields missing.");
                Check(!Find(*card,"Thumbnail") && title->GetComponent<TextVisualComponent>() && title->NodeSize().height==20 &&
                          artist->Bounds().x==title->Bounds().x,
                      "Every card shows one title line and one artist line without a BG thumbnail.");
            }
            auto *button=Find(canvas->Root(),"FilterButton");
            Check(button && button->GetComponent<TextVisualComponent>()->TextColor().alpha==1,
                  "Translucent control surfaces keep opaque captions.");
        }
        Check(renderer.loaded.size()==1,"Resizing list cards must not request unselected chart BGs.");
        view.Shutdown();
    }
    SongCatalogLoadResult AwaitCatalog(SongCatalogLoader &loader)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        do
        {
            if (auto result = loader.Poll())
                return std::move(*result);
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("Catalog worker timed out.");
    }
    void TestConverterProcess()
    {
        namespace fs = std::filesystem;
        const auto directory =
            fs::temp_directory_path() /
            (L"YOUMU-import-test-한글 " + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(directory);
        const auto cleanup = [](fs::path *path)
        {
            std::error_code ignored;
            fs::remove_all(*path, ignored);
            delete path;
        };
        const auto guard = std::unique_ptr<fs::path, decltype(cleanup)>(new fs::path(directory), cleanup);
        const auto input = directory / L"map.osu", root = directory / L"Songs";
        std::ofstream(input) << "osu file format v14\n[General]\nAudioFilename: audio.mp3\nMode: 1\n"
                                "[Metadata]\nTitle:Song\nArtist:Artist\nCreator:Author\nVersion:Oni\n"
                                "[Difficulty]\nSliderMultiplier:1.4\nSliderTickRate:1\nOverallDifficulty:5\n"
                                "[TimingPoints]\n0,500,4,1,0,100,1,0\n[HitObjects]\n0,0,0,1,0\n";
        std::ofstream(directory / "audio.mp3", std::ios::binary) << "audio fixture";
        EditorOsuImport importer;
        const auto await = [&]
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (auto result = importer.Poll())
                    return *result;
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            throw std::runtime_error("Converter process did not finish.");
        };
        importer.Start(input, root);
        const auto result = await();
        Check(result.success, result.message.c_str());
        const auto catalog = SongCatalog{}.Load(root);
        Check(catalog.Succeeded() && catalog.songs.size() == 1 && catalog.PatternCount() == 1,
              "The bundled converter process must produce a complete catalog entry.");
        const auto &convertedPattern = catalog.songs.front().patterns.front().pattern;
        Check(convertedPattern.judgementLevel == JudgeLevel{34.28},
              "An OD 5 import must retain hundredth precision and select the nearest GOOD window.");
        const AccuracyRange convertedRange("Imported.OD5", convertedPattern.judgementLevel);
        Check(convertedRange.HalfWindow(JudgementGrade::Good) == RhythmDuration{79'492},
              "The Client must read the imported level as a 79.492 ms GOOD half-window.");
        Check(convertedRange.Evaluate({}, RhythmTime{79'492}).grade == JudgementGrade::Good &&
                  convertedRange.Evaluate({}, RhythmTime{79'493}).grade == JudgementGrade::Bad,
              "The saved level must control the Client's actual GOOD boundary.");
        importer.Start(input, root);
        const auto collision = await();
        Check(!collision.success && !collision.message.empty(),
              "Converter collisions must report failure through Poll.");
        Check(SongCatalog{}.Load(root).PatternCount() == 1,
              "A failed process must preserve the existing pattern.");
    }
    void TestDifficultyClick(ScreenVisual2DManager &visuals, SongSelectionState &selection,
                             TextCatalog &texts, OptionSettings &options,
                             const EngineServices &services)
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            selection.SelectVisibleSong(0);
            selection.selectedPatternIndex_ = 0;
            MusicSelectView clickable(visuals, selection, purpose, texts, options);
            clickable.Initialize(services);
            auto *canvas = visuals.FindCanvas(purpose == SongSelectPurpose::Gameplay ? 2 : 3);
            if (!canvas)
                throw std::runtime_error("Clickable selection Canvas missing.");
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                const auto *difficulty = Find(canvas->Root(), "Difficulty.1");
                if (!difficulty)
                    throw std::runtime_error("Clickable difficulty missing.");
                Click(*canvas, *difficulty);
                InputState noKeys;
                if (attempt == 0)
                {
                    Check(clickable.Update(noKeys, 0) == SongSelectCommand::None && selection.selectedPatternIndex_ == 1,
                          "The first click on a difficulty only selects it.");
                    difficulty = Find(canvas->Root(), "Difficulty.1");
                    if (!difficulty)
                        throw std::runtime_error("Selected difficulty missing.");
                    Click(*canvas, *difficulty);
                }
                Check(clickable.Update(noKeys, 0) == SongSelectCommand::Launch && selection.selectedPatternIndex_ == 1,
                      "Clicking the selected difficulty starts it.");
                Check(clickable.Update(noKeys, 0) == SongSelectCommand::None,
                      "A difficulty click must emit the launch command only once.");
            }
            if(purpose==SongSelectPurpose::Editor)
            {
                Check(!Find(canvas->Root(),"GameModeSelect"),"Editor selection retains its existing footer scope.");
                const auto *optionsButton=Find(canvas->Root(),"OptionSelect");
                if(!optionsButton)throw std::runtime_error("Editor global options button is missing.");
                Click(*canvas,*optionsButton);InputState input;static_cast<void>(clickable.Update(input,.2));
                static_cast<void>(clickable.Update(input,.2));
                Check(Find(canvas->Root(),"Options.Panel")->IsVisible(),"Editor footer still opens its global settings.");
            }
            clickable.Shutdown();
        }
    }
    void TestMetadataLines()
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            Renderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            OptionSettings options("build/metadata-lines-unused.ini");
            SongSelectionState state;
            SongCatalogEntry song;
            song.music.names = {"Angel Dream", "", "天使の夢", "천사의 꿈", "Alias four", "Alias five"};
            song.music.artists = {"Artist A", "Artist B", "Artist C", "Artist D", "Artist E"};
            song.patterns.resize(2);
            song.patterns[0].pattern.makers = {"Maker A", "Maker B", "Maker C", "Maker D"};
            song.patterns[1].pattern.makers = {"Other maker"};
            state.catalog_.songs.push_back(song);
            for (int i = 0; i < 20; ++i)
            {
                SongCatalogEntry other;
                other.music.names = {"Other " + std::to_string(i)};
                other.patterns.resize(1);
                state.catalog_.songs.push_back(std::move(other));
            }
            MusicSelectView view(visuals, state, purpose, texts, options);
            view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Metadata-lines Canvas missing.");
            const auto field = [&](Visual2DNode &parent, const char *name) -> std::wstring_view
            {
                auto *node = Find(parent, name);
                if (!node) throw std::runtime_error("Metadata field missing.");
                auto *component = node->GetComponent<TextVisualComponent>();
                if (!component) throw std::runtime_error("Metadata text component missing.");
                return component->Text();
            };
            Check(field(canvas->Root(), "SelectedSong") == L"Angel Dream" &&
                      field(canvas->Root(), "SelectedArtist") == L"Artist A" &&
                      field(canvas->Root(), "SelectedCreator") == L"Maker A",
                  "Song details show only the representative name, artist and maker, one line each.");
            auto *card = Find(canvas->Root(), "Song.0");
            auto *content = Find(canvas->Root(), "SongContent");
            if (!card || !content) throw std::runtime_error("Metadata song card missing.");
            Check(field(*card, "Title") == L"Angel Dream" && field(*card, "Artist") == L"Artist A",
                  "Song cards show the same representative names as the detail view.");
            const auto captionBounds = Find(*card, "Title")->BoundsInCanvas();
            const Point point{captionBounds.x + 5, captionBounds.y + captionBounds.height / 2};
            const float originalY = content->Bounds().y;
            InputState input;
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view.Update(input, .3));
            input = {};
            InputStateTestAccess::Mouse(input, point, -1);
            static_cast<void>(view.Update(input, .3));
            Check(content->Bounds().y == originalY, "Global options prevent background list scrolling.");
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view.Update(input, .3));
            input = {};
            static_cast<void>(view.Update(input, .3));
            input = {};
            InputStateTestAccess::Mouse(input, point, -1);
            static_cast<void>(view.Update(input, 0));
            Check(content->Bounds().y != originalY, "Wheeling over a card caption scrolls the song list.");
            state.SelectPattern(1);
            view.RefreshSelectionPresentation();
            Check(field(canvas->Root(), "SelectedCreator") == L"Other maker",
                  "Changing the pattern refreshes its maker.");
            state.catalog_.songs.clear();
            view.RebuildVisibleSongs();
            Check(field(canvas->Root(), "SelectedSong").empty() && field(canvas->Root(), "SelectedArtist").empty() &&
                      field(canvas->Root(), "SelectedCreator").empty(),
                  "An empty catalog leaves the detail column empty.");
            view.Shutdown();
        }
    }
    void FavoriteNameDialog(MusicSelectView &view, Visual2DCanvas &canvas, const char *action, std::wstring value, bool cancel = false)
    {
        struct Driver { std::wstring value; bool cancel{}; std::atomic_bool handled{}; } driver;
        driver.value = std::move(value);
        driver.cancel = cancel;
        const auto thread = GetCurrentThreadId();
        std::jthread input([&](std::stop_token stop)
        {
            while (!stop.stop_requested() && !driver.handled)
            {
                EnumThreadWindows(thread, [](HWND window, LPARAM parameter) -> BOOL
                {
                    wchar_t type[32]{};
                    GetClassNameW(window, type, 32);
                    if (std::wstring_view(type) != L"#32770" || !GetDlgItem(window, 100)) return TRUE;
                    auto &data = *reinterpret_cast<Driver *>(parameter);
                    ShowWindow(window, SW_HIDE);
                    SetDlgItemTextW(window, 100, data.value.c_str());
                    PostMessageW(window, WM_COMMAND, data.cancel ? IDCANCEL : IDOK, 0);
                    data.handled = true;
                    return FALSE;
                }, reinterpret_cast<LPARAM>(&driver));
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        });
        auto *button = Find(canvas.Root(), action);
        if (!button) throw std::runtime_error("Favorite name action missing.");
        Click(canvas, *button);
        InputState none;
        Check(view.Update(none, 0) == SongSelectCommand::None, "Favorite names never emit scene commands.");
        input.request_stop();
        input.join();
        Check(driver.handled, "Favorite create/rename uses the native Unicode name dialog.");
    }
    void TestFavoriteFolders()
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            Renderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            OptionSettings options("build/favorites-view-unused.ini");
            SongSelectionState state;
            SongCatalogEntry song;
            song.music.names = {"Favorite song"};
            song.patterns.resize(3);
            for (std::size_t i = 0; i < 3; ++i)
            {
                song.patterns[i].patternPath = "build/favorite-view-" + std::to_string(i) + ".ymp";
                song.patterns[i].noteCount = (i + 1) * 100;
            }
            const auto key = SongFavorites::PatternKey(song.patterns[2].patternPath);
            state.catalog_.songs.push_back(std::move(song));
            SongFavoritesValues values;
            for (std::size_t i = 0; i < 12; ++i) values.Create(L"Folder " + std::to_wstring(i + 1));
            values.SetMembership(1, SongFavorites::PatternKey(state.catalog_.songs[0].patterns[0].patternPath), true);
            values.SetMembership(1, key, true);
            std::string error;
            Check(state.favorites_.Save(values, error), "Seed in-memory favorites.");
            MusicSelectView view(visuals, state, purpose, texts, options);
            view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Favorites Canvas missing.");
            InputState none;
            const auto click = [&](const std::string &name)
            {
                auto *node = Find(canvas->Root(), name);
                if (!node) throw std::runtime_error("Favorite control missing: " + name);
                Click(*canvas, *node);
                Check(view.Update(none, 0) == SongSelectCommand::None, "Favorite controls consume their own actions.");
            };
            auto *content = Find(canvas->Root(), "Favorites.TabContent");
            auto *tabs = Find(canvas->Root(), "Favorites.Tabs");
            auto *scroll = Find(canvas->Root(), "Favorites.TabScroll");
            if (!content || !tabs || !scroll) throw std::runtime_error("Favorite tab nodes missing.");
            Check(scroll->IsVisible(), "Overflowing folders expose horizontal tab scrolling.");
            const auto bounds = tabs->BoundsInCanvas();
            const auto songY = Find(canvas->Root(), "SongContent")->Bounds().y;
            InputState wheel;
            InputStateTestAccess::Mouse(wheel, {bounds.x + 20, bounds.y + 20}, -1);
            static_cast<void>(view.Update(wheel, 0));
            Check(content->Bounds().x < 0 && Find(canvas->Root(), "SongContent")->Bounds().y == songY,
                  "Wheel over tabs scrolls horizontally without moving the song list.");
            click("Favorites.Tab.1");
            Check(state.favoriteFolder_ == 1 && !Find(canvas->Root(), "Difficulty.1") && Find(canvas->Root(), "Difficulty.2"),
                  "Folder tab projects the matching real pattern rows.");
            InputState down;
            InputStateTestAccess::Key(down, VK_DOWN, true);
            static_cast<void>(view.Update(down, 0));
            Check(state.selectedPatternIndex_ == 2, "Favorite keyboard navigation skips nonmember patterns.");
            click("Favorites.Manage");
            auto *overlay = Find(canvas->Root(), "Favorites.Overlay");
            Check(overlay && overlay->IsVisible(), "Favorites opens one modal folder/membership editor.");
            click("Favorites.Member.2");
            Check(!state.favorites_.Values().Find(2)->patternKeys.contains(key), "Membership edits remain a draft.");
            click("Favorites.Close");
            Check(!state.favorites_.Values().Find(2)->patternKeys.contains(key), "Close cancels membership changes.");
            click("Favorites.Manage");
            InputState blocked;
            InputStateTestAccess::Key(blocked, VK_SPACE, true);
            InputStateTestAccess::Key(blocked, VK_DOWN, true);
            Check(view.Update(blocked, 0) == SongSelectCommand::None && state.selectedPatternIndex_ == 2,
                  "Favorites modal blocks navigation and launch keys.");
            click("Favorites.Member.2");
            click("Favorites.Apply");
            Check(state.favorites_.Values().Find(1)->patternKeys.contains(key) && state.favorites_.Values().Find(2)->patternKeys.contains(key),
                  "Apply preserves simultaneous membership in multiple folders.");
            click("Favorites.Manage");
            FavoriteNameDialog(view, *canvas, "Favorites.Create", L"練習 한글 🎵");
            FavoriteNameDialog(view, *canvas, "Favorites.Rename", L"Renamed 練習");
            FavoriteNameDialog(view, *canvas, "Favorites.Rename", L"Cancelled name", true);
            Check(state.favorites_.Values().folders.size() == 12, "Create and rename are also transactional drafts.");
            click("Favorites.Apply");
            Check(state.favorites_.Values().Find(13) && state.favorites_.Values().Find(13)->name == L"Renamed 練習",
                  "Native names persist together, and cancelling a name dialog preserves the draft name.");
            state.favoriteFolder_ = 13;
            view.RebuildVisibleSongs();
            Check(state.visibleSongIndices_.empty(), "Selecting a new empty favorite folder shows an empty result.");
            click("Favorites.Manage");
            Check(!Find(canvas->Root(), "Favorites.Member.1")->IsEnabled(), "No selected pattern disables membership editing while folder management remains available.");
            const auto list = Find(canvas->Root(), "Favorites.Folders")->BoundsInCanvas();
            InputState listWheel;
            InputStateTestAccess::Mouse(listWheel, {list.x + 10, list.y + 10}, -1);
            static_cast<void>(view.Update(listWheel, 0));
            Check(!Find(canvas->Root(), "Favorites.Folder.1") && Find(canvas->Root(), "Favorites.Folder.7"), "Folder management scrolls its own list.");
            while (!Find(canvas->Root(), "Favorites.Folder.13")) click("Favorites.Next");
            click("Favorites.Folder.13");
            click("Favorites.Delete");
            click("Favorites.Apply");
            Check(state.favoriteFolder_ == 0 && state.visibleSongIndices_.size() == 1 && state.VisiblePatternCount(0) == 3,
                  "Deleting the selected folder returns to All and leaves every chart intact.");
            texts.SetLanguage(Language::English);
            static_cast<void>(view.Update(none, 0));
            Check(Find(canvas->Root(), "Favorites.Tab.0")->GetComponent<TextVisualComponent>()->Text() == L"All",
                  "Folder controls follow language changes while custom names retain Unicode.");
            view.Shutdown();
            Check(visuals.CanvasCount() == 0, "Favorite controls release the view Canvas.");
        }
    }
    void TestFilterProjection()
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            Renderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            OptionSettings options("build/song-filter-unused.ini");
            SongSelectionState state;
            SongCatalogEntry song;
            song.music.names = {"Filtered song"};
            song.patterns.resize(3);
            for (std::size_t i = 0; i < 3; ++i) song.patterns[i].noteCount = 100 * i;
            state.catalog_.songs.push_back(std::move(song));
            state.filter_.minimumNotes = 100;
            MusicSelectView view(visuals, state, purpose, texts, options);
            view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Filter projection Canvas missing.");
            Check(!Find(canvas->Root(), "Difficulty.0") && Find(canvas->Root(), "Difficulty.1") &&
                      Find(canvas->Root(), "Difficulty.2"), "Only matching actual pattern rows are drawn.");
            Click(*canvas, *Find(canvas->Root(), "Difficulty.2"));
            InputState input;
            Check(view.Update(input, 0) == SongSelectCommand::None && state.selectedPatternIndex_ == 2,
                  "The first click on a projected pattern selects its original catalog index.");
            Click(*canvas, *Find(canvas->Root(), "Difficulty.2"));
            Check(view.Update(input, 0) == SongSelectCommand::Launch && state.selectedPatternIndex_ == 2,
                  "Clicking a projected pattern launches its original catalog index.");
            Check(view.Update(input, 0) == SongSelectCommand::None, "Filtered clicks launch only once.");
            auto *button = Find(canvas->Root(), "FilterButton");
            if (!button) throw std::runtime_error("Song filter button missing.");
            Click(*canvas, *button);
            static_cast<void>(view.Update(input, 0));
            const auto knob = [&](std::size_t row) {
                return Find(*Find(canvas->Root(), "SongFilter.Switch." + std::to_string(row)), "Knob")->Bounds().x;
            };
            Check(knob(0) > 10 && knob(4) < 10 && !Find(canvas->Root(), "SongFilter.Switch.4")->IsEnabled(),
                  "A row with a value shows its switch on; an empty row's switch is off and inert.");
            Click(*canvas, *Find(canvas->Root(), "SongFilter.Switch.0"));
            static_cast<void>(view.Update(input, 0));
            Click(*canvas, *Find(canvas->Root(), "SongFilter.Apply"));
            static_cast<void>(view.Update(input, 0));
            Check(!state.filter_.IsActive() && state.filter_.minimumNotes == 100 && Find(canvas->Root(), "Difficulty.0"),
                  "Switching a row off keeps its value but stops it filtering.");
            Click(*canvas, *button);
            static_cast<void>(view.Update(input, 0));
            Check(knob(0) < 10 && Find(canvas->Root(), "SongFilter.Switch.0")->IsEnabled(),
                  "Reopening keeps the row switched off and switchable.");
            Click(*canvas, *Find(canvas->Root(), "SongFilter.Switch.0"));
            static_cast<void>(view.Update(input, 0));
            const auto selected = state.selectedPatternIndex_;
            const auto scroll = Find(canvas->Root(), "SongContent")->Bounds().y;
            InputStateTestAccess::Key(input, VK_DOWN, true);
            InputStateTestAccess::Key(input, VK_SPACE, true);
            Check(view.Update(input, 0) == SongSelectCommand::None && state.selectedPatternIndex_ == selected &&
                      Find(canvas->Root(), "SongContent")->Bounds().y == scroll,
                  "Filter modal blocks underlying navigation and launch commands.");
            input = {};
            auto *reset = Find(canvas->Root(), "SongFilter.Reset");
            auto *apply = Find(canvas->Root(), "SongFilter.Apply");
            if (!reset || !apply) throw std::runtime_error("Filter action buttons missing.");
            Click(*canvas, *reset);
            static_cast<void>(view.Update(input, 0));
            Click(*canvas, *apply);
            static_cast<void>(view.Update(input, 0));
            Check(!state.filter_.IsActive() && Find(canvas->Root(), "Difficulty.0"),
                  "Applying a reset restores the hidden difficulty row.");
            for (const auto width : {900U, 1920U})
            {
                visuals.OnResize(width, 720);
                view.OnResize(width, 720);
                const auto sort = Find(canvas->Root(), "SortSelector")->BoundsInCanvas();
                const auto filter = button->BoundsInCanvas();
                Check(sort.x + sort.width < filter.x && sort.y == filter.y && sort.height == filter.height,
                      "Filter and sort controls share their responsive row without overlap.");
            }
            view.Shutdown();
        }
    }
    void TestHpInformation()
    {
        for (const auto purpose : {SongSelectPurpose::Gameplay, SongSelectPurpose::Editor})
        {
            Renderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            OptionSettings options("build/hp-information-unused.ini");
            SongSelectionState selection;
            SongCatalogEntry song;
            song.music.names = {"HP information"};
            song.patterns.resize(2);
            song.patterns[0].pattern.hpLevel = HpLevel{17.25};
            selection.catalog_.songs.push_back(std::move(song));
            MusicSelectView view(visuals, selection, purpose, texts, options);
            view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("HP information Canvas missing.");
            auto *hp = Find(canvas->Root(), "Detail.HP.Value");
            if (!hp) throw std::runtime_error("HP information label missing.");
            auto *value = hp->GetComponent<TextVisualComponent>();
            if (!value) throw std::runtime_error("HP information text missing.");
            Check(value->Text() == L"17.25", "Song details show the selected pattern's exact HP level.");
            selection.SelectPattern(1);
            view.RefreshSelectionPresentation();
            Check(value->Text() == L"9.00", "Unspecified HP displays the existing 9.00 default.");
            texts.SetLanguage(Language::English);
            InputState input;
            static_cast<void>(view.Update(input, 0));
            Check(value->Text() == L"9.00", "Language switching refreshes HP without changing its value.");
            auto *panel = Find(canvas->Root(), "SongInformation");
            if (!panel) throw std::runtime_error("Song information column missing.");
            for (const auto width : {900U, 1280U, 1920U})
            {
                visuals.OnResize(width, 720);
                view.OnResize(width, 720);
                const auto bounds = panel->BoundsInCanvas();
                float previousRight = bounds.x;
                for (const auto *name : {"Detail.BPM.Value", "Detail.Length.Value", "Detail.Notes.Value"})
                {
                    auto *cell = Find(canvas->Root(), name);
                    if (!cell) throw std::runtime_error("Pattern figure missing.");
                    const auto actual = cell->BoundsInCanvas();
                    Check(actual.x >= previousRight - .01F && actual.x + actual.width <= bounds.x + bounds.width + .01F &&
                              actual.y >= bounds.y && actual.y + actual.height <= bounds.y + bounds.height + .01F,
                          "The figures sit side by side inside the detail column without overlap.");
                    previousRight = actual.x + actual.width;
                }
            }
            selection.catalog_.songs[0].patterns.clear();
            view.RebuildVisibleSongs();
            Check(value->Text().empty(), "A song without a pattern clears HP instead of showing a default chart.");
            selection.catalog_.songs.clear();
            view.RebuildVisibleSongs();
            Check(value->Text().empty(), "An empty catalog clears the former HP information.");
            view.Shutdown();
            Check(visuals.CanvasCount() == 0, "HP information releases its Canvas.");
        }
    }
    void TestBackgroundPreview(SongSelectPurpose purpose)
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        texts.SetLanguage(Language::English);
        OptionSettings options("build/background-test-unused.ini");
        SongSelectionState selection;
        SongCatalogEntry song;
        song.music.names.push_back("Background test");
        for (const auto *file : {"preview-wide.jpg", "preview-portrait.jpg", "", "preview-cpu-fail.jpg", "preview-gpu-fail.jpg"})
        {
            SongCatalogPattern pattern;
            pattern.pattern.backgroundFile = file;
            pattern.patternPath = options.SongsRoot() / L"Background test/Chart/Set/background.ymp";
            song.patterns.push_back(std::move(pattern));
        }
        song.patterns.front().pattern.judgementLevel = JudgeLevel{40.37};
        song.patterns.front().pattern.backgroundOffsetX = 70;
        song.patterns.front().pattern.backgroundOffsetY = -25;
        song.patterns.front().bpmRange = std::pair{120.0, 180.0};
        selection.selectedDurationMicroseconds_ = 125'000'000;
        selection.catalog_.songs.push_back(std::move(song));
        MusicSelectView view(visuals, selection, purpose, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas) throw std::runtime_error("Background Canvas missing.");
        const auto detail = [&](const char *name) -> std::wstring_view {
            const auto *node = Find(canvas->Root(), name);
            if (!node || !node->GetComponent<TextVisualComponent>()) throw std::runtime_error("Missing detail label.");
            return node->GetComponent<TextVisualComponent>()->Text();
        };
        Check(detail("Detail.JudgeLevel.Value") == L"40.37" &&
              detail("Detail.HP.Value") == L"9.00" &&
              detail("Detail.BPM.Value") == L"120–180" &&
              detail("Detail.Length.Value") == L"2:05", "Selected pattern details use real data.");
        {
            // The figures sit in a 448-wide table: strong rules above and below, light ones between rows and columns.
            const auto *information = Find(canvas->Root(), "SongInformation");
            if (!information) throw std::runtime_error("Song information column missing.");
            const auto column = information->BoundsInCanvas();
            const float tableLeft = column.x + column.width * .5F - 224.0F;
            const float tableTop = column.y + column.height - 419.5F;
            std::array<Rect, 5> rules{};
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                const auto *rule = Find(canvas->Root(), ("Detail.Rule." + std::to_string(i)).c_str());
                Check(rule != nullptr && rule->IsVisible(), "Every figure table rule is shown for a chart.");
                rules[i] = rule->BoundsInCanvas();
            }
            Check(std::abs(rules[0].x - tableLeft) < .01F && std::abs(rules[0].width - 448.0F) < .01F &&
                      std::abs(rules[0].y + 1.0F - tableTop) < .01F && std::abs(rules[1].y - (tableTop - 137.0F)) < .01F,
                  "Strong rules close the figure table above and below.");
            Check(std::abs(rules[2].y - (tableTop - 69.0F)) < .01F && std::abs(rules[3].x - (tableLeft + 448.0F / 3.0F)) < .01F &&
                      std::abs(rules[4].x - (tableLeft + 896.0F / 3.0F)) < .01F && std::abs(rules[3].height - 137.0F) < .01F,
                  "Light rules divide the two rows and the three columns.");
            const auto *artistIcon = Find(canvas->Root(), "ArtistIcon");
            const bool hasArtist = !detail("SelectedArtist").empty();
            Check(artistIcon != nullptr && artistIcon->IsVisible() == hasArtist &&
                      artistIcon->BoundsInCanvas().x + 16.0F < column.x + column.width * .5F,
                  "A music note stands before the artist name, as the quill does before the creator.");
        }
        const auto *keyPanel = Find(canvas->Root(), "Options.Taiko.Keys");
        if (!keyPanel)
            throw std::runtime_error("Taiko key settings modal missing.");
        // Column heads: the Gameplay key faces at one height, Kat raised above Don as on the input panel.
        constexpr std::array<Rect, 4> gameplayKeys{{
            {16, 68, 39, 70}, {68, 92, 39, 70}, {120, 92, 39, 70}, {172, 68, 39, 70}}};
        constexpr float columnWidth = (432.0F - 48.0F - 30.0F) / 4.0F, faceScale = 24.0F / 70.0F;
        const auto panelHeight = keyPanel->NodeSize().height;
        std::size_t keyIndex{};
        for (const auto &child : keyPanel->Children())
        {
            if (child->Name() != "Options.Taiko.KeyImage") continue;
            Check(keyIndex < gameplayKeys.size(), "Unexpected extra Gameplay key image.");
            const auto actual = child->Bounds();
            const auto expected = gameplayKeys[keyIndex];
            const float centre = 24.0F + columnWidth * .5F + (columnWidth + 10.0F) * static_cast<float>(keyIndex++);
            const float top = 78.0F + (expected.y - 68.0F) * faceScale;
            Check(child->IsVisible() && std::abs(actual.x + actual.width * .5F - centre) < .01F &&
                  std::abs(panelHeight - actual.y - actual.height - top) < .01F &&
                  std::abs(actual.width - expected.width * faceScale) < .01F && std::abs(actual.height - 24.0F) < .01F,
                  "Taiko key settings heads must reuse the Gameplay key faces over their columns.");
        }
        Check(keyIndex == gameplayKeys.size(), "Taiko key settings must display all four Gameplay key faces.");
        auto *image = Find(canvas->Root(), "PreviewImage");
        auto *preview = Find(canvas->Root(), "BackgroundPreview");
        if (!image || !preview) throw std::runtime_error("Background nodes missing.");
        auto *sprite = image->GetComponent<SpriteVisualComponent>();
        if (!sprite) throw std::runtime_error("Background sprite missing.");
        auto *pageBackground = Find(canvas->Root(), "SongBackground");
        if (!pageBackground) throw std::runtime_error("Full-screen song background missing.");
        auto *pageSprite = pageBackground->GetComponent<SpriteVisualComponent>();
        if (!pageSprite) throw std::runtime_error("Full-screen song background sprite missing.");
        InputState input;
        const auto tick = [&] { static_cast<void>(view.Update(input, 0)); };
        const auto select = [&](std::size_t index) {
            selection.selectedPatternIndex_ = index;
            view.RefreshSelectionPresentation();
        };
        const auto songBackgrounds = options.SongsRoot() / L"Background test/BG";
        Check(renderer.loaded.size() == 1 && renderer.loaded.back() == songBackgrounds / L"preview-wide.jpg",
              "Artwork must load from the BG folder of the selected pattern's song folder.");
        Check(pageBackground->IsVisible() && pageSprite->Style().normalImage.value == sprite->Style().normalImage.value,
              "The selected pattern image is shared by the full-screen background and existing preview.");
        select(1);
        tick();
        Check(renderer.loaded.size() == 2 && renderer.loaded.back() == songBackgrounds / L"preview-portrait.jpg",
              "Changing a difficulty must load the selected pattern from its song folder's BG.");
        Check(image->IsVisible() && sprite->Style().normalImage.value,
              "Ready artwork fills the jacket.");
        Check(std::abs(sprite->UvScale().x - 1) < .001F && std::abs(sprite->UvScale().y - .5625F) < .001F &&
                  std::abs(sprite->UvOffset().y - .21875F) < .001F,
              "Portrait artwork must use a centered aspect-preserving crop.");
        select(0);
        tick();
        Check(std::abs(sprite->UvScale().x - .5625F) < .001F && std::abs(sprite->UvScale().y - 1) < .001F,
              "Landscape artwork must retain its original aspect ratio.");
        const auto calls = renderer.loaded.size();
        selection.catalog_.songs[0].patterns[0].pattern.backgroundOffsetX = 35;
        selection.catalog_.songs[0].patterns[0].pattern.backgroundOffsetY = -15;
        view.RefreshSelectionPresentation();
        for (int i = 0; i < 10; ++i) tick();
        Check(renderer.loaded.size() == calls, "Ready artwork must not be requested every frame.");
        for (const auto width : {900U, 1920U})
        {
            visuals.OnResize(width, 720);
            view.OnResize(width, 720);
            Check(image->NodeSize().width == 196 && image->NodeSize().height == 196 &&
                      preview->NodeSize().width == 206,
                  "The square jacket keeps its size inside the frame's rim on every screen width.");
            const auto bg = pageBackground->BoundsInCanvas();
            const auto size = canvas->LogicalSize();
            // Narrower than 16:9, the 16:9 arrangement is scaled down between letterbox bands.
            const float scale = std::min(1.0F, size.width / 1280.0F);
            const float coveredWidth = std::max(size.width, 1280.0F) * scale, coveredHeight = 720.0F * scale;
            Check(bg.x <= -coveredWidth / 2 + .01F && bg.x + bg.width >= coveredWidth / 2 - .01F &&
                      bg.y <= -coveredHeight / 2 + .01F && bg.y + bg.height >= coveredHeight / 2 - .01F &&
                      std::abs(bg.width / bg.height - 1600.0F / 900) < .001F,
                  "Selected artwork covers the whole 16:9-or-wider arrangement with its source aspect ratio.");
            Check(std::abs(bg.x + bg.width / 2 - 35 * scale) < .01F && std::abs(bg.y + bg.height / 2 + 15 * scale) < .01F,
                  "Changing authored offsets for the same BG updates its framing without another image load.");
            const auto *band = Find(canvas->Root(), "SongSelect.LetterboxBand");
            Check(band && band->IsVisible() == (scale < 1.0F),
                  "Letterbox bands appear only when the screen is narrower than 16:9.");
        }
        select(2);
        tick();
        Check(!image->IsVisible() && !sprite->Style().normalImage.value,
              "An unassigned background must clear the previous image.");
        Check(!pageBackground->IsVisible() && !pageSprite->Style().normalImage,
              "An unassigned BG restores the skin background instead of leaving the previous chart image.");
        for (const auto index : {3U, 4U})
        {
            select(index);
            tick();
            Check(!image->IsVisible(), "Decode/upload errors keep the empty jacket.");
            Check(!pageBackground->IsVisible(), "Failed BG loads also preserve the screen's existing background.");
            const auto failedCalls = renderer.loaded.size();
            tick();
            view.RefreshSelectionPresentation();
            Check(renderer.loaded.size() == failedCalls, "Failed artwork must not retry every frame.");
        }
        select(0);
        tick();
        selection.searchText_ = L"no match";
        view.RebuildVisibleSongs();
        Check(!image->IsVisible(), "Empty search results must clear the selected artwork.");
        Check(!pageBackground->IsVisible(), "Empty search results clear the full-screen selected BG.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Background shutdown must release the Canvas.");
    }
    void TestRecordHistoryView()
    {
        const auto directory=std::filesystem::temp_directory_path()/
            ("YOUMU-record-view-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);} } cleanup{directory};
        const auto database=directory/"plays.sqlite3", patternPath=directory/"fixture.ymp";
        std::ofstream(patternPath)<<"Mode: Taiko";
        PlayRecord record;record.patternPath=patternPath;record.mode="Taiko";record.completed=true;record.cleared=true;
        record.totalNotes=record.processedNotes=1;
        for(int i=0;i<8;++i)
        {
            record.options.random=i%2==0;record.judgeLevel=record.options.random?70:50;
            record.accuracy=AccuracyScore::FromRate(.90+i*.001);PlayRecordStore(database).Save(record);
        }
        Renderer renderer;ScreenVisual2DManager visuals;visuals.Initialize(renderer,{1280,720});
        MeshRenderSystem meshes;TextRenderSystem textRendering;AudioSystem audio;
        TextCatalog texts("Client/Texts/Languages");OptionSettings options(directory/"Option.ini");
        SongSelectionState selection;SongCatalogEntry song;song.music.names.push_back("Record fixture");
        SongCatalogPattern pattern;pattern.patternPath=patternPath;pattern.pattern.mode="Taiko";pattern.pattern.name="Recorded";
        song.patterns.push_back(pattern);pattern.patternPath=directory/"other.ymp";song.patterns.push_back(pattern);
        selection.catalog_.songs.push_back(std::move(song));
        for (int i=0;i<20;++i)
        {
            auto extra=selection.catalog_.songs.front();
            extra.music.names={"Record fixture "+std::to_string(i)};
            selection.catalog_.songs.push_back(std::move(extra));
        }
        selection.searchText_=L"Record";
        MusicSelectView view(visuals,selection,SongSelectPurpose::Gameplay,texts,options,database);
        view.Initialize({meshes,textRendering,renderer,audio,1280,720});auto *canvas=visuals.FindCanvas(1);
        Check(canvas!=nullptr,"Record history owns its selection canvas.");InputState input;
        const auto rows=[&]{return Find(canvas->Root(),"RecordRows")->Children().size();};
        const auto wait=[&](std::size_t expected){
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{5};
            do{static_cast<void>(view.Update(input,0));if(rows()==expected)return;std::this_thread::sleep_for(std::chrono::milliseconds{1});}
            while(std::chrono::steady_clock::now()<deadline);
            throw std::runtime_error("Record history page did not publish the expected rows.");
        };
        wait(6);auto *next=Find(canvas->Root(),"RecordNext");if(!next)throw std::runtime_error("Record next page control is missing.");Check(next->IsEnabled(),"Actual history has an older page.");
        {
            // The column has no sheet: rows are plates of one width from the left screen edge, 58 apart.
            const auto &recordRows=Find(canvas->Root(),"RecordRows")->Children();
            const auto first=recordRows.front()->BoundsInCanvas();
            bool aligned=true;
            for(std::size_t i=0;i<recordRows.size();++i)
            {
                const auto bounds=recordRows[i]->BoundsInCanvas();
                const bool placed=std::abs(bounds.x+640)<.01F && std::abs(bounds.width-336)<.01F &&
                                  std::abs(first.y-bounds.y-58*static_cast<float>(i))<.01F;
                aligned=aligned && placed && recordRows[i]->GetComponent<SpriteVisualComponent>()!=nullptr;
            }
            Check(aligned && std::abs(first.height-52)<.01F,"Record rows are equal plates from the left screen edge, not following the sheet edge.");
            const auto *pagerPlate=Find(canvas->Root(),"RecordPagerPlate"), *notice=Find(canvas->Root(),"RecordMessagePlate");
            if(!pagerPlate||!notice)throw std::runtime_error("Record plates are missing.");
            Check(pagerPlate->IsVisible() && !notice->IsVisible(),"A history longer than one page puts its page controls on a plate and shows no notice.");
            const auto plate=pagerPlate->BoundsInCanvas(), previous=Find(canvas->Root(),"RecordPrevious")->BoundsInCanvas();
            const auto following=next->BoundsInCanvas();
            Check(plate.x<=previous.x && following.x+following.width<=plate.x+plate.width,"The page plate lies under both page buttons.");
        }
        Click(*canvas,*next);static_cast<void>(view.Update(input,0));wait(2);
        Click(*canvas,*Find(canvas->Root(),"RecordPrevious"));static_cast<void>(view.Update(input,0));wait(6);
        const auto viewport=Find(canvas->Root(),"SongViewport")->BoundsInCanvas();
        InputStateTestAccess::Mouse(input,{viewport.x+viewport.width/2,viewport.y+viewport.height/2},-3);
        static_cast<void>(view.Update(input,0));input={};
        const auto contentY=Find(canvas->Root(),"SongContent")->BoundsInCanvas().y;
        const auto selectedSong=selection.focusedSongPosition_, selectedPattern=selection.selectedPatternIndex_;
        auto *recordPanel = Find(canvas->Root(),"RecordPanel");
        if (!recordPanel) throw std::runtime_error("Record panel missing.");
        const auto recordBounds = recordPanel->BoundsInCanvas();
        Click(*canvas,*Find(canvas->Root(),"RecordQuery"));static_cast<void>(view.Update(input,0));
        auto *filter = Find(canvas->Root(),"RecordFilter.Overlay");
        if (!filter) throw std::runtime_error("Record filter overlay missing.");
        Check(filter && filter->IsVisible() && rows()==6,"Record filtering opens separately and leaves records in their assigned area.");
        Click(*canvas,*Find(canvas->Root(),"RecordFilter.Choice.1.1"));static_cast<void>(view.Update(input,0));
        Check(filter->IsVisible() && rows()==6,"Choosing Random On remains a draft until Apply.");
        Click(*canvas,*Find(canvas->Root(),"RecordFilter.Apply"));static_cast<void>(view.Update(input,0));
        Check(!filter->IsVisible(),"Apply closes only the record filter.");wait(4);
        Check(recordPanel->BoundsInCanvas().x==recordBounds.x && recordPanel->BoundsInCanvas().width==recordBounds.width,
              "Record results reuse the existing panel without relocating it.");
        Check(Find(canvas->Root(),"SongContent")->BoundsInCanvas().y==contentY && selection.searchText_==L"Record" &&
                  selection.focusedSongPosition_==selectedSong && selection.selectedPatternIndex_==selectedPattern,
              "Applying a record filter preserves song list scroll, search and the selected chart.");
        for(const auto &row:Find(canvas->Root(),"RecordRows")->Children())
            Check(Find(*row,"Details")->GetComponent<TextVisualComponent>()->Text().find(L"JL 70.00")!=std::wstring::npos,"Only the Random plays (JudgeLevel 70) appear.");
        selection.selectedPatternIndex_=1;view.RefreshSelectionPresentation();static_cast<void>(view.Update(input,0));wait(0);
        {
            const auto *notice=Find(canvas->Root(),"RecordMessagePlate");
            const auto plate=notice->BoundsInCanvas();
            Check(notice->IsVisible() && Find(canvas->Root(),"NoRecord")->IsVisible() && !Find(canvas->Root(),"RecordPagerPlate")->IsVisible(),
                  "A chart without history shows its notice on a plate and no page controls.");
            Check(std::abs(plate.x+640)<.01F && std::abs(plate.width-336)<.01F && std::abs(plate.height-52)<.01F,
                  "The notice plate takes the first record row's place.");
        }
        view.Shutdown();
    }
    void TestOptionsToggle(MusicSelectView &view, Visual2DCanvas &canvas)
    {
        auto *button = Find(canvas.Root(), "OptionSelect");
        auto *panel = Find(canvas.Root(), "Options.Panel");
        if (!button || !panel)
            throw std::runtime_error("Options toggle controls missing.");
        const auto *mode = Find(canvas.Root(), "GameModeSelect");
        Check(mode && mode->BoundsInCanvas().x + mode->BoundsInCanvas().width <= button->BoundsInCanvas().x, "Mode and the gear must have separate footer bounds.");
        const auto *playOptions = Find(canvas.Root(), "PlayOptions");
        Check(playOptions && mode->BoundsInCanvas().x + mode->BoundsInCanvas().width <= playOptions->BoundsInCanvas().x &&
                  playOptions->BoundsInCanvas().x + playOptions->BoundsInCanvas().width <= button->BoundsInCanvas().x,
              "Mode, Play options and the gear sit side by side in the footer.");
        Check(Find(canvas.Root(), "RecordQuery") != nullptr, "History panel exposes its condition query action.");
        InputState keys;
        InputStateTestAccess::Key(keys,VK_LCONTROL,true);
        InputStateTestAccess::Key(keys,'O',true);
        static_cast<void>(view.Update(keys,.2));
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',false);
        static_cast<void>(view.Update(keys,.2));
        Check(panel->IsVisible(), "Ctrl+O opens global settings independently of play options.");
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',true);
        static_cast<void>(view.Update(keys,.2));
        InputStateTestAccess::Begin(keys);
        InputStateTestAccess::Key(keys,'O',false);
        static_cast<void>(view.Update(keys,.2));
        Check(!panel->IsVisible(), "Ctrl+O closes global settings.");
    }
    // While the shared alert is open, Enter and Esc answer it and never reach song select.
    void TestAlertHoldsKeys()
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts("Client/Texts/Languages");
        OptionSettings options("build/alert-unused.ini");
        SongSelectionState state;
        SongCatalogEntry song;
        song.music.names = {"Song"};
        song.patterns.resize(1);
        state.catalog_.songs.push_back(std::move(song));
        state.RebuildVisibleSongs();
        MusicSelectView view(visuals, state, SongSelectPurpose::Gameplay, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas) throw std::runtime_error("Alert Canvas missing.");
        InputState input;
        const auto press = [&](const std::uint16_t key) {
            input = {};
            InputStateTestAccess::Key(input, key, true);
            return view.Update(input, 0);
        };
        for (const std::uint16_t key : {std::uint16_t{VK_RETURN}, std::uint16_t{VK_ESCAPE}})
        {
            view.ShowAlert({AlertKind::Error, L"Conversion failed", L"", L"converter output", {{L"OK", true}}});
            auto *alert = Find(canvas->Root(), "Alert");
            if (!alert) throw std::runtime_error("Alert node missing.");
            Check(press(key) == SongSelectCommand::None && alert->IsVisible(),
                  "The frame that opens an alert passes its keys to nothing.");
            Check(press(key) == SongSelectCommand::None && !alert->IsVisible(),
                  "Enter or Esc answers the alert without reaching song select.");
            const auto expected = key == VK_RETURN ? SongSelectCommand::Launch : SongSelectCommand::Back;
            Check(press(key) == expected, "Once the alert closes, song select takes its keys again.");
        }
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "The alert test releases its Canvas.");
    }
} // namespace

int main()
{
    try
    {
        TestConverterProcess();
        {
            Visual2DCanvas canvas;
            auto &label = CreateLabel(canvas.Root(), {0, 0, 100, 30}, L"");
            const std::wstring unicode = L"\U0001F600ABCDEFGH";
            auto &marquee = label.AddComponent<MarqueeTextComponent>(unicode, 0.28);
            marquee.Update(0);
            const auto initial = canvas.BuildDrawList();
            const auto source = initial.back().text;
            source->measuredSize = Size{180, 20};
            marquee.Update(1.3);
            const auto moved = canvas.BuildDrawList();
            if (moved.size() != 2 || moved[0].text != source || moved[1].text != source)
                throw std::runtime_error("Marquee must preserve its Unicode content and share its repeated layout.");
            if (moved[0].textOffset.x >= 0 || moved[1].textOffset.x <= 0 || source->value != unicode)
                throw std::runtime_error("Marquee must preserve its Unicode content and share its repeated layout.");
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source->value.data(),
                                    static_cast<int>(source->value.size()), nullptr, 0, nullptr, nullptr) <= 0)
                throw std::runtime_error("Marquee scrolling corrupted UTF-16.");
            label.SetSize({300, 30});
            marquee.Update(0);
            const auto resized = canvas.BuildDrawList();
            if (resized.size() != 1 || resized[0].textOffset.x != 0 || !label.ClipRect())
                throw std::runtime_error("A resized label must stop scrolling when the full title fits.");
            marquee.SetText(L"Short");
            marquee.Update(0);
            if (canvas.BuildDrawList().back().text->value != L"Short")
                throw std::runtime_error("Marquee content replacement failed.");
        }
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        TestMetadataLines();
        TestBaseBpmSortView();
        TestFilterProjection();
        TestFavoriteFolders();
        TestHpInformation();
        TestRecordHistoryView();
        TestAlertHoldsKeys();
        TestBackgroundPreview(SongSelectPurpose::Gameplay);
        TestBackgroundPreview(SongSelectPurpose::Editor);
        TestSongPanelTransparency(SongSelectPurpose::Gameplay);
        TestSongPanelTransparency(SongSelectPurpose::Editor);
        TestSongListCommonParts(SongSelectPurpose::Gameplay);
        TestSongListCommonParts(SongSelectPurpose::Editor);
        TestPerformanceBand();
        SongCatalogLoader loader;
        loader.Refresh(UserSongs());
        const auto first = AwaitCatalog(loader);
        Check(!first.songs.empty(), "Worker must publish the real catalog.");
        const auto playable = std::ranges::find_if(first.songs, [](const auto &song) { return !song.patterns.empty(); });
        Check(playable != first.songs.end(), "The worker regression requires a real playable chart.");
        const auto &pattern = playable->patterns.front();
        Check(pattern.pattern.notes.empty() && pattern.noteCount.has_value(),
              "Runtime catalog loading must publish compact metadata with the logical note count.");
        PlaySessionLoader sessions;
        sessions.Start({pattern.patternPath, pattern.effectPath, playable->audioPath, pattern.pattern.mode});
        Check(sessions.Busy(), "A session worker must retain its job until publication.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        std::optional<ModeLoadResult> loaded;
        while (!(loaded = sessions.Poll()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        Check(loaded && loaded->Succeeded() && !sessions.Busy() && !loaded->session->Gear().Lanes().empty(),
              "The worker must create and transfer a real playable session without loading on Poll.");
        Check(pattern.noteCount == loaded->session->PlayableNoteCount(),
              "Displayed catalog counts must equal the playable session logical note objects.");
        sessions.Start({"missing-regression.ymp", {}, "missing.mp3", "Taiko"});
        loaded.reset();
        while (!(loaded = sessions.Poll()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        Check(loaded && !loaded->Succeeded() && !loaded->diagnostics.empty(),
              "A failed pattern worker must return diagnostics instead of throwing on the UI thread.");
        loader.Refresh(UserSongs());
        const auto repeated = AwaitCatalog(loader);
        Check(first.songs.size() == repeated.songs.size() && first.PatternCount() == repeated.PatternCount(),
              "A retained parser cache must preserve catalog results.");
        loader.Refresh(UserSongs() / "nonexistent-regression-directory");
        const auto failed = AwaitCatalog(loader);
        Check(failed.songs.empty() && !failed.diagnostics.empty(),
              "Worker I/O errors must be published as diagnostics.");
        loader.Refresh(UserSongs());
        loader.Refresh(UserSongs() / "missing-new-root");
        const auto superseded = AwaitCatalog(loader);
        Check(superseded.songs.empty() && !superseded.diagnostics.empty(),
              "A root change must discard the older pending catalog.");
        loader.Refresh(UserSongs());
        loader.Refresh(UserSongs() / "missing-new-root");
        loader.Refresh(UserSongs());
        Check(!AwaitCatalog(loader).songs.empty(), "Switching back must cancel an obsolete queued root.");

        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        OptionSettings options("build/performance-view-test-unused.ini");
        SongSelectionState selection;
        // Synthetic data stays in this test process; no song or settings file is written.
        for (int i = 0; i < 5000; ++i)
        {
            SongCatalogEntry song;
            song.music.names.push_back("Test song " + std::to_string(i));
            song.music.artists.push_back("Test artist");
            song.patterns.resize(2);
            selection.catalog_.songs.push_back(std::move(song));
        }
        MusicSelectView view(visuals, selection, SongSelectPurpose::Gameplay, texts, options);
        view.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Music selection Canvas missing.");
        Check(canvas && !Find(canvas->Root(), "AddDifficulty.0"), "Gameplay cards must not offer chart creation.");
        Check(canvas != nullptr && Cards(canvas->Root()) > 0 && Cards(canvas->Root()) < 20,
              "A 5,000-song library must create only viewport cards.");
        for (const auto target : {2500U, 4999U, 0U})
        {
            selection.SelectVisibleSong(target);
            view.RebuildVisibleSongs();
            Check(Cards(canvas->Root()) < 20 && HasCard(canvas->Root(), target),
                  "Focus changes must preserve the original row index and keep the active tree bounded.");
        }
        visuals.OnResize(900, 720);
        view.OnResize(900, 720);
        Check(Cards(canvas->Root()) < 20 && HasCard(canvas->Root(), 0), "Resize must retain the focused visible card.");
        selection.searchText_ = L"4999 ARTIST";
        view.RebuildVisibleSongs();
        Check(selection.visibleSongIndices_ == std::vector<std::size_t>{4999} && HasCard(canvas->Root(), 0),
              "AND search must update actual cards using separate title/artist fields.");
        selection.searchText_ = L"ARTIST 4999 missing";
        view.RebuildVisibleSongs();
        Check(Cards(canvas->Root()) == 0, "One unmatched AND term must remove all former cards.");
        selection.searchText_ = L"no matching song";
        view.RebuildVisibleSongs();
        Check(Cards(canvas->Root()) == 0, "Empty searches must remove the former cards.");
        TestOptionsToggle(view, *canvas);
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Virtualized view must release its Canvas.");
        selection.searchText_.clear();
        TestDifficultyClick(visuals, selection, texts, options, {meshes, textRendering, renderer, audio, 1280, 720});
        selection.catalog_.songs[0].patterns.clear();
        MusicSelectView editor(visuals, selection, SongSelectPurpose::Editor, texts, options);
        editor.Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        canvas = visuals.FindCanvas(4);
        if (!canvas)
            throw std::runtime_error("Editor song-select Canvas missing.");
        auto *add = Find(canvas->Root(), "AddDifficulty.0");
        if (!add)
            throw std::runtime_error("Songs without a chart must offer the plus button.");
        Check(add->IsEnabled(), "Chart creation must be enabled after catalog loading.");
        Click(*canvas, *add);
        InputState input;
        Check(editor.Update(input, 0) == SongSelectCommand::None && selection.focusedSongPosition_ == 0,
              "Plus must select its song and open the menu without creating files.");
        auto *menu = Find(canvas->Root(), "SongAddition.Menu");
        auto *create = Find(canvas->Root(), "SongAddition.NewDifficulty");
        auto *convert = Find(canvas->Root(), "SongAddition.ConvertOsu");
        if (!menu || !create || !convert)
            throw std::runtime_error("Song addition menu is incomplete.");
        Check(menu->IsVisible(), "Plus must show the addition popup.");
        Click(*canvas, *create);
        Check(editor.Update(input, 0) == SongSelectCommand::CreateDifficulty && !menu->IsVisible(),
              "New Difficulty must close the popup and preserve its existing command.");
        add = Find(canvas->Root(), "AddDifficulty.0");
        if (!add)
            throw std::runtime_error("Refreshed plus button missing.");
        Click(*canvas, *add);
        static_cast<void>(editor.Update(input, 0));
        Click(*canvas, *convert);
        Check(editor.Update(input, 0) == SongSelectCommand::ConvertOsu && !menu->IsVisible(),
              "Convert .osu must issue only the import command.");
        editor.Shutdown();
        TestNativeSongSearch();
        TestPlayOptionsPanel();
        std::cout << "Music selection viewport, catalog worker and native Unicode search tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
