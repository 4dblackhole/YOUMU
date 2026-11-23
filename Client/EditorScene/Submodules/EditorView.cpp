#include "EditorView.h"
#include "App/AssetPaths.h"
#include "Catalog/SongLayout.h"
#include "Presentation/BackgroundLayout.h"
#include "Presentation/UiFonts.h"
#include "Tabs/EditorAudioTab.h"
#include "Tabs/EditorComposeTab.h"
#include "Tabs/EditorMetadataTab.h"
#include <algorithm>
#include <format>
#include <cwctype>
#include <sstream>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

EditorView::EditorView(ScreenVisual2DManager &manager, EditorWorkspace &state, TextCatalog &texts)
    : visuals(manager), state_(state), texts_(texts)
{
    tabs_[static_cast<std::size_t>(EditorTab::Compose)] = std::make_unique<EditorComposeTab>(*this, state, texts);
    tabs_[static_cast<std::size_t>(EditorTab::Audio)] = std::make_unique<EditorAudioTab>(*this, state, texts, manager);
    tabs_[static_cast<std::size_t>(EditorTab::Metadata)] = std::make_unique<EditorMetadataTab>(*this, state, texts);
}

EditorView::~EditorView() = default;

IEditorTab &EditorView::CurrentTab() const noexcept
{
    return *tabs_[static_cast<std::size_t>(state_.Tab())];
}

void EditorView::Initialize(const EngineServices &services)
{
    width = services.windowWidth;
    height = services.windowHeight;
    canvas = visuals.CreateOwnedCanvas({{layout::BaseWidth, layout::Height}, CanvasScaleMode::FixedHeight});
    static_cast<void>(canvas.SetVisible(true));
    node = &canvas.Get()->CreateNode(Anchor::Center, "Editor");
    // The editor draws on a surface above the sliders, whose own track and thumb it replaces;
    // the sliders still take the pointer through it. Spectrum tiles sit above the surface.
    auto &surface = node->CreateChild("Editor.Surface");
    surface.SetZIndex(1);
    visual = &surface.AddComponent<EditorVisual>();
    VisualStyle sliderStyle{};
    sliderStyle.normal.alpha = 0;
    sliderStyle.hovered.alpha = 0;
    sliderStyle.pressed.alpha = 0;
    sliderStyle.disabled.alpha = 0;
    // The sliders own dragging; the editor draws their tracks itself.
    divisionSliderNode_ = &CreateSlider(*node, {}, (state_.Score().division - 1) / 15.0F, "Beat division");
    divisionSliderNode_->GetComponent<SpriteVisualComponent>()->SetStyle(sliderStyle);
    timelineSliderNode_ = &CreateSlider(*node, {}, 0, "Editor timeline");
    timelineSliderNode_->GetComponent<SpriteVisualComponent>()->SetStyle(sliderStyle);
    for (auto &tab : tabs_)
        tab->Initialize(*node);
    menuBar_.Initialize(*node, texts_);
    alert_.Initialize(*canvas.Get(), &visuals);
    fields_.Initialize(services.windowControl);
    ReloadSize();
    Build();
}

void EditorView::Shutdown() noexcept
{
    controls.clear();
    if (canvas.Get())
    {
        sliderInput_.Reset(*canvas.Get());
        alertInput_.Reset(*canvas.Get());
    }
    alert_.Shutdown();
    menuBar_.Shutdown();
    fields_.Shutdown();
    fieldRects_.clear();
    for (auto &tab : tabs_)
        tab->Shutdown();
    canvas.Reset();
    node = nullptr;
    divisionSliderNode_ = nullptr;
    timelineSliderNode_ = nullptr;
    divisionRect_.reset();
    visual = nullptr;
}

Rect EditorView::ToNode(Rect r) const noexcept
{
    return {r.x - frame_.width * .5F, layout::Height * .5F - r.y - r.height, r.width, r.height};
}

void EditorView::PlaceNode(Visual2DNode &target, Rect r) const
{
    target.SetBounds(ToNode(r));
}

float EditorView::ContentRight() const noexcept
{
    return layout::ContentRight(frame_.width, CurrentTab().HasInspector());
}

Rect EditorView::PanelBody() const noexcept
{
    const float left = frame_.width - layout::InspectorWidth;
    return {left + 20, layout::TrackTop + 48 + 12, layout::InspectorWidth - 40,
            layout::FooterTop - layout::TrackTop - 48 - 12 - 16};
}

std::optional<Point> EditorView::DesignPoint(const InputState &input) const
{
    const auto mapped =
        MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                         {static_cast<float>(width), static_cast<float>(height)}, *canvas.Get());
    if (!mapped)
        return std::nullopt;
    return Point{mapped->x / frame_.scale + frame_.width * .5F, layout::Height * .5F - mapped->y / frame_.scale};
}

