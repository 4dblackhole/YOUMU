#pragma once
#include "App/OptionSettings.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The footer notice's state, shared by every category: a failure replaces the category notices.
enum class OptionsStatus
{
    None,
    SaveFailed,
    AudioFailed,
    WindowFailed,
    InvalidInput
};

// What a category's pointer handling did with this frame: nothing (the panel goes on), a slider drag in
// progress (the panel stops and reports only its keyboard use), or a drag that consumed the frame.
enum class OptionsPointer
{
    Free,
    Sliding,
    Consumed
};

// What the options panel lends its categories: the shared services, the layout cursor of the body being
// built, saving and the footer notice, and the panel-wide edit rules (one keyboard edit, one open drop-down).
class OptionsContext
{
  public:
    virtual ~OptionsContext() = default;

    [[nodiscard]] virtual TextCatalog &Texts() const noexcept = 0;
    [[nodiscard]] virtual OptionSettings &Settings() const noexcept = 0;
    [[nodiscard]] virtual PhantomCore::ScreenVisual2DManager *Visuals() const noexcept = 0;
    [[nodiscard]] virtual PhantomCore::AudioSystem &Audio() const noexcept = 0;
    // Null in offscreen contexts (tests, captures): window options then show the saved values.
    [[nodiscard]] virtual PhantomCore::IWindowControl *Window() const noexcept = 0;
    [[nodiscard]] virtual PhantomCore::Visual2DCanvas &Canvas() const noexcept = 0;
    // The scrolling body the fields live in, and its height for top-left layout.
    [[nodiscard]] virtual PhantomCore::Visual2DNode &Content() const noexcept = 0;
    [[nodiscard]] virtual float ContentHeight() const noexcept = 0;
    [[nodiscard]] virtual bool IsOpen() const noexcept = 0;

    // While a category creates its fields: the top of the next captioned field or inline row block.
    [[nodiscard]] virtual float NextField(float controlHeight) = 0;
    [[nodiscard]] virtual float NextInlineRows(std::size_t rows) = 0;

    // Saves and records a failure in the footer; true when Option.ini was written.
    virtual bool Save(const OptionValues &candidate) = 0;
    virtual void SetStatus(OptionsStatus status) noexcept = 0;
    [[nodiscard]] virtual OptionsStatus Status() const noexcept = 0;
    // The detail under the notice; engine calls write their error into it.
    [[nodiscard]] virtual std::string &StatusDetail() noexcept = 0;
    [[nodiscard]] virtual std::wstring StatusText(bool &error) const = 0;
    virtual void RefreshStatus() = 0;
    virtual void RefreshTexts() = 0;

    virtual void CancelKeyboardEdit() = 0;
    virtual void CollapseCombos() = 0;
    // A layout change outside input handling: the router hit-tests again on its next call.
    virtual void InvalidateHitTest() noexcept = 0;
    // The skin set changed: icons and skin pictures of every category are loaded again.
    virtual void SkinChanged() = 0;

    // Icons come from the skin's Common/Icons; bounds are from the parent's top-left.
    virtual PhantomCore::Visual2DNode &CreateIcon(PhantomCore::Visual2DNode &parent, PhantomCore::Rect bounds,
                                                  std::string key, PhantomCore::Color tint, std::string name) = 0;
    virtual void SetIconKey(PhantomCore::Visual2DNode &icon, std::string key) = 0;
    virtual void RefreshIcons() = 0;
    virtual void BindPart(PhantomCore::Visual2DNode &node, std::string key) = 0;
};

// One section of the options body (General, Display, Audio, Input). It owns its controls from creation to
// shutdown, says how tall its fields are so the panel can lay the sections out, and applies and saves its own
// options. Adding an option changes only its category.
class OptionsCategory
{
  public:
    explicit OptionsCategory(OptionsContext &context) noexcept : context_(context) {}
    virtual ~OptionsCategory() = default;
    OptionsCategory(const OptionsCategory &) = delete;
    OptionsCategory &operator=(const OptionsCategory &) = delete;

    // Reads what the fields depend on (the Input category's key layouts) before the layout is measured.
    virtual void Load() {}
    // Field heights in creation order, each including its gap above; Create must take exactly these.
    [[nodiscard]] virtual std::vector<float> FieldHeights() const = 0;
    virtual void Create() = 0;
    // Captions and shown values, after a language, skin or saved value changed.
    virtual void Refresh() = 0;
    virtual void Shutdown() noexcept = 0;

    // Clicks and selections on its own controls; false leaves the action to the next category.
    [[nodiscard]] virtual bool ProcessAction(const PhantomCore::Action &action) = 0;
    // Drags on its own controls, asked in section order before the panel scrolls.
    [[nodiscard]] virtual OptionsPointer ProcessPointer(const PhantomCore::InputState &input,
                                                        std::optional<PhantomCore::Point> canvasPointer,
                                                        PhantomCore::NodeId hoveredNode);
    // A keyboard edit of this category (a number being typed, a key being captured).
    [[nodiscard]] virtual bool OwnsKeyboard() const noexcept { return false; }
    virtual void HandleKey(std::uint16_t virtualKey);
    virtual void CancelKeyboardEdit() {}
    virtual void AppendCombos(std::vector<PhantomCore::Visual2DNode *> &combos) const;

    // The panel opens or closes by toggling (every drag stops), or closes by Escape or its button.
    virtual void OnToggle() {}
    virtual void OnOpened() {}
    virtual void OnClosed() {}
    virtual void OnSkinChanged() {}
    // Views of the shared footer notice outside the footer (the key settings modal).
    virtual void OnStatusChanged() {}
    // A footer notice while nothing failed (the fallback output, an unavailable driver, a pending restart).
    [[nodiscard]] virtual std::wstring Notice() const { return {}; }

    // A centred modal of this category (the key settings) while it is shown: only its controls take actions.
    [[nodiscard]] virtual const PhantomCore::Visual2DNode *VisibleModal() const noexcept { return nullptr; }
    virtual void EscapeModal() {}
    virtual void UpdateModal(PhantomCore::NodeId hoveredNode);

  protected:
    // A caption with a white drop-down box under it and the 2px field line, at the next field's top.
    PhantomCore::Visual2DNode &CreateComboField(const std::string &name, PhantomCore::Visual2DNode *&label,
                                                std::vector<std::wstring> items = {});

    OptionsContext &context_;
};
