#include "OptionsAudio.h"
#include "OptionsPanelStyle.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    constexpr std::array VolumeNames{"Master", "Music", "Effects"};
    // A press must move this far before it becomes a drag; then one Canvas pixel is one millisecond.
    constexpr float OffsetDragThreshold = 4.0F, OffsetPixelsPerMs = 1.0F;
} // namespace

std::vector<float> OptionsAudio::FieldHeights() const
{
    constexpr float control = FieldHeight(ControlHeight);
    // Volumes, then middleware, output, driver, buffer and offset.
    return {InlineRowsHeight(VolumeSliderCount), control, control, control, control, control};
}

void OptionsAudio::Create()
{
    CreateVolumeControls();
    CreateDeviceControls();
    CreateOffsetControls();
}

void OptionsAudio::CreateVolumeControls()
{
    const std::array<OptionsPercentSlider::Binding, VolumeSliderCount> bindings{
        OptionsPercentSlider::Binding{[](const OptionValues &values) { return values.volumes.master; },
                                      [](OptionValues &values, const int percent) { values.volumes.master = percent; },
                                      "Options.masterVolume"},
        OptionsPercentSlider::Binding{[](const OptionValues &values) { return values.volumes.music; },
                                      [](OptionValues &values, const int percent) { values.volumes.music = percent; },
                                      "Options.musicVolume"},
        OptionsPercentSlider::Binding{[](const OptionValues &values) { return values.volumes.effects; },
                                      [](OptionValues &values, const int percent) { values.volumes.effects = percent; },
                                      "Options.effectsVolume"}};
    // The three volumes share one block of inline rows to keep the Audio section short.
    const float top = context_.NextInlineRows(VolumeSliderCount);
    for (std::size_t i = 0; i < VolumeSliderCount; ++i)
        volumes_[i].Create(context_, bindings[i], std::string("Options.Volume.") + VolumeNames[i],
                           top + InlineRowHeight * static_cast<float>(i));
}

void OptionsAudio::CreateDeviceControls()
{
    middlewareCombo_ = &CreateComboField("Middleware", middlewareLabel_);
    outputCombo_ = &CreateComboField("Output", outputLabel_);
    driverCombo_ = &CreateComboField("Driver", driverLabel_);
    bufferCombo_ = &CreateComboField("Buffer", bufferLabel_);
    RequireDropdown(*middlewareCombo_).SetItems({std::wstring(context_.Texts().Get("Options.middlewareFmod"))});
}

void OptionsAudio::CreateOffsetControls()
{
    // A short number box followed by its unit.
    constexpr float FieldBoxWidth = 112;
    auto &content = context_.Content();
    const float contentHeight = context_.ContentHeight();
    const float top = context_.NextField(ControlHeight);
    offsetLabel_ = &CreateFieldLabel(content, top, "Options.Offset.Label");
    offsetField_ = &CreateButton(content, TopLeft({ContentLeft, top + ControlOffset, FieldBoxWidth, ControlHeight}, contentHeight),
                                 L"", "Options.Offset.Field");
    StyleFieldButton(*offsetField_, true);
    AddFieldLine(content, *offsetField_, "Options.Offset.Line");
    offsetUnit_ = &CreateCaption(content, {ContentLeft + FieldBoxWidth + 8, top + ControlOffset, 40, ControlHeight}, 13, 400,
                                 ui_colors::theme::Caption, "Options.Offset.Unit");
}

void OptionsAudio::Refresh()
{
    RefreshDeviceControls();
    for (auto &volume : volumes_)
        volume.Refresh(context_);
    RefreshOffsetControls();
}

