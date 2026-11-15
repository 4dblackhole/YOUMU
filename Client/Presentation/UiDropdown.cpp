#include "UiDropdown.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <Windows.h>

using namespace PhantomCore;

// Lives on the open list's full-canvas scrim and links it to its drop-down. Whichever goes first
// tells the other, so neither keeps a dangling pointer.
class UiDropdownLink final : public Visual2DComponent
{
  public:
    explicit UiDropdownLink(UiDropdownComponent *owner) noexcept : dropdown_(owner) {}
    ~UiDropdownLink() override
    {
        if (dropdown_ != nullptr)
            dropdown_->Detach();
    }
    UiDropdownLink(const UiDropdownLink &) = delete;
    UiDropdownLink &operator=(const UiDropdownLink &) = delete;

    void Row(const std::size_t index, const PointerEvent &event, std::vector<Action> &actions)
    {
        if (dropdown_ == nullptr)
            return;
        if (event.type == PointerEventType::Enter)
            dropdown_->SetActive(index);
        else if (event.type == PointerEventType::Wheel)
            dropdown_->ScrollBy(-event.wheelDelta * dropdown_->rowHeight_);
        else if (event.type == PointerEventType::Click)
            dropdown_->Choose(index, actions, event.timestampTicks);
    }
    void List(const PointerEvent &event)
    {
        if (dropdown_ != nullptr && event.type == PointerEventType::Wheel)
            dropdown_->ScrollBy(-event.wheelDelta * dropdown_->rowHeight_);
    }
    // A press or the wheel outside the list closes it without a choice.
    void Outside(const PointerEvent &event)
    {
        const bool closing = event.type == PointerEventType::Press || event.type == PointerEventType::Wheel;
        if (dropdown_ != nullptr && closing)
            dropdown_->Collapse();
    }
    [[nodiscard]] UiDropdownComponent *Dropdown() const noexcept { return dropdown_; }

  private:
    friend class UiDropdownComponent;
    UiDropdownComponent *dropdown_{};
};

namespace
{
    constexpr float RowHeight = 34, VisibleRows = 8.5F, ListPadding = 4, ListLine = 3, ListMaxWidth = 360;
    constexpr float RowTextLeft = 34, RowTextRight = 14, ValueLeft = 12, ArrowRoom = 30, ScreenMargin = 8;
    // The scrim covers any Canvas; the list is placed in its local space.
    constexpr float ScrimExtent = 20000, ScrimOrigin = -ScrimExtent * .5F;
    constexpr float QuarterTurn = std::numbers::pi_v<float> * .25F;
    constexpr Color Transparent{1, 1, 1, 0}, ActiveFill{0.121F, 0.298F, 0.6F, .09F};
    constexpr const wchar_t *EmptyText = L"No items";

    template <typename Component> Component &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (component == nullptr)
            throw std::logic_error("A drop-down node is missing a required component.");
        return *component;
    }
    void Paint(Visual2DNode &node, const Color colour)
    {
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        Require<SpriteVisualComponent>(node).SetStyle(style);
    }
    // A short bar turned about its centre: the chevron and the check are drawn from two of them.
    Visual2DNode &Bar(Visual2DNode &parent, std::string name, const Color colour)
    {
        auto &bar = CreateSprite(parent, {0, 0, 1, 1}, {}, std::move(name));
        Paint(bar, colour);
        bar.SetPivot({.5F, .5F});
        return bar;
    }
    void PlaceBar(Visual2DNode &bar, const Point centre, const float length, const float thickness, const float angle)
    {
        bar.SetSize({length, thickness});
        bar.SetPosition(centre);
        bar.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, angle);
    }
    float EstimateWidth(const std::wstring_view text, const float size) noexcept
    {
        float width = 0;
        for (const wchar_t character : text)
            width += size * (character >= 0x1100 ? 1.0F : (character == L' ' ? 0.3F : 0.58F));
        return width;
    }
    void StyleText(TextVisualComponent &text, const std::wstring &value, const float size, const std::uint16_t weight,
                   const Color colour)
    {
        if (text.Text() != value)
            text.SetText(value);
        text.SetFont(ui_fonts::Text(value));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(colour);
    }
} // namespace

