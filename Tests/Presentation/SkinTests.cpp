#include "App/AssetPaths.h"
#include "App/SkinManifest.h"
#include "GameScene/YOUMULogoScene/Submodules/LogoView.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "Presentation/SkinVisualComponent.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    namespace fs = std::filesystem;

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const fs::path &path, std::string_view bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(out.good(), "Could not write an isolated skin fixture.");
    }
    void Copy(const fs::path &source, const fs::path &target)
    {
        fs::create_directories(target.parent_path());
        fs::copy_file(source, target, fs::copy_options::overwrite_existing);
    }
    void ManifestChecks(const fs::path &root, const fs::path &defaults)
    {
        const auto skin = root / "manifest";
        Write(skin / L"그림 폴더/아이콘 #1.png", "isolated fixture");
        Write(skin / "skin.ini", "\xEF\xBB\xBF[Folders]\r\nLogo=그림 폴더\r\n[Logo]\r\n"
                                 "Background=\"아이콘 #1.png\"\r\nSelectionCursor=missing.png\r\n"
                                 "Menu.Button=../escape.png\r\n[MusicSelect]\r\nBackground=\r\n");
        const auto images = LoadSkinImages(skin, defaults);
        Check(images.paths.at("Logo.Background") == skin / L"그림 폴더/아이콘 #1.png",
              "Both directory and filename must be relative, with UTF-8/BOM/CRLF support.");
        Check(images.paths.at("Logo.SelectionCursor") == defaults / "Logo/Menu/SelectionCursor.png",
              "A missing custom filename must fall back by component, not by custom filename.");
        Check(images.paths.at("Logo.Menu.Button") == defaults / "Logo/Menu/Button.png",
              "Parent traversal must fall back without affecting other components.");
        Check(images.paths.at("InGame.note") == defaults / "InGame/note.png" &&
                  images.paths.at("MusicSelect.Background").empty() && images.diagnostics.size() == 2,
              "Unspecified scenes and procedural defaults must remain intact.");
        Write(skin / "skin.ini", "[Folders]\nLogo=C:\\absolute\n[Logo]\nBackground=/rooted.png\n"
                                 "SelectionCursor=../escape.png\nMenu.Button=\\\\server\\share.png\n"
                                 "[Broken\nBackground=wrong.png\n[Logo]\nBackground=\"\n");
        const auto invalid = LoadSkinImages(skin, defaults);
        Check(invalid.paths.at("Logo.Background") == defaults / "Logo/Background.png" && invalid.diagnostics.size() >= 6,
              "Invalid paths and sections must not escape into scene loading.");
        Write(skin / "skin.ini", "[Logo]\nBackground=missing.png\nBackground=other.png\n");
        Check(LoadSkinImages(skin, defaults).diagnostics.size() == 2,
              "Duplicate keys and missing files must be diagnosed.");
        Write(skin / "skin.ini", "[Logo]\nBackground=\xFF\n");
        Check(LoadSkinImages(skin, defaults).paths.at("Logo.Background") == defaults / "Logo/Background.png",
              "Invalid UTF-8 must leave the embedded defaults usable.");
        Write(root / "partial/InGame/note.png", "isolated fixture");
        Check(LoadSkinImages(root / "partial", defaults).paths.at("InGame.note") ==
                  root / "partial/InGame/note.png",
              "Partial skins may inherit the default manifest mappings.");
    }
    class Renderer final : public Visual2DRenderSystem
    {
      public:
        ImageHandle LoadImageFile(const fs::path &path) override
        {
            loaded.push_back(path);
            return {loaded.size()};
        }
        Size GetImageSize(ImageHandle) const noexcept override
        {
            return {96, 96};
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
        std::vector<fs::path> loaded;
    };
    Visual2DNode &Find(Visual2DNode &root, std::string_view name)
    {
        const auto search = [&](auto &&self, Visual2DNode &node) -> Visual2DNode *
        {
            if (node.Name() == name)
                return &node;
            for (const auto &child : node.Children())
                if (auto *found = self(self, *child))
                    return found;
            return nullptr;
        };
        auto *node = search(search, root);
        if (!node)
            throw std::runtime_error("Skin view node is missing: " + std::string(name));
        return *node;
    }
    ImageHandle Image(Visual2DNode &node)
    {
        return node.GetComponent<SpriteVisualComponent>()->Style().normalImage;
    }
    void ViewChecks(const fs::path &repo, const fs::path &skin, const fs::path &defaults)
    {
        Copy(defaults / "Result/Background/Sky.png", skin / "Title/sky.png");
        Copy(defaults / "InGame/Progress/Track.png", skin / "Title/button.png");
        Copy(defaults / "InGame/Progress/Fill.png", skin / "Title/hover.png");
        Copy(defaults / "Result/Background/Sky.png", skin / "Select/background.png");
        Copy(defaults / "InGame/Progress/Track.png", skin / "Select/panel.png");
        Copy(defaults / "InGame/note.png", skin / "Play/custom.png");
        Write(skin / "skin.ini", "[Folders]\nLogo=Title\nMusicSelect=Select\nInGame=Play\n"
                                 "[Logo]\nBackground=sky.png\nMenu.Button=button.png\n"
                                 "Menu.Button.Hovered=hover.png\n[MusicSelect]\nBackground=background.png\n"
                                 "Panels.Sheet=panel.png\nBack=panel.png\nSongCardFocused=panel.png\n"
                                 "DifficultySelected=panel.png\n[InGame]\nnote=custom.png\n");
        auto &selection = SkinSetSelection::Instance();
        selection.Initialize(L"Test Skin");
        Check(selection.CurrentName() == L"Test Skin", "The isolated custom skin must be selected.");
        Check(InGame("note") == skin / "Play/custom.png",
              "Gameplay and editor must use the manifest's shared image mapping.");
        Renderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        OptionSettings options(skin / "unused.ini");
        const EngineServices services{meshes, textRendering, renderer, audio, 1280, 720};
        LogoView logo(visuals, texts, options);
        logo.Initialize(services);
        logo.BeginScene();
        auto &logoRoot = visuals.FindCanvas(1)->Root();
        // Game Start is the chosen item and uses the selected bar; Editor shows the plain bar.
        auto &start = Find(logoRoot, "YOUMU.Editor");
        const auto &style = start.GetComponent<SpriteVisualComponent>()->Style();
        const auto customButton = style.normalImage;
        Check(style.normalImage && style.hoveredImage && style.normalImage.value != style.hoveredImage.value &&
                  style.pressedImage && style.pressedImage.value != style.hoveredImage.value,
              "Buttons must use authored states; a state the skin omits falls back to the built-in part.");
        Check(Find(logoRoot, "YOUMU.LogoBackground").IsVisible() && Image(Find(logoRoot, "YOUMU.LogoBackground")),
              "The title shows the skin's full-screen sky.");
        const auto &backgroundStyle = Find(logoRoot, "YOUMU.LogoBackground")
                                          .GetComponent<SpriteVisualComponent>()->Style();
        Check(backgroundStyle.normal.red == 1 && backgroundStyle.normal.green == 1 && backgroundStyle.normal.blue == 1,
              "A custom logo background must retain its authored RGB.");
        SongSelectionState songs;
        songs.catalog_ = SongCatalog{}.Load(repo / "Client/Assets/Songs");
        Check(!songs.catalog_.songs.empty(), "The skin integration test needs the real bundled song catalog.");
        MusicSelectView music(visuals, songs, SongSelectPurpose::Gameplay, texts, options);
        music.Initialize(services);
        music.SetVisible(true);
        auto &musicRoot = visuals.FindCanvas(2)->Root();
        auto &sheet = Find(musicRoot, "Sheet");
        auto &back = Find(musicRoot, "Back");
        const auto customSheet = Image(sheet);
        Check(customSheet && Image(back).value == customSheet.value &&
                  Image(Find(musicRoot, "SheetExtension")).value == customSheet.value,
              "A selected skin replaces the sheet, its wide-screen extension and button images.");
        Check(back.GetComponent<SpriteVisualComponent>()->Style().normal.alpha==1,
              "Skinned controls retain their authored opacity.");
        Check(sheet.GetComponent<SpriteVisualComponent>()->Style().normal.alpha==1,
              "A part image is drawn at its own opacity; the translucent fallback colour does not thin it again.");
        auto &card = Find(musicRoot, "Song.0");
        Check(Image(card).value == customSheet.value, "Virtualized song cards must accept skin images.");
        const auto loads = renderer.loaded.size();
        for (int i = 0; i < 100; ++i)
            visuals.Update(.016);
        Check(renderer.loaded.size() == loads, "Steady frames must reuse registered images.");
        // Editing a manifest does not perform I/O during a frame. Re-selecting
        // another skin and returning explicitly invalidates the snapshot.
        Write(skin / "skin.ini", "[Folders]\nInGame=Play\n[InGame]\nnote=missing.png\n");
        Check(selection.Image("InGame", "note") == skin / "Play/custom.png", "The manifest must be cached.");
        std::string error;
        Check(selection.Select(L"Default Skin", error), error.c_str());
        visuals.Update(0);
        static_cast<void>(logo.Update(InputState{}, 0));
        static_cast<void>(logo.Update(InputState{}, 0));
        Check(static_cast<bool>(Image(Find(Find(logoRoot, "Options.Rail.General"), "Icon"))),
              "Options rail icons keep their skin image while their colour follows hover and the lit section.");
        Check(Image(start) && Image(start).value != customButton.value && Image(sheet) &&
                  Image(sheet).value != customSheet.value &&
                  Image(sheet).value == visuals.RegisterImage(selection.Image("MusicSelect", "Panels.Sheet")).value &&
                  Image(back) && Image(back).value != customSheet.value && Image(Find(logoRoot, "YOUMU.LogoBackground")),
              "Returning to the default skin must restore the built-in title and song-select parts.");
        Check(selection.Select(L"Test Skin", error), error.c_str());
        Check(selection.Image("InGame", "note") == defaults / "InGame/note.png",
              "Re-selecting must reload edited configuration and use per-element fallback.");
        music.Shutdown();
        logo.Shutdown();
        Check(visuals.CanvasCount() == 0, "View shutdown must destroy owned skin bindings.");
    }
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Check(argc == 2, "Expected repository root.");
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        const auto root = UserAssetRoot();
        Check(root == AssetExecutableDirectory() / "Asset",
              "Skin fixtures must run in an isolated deployment directory.");
        const auto defaults = BuiltInAssetRoot() / "assets/skins/Default Skin";
        ManifestChecks(root / "fixtures", defaults);
        ViewChecks(fs::path(argv[1]), root / "Skins/Test Skin", defaults);
        std::cout
            << "Skin manifest paths, fallback, cache, logo, button states and song-selection integration passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
