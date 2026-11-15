#include "PerformanceBand.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    constexpr float PaddingLeft = 18, PaddingRight = 12, Gap = 6, RuleMargin = 4, RuleHeight = 10;
    constexpr float NumberMinWidth = 30, Slant = 8, NameSize = 11, NumberSize = 15, TextSlack = 4;
    constexpr std::uint32_t CanvasOrder = 1000;
    constexpr auto BandColour = ui_colors::Rgb(0x142F5E, .86F);
    constexpr Color NameColour{1, 1, 1, .62F}, RuleColour{1, 1, 1, .3F};
    constexpr const wchar_t *Unmeasured = L"—";

    void Paint(Visual2DNode &node, const Color colour)
    {
        auto *sprite = node.GetComponent<SpriteVisualComponent>();
        if (sprite == nullptr)
            throw std::logic_error("A performance band part is missing its sprite.");
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        sprite->SetStyle(style);
    }
    TextVisualComponent &Text(Visual2DNode &node)
    {
        auto *text = node.GetComponent<TextVisualComponent>();
        if (text == nullptr)
            throw std::logic_error("A performance band label is missing its text.");
        return *text;
    }
    // Width before the renderer has measured a string.
    float Width(Visual2DNode &node, const float size)
    {
        const auto &text = Text(node);
        if (const auto measured = text.MeasuredSize())
            return measured->width;
        float width = 0;
        for (const wchar_t character : text.Text())
            width += size * (character == L' ' ? 0.3F : 0.56F);
        return width;
    }
    void SetText(Visual2DNode &node, const std::wstring &value)
    {
        auto &text = Text(node);
        if (text.Text() != value)
            text.SetText(value);
    }
} // namespace

