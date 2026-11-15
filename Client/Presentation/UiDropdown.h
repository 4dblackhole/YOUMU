#pragma once

#include "PhantomCore.h"
#include "Presentation/UiScrollbar.h"

#include <string>
#include <vector>

class UiDropdownLink;

// A drop-down list (UI-02 common part). The box holds the value; the open list is drawn on the
// Canvas root above every panel so no parent clip cuts it. Items are 34 tall, eight and a half are
// visible before the list scrolls, the chosen item has a check and bold cobalt text and the pointed
// item a light cobalt fill with a left line. The list opens upward when there is no room below.
// Choosing an item queues a SelectionChanged action from the box, as the engine combo box did.
class UiDropdownComponent final : public PhantomCore::Visual2DComponent
{
  public:
    explicit UiDropdownComponent(std::vector<std::wstring> items = {});
    ~UiDropdownComponent() override;
    UiDropdownComponent(const UiDropdownComponent &) = delete;
    UiDropdownComponent &operator=(const UiDropdownComponent &) = delete;

    // Builds the box's value, arrow and underline; called once after the component is attached.
    void Attach();
    void SetItems(std::vector<std::wstring> items);
    [[nodiscard]] const std::vector<std::wstring> &Items() const noexcept { return items_; }
    [[nodiscard]] std::size_t SelectedIndex() const noexcept { return selected_; }
    void SetSelectedIndex(std::size_t index);
    [[nodiscard]] bool IsExpanded() const noexcept { return expanded_; }
    void Expand();
    void Collapse() noexcept;
    // The box's value text; items keep the list style. Defaults to 14px semibold ink.
    void SetValueStyle(float size, std::uint16_t weight, PhantomCore::Color colour);

    void Update(double elapsedSeconds) override;
    void OnPointerEvent(const PhantomCore::PointerEvent &event, std::vector<PhantomCore::Action> &actions) override;

  private:
    friend class UiDropdownLink;
    struct Row
    {
        PhantomCore::Visual2DNode *node{}, *bar{}, *checkShort{}, *checkLong{}, *label{};
    };
    void RefreshBox();
    void LayoutBox();
    void EnsurePopup();
    void BuildRows();
    void LayoutPopup();
    void RefreshRows();
    void ScrollBy(float delta);
    void Choose(std::size_t index, std::vector<PhantomCore::Action> &actions, std::int64_t timestamp);
    void SetActive(std::size_t index);
    void Detach() noexcept;

    std::vector<std::wstring> items_;
    std::size_t selected_{}, active_{};
    bool expanded_{}, rowsDirty_{true};
    float scroll_{}, rowHeight_{34}, viewportHeight_{}, contentHeight_{};
    PhantomCore::Size laidOutBox_{};
    // Box parts: the value and a chevron of two short bars (no image, so it needs no skin).
    PhantomCore::Visual2DNode *value_{}, *arrowLeft_{}, *arrowRight_{};
    PhantomCore::Color valueColour_{};
    // Open list on the Canvas root, kept and reused; the link tells either side when the other goes.
    PhantomCore::Visual2DNode *scrim_{}, *popup_{}, *popupLine_{}, *viewport_{}, *content_{};
    UiScrollbar scrollbar_;
    UiDropdownLink *link_{};
    std::vector<Row> rows_;
};

[[nodiscard]] PhantomCore::Visual2DNode &CreateDropdown(PhantomCore::Visual2DNode &parent, PhantomCore::Rect bounds,
                                                       std::vector<std::wstring> items, std::string name);
[[nodiscard]] UiDropdownComponent &RequireDropdown(PhantomCore::Visual2DNode &node);

// While a drop-down of this Canvas is open no key reaches the screen behind it; Esc and Tab close it.
// Returns true when a key was taken. Hosts call it before their own keyboard handling.
bool ProcessDropdownKeys(PhantomCore::Visual2DCanvas &canvas, const PhantomCore::InputState &input);
// Closes the Canvas's open drop-down, if any, without a choice (screen changes and hidden views).
void CollapseDropdowns(PhantomCore::Visual2DCanvas &canvas) noexcept;
