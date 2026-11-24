#pragma once
#include <string_view>

// Faces for the engine's native text inputs (Win32 EDIT boxes, drawn by GDI rather than the game's renderer).
// GDI finds only installed fonts and the ones this process registered, so the bundled number face is registered for
// this process alone (never installed) the first time a number input asks for it, and removed when the process
// ends. If it cannot be registered the inputs still ask for it by name and Windows draws its substitute.
namespace native_input_fonts
{
    inline constexpr std::wstring_view Text = L"Segoe UI";
    // Barlow Semi Condensed Medium, the face of a number field's own text (ui_fonts::Number(500, false)).
    [[nodiscard]] std::wstring_view Number() noexcept;
} // namespace native_input_fonts
