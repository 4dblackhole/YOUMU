#include "Presentation/PlayOptionsPanel.h"
#include <algorithm>
#include <cmath>

void PlayOptionsPanel::SetViewport(std::uint32_t height)
{
    height_ = height;
    if (panel_) Layout();
}

void PlayOptionsPanel::Layout()
{
    if (!panel_ || height_ == 0) return;
    const auto logical = canvas_->LogicalSize();
    const float width = std::max(1.0F, std::min(1180.0F, logical.width - 48));
    const float height = std::max(180.0F, logical.height - 48);
    const float eased = progress_ * progress_ * (3 - 2 * progress_);
    overlay_->SetBounds({-logical.width / 2, -logical.height / 2, logical.width, logical.height});
    overlay_->SetClipRect({0, 0, logical.width, logical.height});
    panel_->SetBounds({(logical.width - width) / 2, 24 - (1 - eased) * (height + 24), width, height});
    title_->SetBounds({24, height - 60, std::max(width - 160, 1.0F), 40});
    close_->SetBounds({width - 116, height - 54, 92, 34});
    reset_->SetBounds({width - 124, 24, 100, 34});
    status_->SetBounds({24, 14, std::max(width - 172, 1.0F), 58});
    viewportHeight_ = std::max(height - 164, 1.0F);
    const float bodyWidth = std::max(width - 48, 1.0F);
    viewport_->SetBounds({24, 86, bodyWidth, viewportHeight_});
    viewport_->SetClipRect({0, 0, bodyWidth, viewportHeight_});
    const float column = std::max((bodyWidth - 40) / 3, 1.0F);
    const auto place = [column](Visual2DNode &node, std::size_t group, float top, float height) {
        node.SetBounds({(column + 20) * static_cast<float>(group), -top - height, column, height});
    };
    for (std::size_t i = 0; i < headings_.size(); ++i) place(*headings_[i], i, 0, 30);
    float difficulty = 46, layout = 46, assist = 46;
    const auto field = [&](Field index, std::size_t group, float &top) {
        place(*labels_[index], group, top, index == Rate ? 30.0F : 22.0F);
        top += index == Rate ? 36 : 28;
        place(*fields_[index], group, top, 34);
        top += 50;
    };
    const auto flag = [&](Flag index, std::size_t group, float &top) {
        place(*flags_[index], group, top, 34); top += 42;
        flags_[index]->GetComponent<TextVisualComponent>()->SetContentBounds({38, 0, std::max(column - 42, 1.0F), 34});
    };
    place(*unitLabel_, 0, difficulty, 22); difficulty += 28;
    place(*unit_, 0, difficulty, 34); difficulty += 50;
    field(Speed, 0, difficulty);
    field(Rate, 0, difficulty);
    flags_[MusicPitch]->SetVisible(rateExpanded_);
    flags_[HitPitch]->SetVisible(rateExpanded_);
    if (rateExpanded_) { flag(MusicPitch, 0, difficulty); flag(HitPitch, 0, difficulty); }
    flag(OverrideJudge, 0, difficulty);
    labels_[Judge]->SetVisible(overrideJudge_); fields_[Judge]->SetVisible(overrideJudge_);
    if (overrideJudge_) field(Judge, 0, difficulty);
    flag(Mirror, 1, layout); flag(Random, 1, layout);
    // A hidden invalid random value must remain repairable without enabling Random.
    const bool randomDetails = draft_.random || errorField_ == Seed || errorField_ == Probability;
    for (const auto index : {Seed, Probability})
    {
        labels_[index]->SetVisible(randomDetails); fields_[index]->SetVisible(randomDetails);
        if (randomDetails) field(index, 1, layout);
    }
    flag(IgnoreScroll, 2, assist); flag(Metronome, 2, assist);
    contentHeight_ = std::max({difficulty, layout, assist});
    scroll_ = std::clamp(scroll_, 0.0F, std::max(contentHeight_ - viewportHeight_, 0.0F));
    content_->SetPosition({0, viewportHeight_ + scroll_});
    for (auto *node : fields_)
    {
        node->SetClipRect({0, 0, column, 34});
        node->GetComponent<TextVisualComponent>()->SetContentBounds({6, 0, std::max(column - 12, 1.0F), 34});
    }
    UpdateInputs();
}

void PlayOptionsPanel::UpdateInputs()
{
    const auto logical = canvas_->LogicalSize();
    if (height_ == 0 || logical.height <= 0) return;
    const float scale = static_cast<float>(height_) / logical.height;
    const auto viewport = viewport_->BoundsInCanvas();
    const bool available = IsVisible() && progress_ >= 1 && !unit_->GetComponent<ComboBoxBehaviorComponent>()->IsExpanded();
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        const auto bounds = fields_[i]->BoundsInCanvas();
        const bool visible = available && fields_[i]->IsVisible() &&
            bounds.y >= viewport.y - .01F && bounds.y + bounds.height <= viewport.y + viewport.height + .01F;
        fields_[i]->GetComponent<TextVisualComponent>()->SetText(inputs_[i] && visible ? L"" : numbers_[i]);
        if (!inputs_[i]) continue;
        const auto font = static_cast<std::uint32_t>(std::max(1.0F, std::round(16 * scale)));
        const float editHeight = std::min(static_cast<float>(font) + 4, 28 * scale);
        inputs_[i]->SetBounds({static_cast<std::int32_t>(std::round((bounds.x + logical.width / 2 + 6) * scale)),
            static_cast<std::int32_t>(std::round((logical.height / 2 - bounds.y - bounds.height / 2) * scale - editHeight / 2)),
            static_cast<std::uint32_t>(std::max(1.0F, std::round((bounds.width - 12) * scale))),
            static_cast<std::uint32_t>(std::max(1.0F, std::round(editHeight)))});
        inputs_[i]->SetFont(L"Segoe UI", font);
        inputs_[i]->SetVisible(visible);
    }
}