void EditorView::ReloadSize()
{
    frame_ = layout::CalculateFrame(canvas.Get()->LogicalSize().width);
    node->Transform().SetScale(frame_.scale, frame_.scale, 1);
    menuBar_.Resize(frame_.width);
    for (auto &tab : tabs_)
        tab->Resize();
    sliderInput_.InvalidateHitTest();
    state_.RequestRebuild();
}

void EditorView::DrawLetterbox()
{
    // Bands cover the canvas outside the frame; their node units grow as the frame shrinks.
    const auto size = canvas.Get()->LogicalSize();
    const float halfWidth = size.width * .5F / frame_.scale, halfHeight = size.height * .5F / frame_.scale;
    const float frameHalf = frame_.width * .5F, frameTop = layout::Height * .5F;
    const auto band = [this](Rect r) {
        if (r.width > .01F && r.height > .01F)
            visual->AddRectangle(r, ui_colors::theme::Letterbox, 0);
    };
    band({-halfWidth, frameTop, halfWidth * 2, halfHeight - frameTop});
    band({-halfWidth, -halfHeight, halfWidth * 2, halfHeight - frameTop});
    band({-halfWidth, -frameTop, halfWidth - frameHalf, frameTop * 2});
    band({frameHalf, -frameTop, halfWidth - frameHalf, frameTop * 2});
}

void EditorView::Box(Rect r, Color color, float radius)
{
    visual->AddRectangle(ToNode(r), color, radius);
}

float EditorView::PixelScale() const noexcept
{
    const auto *surface = canvas.Get();
    return surface ? std::max(.001F, surface->PixelScale() * frame_.scale) : 1;
}

void EditorView::Image(Rect r, const std::filesystem::path &path, Color tint)
{
    DrawPacket packet{};
    packet.type = DrawPacketType::Image;
    packet.bounds = ToNode(r);
    packet.image = visuals.RegisterImage(path);
    if (packet.image.value == 0)
        throw std::runtime_error("Editor skin image could not be loaded.");
    packet.color = tint;
    visual->AddPacket(std::move(packet));
}

void EditorView::ContainedImage(Rect r, const std::filesystem::path &image, Color tint)
{
    const auto handle = visuals.RegisterImage(image);
    if (handle.value == 0)
        throw std::runtime_error("The chart background image could not be loaded.");
    Image(ContainedImageBounds(r, visuals.GetImageSize(handle)), image, tint);
}

void EditorView::ChartBackground(Rect r, const std::filesystem::path &image, Point offset, Size screen)
{
    // r shows the centre of the play screen at one scale; the texture is cropped instead of stretched.
    const float scale = screen.width > 0 ? r.width / screen.width : 1;
    const Size shown{r.width / scale, r.height / scale};
    DrawPacket packet{};
    packet.type = DrawPacketType::Image;
    packet.bounds = ToNode(r);
    packet.image = visuals.RegisterImage(image);
    if (packet.image.value == 0)
        throw std::runtime_error("The chart background image could not be loaded.");
    const auto crop = ChartBackgroundCrop(screen, {-shown.width / 2, -shown.height / 2, shown.width, shown.height},
                                          visuals.GetImageSize(packet.image), offset);
    packet.uvOffset = {crop.x, crop.y};
    packet.uvScale = {crop.width, crop.height};
    packet.color = {1, 1, 1, 1};
    visual->AddPacket(std::move(packet));
}

void EditorView::Icon(Rect r, std::wstring_view name, Color tint)
{
    Image(r, Common(L"Icons." + std::wstring(name)), tint);
}

void EditorView::Text(Rect r, std::wstring text, float size, Color color)
{
    DrawPacket p{};
    p.type = DrawPacketType::Text;
    p.bounds = ToNode(r);
    p.color = color;
    auto font = ui_fonts::Text(text);
    p.text = std::make_shared<TextContent>(
        TextContent{std::move(text), std::move(font), size, TextAlignment::Leading, {}, 600});
    visual->AddPacket(std::move(p));
}

void EditorView::Label(Rect r, std::wstring text, EditorTextStyle style)
{
    // Captions are laid out from the top of their box; centre one line in the rectangle.
    const float line = style.size * 1.36F;
    DrawPacket p{};
    p.type = DrawPacketType::Text;
    p.bounds = ToNode({r.x, r.y + (r.height - line) * .5F, r.width, line});
    p.color = style.color;
    TextFont font;
    std::uint16_t weight = style.weight;
    if (style.number)
    {
        auto face = ui_fonts::Number(style.weight);
        font = std::move(face.font);
        weight = face.weight;
    }
    else
        font = ui_fonts::Text(text);
    p.text = std::make_shared<TextContent>(
        TextContent{std::move(text), std::move(font), style.size, style.alignment, {}, weight});
    visual->AddPacket(std::move(p));
}