void OptionsAudio::RefreshDeviceControls()
{
    const auto &text = Options(context_.Texts());
    auto &audio = context_.Audio();
    const std::array labels{std::pair{middlewareLabel_, text.middleware}, std::pair{outputLabel_, text.output},
                            std::pair{driverLabel_, text.driver}, std::pair{bufferLabel_, text.buffer}};
    for (const auto &[node, value] : labels)
        SetCaption(*node, std::wstring(value));

    auto &output = RequireDropdown(*outputCombo_);
    std::vector<std::wstring> names;
    for (auto backend : Outputs)
        names.push_back(OutputName(backend, text));
    output.SetItems(std::move(names));
    const auto requested = std::ranges::find(Outputs, audio.RequestedOutput());
    if (requested != Outputs.end())
        output.SetSelectedIndex(static_cast<std::size_t>(requested - Outputs.begin()));

    names.clear();
    driverIndices_.clear();
    for (const auto &driver : audio.OutputDrivers())
    {
        driverIndices_.push_back(driver.driverIndex);
        names.push_back(std::to_wstring(driver.driverIndex) + L": " + WideText(driver.name));
    }
    auto &driver = RequireDropdown(*driverCombo_);
    if (names.empty())
        names.emplace_back(text.noDrivers);
    driver.SetItems(std::move(names));
    driverCombo_->SetEnabled(!driverIndices_.empty());
    const auto selected = std::ranges::find(driverIndices_, audio.ActiveDriverIndex());
    if (selected != driverIndices_.end())
        driver.SetSelectedIndex(static_cast<std::size_t>(selected - driverIndices_.begin()));

    // The saved buffer, which the running mixer uses only after a restart (Notice says so).
    names.clear();
    for (const auto length : DspBufferLengths)
        names.push_back(std::to_wstring(length) + L" " + std::wstring(text.bufferSamples));
    auto &buffer = RequireDropdown(*bufferCombo_);
    buffer.SetItems(std::move(names));
    const auto savedBuffer = std::ranges::find(DspBufferLengths, context_.Settings().Values().dspBufferLength);
    if (savedBuffer != DspBufferLengths.end())
        buffer.SetSelectedIndex(static_cast<std::size_t>(savedBuffer - DspBufferLengths.begin()));
}

void OptionsAudio::RefreshOffsetControls()
{
    SetCaption(*offsetLabel_, std::wstring(Options(context_.Texts()).globalOffset));
    SetCaption(*offsetUnit_, L"ms");
    auto &text = RequireComponent<TextVisualComponent>(*offsetField_);
    if (offsetDrag_ && offsetDrag_->moving)
        text.SetText(std::to_wstring(offsetDrag_->value));
    else
        text.SetText(editingOffset_ ? offsetDraft_ + L"_" : std::to_wstring(context_.Settings().Values().globalOffsetMs));
}

std::wstring OptionsAudio::Notice() const
{
    const auto &text = Options(context_.Texts());
    const auto &saved = context_.Settings().Values();
    auto &audio = context_.Audio();
    if (audio.RequestedOutput() != saved.audioOutput)
        return std::wstring(text.outputFallback) + L" " + OutputName(audio.ActiveOutput(), text);
    // A saved name identifies the driver; without one, a saved index must still be active.
    const auto active = audio.ActiveDriverIndex();
    const bool driverUnavailable = saved.driverName.empty() ? saved.driverIndex >= 0 && saved.driverIndex != active
                                                            : saved.driverName != DriverName(active);
    if (driverUnavailable)
        return std::wstring(text.driverUnavailable);
    if (BufferAwaitsRestart())
        return std::wstring(text.bufferRestart);
    return {};
}

bool OptionsAudio::ProcessAction(const Action &action)
{
    if (action.type == ActionType::Clicked && action.source == offsetField_->Id())
    {
        StartOffsetEdit();
        return true;
    }
    if (action.type != ActionType::SelectionChanged)
        return false;
    return SelectDevice(action);
}

bool OptionsAudio::SelectDevice(const Action &action)
{
    if (action.source == middlewareCombo_->Id())
        return true; // FMOD is the sole installed middleware.
    if (action.source == outputCombo_->Id() && action.selectedIndex < Outputs.size())
        SelectOutput(Outputs[action.selectedIndex]);
    else if (action.source == driverCombo_->Id() && action.selectedIndex < driverIndices_.size())
        SelectDriver(driverIndices_[action.selectedIndex]);
    else if (action.source == bufferCombo_->Id() && action.selectedIndex < DspBufferLengths.size())
        SelectBuffer(action.selectedIndex);
    else
        return false;
    context_.RefreshTexts();
    return true;
}

