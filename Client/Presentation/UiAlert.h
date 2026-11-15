#pragma once

#include "PhantomCore.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// Severity of an alert: the 4px top band and the icon take its colour.
enum class AlertKind
{
    Error,
    Warning,
    Info,
};

struct AlertButton
{
    std::wstring label;
    bool primary{}; // Cobalt slanted button at the right end, chosen by Enter.
    bool left{};    // Irreversible actions such as Discard sit apart at the left end.
    bool cancel{};  // Chosen by Esc.
};

// Title and button captions are English; the message is in the selected language and the detail is
// the raw diagnosis (path, error code) shown in a monospaced box. No buttons means a single OK.
struct AlertContent
{
    AlertKind kind{AlertKind::Error};
    std::wstring title, message, detail;
    std::vector<AlertButton> buttons;
};

// The shared alert dialog (UI-02 common part), drawn on its host's Canvas above everything else.
// A 480 wide panel over a navy scrim; a press outside it does nothing and reaches nothing below.
// The host routes pointer input to its Canvas as usual and, while IsOpen(), calls Update instead of
// handling its own input. Ctrl+C copies the detail (or the message) because engine text cannot be
// selected.
class UiAlert final
{
  public:
    static constexpr float Width = 480;

    // visuals may be null (tests); the icon is then left out.
    void Initialize(PhantomCore::Visual2DCanvas &canvas, PhantomCore::ScreenVisual2DManager *visuals);
    void Shutdown() noexcept;
    void Show(AlertContent content);
    void Hide() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] const AlertContent &Content() const noexcept
    {
        return content_;
    }
    // Returns the chosen button and closes the alert: a click, Enter for the primary button, Esc for
    // the cancel button (or the only one). Keys count from the frame after Show.
    [[nodiscard]] std::optional<std::size_t> Update(const PhantomCore::InputState &input);
    [[nodiscard]] PhantomCore::Visual2DNode *ButtonNode(std::size_t index) const noexcept;

  private:
    struct Button
    {
        PhantomCore::Visual2DNode *node{}, *fill{}, *line{}, *label{};
        std::array<PhantomCore::Visual2DNode *, 2> caps{}, capFills{};
    };
    void EnsureButtons(std::size_t count);
    void Pointer(std::size_t index, const PhantomCore::PointerEvent &event);
    void ScrollDetail(float wheel);
    void PaintButton(std::size_t index);
    [[nodiscard]] std::vector<float> Measurements() const;
    void Layout();
    [[nodiscard]] float TextHeight(PhantomCore::Visual2DNode &node, float estimate) const;
    [[nodiscard]] float TextWidth(PhantomCore::Visual2DNode &node, float estimate) const;
    void Choose(std::size_t index) noexcept;

    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::ScreenVisual2DManager *visuals_{};
    PhantomCore::Visual2DNode *scrim_{}, *panel_{}, *band_{}, *icon_{}, *title_{}, *message_{};
    PhantomCore::Visual2DNode *detailBox_{}, *detailLine_{}, *detailView_{}, *detail_{}, *rule_{};
    std::vector<Button> buttons_;
    AlertContent content_;
    std::optional<std::size_t> chosen_;
    std::optional<std::size_t> hovered_, pressed_;
    float detailScroll_{}, detailOverflow_{};
    // Heights and widths the last layout used, so measured text can correct it once.
    std::vector<float> laidOut_;
    bool keysReady_{};
};
