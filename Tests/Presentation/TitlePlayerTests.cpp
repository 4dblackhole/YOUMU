// Title music player controls through the production LogoView: keys, buttons, seek bar
// click and drag, the empty state, and Space no longer running the menu.
#include <Windows.h>
#include "App/AssetPaths.h"
#include "GameScene/YOUMULogoScene/Submodules/LogoView.h"
#include "YOUMUAssets.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <utility>
#include <stdexcept>

namespace PhantomCore
{
    struct InputStateTestAccess
    {
        // A frame forgets whether the mouse is inside the window; the window reports it again each frame.
        static void Begin(InputState &input)
        {
            input.BeginUpdate();
            if (const auto &last = LastPointer())
                input.SetMousePosition(last->first, last->second, true);
        }
        static std::optional<std::pair<std::int32_t, std::int32_t>> &LastPointer()
        {
            static std::optional<std::pair<std::int32_t, std::int32_t>> position;
            return position;
        }
        static void Key(InputState &input, const std::uint16_t key, const bool down) { input.SetKey(key, down, 1); }
        // Window pixels with a top-left origin; a 1280x720 window maps them one to one onto the title board.
        static void Pointer(InputState &input, const float x, const float y)
        {
            LastPointer() = std::pair{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
            input.SetMousePosition(LastPointer()->first, LastPointer()->second, true);
        }
        // Pointer events arrive in QPC order, so every button change gets a later timestamp.
        static void Button(InputState &input, const bool down) { input.SetMouseButton(MouseButton::Left, down, ++Clock()); }
        static std::int64_t &Clock()
        {
            static std::int64_t ticks = 100;
            return ticks;
        }
    };
} // namespace PhantomCore

using namespace PhantomCore;

namespace
{
    namespace fs = std::filesystem;
    using Access = InputStateTestAccess;