bool OptionsAudio::BufferAwaitsRestart() const noexcept
{
    // A backend that reports no buffer (none, or a test double) cannot be compared.
    const auto running = context_.Audio().DspBufferLength();
    return running != 0 && running != context_.Settings().Values().dspBufferLength;
}

void OptionsAudio::SelectBuffer(const std::size_t index)
{
    auto candidate = context_.Settings().Values();
    candidate.dspBufferLength = DspBufferLengths[index];
    static_cast<void>(context_.Save(candidate));
}

std::string OptionsAudio::DriverName(const int index) const
{
    for (const auto &driver : context_.Audio().OutputDrivers())
        if (driver.driverIndex == index)
            return driver.name;
    return {};
}

void OptionsAudio::RestoreAudio(const AudioOutputBackend output, const int driver, std::string &error)
{
    auto &audio = context_.Audio();
    std::string restorationError;
    if (!audio.SetOutputBackend(output, restorationError) ||
        (driver >= 0 && !audio.SetOutputDriver(driver, restorationError)))
        error += " | Could not restore the previous audio selection: " + restorationError;
}

void OptionsAudio::SelectOutput(const AudioOutputBackend output)
{
    auto &audio = context_.Audio();
    const auto previousOutput = audio.RequestedOutput();
    const int previousDriver = audio.ActiveDriverIndex();
    auto candidate = context_.Settings().Values();
    candidate.audioOutput = output;
    if (!audio.SetOutputBackend(output, context_.StatusDetail()))
    {
        RestoreAudio(previousOutput, previousDriver, context_.StatusDetail());
        context_.SetStatus(OptionsStatus::AudioFailed);
        return;
    }
    candidate.driverIndex = audio.ActiveDriverIndex();
    candidate.driverName = DriverName(candidate.driverIndex);
    if (!context_.Save(candidate))
        RestoreAudio(previousOutput, previousDriver, context_.StatusDetail());
}

void OptionsAudio::SelectDriver(const int index)
{
    auto &audio = context_.Audio();
    const int previousDriver = audio.ActiveDriverIndex();
    auto candidate = context_.Settings().Values();
    candidate.audioOutput = audio.RequestedOutput();
    candidate.driverIndex = index;
    candidate.driverName = DriverName(index);
    if (!audio.SetOutputDriver(index, context_.StatusDetail()))
    {
        context_.SetStatus(OptionsStatus::AudioFailed);
        return;
    }
    if (!context_.Save(candidate))
        RestoreAudio(audio.RequestedOutput(), previousDriver, context_.StatusDetail());
}

OptionsPointer OptionsAudio::ProcessPointer(const InputState &input, const std::optional<Point> canvasPointer,
                                            const NodeId hoveredNode)
{
    for (auto &volume : volumes_)
        if (volume.ProcessPointer(context_, input, canvasPointer, hoveredNode))
            return OptionsPointer::Sliding;
    // A finished offset drag also drops the click the release produced, so no keyboard edit starts.
    return ProcessOffsetDrag(input, canvasPointer, hoveredNode) ? OptionsPointer::Consumed : OptionsPointer::Free;
}

