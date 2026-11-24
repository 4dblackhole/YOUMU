#include "Presentation/NativeInputFonts.h"
#include "Presentation/UiFonts.h"

#include <Windows.h>

#include <string>
#include <utility>

namespace
{
    // A font file registered for this process only (FR_PRIVATE) while the object lives.
    class PrivateFontFile final
    {
      public:
        explicit PrivateFontFile(std::wstring path) noexcept : path_(std::move(path))
        {
            registered_ = !path_.empty() && AddFontResourceExW(path_.c_str(), FR_PRIVATE, nullptr) > 0;
        }
        ~PrivateFontFile()
        {
            if (registered_)
                RemoveFontResourceExW(path_.c_str(), FR_PRIVATE, nullptr);
        }
        PrivateFontFile(const PrivateFontFile &) = delete;
        PrivateFontFile &operator=(const PrivateFontFile &) = delete;

      private:
        std::wstring path_;
        bool registered_{};
    };

    // The same file ui_fonts resolves once for the game's own number text.
    [[nodiscard]] std::wstring NumberFontFile() noexcept
    {
        try
        {
            return ui_fonts::faces::NumberMedium().value;
        }
        catch (...)
        {
            return {};
        }
    }
} // namespace

std::wstring_view native_input_fonts::Number() noexcept
{
    // Registered on the first number input; removed in static destruction at exit, after the Client and its inputs.
    [[maybe_unused]] static const PrivateFontFile font(NumberFontFile());
    return L"Barlow Semi Condensed Medium";
}