void PerformanceBand::Initialize(ScreenVisual2DManager &visuals, TextCatalog &texts)
{
    texts_ = &texts;
    canvas_ = visuals.CreateOwnedCanvas({{1280, 720}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
    auto *canvas = canvas_.Get();
    if (canvas == nullptr)
        throw std::runtime_error("Failed to create the performance band canvas.");
    band_ = &canvas->AnchorNode(Anchor::BottomRight).CreateChild("Performance.Band");
    body_ = &CreateSprite(*band_, {}, {}, "Body");
    Paint(*body_, BandColour);
    // The left edge leans like "/": a clipped strip holds a turned rectangle whose left side is the edge.
    auto &edge = band_->CreateChild("Edge");
    edge.SetBounds({0, 0, Slant, Height});
    edge.SetClipRect({0, 0, Slant, Height});
    slant_ = &CreateSprite(edge, {}, {}, "Slant");
    Paint(*slant_, BandColour);
    slant_->SetPivot({0, 0});
    slant_->SetSize({Slant * 3, Height * 3});
    slant_->SetPosition({0, 0});
    slant_->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -std::atan2(Slant, Height));
    BuildSegments(nullptr);
    static_cast<void>(canvas_.SetVisible(visible_));
}

void PerformanceBand::Shutdown() noexcept
{
    parts_.clear();
    segments_.clear();
    band_ = body_ = slant_ = nullptr;
    texts_ = nullptr;
    canvas_.Reset();
}

void PerformanceBand::SetVisible(const bool visible) noexcept
{
    visible_ = visible;
    static_cast<void>(canvas_.SetVisible(visible));
}

void PerformanceBand::Refresh(const PerformanceStatistics &performance)
{
    last_ = performance;
    if (band_ != nullptr)
        BuildSegments(&last_);
}

void PerformanceBand::Update()
{
    if (band_ == nullptr || !visible_)
        return;
    if (textRevision_ != texts_->Revision())
    {
        BuildSegments(last_.hasMeasurement ? &last_ : nullptr);
        return;
    }
    // Measured glyph widths arrive a frame after a text changes.
    float width = PaddingLeft + PaddingRight;
    for (std::size_t i = 0; i < parts_.size(); ++i)
    {
        width += Width(*parts_[i].name, NameSize) + Gap + std::max(NumberMinWidth, Width(*parts_[i].value, NumberSize));
        if (i + 1 < parts_.size())
            width += Gap;
        if (segments_[i].separated)
            width += RuleMargin * 2 + 1 + Gap;
    }
    if (std::abs(width - laidOutWidth_) > 0.25F)
        Layout();
}

void PerformanceBand::BuildSegments(const PerformanceStatistics *performance)
{
    const auto value = [performance](const std::uint64_t PerformanceStatistics::*member) {
        return performance != nullptr ? std::to_wstring(performance->*member) : std::wstring(Unmeasured);
    };
    const auto name = [this](const char *key) { return std::wstring(texts_->Get(key)); };
    segments_ = {{name("Performance.fps"), value(&PerformanceStatistics::framesPerSecond), false},
                 {name("Performance.ups"), value(&PerformanceStatistics::updatesPerSecond), true}};
    if constexpr (ShowsDrawCalls)
    {
        segments_.push_back({name("Performance.draws"), value(&PerformanceStatistics::drawCallsPerFrame), true});
        segments_.push_back({name("Performance.peak"), value(&PerformanceStatistics::peakDrawCallsPerFrame), false});
    }
    textRevision_ = texts_->Revision();
    const auto numberFace = ui_fonts::Number(800);
    while (parts_.size() < segments_.size())
    {
        Parts parts;
        parts.name = &CreateLabel(*band_, {}, L"", "Name");
        auto &nameText = Text(*parts.name);
        nameText.SetFontSize(NameSize);
        nameText.SetFontWeight(600);
        nameText.SetTextColor(NameColour);
        parts.value = &CreateLabel(*band_, {}, L"", "Value");
        auto &valueText = Text(*parts.value);
        valueText.SetFont(numberFace.font);
        valueText.SetFontSize(NumberSize);
        valueText.SetFontWeight(numberFace.weight);
        valueText.SetTextColor(ui_colors::White);
        parts.rule = &CreateSprite(*band_, {}, {}, "Rule");
        Paint(*parts.rule, RuleColour);
        parts_.push_back(parts);
    }
    for (std::size_t i = 0; i < parts_.size(); ++i)
    {
        const bool used = i < segments_.size();
        parts_[i].name->SetVisible(used);
        parts_[i].value->SetVisible(used);
        parts_[i].rule->SetVisible(used && segments_[i].separated);
        if (!used)
            continue;
        SetText(*parts_[i].name, segments_[i].name);
        Text(*parts_[i].name).SetFont(ui_fonts::Text(segments_[i].name));
        SetText(*parts_[i].value, segments_[i].value);
    }
    Layout();
}

void PerformanceBand::Layout()
{
    // Like the mockup's flex row: name, gap, value (at least 30 wide), and a rule between segments.
    float x = PaddingLeft;
    for (std::size_t i = 0; i < segments_.size(); ++i)
    {
        const auto &parts = parts_[i];
        if (segments_[i].separated)
        {
            x += RuleMargin;
            parts.rule->SetBounds({x, (Height - RuleHeight) * 0.5F, 1, RuleHeight});
            x += 1 + RuleMargin + Gap;
        }
        const float nameWidth = Width(*parts.name, NameSize);
        // The small caption sits a pixel low so it shares the numbers' baseline.
        parts.name->SetBounds({x, -1, nameWidth + TextSlack, Height});
        x += nameWidth + Gap;
        const float valueWidth = std::max(NumberMinWidth, Width(*parts.value, NumberSize));
        parts.value->SetBounds({x, 0, valueWidth + TextSlack, Height});
        x += valueWidth;
        if (i + 1 < segments_.size())
            x += Gap;
    }
    const float width = x + PaddingRight;
    laidOutWidth_ = width;
    band_->SetBounds({-width, 0, width, Height});
    body_->SetBounds({Slant, 0, width - Slant, Height});
}