UiDropdownComponent::UiDropdownComponent(std::vector<std::wstring> items)
    : items_(std::move(items)), valueColour_(ui_colors::theme::Ink)
{
}

UiDropdownComponent::~UiDropdownComponent()
{
    if (link_ == nullptr)
        return;
    link_->dropdown_ = nullptr;
    // The list stays on the Canvas root until the Canvas goes; it must not stay open without its box.
    scrim_->SetVisible(false);
}

void UiDropdownComponent::Detach() noexcept
{
    scrim_ = popup_ = popupLine_ = viewport_ = content_ = nullptr;
    scrollbar_.Detach();
    link_ = nullptr;
    rows_.clear();
    rowsDirty_ = true;
    expanded_ = false;
}

void UiDropdownComponent::Attach()
{
    auto &box = Owner();
    value_ = &CreateLabel(box, {}, L"", "Value");
    auto &value = Require<TextVisualComponent>(*value_);
    value.SetHorizontalAlignment(TextAlignment::Leading);
    value.SetFontSize(14);
    value.SetFontWeight(600);
    arrowLeft_ = &Bar(box, "ArrowLeft", ui_colors::theme::Caption);
    arrowRight_ = &Bar(box, "ArrowRight", ui_colors::theme::Caption);
    LayoutBox();
    RefreshBox();
}

void UiDropdownComponent::SetValueStyle(const float size, const std::uint16_t weight, const Color colour)
{
    valueColour_ = colour;
    auto &text = Require<TextVisualComponent>(*value_);
    text.SetFontSize(size);
    text.SetFontWeight(weight);
    RefreshBox();
}

void UiDropdownComponent::SetItems(std::vector<std::wstring> items)
{
    items_ = std::move(items);
    if (selected_ >= items_.size())
        selected_ = 0;
    rowsDirty_ = true;
    if (expanded_)
        Collapse();
    if (value_ != nullptr)
        RefreshBox();
}

void UiDropdownComponent::SetSelectedIndex(const std::size_t index)
{
    if (index >= items_.size())
        throw std::out_of_range("Drop-down selection is outside its items.");
    selected_ = index;
    if (value_ != nullptr)
        RefreshBox();
    if (expanded_)
        RefreshRows();
}

void UiDropdownComponent::RefreshBox()
{
    auto &text = Require<TextVisualComponent>(*value_);
    const bool empty = items_.empty();
    const std::wstring value = empty ? std::wstring(EmptyText) : items_[selected_];
    if (text.Text() != value)
        text.SetText(value);
    text.SetFont(ui_fonts::Text(value));
    const bool muted = empty || !Owner().IsEnabled();
    text.SetTextColor(muted ? Color{0.294F, 0.380F, 0.518F, .6F} : valueColour_);
    const auto arrow = expanded_ ? ui_colors::theme::Brand : ui_colors::theme::Caption;
    Paint(*arrowLeft_, arrow);
    Paint(*arrowRight_, arrow);
}

void UiDropdownComponent::LayoutBox()
{
    const auto size = Owner().NodeSize();
    laidOutBox_ = size;
    const float width = std::max(size.width - ValueLeft - ArrowRoom, 1.0F);
    value_->SetBounds({ValueLeft, 0, width, size.height});
    value_->SetClipRect({0, 0, width, size.height});
    // A "v" when closed, flipped when the list is open.
    const Point centre{size.width - 17.0F, size.height * .5F};
    const float rise = expanded_ ? -1.0F : 1.0F;
    PlaceBar(*arrowLeft_, {centre.x - 2.5F, centre.y + .5F * rise}, 7.5F, 2.0F, -QuarterTurn * rise);
    PlaceBar(*arrowRight_, {centre.x + 2.5F, centre.y + .5F * rise}, 7.5F, 2.0F, QuarterTurn * rise);
}

