#pragma once
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include <array>
#include <cstdint>
#include <string>

// UI-02 flat modal (popups.css .modal) shared by Play options and the song and record filters. A navy scrim
// covers the canvas and takes every pointer; the centred light panel carries a 4px cobalt band, "✦ Title" at the
// top left, an optional ✕, the body, an optional error line, a rule and the footer: Reset at the left, Cancel
// and the primary action at the right. Body widgets are placed from the body's top-left in logical pixels.
// A screen narrower than 16:9 scales the whole modal like the song select board.
class UiModal final
{
  public:
    struct Setup
    {
        std::string prefix; // Node names: <prefix>.Overlay, .Panel, .Body, .Title, .Close, .Reset, .Cancel, .Apply.
        float width{560};
        bool close{true};
        bool cancel{true};
        PhantomCore::ScreenVisual2DManager *visuals{}; // Icons are skipped without it (tests).
    };
    static constexpr float Padding = 24, BodyTop = 81, StatusGap = 12, StatusHeight = 30;
    static constexpr float FooterGap = 16, FooterPadding = 16, ButtonHeight = 40, BottomPadding = 24;
    static constexpr float BodyBottomSpace = FooterGap + 1 + FooterPadding + ButtonHeight + BottomPadding;

    void Create(PhantomCore::Visual2DCanvas &canvas, Setup setup);
    void Forget() noexcept;
    void SetTexts(std::wstring_view title, std::wstring_view reset, std::wstring_view cancel, std::wstring_view primary);
    // An empty text hides the error line.
    void SetStatus(std::wstring_view text);
    // Sizes the panel for a body of this height, centres it on the canvas and scales it on narrow screens.
    void Layout(float bodyHeight);
    void SetVisible(bool visible) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    [[nodiscard]] PhantomCore::Visual2DNode &Body() const;
    [[nodiscard]] float BodyWidth() const noexcept { return width_ - 2 * Padding; }
    [[nodiscard]] float BodyHeight() const noexcept { return bodyHeight_; }
    [[nodiscard]] float Scale() const noexcept { return scale_; }
    // True when the node lies inside the visible panel (actions from the scrim or hidden rows are ignored).
    [[nodiscard]] bool Owns(PhantomCore::NodeId source) const noexcept;
    [[nodiscard]] bool IsClose(PhantomCore::NodeId id) const noexcept { return close_ && close_->Id() == id; }
    [[nodiscard]] bool IsReset(PhantomCore::NodeId id) const noexcept { return reset_ && reset_->Id() == id; }
    [[nodiscard]] bool IsCancel(PhantomCore::NodeId id) const noexcept { return cancel_ && cancel_->Id() == id; }
    [[nodiscard]] bool IsPrimary(PhantomCore::NodeId id) const noexcept { return primary_ && primary_->Id() == id; }

  private:
    void LayoutFooter(float panelHeight);
    void LayoutPrimary(float right, float top);
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *band_{}, *sparkle_{}, *title_{}, *close_{}, *body_{};
    PhantomCore::Visual2DNode *status_{}, *statusBar_{}, *statusIcon_{}, *statusText_{}, *rule_{};
    PhantomCore::Visual2DNode *reset_{}, *resetIcon_{}, *resetText_{}, *cancel_{}, *primary_{}, *primaryText_{};
    std::array<PhantomCore::Visual2DNode *, 2> primaryCaps_{}, primaryCapFills_{};
    float width_{560}, bodyHeight_{}, scale_{1};
    bool statusVisible_{};
};

// Widgets of the flat modals (flat.css): white fields with a 2px underline, 40x24 switches, underline tabs.
namespace ui_modal
{
    inline constexpr auto Ink = ui_colors::theme::Ink, Caption = ui_colors::theme::Caption;
    inline constexpr auto BrandText = ui_colors::theme::BrandText, Brand = ui_colors::theme::Brand;
    inline constexpr auto FieldLine = ui_colors::Rgb(0xB5CFF5), Rule = ui_colors::theme::Rule;

    // Visual2D places children from the parent's bottom-left; layouts here are written from the top-left.
    [[nodiscard]] constexpr PhantomCore::Rect TopLeft(const PhantomCore::Rect bounds, const float parentHeight) noexcept
    {
        return {bounds.x, parentHeight - bounds.y - bounds.height, bounds.width, bounds.height};
    }
    void Place(PhantomCore::Visual2DNode &node, PhantomCore::Rect topLeftBounds, float parentHeight);

    // Captions are English (Nunito); a localized string picks Noto Sans KR by itself.
    [[nodiscard]] PhantomCore::Visual2DNode &CreateText(PhantomCore::Visual2DNode &parent, std::string name, float size,
                                                        std::uint16_t weight, PhantomCore::Color color);
    void SetText(PhantomCore::Visual2DNode &node, std::wstring_view text);
    // Rough caption width for button sizing (Nunito, Latin).
    [[nodiscard]] float EstimateWidth(std::wstring_view text, float size) noexcept;

    // A text box: its own text shows the value when no native edit covers it. A number field draws Barlow Semi
    // Condensed (the game's number face), a text field Nunito or Noto Sans KR by its text.
    [[nodiscard]] PhantomCore::Visual2DNode &CreateField(PhantomCore::Visual2DNode &parent, std::string name, bool number);
    // A field's own text; a number field keeps its number face whatever the text is.
    void SetFieldText(PhantomCore::Visual2DNode &field, std::wstring_view text, bool number);
    void PlaceField(PhantomCore::Visual2DNode &field, PhantomCore::Rect topLeftBounds, float parentHeight);
    void StyleField(PhantomCore::Visual2DNode &field, bool focused, bool enabled);

    // A clickable 40x24 switch; the owner keeps its state and calls SetSwitch.
    [[nodiscard]] PhantomCore::Visual2DNode &CreateSwitch(PhantomCore::Visual2DNode &parent, std::string name);
    void SetSwitch(PhantomCore::Visual2DNode &node, bool on, bool enabled);

    // One item of an underline tab group; the selected one is cobalt with a 3px underline.
    [[nodiscard]] PhantomCore::Visual2DNode &CreateTab(PhantomCore::Visual2DNode &parent, std::string name,
                                                       std::wstring_view text, float size);
    void SetTabSelected(PhantomCore::Visual2DNode &node, bool selected);

    [[nodiscard]] PhantomCore::Visual2DNode &CreateRule(PhantomCore::Visual2DNode &parent, std::string name);
    // A skin line icon tinted to a caption colour; transparent when the icon or the skin is missing.
    [[nodiscard]] PhantomCore::Visual2DNode &CreateIcon(PhantomCore::Visual2DNode &parent, std::string name,
                                                        PhantomCore::ScreenVisual2DManager *visuals,
                                                        const wchar_t *key, PhantomCore::Color tint);

    // Puts a native edit over a field (canvas bounds include the modal scale) with the field's own face and size,
    // so the value looks the same whether the edit covers the field or is hidden (number: the registered Barlow
    // face, see NativeInputFonts.h).
    void PlaceTextInput(PhantomCore::ITextInput &input, const PhantomCore::Visual2DNode &field,
                        const PhantomCore::Visual2DCanvas &canvas, std::uint32_t windowHeight, float scale, bool number);
} // namespace ui_modal
