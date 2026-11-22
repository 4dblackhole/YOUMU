#pragma once

#include "App/OptionSettings.h"
#include "App/SkinSetSelection.h"
#include "PhantomCore.h"
#include "Presentation/OptionsPanel/Submodules/OptionsCategory.h"
#include "Presentation/UiTooltip.h"
#include "Presentation/UiScrollbar.h"
#include "Texts/TextCatalog.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

// The left options drawer (UI-02): a category rail and one scrolling body in which the General, Display,
// Audio and Input sections follow each other. Every choice applies and saves at once.
//
// The panel owns the drawer: its frame, the rail, section layout and scrolling, the slide animation, input
// distribution, saving and the footer notice. Each section is a category class (OptionsGeneral,
// OptionsDisplay, OptionsAudio, OptionsInput under OptionsPanel/Submodules) that creates, refreshes and
// applies its own options through OptionsContext, which the panel implements.
class OptionsPanel final : private OptionsContext
{
  public:
    OptionsPanel(TextCatalog &texts, OptionSettings &options, PhantomCore::ScreenVisual2DManager *visuals = nullptr);
    ~OptionsPanel() override;
    OptionsPanel(const OptionsPanel &) = delete;
    OptionsPanel &operator=(const OptionsPanel &) = delete;

    void Initialize(PhantomCore::Visual2DCanvas &canvas, PhantomCore::AudioSystem &audio,
                    PhantomCore::IWindowControl *window = nullptr);
    void Shutdown() noexcept;
    void Toggle();
    void Update(double deltaSeconds);
    // True consumes a panel keyboard command, including the closing frame.
    bool ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                      PhantomCore::NodeId hoveredNode, PhantomCore::Visual2DInputRouter &inputRouter);
    bool HandleKey(std::uint16_t virtualKey, PhantomCore::Visual2DInputRouter &inputRouter);
    [[nodiscard]] bool ProcessAction(const PhantomCore::Action &action);
    void RefreshTexts() override;
    // A number being typed or a key being captured in any category.
    [[nodiscard]] bool OwnsKeyboard() const noexcept;
    [[nodiscard]] bool IsVisible() const noexcept;

  private:
    static constexpr std::size_t SectionCount = 4; // General, Display, Audio, Input.
    struct Section
    {
        PhantomCore::Visual2DNode *railButton{}, *railIcon{}, *railLabel{};
        PhantomCore::Visual2DNode *headerIcon{}, *headerLabel{}, *rule{};
        float top{}, height{};
    };

    // OptionsContext: what the categories use.
    [[nodiscard]] TextCatalog &Texts() const noexcept override { return texts_; }
    [[nodiscard]] OptionSettings &Settings() const noexcept override { return options_; }
    [[nodiscard]] PhantomCore::ScreenVisual2DManager *Visuals() const noexcept override { return visuals_; }
    [[nodiscard]] PhantomCore::AudioSystem &Audio() const noexcept override { return *audio_; }
    [[nodiscard]] PhantomCore::IWindowControl *Window() const noexcept override { return window_; }
    [[nodiscard]] PhantomCore::Visual2DCanvas &Canvas() const noexcept override { return *canvas_; }
    [[nodiscard]] PhantomCore::Visual2DNode &Content() const noexcept override { return *content_; }
    [[nodiscard]] float ContentHeight() const noexcept override { return contentHeight_; }
    [[nodiscard]] bool IsOpen() const noexcept override { return targetOpen_; }
    [[nodiscard]] float NextField(float controlHeight) override;
    [[nodiscard]] float NextInlineRows(std::size_t rows) override;
    bool Save(const OptionValues &candidate) override;
    void SetStatus(OptionsStatus status) noexcept override { status_ = status; }
    [[nodiscard]] OptionsStatus Status() const noexcept override { return status_; }
    [[nodiscard]] std::string &StatusDetail() noexcept override { return statusDetail_; }
    [[nodiscard]] std::wstring StatusText(bool &error) const override;
    void RefreshStatus() override;
    void CancelKeyboardEdit() override;
    void CollapseCombos() override;
    void InvalidateHitTest() noexcept override { hitTestDirty_ = true; }
    void SkinChanged() override;
    PhantomCore::Visual2DNode &CreateIcon(PhantomCore::Visual2DNode &parent, PhantomCore::Rect bounds, std::string key,
                                          PhantomCore::Color tint, std::string name) override;
    void SetIconKey(PhantomCore::Visual2DNode &icon, std::string key) override;
    void RefreshIcons() override;
    void BindPart(PhantomCore::Visual2DNode &node, std::string key) override;

    // Drawer frame, rail and section navigation (OptionsRail).
    void CreateFrame();
    void CreateRail();
    void CreateSections();
    void LayoutSections();
    void CreateSectionHeader(std::size_t index);
    void RefreshRail();
    void UpdateRailExpansion(double deltaSeconds);
    void ApplyRailWidth();
    void ScrollToSection(std::size_t index);
    void StepSection(int delta);
    void SpySection();
    void UpdateContentLayout();
    void UpdateScrollbar();
    [[nodiscard]] std::vector<PhantomCore::Visual2DNode *> Combos() const;
    // An open drop-down owns the wheel: its list scrolls (or closes) and the body stays put.
    [[nodiscard]] bool AnyComboExpanded() const;
    // The category whose modal is shown (the key settings), if any.
    [[nodiscard]] OptionsCategory *ModalCategory() const noexcept;
    bool CanProcessAction(PhantomCore::NodeId source) const noexcept;
    void ApplyIcon(PhantomCore::Visual2DNode &icon, const std::string &key);
    void LayoutFooter();

    void Close(PhantomCore::Visual2DInputRouter *inputRouter);
    void ApplyAnimationPosition();
    void ApplyScrollOffset(PhantomCore::Visual2DInputRouter &inputRouter);
    // Drags of the categories, then the scrollbar, the wheel and the body drag.
    bool ProcessPointer(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                        PhantomCore::NodeId hoveredNode, PhantomCore::Visual2DInputRouter &inputRouter, bool consumedKey);

    static constexpr float AnimationSeconds = 0.2F;
    static constexpr float RailExpandDelaySeconds = 0.3F;

    // Sections in order; each owns its controls.
    std::array<std::unique_ptr<OptionsCategory>, SectionCount> categories_;

    // Frame.
    PhantomCore::Visual2DNode *panel_{}, *rail_{}, *title_{}, *close_{}, *headerRule_{};
    PhantomCore::Visual2DNode *viewport_{}, *content_{};
    UiScrollbar scrollbar_;
    PhantomCore::Visual2DNode *footer_{}, *footerRule_{}, *statusBar_{}, *statusIcon_{}, *statusLabel_{};
    std::vector<std::pair<PhantomCore::Visual2DNode *, std::string>> icons_;
    std::array<Section, SectionCount> sections_{};
    std::size_t activeSection_{};
    std::optional<std::size_t> railSkinSection_;
    // A rail click keeps its section lit until the body is scrolled by hand.
    std::optional<std::size_t> heldSection_;
    float contentHeight_{}, viewportHeight_{}, footerHeight_{};
    float layoutCursor_{};
    float scrollOffset_{};
    bool railPointerInside_{}, railKeyboard_{}, railExpanded_{};
    float railHoverSeconds_{};
    // Layout changes outside ProcessInput invalidate the router's hit test on its next call.
    bool hitTestDirty_{};
    // Rail names and the close key; it runs in ProcessInput with the frame time Update saw.
    UiTooltip tooltip_;
    double frameSeconds_{};

    TextCatalog &texts_;
    OptionSettings &options_;
    PhantomCore::ScreenVisual2DManager *visuals_{};
    PhantomCore::AudioSystem *audio_{};
    PhantomCore::IWindowControl *window_{};
    PhantomCore::Visual2DCanvas *canvas_{};
    OptionsStatus status_{};
    std::string statusDetail_;
    PhantomCore::NodeId panelId_{};
    std::optional<PhantomCore::Point> previousDragPoint_;
    float animationProgress_{};
    bool targetOpen_{};
    bool dragging_{};
    std::uint64_t optionRevision_{};
};