void EditorView::Slanted(Rect r, float leftLean, float rightLean, Color color)
{
    // One-pixel rows draw the leaning edge; the batch keeps them a single packet.
    const float step = 1.0F / std::max(1.0F, PixelScale());
    for (float y = 0; y < r.height; y += step)
    {
        const float depth = (y + step * .5F) / r.height;
        const float left = r.x + leftLean * (1 - depth);
        const float right = r.x + r.width - rightLean * depth;
        Box({left, r.y + y, std::max(0.0F, right - left), std::min(step, r.height - y) + .05F}, color);
    }
}

EditorTextSet EditorView::Texts() const noexcept
{
    return Editor(texts_);
}

void EditorView::Control(Rect r, std::function<void()> action)
{
    controls.push_back({r, std::move(action)});
}

void EditorView::Button(Rect r, std::wstring text, std::function<void()> action, bool selected)
{
    // Modes use this for their own buttons: flat, cobalt when selected.
    Box(r, selected ? ui_colors::theme::Brand : White);
    if (!selected)
    {
        Box({r.x, r.y, r.width, 1}, ui_colors::theme::RuleStrong);
        Box({r.x, r.y + r.height - 1, r.width, 1}, ui_colors::theme::RuleStrong);
        Box({r.x, r.y, 1, r.height}, ui_colors::theme::RuleStrong);
        Box({r.x + r.width - 1, r.y, 1, r.height}, ui_colors::theme::RuleStrong);
    }
    // Long captions shrink to fit their button.
    float size = 13;
    while (size > 9 && EstimateTextWidth(text, size) > r.width - 10)
        --size;
    Label({r.x + 4, r.y, r.width - 8, r.height}, std::move(text),
          {size, selected ? White : ui_colors::theme::BrandText, TextAlignment::Center, 700});
    Control(r, std::move(action));
}

void EditorView::FlatButton(Rect r, std::wstring text, std::function<void()> action, bool primary)
{
    Button(r, std::move(text), std::move(action), primary);
}

void EditorView::DrawSwitch(Rect row, std::wstring caption, bool on, std::function<void()> toggle)
{
    Label({row.x, row.y, row.width - 56, row.height}, std::move(caption), {14, theme::Ink, TextAlignment::Leading, 600});
    const Rect track{row.x + row.width - 44, row.y + (row.height - 24) * .5F, 44, 24};
    Box(track, on ? theme::Brand : ui_colors::Rgb(0x0F2A47, .2F), 12);
    Box({on ? track.x + 22 : track.x + 2, track.y + 2, 20, 20}, White, 10);
    Control(row, std::move(toggle));
}

void EditorView::Field(Rect r, const wchar_t *label, std::string &value)
{
    InputField(r, label, Wide(value), [this, &value](const std::string &typed) {
        value = typed;
        state_.RequestRebuild();
    }, true);
}

TextInputBounds EditorView::ScreenBounds(Rect r) const noexcept
{
    // Frame -> canvas logical (centre origin) -> window pixels.
    const auto *surface = canvas.Get();
    const float pixel = surface ? surface->PixelScale() : 1;
    const auto size = surface ? surface->LogicalSize() : Size{layout::BaseWidth, layout::Height};
    const float left = (r.x - frame_.width * .5F) * frame_.scale + size.width * .5F;
    const float top = (r.y - layout::Height * .5F) * frame_.scale + size.height * .5F;
    return {static_cast<std::int32_t>(std::lround(left * pixel)), static_cast<std::int32_t>(std::lround(top * pixel)),
            static_cast<std::uint32_t>(std::max(1L, std::lround(r.width * frame_.scale * pixel))),
            static_cast<std::uint32_t>(std::max(1L, std::lround(r.height * frame_.scale * pixel)))};
}

