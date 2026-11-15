#include "ResultScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Submodules/ResultViewFactory.h"
#include <stdexcept>
#include "Records/PlayRecordStore.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <Windows.h>
using namespace PhantomCore;

namespace
{
    // Order of the record failure alert's buttons: Close (Esc) then Retry (Enter).
    constexpr std::size_t RetryButton = 1;

    // SQLite reports UTF-8; standard library errors may use the system code page.
    std::wstring DecodeError(const std::string &text)
    {
        if (text.empty())
            return {};
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int length = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            length = MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
        }
        if (length <= 0)
            return L"?";
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }
} // namespace

ResultScene::ResultScene(std::unique_ptr<IPlayResult> result,
                         ScreenVisual2DManager &visuals, TextCatalog &texts,
                         ResultViewContext context, RetryHandler retry)
    : result_(std::move(result)), retry_(std::move(retry)), visuals_(visuals), texts_(texts)
{
    if (!result_)
        throw std::invalid_argument("Result Scene requires a play result.");
    recordPath_ = context.recordDatabase;
    if (!recordPath_.empty())
        StartRecordWrite();
    context.canRetry = context.canRetry && static_cast<bool>(retry_);
    view_ = CreateResultView(*result_, visuals, texts, std::move(context));
}
ResultScene::~ResultScene() = default;
void ResultScene::StartRecordWrite()
{
    recordError_.clear();
    recordWrite_ = std::async(std::launch::async, [path = recordPath_, record = PlayRecord::From(*result_)] {
        try { PlayRecordStore(path).Save(record); return std::string{}; }
        catch (const std::exception &error) { return std::string(error.what()); }
    });
}
void ResultScene::Initialize(const EngineServices &services)
{
    view_->Initialize(services);
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    if (recordPath_.empty())
        return;
    recordStatusCanvas_ = visuals_.CreateOwnedCanvas({{1280,720}, CanvasScaleMode::FixedHeight});
    auto *canvas = recordStatusCanvas_.Get();
    if (!canvas) throw std::runtime_error("Failed to create the record status canvas.");
    // Save line in the bottom bar, right after the Back block.
    recordStatus_ = &CreateLabel(canvas->AnchorNode(Anchor::BottomLeft), {156, 0, 560, 64},
        std::wstring(texts_.Get("Records.saving")), "Result.RecordStatus");
    auto &text = *recordStatus_->GetComponent<TextVisualComponent>();
    text.SetFontSize(13); text.SetTextColor(ui_colors::theme::Caption);
    recordAlert_.Initialize(*canvas, &visuals_);
}
void ResultScene::BeginScene()
{
    view_->SetVisible(true);
    static_cast<void>(recordStatusCanvas_.SetVisible(true));
}
void ResultScene::EndScene() noexcept
{
    view_->SetVisible(false);
    static_cast<void>(recordStatusCanvas_.SetVisible(false));
}
void ResultScene::PollRecordWrite()
{
    if (!recordWrite_.valid() || recordWrite_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
        return;
    recordError_ = recordWrite_.get();
    if (recordError_.empty() || !recordStatusCanvas_)
        return;
    // The status line only says the record was not kept; the alert names the file and the cause.
    recordAlert_.Show({AlertKind::Error, std::wstring(texts_.Get("Records.saveFailedTitle")),
                       std::wstring(texts_.Get("Records.saveFailedMessage")),
                       recordPath_.wstring() + L"\n" + DecodeError(recordError_),
                       {{std::wstring(texts_.Get("Alert.close")), false, false, true},
                        {std::wstring(texts_.Get("Alert.retry")), true}}});
}
void ResultScene::UpdateRecordStatus()
{
    if (!recordStatus_)
        return;
    auto &text = *recordStatus_->GetComponent<TextVisualComponent>();
    const bool saving = recordWrite_.valid();
    auto status = std::wstring(texts_.Get(saving ? "Records.saving"
                                          : recordError_.empty() ? "Records.saved" : "Records.saveError"));
    if (text.Text() == status)
        return;
    // Saved is green, a failed save red; the result itself stays on screen.
    text.SetTextColor(saving ? ui_colors::theme::Caption
                      : recordError_.empty() ? ui_colors::theme::Ok : ui_colors::theme::Danger);
    text.SetFont(ui_fonts::Text(status));
    text.SetText(std::move(status));
}
void ResultScene::UpdateRecordAlert(const InputState &input)
{
    auto &canvas = *recordStatusCanvas_.Get();
    const auto point = input.IsMouseInsideWindow()
        ? MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                           {static_cast<float>(width_), static_cast<float>(height_)}, canvas)
        : std::nullopt;
    PointerInput pointer{};
    pointer.available = point.has_value();
    pointer.position = point.value_or(Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    pointer.wheelDelta = input.MouseWheelDelta();
    recordAlertInput_.Process(canvas, pointer);
    static_cast<void>(canvas.TakeActions());
    if (recordAlert_.Update(input) == RetryButton)
        StartRecordWrite();
    if (!recordAlert_.IsOpen())
        recordAlertInput_.Reset(canvas);
}
void ResultScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    PollRecordWrite();
    if (recordAlert_.IsOpen())
    {
        UpdateRecordAlert(context.input);
        UpdateRecordStatus();
        // Behind the alert the result keeps animating but takes no input.
        static_cast<void>(view_->Update(InputState{}, context.deltaSeconds));
        return;
    }
    UpdateRecordStatus();
    const auto action = view_->Update(context.input, context.deltaSeconds);
    if (action == ResultAction::Retry && (!retry_ || !retry_(scenes)))
        throw std::runtime_error("Failed to retry the completed pattern.");
    if (action == ResultAction::Back && !scenes.ChangeScene(LobbySceneId))
        throw std::runtime_error("Failed to return from Result to song select.");
}
void ResultScene::Render(const RenderContext &)
{
}
void ResultScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;
    recordAlertInput_.InvalidateHitTest();
    view_->OnResize(width, height);
}
void ResultScene::Shutdown() noexcept
{
    if (auto *canvas = recordStatusCanvas_.Get())
        recordAlertInput_.Reset(*canvas);
    recordAlert_.Shutdown();
    recordStatus_ = nullptr;
    recordStatusCanvas_.Reset();
    view_->Shutdown();
}
