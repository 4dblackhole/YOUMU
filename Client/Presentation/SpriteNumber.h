#pragma once
#include "PhantomCore.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>

// A borrowed group of reusable sprites; the parent Canvas owns every node.
// Atlas contract: five columns, two rows, digits 0..9 in reading order.
class SpriteNumber final
{
  public:
    void Initialize(PhantomCore::Visual2DNode &parent, PhantomCore::ImageHandle atlas, std::string name)
    {
        Reset();
        root_ = &parent.CreateChild(std::move(name));
        root_->SetZIndex(5);
        for (std::size_t i = 0; i < digits_.size(); ++i)
        {
            digits_[i] = &PhantomCore::CreateSprite(*root_, {}, atlas, "Number.Digit" + std::to_string(i));
            digits_[i]->SetVisible(false);
        }
        SetValue(0);
    }
    void Reset() noexcept { *this = SpriteNumber{}; }
    void SetLayout(PhantomCore::Rect bounds, PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Trailing)
    {
        bounds_ = bounds;
        alignment_ = alignment;
        Present();
    }
    void SetValue(std::uint64_t value)
    {
        if (hasValue_ && value_ == value)
            return;
        value_ = value;
        hasValue_ = true;
        const auto converted = std::to_chars(characters_.data(), characters_.data() + characters_.size(), value);
        count_ = static_cast<std::size_t>(converted.ptr - characters_.data());
        Present();
    }

  private:
    void Present()
    {
        if (!root_ || count_ == 0)
            return;
        root_->SetBounds(bounds_);
        constexpr float Advance = .76F;
        const float units = 1 + static_cast<float>(count_ - 1) * Advance;
        const float height = std::max(0.0F, std::min(bounds_.height, bounds_.width / units));
        const float width = height * units;
        const float left = alignment_ == PhantomCore::TextAlignment::Center ? (bounds_.width - width) * .5F
                         : alignment_ == PhantomCore::TextAlignment::Trailing ? bounds_.width - width : 0;
        for (std::size_t i = 0; i < digits_.size(); ++i)
        {
            auto &node = *digits_[i];
            node.SetVisible(i < count_);
            if (i >= count_)
                continue;
            node.SetBounds({left + static_cast<float>(i) * height * Advance,
                            (bounds_.height - height) * .5F, height, height});
            const auto digit = static_cast<unsigned>(characters_[i] - '0');
            node.GetComponent<PhantomCore::SpriteVisualComponent>()->SetUvTransform(
                {.2F, .5F}, {static_cast<float>(digit % 5) * .2F, static_cast<float>(digit / 5) * .5F});
        }
    }
    PhantomCore::Visual2DNode *root_{};
    std::array<PhantomCore::Visual2DNode *, 20> digits_{};
    std::array<char, 20> characters_{};
    std::size_t count_{};
    std::uint64_t value_{};
    bool hasValue_{};
    PhantomCore::Rect bounds_{};
    PhantomCore::TextAlignment alignment_{PhantomCore::TextAlignment::Trailing};
};