void EditorView::InputField(Rect r, std::wstring label, std::wstring text, EditorTextFields::Commit commit, bool number,
                            bool showLabel, EditorTextFields::Commit typing)
{
    if (showLabel && !label.empty())
        Label({r.x, r.y - 18, r.width, 16}, label, {12, ui_colors::theme::Caption, TextAlignment::Leading, 600});
    Box(r, {1, 1, 1, .74F});
    Box({r.x, r.y + r.height - 2, r.width, 2}, ui_colors::Rgb(0xB5CFF5));
    if (fields_.Available() && !fieldsCovered_)
    {
        // The native editor sits inside the underline, inset from the box.
        const Rect inner{r.x + 6, r.y + 4, r.width - 12, r.height - 8};
        const auto font = static_cast<std::uint32_t>(std::max(1.0F, std::round(15 * PixelScale())));
        fields_.Declare(std::format("{}@{:.0f},{:.0f}", Utf8(label), r.x, r.y), ScreenBounds(inner), font, text,
                        std::move(commit), number, std::move(typing));
        fieldRects_.push_back(r);
        return;
    }
    Label({r.x + 8, r.y, r.width - 16, r.height}, text, {15, ui_colors::theme::Ink, TextAlignment::Leading, 600, number});
    if (fieldsCovered_)
        return;
    Control(r, [this, label, text, commit] {
        if (auto edited = EditText(label, Utf8(text), Texts()))
        {
            commit(*edited);
            state_.RequestRebuild();
        }
    });
}

EditorModeAreas EditorView::Areas() const noexcept
{
    EditorModeAreas areas;
    const float right = ContentRight();
    areas.lane = {layout::HeadWidth, layout::LaneTop, right - layout::HeadWidth, layout::LaneHeight};
    areas.judgeX = layout::HeadWidth + layout::JudgeOffset;
    areas.score = {0, layout::TrackTop, right, layout::FooterTop - layout::TrackTop};
    areas.body = {0, layout::BodyTop, frame_.width, layout::BodyHeight};
    // The chart's BG for Preview and the Metadata thumbnail, from the song folder's BG folder.
    {
        std::error_code error;
        auto path =
            SongLayout::BackgroundPath(state_.Request().SongFolder(), state_.Document().Pattern().backgroundFile);
        if (std::filesystem::is_regular_file(path, error))
            areas.background = std::move(path);
    }
    areas.musicMetadata = state_.MusicMetadataPath();
    areas.tools.y = layout::BodyTop;
    if (state_.Panel() == EditorPanel::Notes)
        areas.inspector = PanelBody();
    return areas;
}

void EditorView::BuildContent()
{
    const auto current = static_cast<std::size_t>(state_.Tab());
    for (std::size_t index = 0; index < tabs_.size(); ++index)
        tabs_[index]->Show(index == current);
    controls.clear();
    divisionRect_.reset();
    fieldRects_.clear();
    fields_.Begin();
    // Native editors would sit above a modal, so fields beneath one are drawn only.
    fieldsCovered_ = curvesOpen_ || findOpen_ || alert_.IsOpen();
    visual->Clear();
    Box({0, 0, frame_.width, layout::Height}, ui_colors::theme::Surface);
    CurrentTab().Build();
    DrawTabs();
    SyncInputControls();
    DrawFooter();
    fieldsCovered_ = alert_.IsOpen();
    DrawCurvesDialog();
    DrawFindDialog();
    DrawLetterbox();
    fields_.End();
    visual->Finish();
    textRevision_ = texts_.Revision();
    state_.FinishBuild();
}

void EditorView::Build()
{
    try
    {
        const auto &request = state_.Request();
        const auto &pattern = state_.Document().Pattern();
        std::wstring mode = Wide(pattern.mode);
        std::ranges::transform(mode, mode.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
        menuBar_.SetDocument({request.songTitle.empty() ? state_.Document().SourcePath().stem().wstring() : request.songTitle,
                              request.songArtist, Wide(pattern.name), std::move(mode), !state_.Creating()});
        menuBar_.Refresh(state_.Document().Dirty());
        BuildContent();
        buildFailed_ = false;
    }
    catch (const std::exception &error)
    {
        // Drop partial packets and their callbacks. Keep navigation and time
        // correction available, with no automatic retry until the next action.
        state_.SetStatus(error.what());
        buildFailed_ = true;
        for (auto &tab : tabs_)
            tab->BuildFailed();
        controls.clear();
        divisionRect_.reset();
        fieldRects_.clear();
        fields_.Begin();
        visual->Clear();
        sliderInput_.Reset(*canvas.Get());
        divisionSliderNode_->SetVisible(false);
        sliderInput_.InvalidateHitTest();
        Box({0, 0, frame_.width, layout::Height}, ui_colors::theme::Surface);
        Label({24, layout::BodyTop + 16, frame_.width - 48, 24}, Wide(state_.Status()),
              {14, ui_colors::theme::Danger, TextAlignment::Leading, 700});
        DrawTabs();
        SyncInputControls();
        DrawFooter();
        DrawLetterbox();
        fields_.End();
        visual->Finish();
        state_.FinishBuild();
        textRevision_ = texts_.Revision();
    }
}