bool OptionsAudio::ProcessOffsetDrag(const InputState &input, const std::optional<Point> canvasPointer,
                                     const NodeId hoveredNode)
{
    if (!offsetDrag_)
    {
        if (!context_.IsOpen() || !canvasPointer || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        if (offsetField_ == nullptr || hoveredNode != offsetField_->Id())
            return false;
        const auto saved = context_.Settings().Values().globalOffsetMs;
        offsetDrag_ = OffsetDrag{canvasPointer->x, saved, saved, false};
        // The press itself still reaches the field, which only reacts to a click.
        return false;
    }
    if (canvasPointer)
    {
        const float moved = canvasPointer->x - offsetDrag_->startX;
        if (!offsetDrag_->moving && std::abs(moved) >= OffsetDragThreshold)
        {
            context_.CancelKeyboardEdit();
            context_.CollapseCombos();
            offsetDrag_->moving = true;
        }
        if (offsetDrag_->moving)
        {
            const auto value = static_cast<std::int64_t>(offsetDrag_->start) + std::llround(moved / OffsetPixelsPerMs);
            offsetDrag_->value = static_cast<std::int32_t>(std::clamp<std::int64_t>(
                value, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
            RefreshOffsetControls();
        }
    }
    const bool moving = offsetDrag_->moving;
    if (input.IsMouseButtonDown(MouseButton::Left) && !input.WasMouseButtonReleased(MouseButton::Left))
        return moving;
    // Released: one save per drag, as the volume sliders do.
    const auto value = offsetDrag_->value;
    offsetDrag_.reset();
    if (moving && value != context_.Settings().Values().globalOffsetMs)
    {
        auto candidate = context_.Settings().Values();
        candidate.globalOffsetMs = value;
        static_cast<void>(context_.Save(candidate));
    }
    RefreshOffsetControls();
    context_.RefreshStatus();
    return moving;
}

void OptionsAudio::StartOffsetEdit()
{
    context_.CancelKeyboardEdit();
    offsetDraft_ = std::to_wstring(context_.Settings().Values().globalOffsetMs);
    editingOffset_ = replaceOffsetDraft_ = true;
    RefreshOffsetControls();
}

void OptionsAudio::HandleKey(const std::uint16_t virtualKey)
{
    EditOffset(virtualKey);
}

void OptionsAudio::EditOffset(const std::uint16_t key)
{
    if (key == VK_RETURN)
        ApplyOffset();
    else
        EditNumberDraft(offsetDraft_, replaceOffsetDraft_, key);
    RefreshOffsetControls();
}

void OptionsAudio::ApplyOffset()
{
    std::string draft;
    draft.reserve(offsetDraft_.size());
    for (const auto character : offsetDraft_)
        draft.push_back(static_cast<char>(character)); // Numeric ASCII draft only.
    std::int32_t value{};
    const auto parsed = std::from_chars(draft.data(), draft.data() + draft.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != draft.data() + draft.size())
    {
        context_.SetStatus(OptionsStatus::InvalidInput);
        context_.StatusDetail() = "Offset must be an integer number of milliseconds within int32 range.";
    }
    else
    {
        auto candidate = context_.Settings().Values();
        candidate.globalOffsetMs = value;
        if (context_.Save(candidate))
            editingOffset_ = false;
    }
    context_.RefreshStatus();
}

void OptionsAudio::CancelKeyboardEdit()
{
    editingOffset_ = false;
    replaceOffsetDraft_ = false;
    if (offsetField_)
        RefreshOffsetControls();
}

void OptionsAudio::AppendCombos(std::vector<Visual2DNode *> &combos) const
{
    combos.insert(combos.end(), {middlewareCombo_, outputCombo_, driverCombo_, bufferCombo_});
}

void OptionsAudio::StopDrags() noexcept
{
    for (auto &volume : volumes_)
        volume.Stop();
    offsetDrag_.reset();
}

void OptionsAudio::OnToggle()
{
    StopDrags();
}

void OptionsAudio::OnClosed()
{
    StopDrags();
}

void OptionsAudio::Shutdown() noexcept
{
    for (auto &volume : volumes_)
        volume.Shutdown();
    middlewareLabel_ = outputLabel_ = driverLabel_ = bufferLabel_ = nullptr;
    middlewareCombo_ = outputCombo_ = driverCombo_ = bufferCombo_ = nullptr;
    driverIndices_.clear();
    offsetLabel_ = offsetField_ = offsetUnit_ = nullptr;
    offsetDraft_.clear();
    editingOffset_ = replaceOffsetDraft_ = false;
    offsetDrag_.reset();
}