void UiDropdownComponent::Update(const double)
{
    if (value_ == nullptr)
        return;
    const auto size = Owner().NodeSize();
    if (size.width != laidOutBox_.width || size.height != laidOutBox_.height)
        LayoutBox();
    if (expanded_)
        LayoutPopup();
}

void UiDropdownComponent::OnPointerEvent(const PointerEvent &event, std::vector<Action> &)
{
    // While open the scrim covers the box, so a click here always opens the list.
    if (event.type == PointerEventType::Click && !expanded_)
        Expand();
}

void UiDropdownComponent::Expand()
{
    if (value_ == nullptr || !Owner().IsEnabled())
        return;
    EnsurePopup();
    if (rowsDirty_)
        BuildRows();
    expanded_ = true;
    active_ = selected_;
    scroll_ = std::max(0.0F, static_cast<float>(selected_) * rowHeight_ - rowHeight_ * 3.0F);
    scrim_->SetVisible(true);
    LayoutBox();
    RefreshBox();
    LayoutPopup();
    RefreshRows();
}

void UiDropdownComponent::Collapse() noexcept
{
    if (!expanded_)
        return;
    expanded_ = false;
    if (scrim_ != nullptr)
        scrim_->SetVisible(false);
    if (value_ != nullptr)
    {
        LayoutBox();
        RefreshBox();
    }
}

void UiDropdownComponent::EnsurePopup()
{
    if (scrim_ != nullptr)
        return;
    auto *root = &Owner();
    while (root->Parent() != nullptr)
        root = root->Parent();
    scrim_ = &CreatePanel(*root, {ScrimOrigin, ScrimOrigin, ScrimExtent, ScrimExtent}, "Dropdown.Scrim");
    scrim_->SetZIndex(260);
    Paint(*scrim_, Transparent);
    link_ = &scrim_->AddComponent<UiDropdownLink>(this);
    scrim_->AddComponent<RectangleCollider2DComponent>();
    auto *link = link_;
    scrim_->AddComponent<PointerReceiverComponent>(
        [link](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) { link->Outside(event); });

    popup_ = &CreatePanel(*scrim_, {}, "Dropdown.List");
    Paint(*popup_, ui_colors::White);
    popup_->AddComponent<RectangleCollider2DComponent>();
    popup_->AddComponent<PointerReceiverComponent>(
        [link](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) { link->List(event); });
    popupLine_ = &CreateSprite(*popup_, {}, {}, "Line");
    Paint(*popupLine_, ui_colors::theme::Brand);
    viewport_ = &popup_->CreateChild("Viewport");
    content_ = &viewport_->CreateChild("Content");
    scrollbar_.Attach(*popup_, "Scrollbar");
    scrollbar_.SetZIndex(2);
    scrollbar_.FollowPointerEvents([this](const float offset) {
        scroll_ = offset;
        LayoutPopup();
    }, [this](const float wheel) { ScrollBy(-wheel * rowHeight_); });
    scrim_->SetVisible(false);
}

void UiDropdownComponent::BuildRows()
{
    const std::size_t count = std::max<std::size_t>(items_.size(), 1);
    while (rows_.size() < count)
    {
        const std::size_t index = rows_.size();
        Row row;
        row.node = &CreatePanel(*content_, {}, "Dropdown.Item");
        Paint(*row.node, Transparent);
        row.node->AddComponent<RectangleCollider2DComponent>();
        auto *link = link_;
        row.node->AddComponent<PointerReceiverComponent>(
            [link, index](Visual2DNode &, const PointerEvent &event, std::vector<Action> &actions) {
                link->Row(index, event, actions);
            });
        row.bar = &CreateSprite(*row.node, {}, {}, "Bar");
        Paint(*row.bar, ui_colors::theme::Brand);
        row.checkShort = &Bar(*row.node, "Check", ui_colors::theme::Brand);
        row.checkLong = &Bar(*row.node, "Check", ui_colors::theme::Brand);
        row.label = &CreateLabel(*row.node, {}, L"", "Label");
        rows_.push_back(row);
    }
    for (std::size_t i = 0; i < rows_.size(); ++i)
        rows_[i].node->SetVisible(i < count);
    rowsDirty_ = false;
}