    void Check(const bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    class Renderer final : public Visual2DRenderSystem
    {
      public:
        ImageHandle LoadImageFile(const fs::path &) override { return {++loaded}; }
        // A released image reports no size, as the D3D12 renderer's does.
        Size GetImageSize(ImageHandle image) const noexcept override
        {
            return released.contains(image.value) ? Size{} : Size{320, 180};
        }
        void ReleaseImage(ImageHandle image) noexcept override { released.insert(image.value); }
        std::set<std::uint64_t> released;
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point, std::uint32_t) override {}
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const Float4x4 &, Size, const Float4x4 &) override {}
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override { return {}; }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &, const RenderContext &) override {}
        std::uint64_t loaded{};
    };

    Visual2DNode &Find(Visual2DNode &root, const std::string_view name)
    {
        const auto search = [&](auto &&self, Visual2DNode &node) -> Visual2DNode * {
            if (node.Name() == name)
                return &node;
            for (const auto &child : node.Children())
                if (auto *found = self(self, *child))
                    return found;
            return nullptr;
        };
        auto *found = search(search, root);
        Check(found != nullptr, "A title player node is missing.");
        return *found;
    }

    std::wstring Text(Visual2DNode &root, const std::string_view name)
    {
        return std::wstring(Find(root, name).GetComponent<TextVisualComponent>()->Text());
    }

    class TitleHarness
    {
      public:
        TitleHarness()
            : options_(fs::temp_directory_path() / L"youmu-title-player-tests" / L"unused.ini"),
              services_{meshes_, textRendering_, renderer_, audio_, 1280, 720}, view_(visuals_, texts_, options_)
        {
            visuals_.Initialize(renderer_, {1280, 720});
            view_.Initialize(services_);
            view_.BeginScene();
        }
        ~TitleHarness() { view_.Shutdown(); }
        TitleHarness(const TitleHarness &) = delete;
        TitleHarness &operator=(const TitleHarness &) = delete;

        template <typename Setup> LogoCommand Frame(const TitlePlayerDisplay &display, Setup setup)
        {
            Access::Begin(input_);
            setup(input_);
            view_.SetPlayer(display);
            return view_.Update(input_, 1.0 / 60.0);
        }
        LogoCommand Key(const TitlePlayerDisplay &display, const std::uint16_t key)
        {
            const auto command = Frame(display, [key](InputState &input) { Access::Key(input, key, true); });
            static_cast<void>(Frame(display, [key](InputState &input) { Access::Key(input, key, false); }));
            return command;
        }
        // Hover, press in the next frame and release in the one after, as a real click arrives.
        LogoCommand Click(const TitlePlayerDisplay &display, const float x, const float y)
        {
            static_cast<void>(Frame(display, [x, y](InputState &input) { Access::Pointer(input, x, y); }));
            const auto pressed = Frame(display, [](InputState &input) { Access::Button(input, true); });
            const auto released = Frame(display, [](InputState &input) { Access::Button(input, false); });
            Check(!pressed.player || !released.player, "One click reports one player request.");
            return pressed.player ? pressed : released;
        }
        Visual2DNode &Root() { return visuals_.FindCanvas(1)->Root(); }
        ScreenVisual2DManager &Visuals() { return visuals_; }
        [[nodiscard]] std::uint64_t Loaded() const { return renderer_.loaded; }
        [[nodiscard]] bool Released(std::uint64_t image) const { return renderer_.released.contains(image); }

      private:
        Renderer renderer_;
        ScreenVisual2DManager visuals_;
        MeshRenderSystem meshes_;
        TextRenderSystem textRendering_;
        AudioSystem audio_;
        TextCatalog texts_;
        OptionSettings options_;
        EngineServices services_;
        LogoView view_;
        InputState input_;
    };

    // Board coordinates of the UI-02 title player at 1280x720 (top-left origin).
    constexpr float PlayX = 542.0F, StopX = 606.0F, NextX = 658.0F, ButtonY = 250.0F;
    constexpr float SeekLeft = 558.0F, SeekWidth = 352.0F, SeekY = 208.0F;

    bool Is(const LogoCommand &command, const TitlePlayerAction action)
    {
        return command.player.has_value() && command.player->action == action;
    }

    void EmptyLibraryDisablesThePlayer()
    {
        TitleHarness title;
        const TitlePlayerDisplay empty;
        for (const auto key : {static_cast<std::uint16_t>(VK_SPACE), static_cast<std::uint16_t>(VK_LEFT),
                               static_cast<std::uint16_t>(VK_RIGHT)})
        {
            const auto command = title.Key(empty, key);
            Check(!command.player && !command.menu, "Without a song the player keys do nothing, Space included.");
        }
        Check(!title.Click(empty, PlayX, ButtonY).player && !title.Click(empty, NextX, ButtonY).player &&
                  !title.Click(empty, SeekLeft + SeekWidth * 0.5F, SeekY).player,
              "Without a song the buttons and the seek bar are off.");
        auto &root = title.Root();
        Check(!Find(root, "YOUMU.PlayerPlay").IsEnabled() && !Find(root, "YOUMU.PlayerSeek").IsEnabled(),
              "The empty player shows its controls disabled.");
        Check(Text(root, "YOUMU.PlayerTitle").empty() && Text(root, "YOUMU.PlayerArtist").empty() &&
              Text(root, "YOUMU.PlayerElapsed").empty(),
              "The empty player leaves every caption blank instead of borrowing another song.");
        Check(Text(root, "YOUMU.PlayerLength").empty(),
              "The empty player leaves every caption blank instead of borrowing another song.");
        Check(Find(root, "YOUMU.PlayerArtworkEmpty").IsVisible() && !Find(root, "YOUMU.PlayerArtwork").IsVisible(),
              "The empty player shows the empty art box.");
        const auto enter = title.Key(empty, VK_RETURN);
        Check(enter.menu == std::size_t{0} && !enter.player, "Enter still runs the selected menu item.");
    }

    void SongPlayerKeysAndButtons()
    {
        TitleHarness title;
        TitlePlayerDisplay song;
        song.hasSong = true;
        song.title = L"Angel Dream";
        song.artist = L"Takahashi";
        song.artwork = L"chart-background.png";
        song.playing = true;
        song.positionMicroseconds = 37'000'000;
        song.durationMicroseconds = 104'000'000;

        static_cast<void>(title.Frame(song, [](InputState &) {}));
        auto &root = title.Root();
        Check(Text(root, "YOUMU.PlayerTitle") == L"Angel Dream" && Text(root, "YOUMU.PlayerArtist") == L"Takahashi",
              "The player shows the song's title and artist.");
        Check(Text(root, "YOUMU.PlayerElapsed") == L"0:37" && Text(root, "YOUMU.PlayerLength") == L"1:44",
              "The player shows elapsed time and length.");
        Check(Find(root, "YOUMU.PlayerArtwork").IsVisible(), "The chart background becomes the album art.");
        const auto &art = *Find(root, "YOUMU.PlayerArtwork").GetComponent<SpriteVisualComponent>();
        Check(std::abs(art.UvScale().x - 0.5625F) < 0.001F && art.UvScale().y == 1.0F,
              "A wide background is cropped to its centre square.");
        {
            // Song select releases backgrounds it no longer keeps: the same song's art registers again.
            const auto before = title.Loaded();
            title.Visuals().ReleaseImage(art.Style().normalImage);
            static_cast<void>(title.Frame(song, [](InputState &) {}));
            Check(title.Loaded() == before + 1 && art.Style().normalImage.value == before + 1 &&
                      Find(root, "YOUMU.PlayerArtwork").IsVisible(),
                  "Released album art is registered again for the same song.");
            // Another song: the player keeps one album art, so the previous one is released.
            const auto previous = art.Style().normalImage.value;
            auto next = song;
            next.artwork = L"next-background.png";
            static_cast<void>(title.Frame(next, [](InputState &) {}));
            Check(title.Released(previous) && art.Style().normalImage.value == title.Loaded(),
                  "Changing songs releases the previous album art.");
        }

        const auto space = title.Key(song, VK_SPACE);
        Check(Is(space, TitlePlayerAction::TogglePlay) && !space.menu, "Space plays or pauses and no longer runs the menu.");
        Check(Is(title.Key(song, VK_LEFT), TitlePlayerAction::Previous), "Left goes to the previous song.");
        Check(Is(title.Key(song, VK_RIGHT), TitlePlayerAction::Next), "Right goes to the next song.");
        const auto enter = title.Key(song, VK_RETURN);
        Check(enter.menu == std::size_t{0} && !enter.player, "Enter runs the menu while a song plays.");

        const auto exit = title.Click(song, 640.0F, 562.0F);
        Check(exit.menu == std::size_t{3} && !exit.player, "Menu items still respond to clicks below the player.");
        Check(Is(title.Click(song, PlayX, ButtonY), TitlePlayerAction::TogglePlay), "The play button plays or pauses.");
        Check(Is(title.Click(song, StopX, ButtonY), TitlePlayerAction::Stop), "The stop button stops.");
        Check(Is(title.Click(song, NextX, ButtonY), TitlePlayerAction::Next), "The next button goes to the next song.");

        // Drag: the bar follows the pointer while held and seeks only on release.
        const auto pressed = title.Frame(song, [](InputState &input) {
            Access::Pointer(input, SeekLeft + SeekWidth * 0.25F, SeekY);
            Access::Button(input, true);
        });
        Check(!pressed.player && Text(root, "YOUMU.PlayerElapsed") == L"0:26",
              "Pressing the seek bar previews the position without seeking.");
        const auto dragged = title.Frame(song, [](InputState &input) { Access::Pointer(input, SeekLeft + SeekWidth * 0.5F, SeekY + 30.0F); });
        Check(!dragged.player && Text(root, "YOUMU.PlayerElapsed") == L"0:52",
              "Dragging keeps following the pointer, even off the bar.");
        const auto released = title.Frame(song, [](InputState &input) { Access::Button(input, false); });
        Check(Is(released, TitlePlayerAction::Seek) && std::abs(released.player->seekRatio - 0.5F) < 0.01F,
              "Releasing seeks to where the drag ended.");
        static_cast<void>(title.Frame(song, [](InputState &) {}));
        Check(Text(root, "YOUMU.PlayerElapsed") == L"0:37", "After the drag the bar shows the playing position again.");

        const auto clicked = title.Click(song, SeekLeft + SeekWidth * 0.75F, SeekY);
        Check(Is(clicked, TitlePlayerAction::Seek) && std::abs(clicked.player->seekRatio - 0.75F) < 0.01F,
              "Clicking the seek bar seeks there.");
        Check(!title.Click(song, SeekLeft - 20.0F, SeekY).player, "A click beside the bar does not seek.");

        auto unknownLength = song;
        unknownLength.durationMicroseconds.reset();
        Check(!title.Click(unknownLength, SeekLeft + SeekWidth * 0.5F, SeekY).player,
              "The bar cannot seek before the song's length is known.");

        // The options panel owns the keyboard while it is open.
        static_cast<void>(title.Frame(song, [](InputState &input) {
            Access::Key(input, VK_LCONTROL, true);
            Access::Key(input, 'O', true);
        }));
        static_cast<void>(title.Frame(song, [](InputState &input) {
            Access::Key(input, VK_LCONTROL, false);
            Access::Key(input, 'O', false);
        }));
        Check(!title.Key(song, VK_SPACE).player, "Player keys are ignored while the options panel is open.");
    }
} // namespace

int wmain()
{
    try
    {
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        SkinSetSelection::Instance().Initialize(L"Default Skin");
        EmptyLibraryDisablesThePlayer();
        SongPlayerKeysAndButtons();
        std::cout << "Title player keys, buttons, seek drag, empty state and Space/Enter split passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
