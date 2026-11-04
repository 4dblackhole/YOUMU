#include "MusicSelectTestInput.h"
#include "App/AssetPaths.h"
#include "SongSearchInputTests.h"
#include "GameScene/MusicSelectScene/Submodules/EditorOsuImport.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
#include "GameFlow/PlaySessionLoader.h"
#include "Judgement/AccuracyRange.h"
#include "Presentation/MarqueeTextComponent.h"
#include "Presentation/ScrollableTextComponent.h"
#include <chrono>
#include <cmath>
#include <map>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
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
        ImageHandle LoadImage(const std::filesystem::path &path) override
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
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const DirectX::XMFLOAT4X4 &,
                         Size, const DirectX::XMFLOAT4X4 &) override
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
            auto *behavior = sort->GetComponent<ComboBoxBehaviorComponent>();
            if (!behavior) throw std::runtime_error("BPM sort behavior missing.");
            auto &combo = *behavior;
            Check(combo.Items().size() == 4 && combo.Items()[3] == L"BASE BPM" && combo.MaxVisibleItems() == 4,
                  "The Korean sort menu exposes Base BPM alongside existing modes.");
            Check(combo.SelectedIndex() == static_cast<std::size_t>(state.sortMode_),
                  "A newly initialized view reflects its retained sort mode.");
            Click(*canvas, *sort);
            Check(combo.IsExpanded(), "The actual sort menu must open.");
            const auto bounds = sort->BoundsInCanvas();
            Click(*canvas, Point{bounds.x + bounds.width / 2, bounds.y - combo.ItemHeight() * 3.5F});
            InputState input;
            Check(view.Update(input, 0) == SongSelectCommand::None &&
                      state.sortMode_ == SongSelectionState::SortMode::BaseBpm &&
                      state.visibleSongIndices_ == std::vector<std::size_t>{1, 2, 0} &&
                      state.focusedSongPosition_ == 2 && HasCard(canvas->Root(), 2),
                  "Clicking Base BPM must reorder real cards and retain the focused song.");
            texts.SetLanguage(Language::English);
            static_cast<void>(view.Update(input, 0));
            Check(combo.Items()[3] == L"BASE BPM" && combo.SelectedIndex() == 3,
                  "Language changes must retain the selected BPM mode.");
            state.searchText_ = L"slow";
            view.RebuildVisibleSongs();
            Check(state.visibleSongIndices_ == std::vector<std::size_t>{1, 2} && combo.SelectedIndex() == 3,
                  "Search results must keep BPM ordering and stable ties.");
            view.Shutdown();
            Check(visuals.CanvasCount() == 0, "BPM sort test releases its Canvas.");
        }
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
            const auto packets = canvas->BuildDrawList();
            for (const auto *name : {"RecordPanel","SongInformation","SongBrowser"})
            {
                auto *panel = Find(canvas->Root(),name);
                if (!panel) { Check(purpose==SongSelectPurpose::Editor, "Gameplay keeps its assigned record area."); continue; }
                const auto *sprite = panel->GetComponent<SpriteVisualComponent>();
                Check(sprite && std::abs(sprite->Style().normal.alpha-.9F)<.0001F,
                      "Only the main panel background uses 90 percent opacity.");
                const auto bounds=panel->BoundsInCanvas();
                const Point interior{bounds.x+5,bounds.y+100}, edge{bounds.x+1,bounds.y+100};
                int fills=0; bool interiorBorder=false, edgeBorder=false;
                for (const auto &packet : packets)
                {
                    const auto inverse=DirectX::XMMatrixInverse(nullptr,DirectX::XMLoadFloat4x4(&packet.nodeTransform));
                    const auto point=[&](Point p) {
                        const auto local=DirectX::XMVector2TransformCoord(DirectX::XMVectorSet(p.x,p.y,0,1),inverse);
                        return Point{DirectX::XMVectorGetX(local),DirectX::XMVectorGetY(local)};
                    };
                    if (packet.type==DrawPacketType::Image && packet.bounds.Contains(point(interior)) &&
                        std::abs(packet.color.alpha-.9F)<.0001F) ++fills;
                    if (packet.type==DrawPacketType::RectangleBatch && packet.rectangles)
                        for (const auto &rectangle : *packet.rectangles)
                        {
                            interiorBorder=interiorBorder || rectangle.bounds.Contains(point(interior));
                            edgeBorder=edgeBorder || (rectangle.color.alpha==1 && rectangle.bounds.Contains(point(edge)));
                        }
                }
                Check(fills==1 && !interiorBorder && edgeBorder,
                      "A single translucent fill covers the interior; the opaque outline leaves that interior empty after resize.");
            }
            for (const auto &card : Find(canvas->Root(),"SongContent")->Children())
            {
                if (!card->Name().starts_with("Song.")) continue;
                auto *title=Find(*card,"Title"), *artist=Find(*card,"Artist");
                if (!title || !artist) throw std::runtime_error("Card metadata fields missing.");
                const float expected=card->NodeSize().width-(purpose==SongSelectPurpose::Editor?76.0F:28.0F)*2/3;
                Check(!Find(*card,"Thumbnail") && std::abs(title->Bounds().x-14.0F*2/3)<.001F &&
                          std::abs(title->NodeSize().width-std::max(expected,1.0F))<.001F &&
                          artist->Bounds().x==title->Bounds().x,
                      "Every card restores the full title/artist area without a BG thumbnail or reserved gap.");
            }
            auto *button=Find(canvas->Root(),"FilterButton");
            Check(button && button->GetComponent<SpriteVisualComponent>()->Style().normal.alpha==1 &&
                      button->GetComponent<TextVisualComponent>()->TextColor().alpha==1,
                  "Panel translucency does not fade child controls.");
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
                Check(clickable.Update(noKeys, 0) == SongSelectCommand::Launch && selection.selectedPatternIndex_ == 1,
                      "A difficulty click must select that pattern and launch, including an already selected pattern.");
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
    void MeasureText(Visual2DCanvas &canvas, std::wstring_view text, Size size)
    {
        bool found = false;
        for (const auto &packet : canvas.BuildDrawList())
            if (packet.text && packet.text->value == text)
            {
                packet.text->measuredSize = size;
                found = true;
            }
        Check(found, "The complete multiline text must reach the renderer.");
    }
    void TestScrollableText()
    {
        Visual2DCanvas canvas;
        auto &field = canvas.Root().CreateChild("Field");
        field.SetBounds({-100, -100, 100, 60});
        auto &scroll = field.AddComponent<ScrollableTextComponent>();
        scroll.SetStyle({TextFontSource::System, L"Segoe UI"}, 12, {0, 0, 0, 1});
        const std::wstring text = L"天使の夢\n한글 \U0001F3B5\nAngel Dream\nMaker";
        scroll.SetText(text);
        scroll.SetClickTarget(777);
        MeasureText(canvas, text, {400, 120});
        scroll.Update(0);
        Check(scroll.Scroll(-1) && scroll.ScrollPosition().y > 0 && scroll.ScrollPosition().x == 0,
              "Wheel scrolling moves multiline text vertically.");
        Check(scroll.Scroll(-1, true) && scroll.ScrollPosition().x > 0,
              "Long lines can scroll horizontally without truncating Unicode.");
        Check(scroll.Text() == text && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                  scroll.Text().data(), static_cast<int>(scroll.Text().size()), nullptr, 0, nullptr, nullptr) > 0,
              "Scrolling preserves every Unicode line and surrogate pair.");
        auto *thumb = Find(field, "TextScroll.VerticalThumb");
        auto *track = Find(field, "TextScroll.VerticalTrack");
        if (!thumb || !track) throw std::runtime_error("Multiline scrollbar missing.");
        Visual2DInputRouter router;
        PointerInput pointer{};
        const auto bounds = thumb->BoundsInCanvas();
        const auto trackBounds = track->BoundsInCanvas();
        pointer.available = true;
        pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonPressed = false;
        pointer.position.y = trackBounds.y;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        Check(scroll.ScrollPosition().y == scroll.ScrollRange().height && canvas.TakeActions().empty(),
              "Dragging the thumb reaches the bottom without selecting the enclosing card.");
        Click(canvas, Point{-80, -80});
        const auto actions = canvas.TakeActions();
        Check(actions.size() == 1 && actions[0].source == 777,
              "Clicking ordinary text retains the enclosing song-card action.");
        field.SetSize({1000, 1000});
        scroll.Update(0);
        Check(scroll.ScrollRange().width == 0 && scroll.ScrollRange().height == 0 &&
                  scroll.ScrollPosition().x == 0 && scroll.ScrollPosition().y == 0,
              "Growing a field removes obsolete overflow and clamps both offsets.");
        scroll.SetText(L"New");
        Check(scroll.ScrollPosition().x == 0 && scroll.ScrollPosition().y == 0,
              "Changing metadata starts at the beginning of the new text.");
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
            const auto field = [&](Visual2DNode &parent, const char *name) -> ScrollableTextComponent &
            {
                auto *node = Find(parent, name);
                if (!node) throw std::runtime_error("Metadata field missing.");
                auto *component = node->GetComponent<ScrollableTextComponent>();
                if (!component) throw std::runtime_error("Metadata scroll component missing.");
                return *component;
            };
            const std::wstring title = L"Angel Dream\n天使の夢\n천사의 꿈\nAlias four\nAlias five";
            Check(field(canvas->Root(), "SelectedSong").Text() == title &&
                      field(canvas->Root(), "SelectedArtist").Text() == L"Artist A\nArtist B\nArtist C\nArtist D\nArtist E" &&
                      field(canvas->Root(), "SelectedCreator").Text() == L"Maker A\nMaker B\nMaker C\nMaker D",
                  "All registered names, artists and makers appear in file order as separate lines.");
            auto *card = Find(canvas->Root(), "Song.0");
            auto *content = Find(canvas->Root(), "SongContent");
            if (!card || !content) throw std::runtime_error("Metadata song card missing.");
            auto *caption = Find(*card, "Title");
            if (!caption) throw std::runtime_error("Card title missing.");
            auto &cardText = field(*card, "Title");
            Check(cardText.Text() == title, "Song cards retain every title alias as well as the detail view.");
            const auto captionBounds = caption->BoundsInCanvas();
            const Point point{captionBounds.x + 5, captionBounds.y + captionBounds.height / 2};
            const float originalY = content->Bounds().y;
            InputState input;
            InputStateTestAccess::Mouse(input, point, -1);
            static_cast<void>(view.Update(input, 0));
            Check(cardText.ScrollPosition().y > 0 && content->Bounds().y == originalY,
                  "Wheeling an overflowing card caption scrolls only that caption.");
            input = {};
            InputStateTestAccess::Mouse(input, point, -1000);
            static_cast<void>(view.Update(input, 0));
            Check(cardText.ScrollPosition().y == cardText.ScrollRange().height && content->Bounds().y == originalY,
                  "A caption at its limit must not start scrolling the song list.");
            MeasureText(*canvas, title, {1500, 300});
            cardText.Update(0);
            const float previousTextY = cardText.ScrollPosition().y;
            input = {};
            InputStateTestAccess::Key(input, VK_LSHIFT, true);
            InputStateTestAccess::Mouse(input, point, -1);
            static_cast<void>(view.Update(input, 0));
            Check(cardText.ScrollPosition().x > 0 && cardText.ScrollPosition().y == previousTextY &&
                      content->Bounds().y == originalY,
                  "Shift+wheel scrolls only the long line horizontally.");
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view.Update(input, .3));
            const auto beforeModalWheel = cardText.ScrollPosition();
            input = {};
            InputStateTestAccess::Mouse(input, point, -1);
            static_cast<void>(view.Update(input, .3));
            Check(cardText.ScrollPosition().x == beforeModalWheel.x &&
                      cardText.ScrollPosition().y == beforeModalWheel.y && content->Bounds().y == originalY,
                  "Global options prevent background text and list scrolling.");
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view.Update(input, .3));
            input = {};
            static_cast<void>(view.Update(input, .3));
            input = {};
            const auto cardBounds = card->BoundsInCanvas();
            InputStateTestAccess::Mouse(input, {cardBounds.x + 1, point.y}, -1);
            static_cast<void>(view.Update(input, 0));
            Check(content->Bounds().y != originalY, "The surrounding song list still scrolls outside text fields.");
            state.SelectPattern(1);
            view.RefreshSelectionPresentation();
            Check(field(canvas->Root(), "SelectedCreator").Text() == L"Other maker",
                  "Changing the pattern refreshes its complete maker list.");
            state.catalog_.songs.clear();
            view.RebuildVisibleSongs();
            Check(field(canvas->Root(), "SelectedArtist").Text().empty() &&
                      field(canvas->Root(), "SelectedCreator").Text() == L"—",
                  "Empty selection clears previous multiline metadata.");
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
            Check(view.Update(input, 0) == SongSelectCommand::Launch && state.selectedPatternIndex_ == 2,
                  "Clicking a projected pattern launches its original catalog index.");
            Check(view.Update(input, 0) == SongSelectCommand::None, "Filtered clicks launch only once.");
            auto *button = Find(canvas->Root(), "FilterButton");
            if (!button) throw std::runtime_error("Song filter button missing.");
            Click(*canvas, *button);
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
            auto *hp = Find(canvas->Root(), "ExtraDetails1");
            if (!hp) throw std::runtime_error("HP information label missing.");
            auto *value = hp->GetComponent<TextVisualComponent>();
            if (!value) throw std::runtime_error("HP information text missing.");
            Check(value->Text() == L"HP level  17.25", "Song details show the selected pattern's exact HP level.");
            selection.SelectPattern(1);
            view.RefreshSelectionPresentation();
            Check(value->Text() == L"HP level  9.00", "Unspecified HP displays the existing 9.00 default.");
            texts.SetLanguage(Language::English);
            InputState input;
            static_cast<void>(view.Update(input, 0));
            Check(value->Text() == L"HP level  9.00", "Language switching refreshes HP without changing its value.");
            auto *panel = Find(canvas->Root(), "DifficultyInformation");
            if (!panel) throw std::runtime_error("Pattern information panel missing.");
            for (const auto width : {900U, 1280U, 1920U})
            {
                visuals.OnResize(width, 720);
                view.OnResize(width, 720);
                const auto bounds = panel->BoundsInCanvas();
                float previousBottom = bounds.y + bounds.height;
                for (std::size_t i = 0; i < 4; ++i)
                {
                    auto *row = Find(canvas->Root(), "ExtraDetails" + std::to_string(i));
                    if (!row) throw std::runtime_error("Pattern information row missing.");
                    const auto actual = row->BoundsInCanvas();
                    Check(actual.x >= bounds.x && actual.x + actual.width <= bounds.x + bounds.width + .01F &&
                              actual.y >= bounds.y && actual.y + actual.height <= previousBottom + .01F,
                          "All four information rows fit inside the responsive panel without overlap.");
                    previousBottom = actual.y;
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
        Check(detail("ExtraDetails0") == L"JudgeLevel  40.37" &&
              detail("ExtraDetails1") == L"HP level  9.00" &&
              detail("ExtraDetails2") == L"BPM range  120 – 180" &&
              detail("ExtraDetails3") == L"Length  2:05", "Selected pattern details use real data.");
        const auto *keyPanel = Find(canvas->Root(), "Options.Taiko.Keys");
        const auto *keyBackground = Find(canvas->Root(), "Options.Taiko.KeyLayout");
        if (!keyPanel || !keyBackground)
            throw std::runtime_error("Taiko settings must use the Gameplay input panel image.");
        constexpr std::array<Rect, 4> gameplayKeys{{
            {16, 68, 39, 70}, {68, 92, 39, 70}, {120, 92, 39, 70}, {172, 68, 39, 70}}};
        const auto panelBounds = keyBackground->Bounds();
        std::size_t keyIndex{};
        for (const auto &child : keyPanel->Children())
        {
            if (child->Name() != "Options.Taiko.KeyImage") continue;
            Check(keyIndex < gameplayKeys.size(), "Unexpected extra Gameplay key image.");
            const auto actual = child->Bounds();
            const auto expected = gameplayKeys[keyIndex++];
            const auto scale = panelBounds.width / 228.0F;
            Check(std::abs(actual.x - (panelBounds.x + expected.x * scale)) < .01F &&
                  std::abs(actual.y - (panelBounds.y + (228 - expected.y - expected.height) * scale)) < .01F &&
                  std::abs(actual.width - expected.width * scale) < .01F &&
                  std::abs(actual.height - expected.height * scale) < .01F,
                  "Taiko settings key images must match the Gameplay panel proportions and positions.");
        }
        Check(keyIndex == gameplayKeys.size(), "Taiko settings must display all four Gameplay key images.");
        auto *image = Find(canvas->Root(), "PreviewImage");
        auto *empty = Find(canvas->Root(), "PreviewEmpty");
        auto *title = Find(canvas->Root(), "PreviewTitle");
        auto *preview = Find(canvas->Root(), "BackgroundPreview");
        if (!image || !empty || !title || !preview) throw std::runtime_error("Background nodes missing.");
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
        Check(renderer.loaded.size() == 1 && renderer.loaded.back() == options.SongsRoot() / L"BG" / L"preview-wide.jpg",
              "Artwork must load from the selected Songs/BG.");
        Check(pageBackground->IsVisible() && pageSprite->Style().normalImage.value == sprite->Style().normalImage.value,
              "The selected pattern image is shared by the full-screen background and existing preview.");
        select(1);
        tick();
        Check(renderer.loaded.size() == 2 && renderer.loaded.back() == options.SongsRoot() / L"BG" / L"preview-portrait.jpg",
              "Changing a difficulty must load the selected pattern from Songs/BG.");
        Check(image->IsVisible() && !empty->IsVisible() && !title->IsVisible() && sprite->Style().normalImage.value,
              "Ready artwork must replace the placeholder.");
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
            Check(image->NodeSize().width == preview->NodeSize().width &&
                      image->NodeSize().height == preview->NodeSize().height,
                  "Artwork must follow the existing responsive preview bounds.");
            const auto bg = pageBackground->BoundsInCanvas();
            const auto size = canvas->LogicalSize();
            Check(bg.x <= -size.width / 2 && bg.x + bg.width >= size.width / 2 &&
                      bg.y <= -size.height / 2 && bg.y + bg.height >= size.height / 2 &&
                      std::abs(bg.width / bg.height - 1600.0F / 900) < .001F,
                  "Selected artwork covers the whole responsive screen with its source aspect ratio.");
            Check(std::abs(bg.x + bg.width / 2 - 35) < .01F && std::abs(bg.y + bg.height / 2 + 15) < .01F,
                  "Changing authored offsets for the same BG updates its framing without another image load.");
        }
        select(2);
        tick();
        Check(!image->IsVisible() && empty->IsVisible() && !sprite->Style().normalImage.value,
              "An unassigned background must clear the previous image.");
        Check(!pageBackground->IsVisible() && !pageSprite->Style().normalImage,
              "An unassigned BG restores the skin background instead of leaving the previous chart image.");
        for (const auto index : {3U, 4U})
        {
            select(index);
            tick();
            Check(!image->IsVisible() && empty->IsVisible(), "Decode/upload errors must preserve NO IMAGE.");
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
        Check(!image->IsVisible() && empty->IsVisible(), "Empty search results must clear the selected artwork.");
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
        Click(*canvas,*next);static_cast<void>(view.Update(input,0));wait(2);
        Click(*canvas,*Find(canvas->Root(),"RecordPrevious"));static_cast<void>(view.Update(input,0));wait(6);
        const auto viewport=Find(canvas->Root(),"SongViewport")->BoundsInCanvas();
        InputStateTestAccess::Mouse(input,{viewport.x+viewport.width/2,viewport.y+viewport.height/2},-3);
        static_cast<void>(view.Update(input,0));input={};
        const auto contentY=Find(canvas->Root(),"SongContent")->BoundsInCanvas().y;
        const auto selectedSong=selection.focusedSongPosition_, selectedPattern=selection.selectedPatternIndex_;
        const auto thread=GetCurrentThreadId();std::atomic_bool handled{};
        std::jthread queryInput([&](std::stop_token stop){
            while(!stop.stop_requested()&&!handled)
            {
                EnumThreadWindows(thread,[](HWND window,LPARAM p)->BOOL{
                    wchar_t name[32]{};GetClassNameW(window,name,32);
                    if(std::wstring_view(name)!=L"#32770"||!GetDlgItem(window,100))return TRUE;
                    ShowWindow(window,SW_HIDE);SetDlgItemTextW(window,100,L"JudgeLevel >= 60 AND Random = true");
                    PostMessageW(window,WM_COMMAND,IDOK,0);*reinterpret_cast<std::atomic_bool *>(p)=true;return FALSE;
                },reinterpret_cast<LPARAM>(&handled));
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        });
        auto *recordPanel = Find(canvas->Root(),"RecordPanel");
        if (!recordPanel) throw std::runtime_error("Record panel missing.");
        const auto recordBounds = recordPanel->BoundsInCanvas();
        Click(*canvas,*Find(canvas->Root(),"RecordQuery"));static_cast<void>(view.Update(input,0));
        auto *filter = Find(canvas->Root(),"RecordFilter.Overlay");
        if (!filter) throw std::runtime_error("Record filter overlay missing.");
        Check(filter && filter->IsVisible() && rows()==6,"Record filtering opens separately and leaves records in their assigned area.");
        Click(*canvas,*Find(canvas->Root(),"RecordFilter.Condition"));static_cast<void>(view.Update(input,0));queryInput.request_stop();queryInput.join();
        Check(handled && filter->IsVisible() && rows()==6,"Direct history conditions remain a draft until Apply.");
        static_cast<void>(view.Update(input,0)); // Release the nested dialog's keyboard gate.
        Click(*canvas,*Find(canvas->Root(),"RecordFilter.Apply"));static_cast<void>(view.Update(input,0));
        Check(!filter->IsVisible(),"Apply closes only the record filter.");wait(4);
        Check(recordPanel->BoundsInCanvas().x==recordBounds.x && recordPanel->BoundsInCanvas().width==recordBounds.width,
              "Record results reuse the existing panel without relocating it.");
        Check(Find(canvas->Root(),"SongContent")->BoundsInCanvas().y==contentY && selection.searchText_==L"Record" &&
                  selection.focusedSongPosition_==selectedSong && selection.selectedPatternIndex_==selectedPattern,
              "Applying a record filter preserves song list scroll, search and the selected chart.");
        for(const auto &row:Find(canvas->Root(),"RecordRows")->Children())
            Check(row->GetComponent<TextVisualComponent>()->Text().find(L"JL 70.00")!=std::wstring::npos,"Only matching effective JudgeLevel records appear.");
        selection.selectedPatternIndex_=1;view.RefreshSelectionPresentation();static_cast<void>(view.Update(input,0));wait(0);
        view.Shutdown();
    }
    void TestOptionsToggle(MusicSelectView &view, Visual2DCanvas &canvas)
    {
        auto *button = Find(canvas.Root(), "OptionSelect");
        auto *panel = Find(canvas.Root(), "Options.Panel");
        if (!button || !panel)
            throw std::runtime_error("Options toggle controls missing.");
        const auto *mode = Find(canvas.Root(), "GameModeSelect");
        Check(mode && mode->BoundsInCanvas().x + mode->BoundsInCanvas().width <= button->BoundsInCanvas().x, "Mode and play option controls must have separate footer bounds.");
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
            if (moved.size() != 2 || moved[0].text != source || moved[1].text != source || moved[0].textOffset.x >= 0 ||
                moved[1].textOffset.x <= 0 || source->value != unicode)
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
        TestScrollableText();
        TestMetadataLines();
        TestBaseBpmSortView();
        TestFilterProjection();
        TestFavoriteFolders();
        TestHpInformation();
        TestRecordHistoryView();
        TestBackgroundPreview(SongSelectPurpose::Gameplay);
        TestBackgroundPreview(SongSelectPurpose::Editor);
        TestSongPanelTransparency(SongSelectPurpose::Gameplay);
        TestSongPanelTransparency(SongSelectPurpose::Editor);
        SongCatalogLoader loader;
        loader.Refresh(UserSongs());
        const auto first = AwaitCatalog(loader);
        Check(!first.songs.empty(), "Worker must publish the real catalog.");
        const auto playable = std::ranges::find_if(first.songs, [](const auto &song) { return !song.patterns.empty(); });
        Check(playable != first.songs.end(), "The worker regression requires a real playable chart.");
        const auto &pattern = playable->patterns.front();
        Check(pattern.pattern.notes.empty() && pattern.noteCount.has_value(),
              "Runtime catalog loading must publish compact metadata with the original note count.");
        PlaySessionLoader sessions;
        sessions.Start({pattern.patternPath, pattern.effectPath, playable->audioPath, pattern.pattern.mode});
        Check(sessions.Busy(), "A session worker must retain its job until publication.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        std::optional<ModeLoadResult> loaded;
        while (!(loaded = sessions.Poll()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        Check(loaded && loaded->Succeeded() && !sessions.Busy() && !loaded->session->Gear().Lanes().empty(),
              "The worker must create and transfer a real playable session without loading on Poll.");
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