void UiDropdownComponent::LayoutPopup()
{
    if (popup_ == nullptr)
        return;
    // List metrics follow the box's on-screen scale (song select shrinks on narrow screens).
    const auto box = Owner().BoundsInCanvas();
    const float boxHeight = Owner().NodeSize().height;
    const float scale = boxHeight > 0 ? box.height / boxHeight : 1.0F;
    rowHeight_ = RowHeight * scale;
    const std::size_t count = std::max<std::size_t>(items_.size(), 1);
    const float visible = std::min(static_cast<float>(count), VisibleRows);
    viewportHeight_ = visible * rowHeight_;
    contentHeight_ = static_cast<float>(count) * rowHeight_;
    const float listHeight = viewportHeight_ + ListPadding * 2 * scale;
    float widest = box.width;
    // A list that scrolls keeps its rows clear of the scrollbar along the right edge.
    const bool scrolls = contentHeight_ > viewportHeight_;
    const float scrollbarWidth = scrolls ? UiScrollbar::GrabWidth * scale : 0.0F;
    for (const auto &item : items_)
        widest = std::max(widest, (EstimateWidth(item, 14) + RowTextLeft + RowTextRight) * scale + scrollbarWidth);
    const float width = std::min(widest, std::max(ListMaxWidth * scale, box.width));
    const float rowWidth = std::max(width - scrollbarWidth, 1.0F);

    // Under the box unless that leaves the screen; then above it with the line at the bottom.
    auto *root = scrim_->Parent();
    const auto screen = root != nullptr ? root->NodeSize() : Size{1280, 720};
    float bottom = box.y - listHeight;
    const bool above = bottom < -screen.height * .5F + ScreenMargin;
    if (above)
        bottom = box.y + box.height;
    const float left = std::clamp(box.x, -screen.width * .5F + ScreenMargin,
                                  std::max(screen.width * .5F - width - ScreenMargin, -screen.width * .5F + ScreenMargin));
    popup_->SetBounds({left - ScrimOrigin, bottom - ScrimOrigin, width, listHeight});
    popupLine_->SetBounds({0, above ? 0.0F : listHeight - ListLine * scale, width, ListLine * scale});
    viewport_->SetBounds({0, ListPadding * scale, width, viewportHeight_});
    viewport_->SetClipRect({0, 0, width, viewportHeight_});
    scroll_ = std::clamp(scroll_, 0.0F, std::max(contentHeight_ - viewportHeight_, 0.0F));
    content_->SetBounds({0, viewportHeight_ - contentHeight_ + scroll_, rowWidth, contentHeight_});
    scrollbar_.SetRail({width - scrollbarWidth, ListPadding * scale, UiScrollbar::GrabWidth * scale, viewportHeight_});
    scrollbar_.SetRange(viewportHeight_, contentHeight_, scroll_);
    for (std::size_t i = 0; i < count; ++i)
    {
        auto &row = rows_[i];
        const float top = contentHeight_ - static_cast<float>(i + 1) * rowHeight_;
        row.node->SetBounds({0, top, rowWidth, rowHeight_});
        row.bar->SetBounds({0, 0, 3 * scale, rowHeight_});
        row.label->SetBounds({RowTextLeft * scale, 0, std::max(rowWidth - (RowTextLeft + RowTextRight) * scale, 1.0F),
                              rowHeight_});
        // A check: a short stroke down to the corner, then a long one up to the right.
        const Point corner{16 * scale, rowHeight_ * .5F - 3 * scale};
        PlaceBar(*row.checkShort, {corner.x - 2 * scale, corner.y + 2 * scale}, 6 * scale, 2 * scale, -QuarterTurn);
        PlaceBar(*row.checkLong, {corner.x + 3.5F * scale, corner.y + 3.5F * scale}, 11 * scale, 2 * scale, QuarterTurn);
    }
}

void UiDropdownComponent::RefreshRows()
{
    if (popup_ == nullptr)
        return;
    const float scale = rowHeight_ / RowHeight;
    if (items_.empty())
    {
        auto &row = rows_.front();
        StyleText(Require<TextVisualComponent>(*row.label), EmptyText, 14 * scale, 600,
                  {0.294F, 0.380F, 0.518F, 1});
        row.bar->SetVisible(false);
        row.checkShort->SetVisible(false);
        row.checkLong->SetVisible(false);
        Paint(*row.node, Transparent);
        return;
    }
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        auto &row = rows_[i];
        const bool chosen = i == selected_, pointed = i == active_;
        StyleText(Require<TextVisualComponent>(*row.label), items_[i], 14 * scale, chosen ? 800 : 600,
                  chosen ? ui_colors::theme::Brand : ui_colors::theme::Ink);
        row.checkShort->SetVisible(chosen);
        row.checkLong->SetVisible(chosen);
        row.bar->SetVisible(pointed);
        Paint(*row.node, pointed ? ActiveFill : Transparent);
    }
}

void UiDropdownComponent::ScrollBy(const float delta)
{
    scroll_ = std::clamp(scroll_ + delta, 0.0F, std::max(contentHeight_ - viewportHeight_, 0.0F));
    LayoutPopup();
}

void UiDropdownComponent::SetActive(const std::size_t index)
{
    if (index >= items_.size() || index == active_)
        return;
    active_ = index;
    RefreshRows();
}

void UiDropdownComponent::Choose(const std::size_t index, std::vector<Action> &actions, const std::int64_t timestamp)
{
    if (index >= items_.size())
        return;
    selected_ = index;
    Collapse();
    actions.push_back(Action{ActionType::SelectionChanged, Owner().Id(), static_cast<float>(index), index, timestamp});
}

Visual2DNode &CreateDropdown(Visual2DNode &parent, const Rect bounds, std::vector<std::wstring> items, std::string name)
{
    auto &node = CreatePanel(parent, bounds, std::move(name));
    node.AddComponent<RectangleCollider2DComponent>();
    node.AddComponent<PointerReceiverComponent>();
    node.AddComponent<UiDropdownComponent>(std::move(items)).Attach();
    return node;
}

UiDropdownComponent &RequireDropdown(Visual2DNode &node)
{
    return Require<UiDropdownComponent>(node);
}

namespace
{
    UiDropdownComponent *OpenDropdown(Visual2DCanvas &canvas) noexcept
    {
        for (const auto &child : canvas.Root().Children())
        {
            if (child->Name() != "Dropdown.Scrim" || !child->IsVisible())
                continue;
            if (const auto *link = child->GetComponent<UiDropdownLink>())
                return link->Dropdown();
        }
        return nullptr;
    }
} // namespace

void CollapseDropdowns(Visual2DCanvas &canvas) noexcept
{
    if (auto *open = OpenDropdown(canvas))
        open->Collapse();
}

bool ProcessDropdownKeys(Visual2DCanvas &canvas, const InputState &input)
{
    auto *open = OpenDropdown(canvas);
    if (open == nullptr || !open->IsExpanded())
        return false;
    bool taken = false;
    for (const auto &event : input.Events())
    {
        if (event.type != InputEventType::KeyPressed)
            continue;
        taken = true;
        if (event.code == VK_ESCAPE || event.code == VK_TAB)
            open->Collapse();
    }
    return taken;
}
