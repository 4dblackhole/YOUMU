#include "OptionSettings.h"
#include "YOUMUAssets.h"
#include "Mode/PlayGameModeFactory.h"
#include <algorithm>

#include <Windows.h>

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace PhantomCore;

namespace
{
    OptionValues DefaultValues()
    {
        OptionValues values;
        switch (PRIMARYLANGID(GetUserDefaultUILanguage()))
        {
        case LANG_KOREAN: values.language = Language::Korean; break;
        case LANG_JAPANESE: values.language = Language::Japanese; break;
        default: values.language = Language::English; break;
        }
        return values;
    }
    std::string_view Trim(std::string_view value)
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos) return {};
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    std::wstring DecodeUtf8(const std::string_view value)
    {
        if (value.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0) throw std::runtime_error("Option.ini contains invalid UTF-8.");
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length);
        return result;
    }

    std::string EncodeUtf8(const std::wstring &value)
    {
        if (value.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0) throw std::runtime_error("The option value is invalid Unicode.");
        std::string result(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
        return result;
    }

    std::string_view OutputName(const AudioOutputBackend output)
    {

        switch (output)
        {
        case AudioOutputBackend::Automatic: return "Automatic";
        case AudioOutputBackend::Wasapi: return "WASAPI";
        case AudioOutputBackend::Asio: return "ASIO";
        default: throw std::runtime_error("Invalid audio output option.");
        }
    }

    std::string_view WindowModeName(const WindowMode mode)
    {
        switch (mode)
        {
        case WindowMode::Windowed: return "windowed";
        case WindowMode::Borderless: return "borderless";
        default: throw std::runtime_error("Invalid window mode option.");
        }
    }

    WindowMode ReadWindowMode(const std::string_view value)
    {
        for (const auto mode : {WindowMode::Windowed, WindowMode::Borderless})
            if (value == WindowModeName(mode))
                return mode;
        throw std::runtime_error("WindowMode must be windowed or borderless.");
    }

    std::string_view RenderRateName(const RenderRateMode mode)
    {
        switch (mode)
        {
        case RenderRateMode::Display: return "display";
        case RenderRateMode::DisplayDouble: return "double";
        case RenderRateMode::Unlimited: return "unlimited";
        default: throw std::runtime_error("Invalid render rate option.");
        }
    }

    RenderRateMode ReadRenderRate(const std::string_view value)
    {
        for (const auto mode : {RenderRateMode::Display, RenderRateMode::DisplayDouble, RenderRateMode::Unlimited})
            if (value == RenderRateName(mode))
                return mode;
        throw std::runtime_error("RenderRate must be display, double or unlimited.");
    }

    bool IsSingleLine(const std::string_view value)
    {
        return value.size() <= 4096 && value.find_first_of("\r\n\0", 0, 3) ==
            std::string_view::npos;
    }

    template <class Integer> Integer ReadInteger(std::string_view value)
    {
        if (value.starts_with('+'))
            value.remove_prefix(1);
        Integer number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw std::runtime_error("The option must be an integer within its valid range.");
        return number;
    }

    bool IsIdentifierCharacter(char c)
    {
        const bool lower = c >= 'a' && c <= 'z';
        const bool upper = c >= 'A' && c <= 'Z';
        const bool letter = lower || upper;
        const bool digit = c >= '0' && c <= '9';
        const bool punctuation = c == '_' || c == '-' || c == '.';
        return letter || digit || punctuation;
    }

    bool IsIdentifier(std::string_view value)
    {
        return !value.empty() && value.size() <= 128 && std::ranges::all_of(value, IsIdentifierCharacter);
    }

    bool IsSupportedLanguage(const Language language)
    {
        switch (language)
        {
        case Language::Korean:
        case Language::English:
        case Language::Japanese:
            return true;
        default:
            return false;
        }
    }

    bool IsValidWindowExtent(const std::uint32_t extent)
    {
        return extent != 0 && extent <= 16384;
    }

    // Layouts that allow unassigned keys store those slots, the primary included, as 0.
    bool IsInvalidSavedKey(const std::uint16_t key, const bool allowUnassigned)
    {
        if (key == 0 && allowUnassigned)
            return false;
        return key < 8 || key > 254;
    }

    [[nodiscard]] bool UsesKey(const ModeInputLayout &layout, const std::uint16_t key)
    {
        return std::ranges::any_of(layout.bindings, [key](const ModeInputBinding &binding) {
            return std::ranges::find(binding.keys, key) != binding.keys.end();
        });
    }

    void Validate(const OptionValues &values)
    {
        if (!IsSupportedLanguage(values.language))
            throw std::runtime_error("Invalid language option.");
        const bool validMiddleware = IsSingleLine(values.audioMiddleware) && !values.audioMiddleware.empty();
        const bool validDriver = values.driverIndex >= -1 && IsSingleLine(values.driverName);
        if (!validMiddleware || !validDriver)
            throw std::runtime_error("Invalid audio option.");
        const auto skin = EncodeUtf8(values.skinSet);
        if (!IsSingleLine(EncodeUtf8(values.songsPath.wstring())))
            throw std::runtime_error("Songs path must fit a single UTF-8 INI line.");
        if (!values.songsPath.is_absolute() && values.songsPath.has_root_path())
            throw std::runtime_error("Songs path must be fully absolute or relative to the executable directory.");
        const bool singleLineSkin = !skin.empty() && IsSingleLine(skin);
        const bool dotSkin = values.skinSet == L"." || values.skinSet == L"..";
        if (!singleLineSkin || dotSkin || values.skinSet.find_first_of(L"/\\") != std::wstring::npos)
            throw std::runtime_error("The skin option must name one folder.");
        if (values.audioMiddleware != "FMOD")
            throw std::runtime_error("Only the FMOD audio middleware is installed.");
        if (!values.volumes.IsValid())
            throw std::runtime_error("Volumes must be whole percentages within 0..100.");
        if (!AudioVolumes::IsValidPercent(values.keyBeamOpacity))
            throw std::runtime_error("Key beam opacity must be a whole percentage within 0..100.");
        if (!AudioVolumes::IsValidPercent(values.backgroundBrightness))
            throw std::runtime_error("Background brightness must be a whole percentage within 0..100.");
        static_cast<void>(OutputName(values.audioOutput));
        static_cast<void>(RenderRateName(values.renderRate));
        static_cast<void>(WindowModeName(values.windowMode));
        if (!IsValidWindowExtent(values.windowWidth) || !IsValidWindowExtent(values.windowHeight))
            throw std::runtime_error("Window dimensions must be within 1..16384 pixels.");
        const auto layouts = AvailableInputLayouts();
        for (const auto &[mode, bindings] : values.modeKeys)
        {
            if (!IsIdentifier(mode))
                throw std::runtime_error("Invalid mode identifier.");
            const auto layout = std::ranges::find(layouts, mode, &ModeInputLayout::modeId);
            const bool allowUnassigned = layout != layouts.end() && layout->allowUnassignedKeys;
            for (const auto &[id, keys] : bindings)
            {
                if (!IsIdentifier(id) || keys.empty() ||
                    std::ranges::any_of(keys, [allowUnassigned](const std::uint16_t key) {
                        return IsInvalidSavedKey(key, allowUnassigned);
                    }))
                    throw std::runtime_error("Invalid mode input keys.");
            }
        }
        // The restart key is held during play, so no play key of any installed mode may share it.
        if (IsReservedGameControlKey(values.restartKey))
            throw std::runtime_error("The restart key is reserved for another game control.");
        for (auto layout : layouts)
        {
            const auto saved = values.modeKeys.find(layout.modeId);
            const auto resolved = ResolveInputLayout(
                std::move(layout), saved == values.modeKeys.end() ? InputKeyOverrides{} : saved->second);
            if (UsesKey(resolved, values.restartKey))
                throw std::runtime_error("The restart key cannot also be a play key.");
        }
    }

    void ReadValue(OptionValues &values, const std::string_view section,
                   const std::string_view key, const std::string_view value)
    {
        if (section == "General")
        {
            if (key == "Language")
            {
                if (value == "ko") values.language = Language::Korean;
                else if (value == "en") values.language = Language::English;
                else if (value == "ja") values.language = Language::Japanese;
                else throw std::runtime_error("Language must be ko, en or ja.");
            }
            else if (key == "SkinSet") values.skinSet = DecodeUtf8(value);
            else if (key == "GlobalOffsetMs")
                values.globalOffsetMs = ReadInteger<std::int32_t>(value);
            else if (key == "SongsPath")
                values.songsPath = std::filesystem::path(DecodeUtf8(value));
        }
        else if (section == "Display")
        {
            if (key == "Width")
                values.windowWidth = ReadInteger<std::uint32_t>(value);
            else if (key == "Height")
                values.windowHeight = ReadInteger<std::uint32_t>(value);
            else if (key == "Notation")
            {
                if (value == "first") values.notation = TextNotation::First;
                else if (value == "romanized") values.notation = TextNotation::Romanized;
                else throw std::runtime_error("Notation must be first or romanized.");
            }
            else if (key == "KeyBeamOpacity")
                values.keyBeamOpacity = ReadInteger<int>(value);
            else if (key == "BackgroundBrightness")
                values.backgroundBrightness = ReadInteger<int>(value);
            else if (key == "RenderRate")
                values.renderRate = ReadRenderRate(value);
            else if (key == "WindowMode")
                values.windowMode = ReadWindowMode(value);
        }
        else if (section == "Controls")
        {
            if (key == "RestartKey")
                values.restartKey = ReadInteger<std::uint16_t>(value);
        }
        else if (section.starts_with("Keys."))
        {
            std::vector<std::uint16_t> keys;
            auto remaining = value;
            for (;;)
            {
                const auto comma = remaining.find(',');
                keys.push_back(ReadInteger<std::uint16_t>(Trim(remaining.substr(0, comma))));
                if (comma == std::string_view::npos)
                    break;
                remaining.remove_prefix(comma + 1);
            }
            values.modeKeys[std::string(section.substr(5))][std::string(key)] = std::move(keys);
        }
        else if (section == "Audio")
        {
            if (key == "Middleware") values.audioMiddleware = value;
            else if (key == "Output")
            {

                if (value == "Automatic") values.audioOutput = AudioOutputBackend::Automatic;
                else if (value == "WASAPI") values.audioOutput = AudioOutputBackend::Wasapi;
                else if (value == "ASIO") values.audioOutput = AudioOutputBackend::Asio;
                else throw std::runtime_error("Output must be Automatic, WASAPI or ASIO.");
            }
            else if (key == "DriverIndex")
            {
                const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                                    values.driverIndex);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                    throw std::runtime_error("DriverIndex must be an integer.");
            }
            else if (key == "DriverName") values.driverName = value;
            else if (key == "MasterVolume") values.volumes.master = ReadInteger<int>(value);
            else if (key == "MusicVolume") values.volumes.music = ReadInteger<int>(value);
            else if (key == "EffectsVolume") values.volumes.effects = ReadInteger<int>(value);
        }
    }

}

OptionSettings::OptionSettings(std::filesystem::path path) : path_(std::move(path)), values_(DefaultValues()) {}

std::filesystem::path OptionSettings::SongsRoot() const
{
    if (values_.songsPath.empty())
        return UserSongsRoot();
    return (values_.songsPath.is_absolute() ? values_.songsPath
                                            : AssetExecutableDirectory() / values_.songsPath)
        .lexically_normal();
}

std::filesystem::path OptionSettings::NextToExecutable()
{
    return AssetExecutableDirectory() / L"Option.ini";
}

bool OptionSettings::Load(std::string &error)
{
    error.clear();
    try
    {
        auto candidate = DefaultValues();
        if (!std::filesystem::exists(path_))
        {
            if (!Save(candidate, error))
                throw std::runtime_error(error);
            return true;
        }
        std::ifstream file(path_, std::ios::binary);
        if (!file) throw std::runtime_error("Unable to read Option.ini.");
        std::string section, line;
        bool firstLine = true;
        while (std::getline(file, line))
        {
            if (line.size() > 8192) throw std::runtime_error("Option.ini line is too long.");
            std::string_view text = line;
            if (firstLine && text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
            firstLine = false;
            text = Trim(text);
            if (text.empty() || text.front() == ';' || text.front() == '#') continue;
            if (text.front() == '[' && text.back() == ']')
            {
                section = Trim(text.substr(1, text.size() - 2));
                continue;
            }
            const auto separator = text.find('=');
            if (separator == std::string_view::npos)
                throw std::runtime_error("Option.ini entry is missing '='.");
            ReadValue(candidate, section, Trim(text.substr(0, separator)),
                      Trim(text.substr(separator + 1)));
        }
        if (!file.eof()) throw std::runtime_error("Unable to finish reading Option.ini.");
        Validate(candidate);
        values_ = std::move(candidate);
        loadWarning_.clear();
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        loadWarning_ = error;
        values_ = DefaultValues();
        ++revision_;
        return false;
    }
}

bool OptionSettings::Save(const OptionValues &values, std::string &error)
{
    error.clear();
    auto temporary = path_;
    temporary += L".tmp";
    try
    {
        Validate(values);
        // Allocate the publication candidate before replacing the persisted file.
        OptionValues candidate = values;
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        const auto language = values.language == Language::Korean ? "ko" : values.language == Language::Japanese ? "ja" : "en";
        file << "[General]\r\nLanguage=" << language
             << "\r\nSkinSet=" << EncodeUtf8(values.skinSet) << "\r\nGlobalOffsetMs=" << values.globalOffsetMs
             << "\r\nSongsPath=" << EncodeUtf8(values.songsPath.wstring())
             << "\r\n\r\n[Display]\r\nWidth=" << values.windowWidth << "\r\nHeight=" << values.windowHeight
             << "\r\nWindowMode=" << WindowModeName(values.windowMode)
             << "\r\nNotation=" << (values.notation == TextNotation::Romanized ? "romanized" : "first")
             << "\r\nKeyBeamOpacity=" << values.keyBeamOpacity
             << "\r\nBackgroundBrightness=" << values.backgroundBrightness
             << "\r\nRenderRate=" << RenderRateName(values.renderRate)
             << "\r\n\r\n[Audio]\r\nMiddleware=" << values.audioMiddleware
             << "\r\nOutput=" << OutputName(values.audioOutput) << "\r\nDriverIndex=" << values.driverIndex
             << "\r\nDriverName=" << values.driverName << "\r\nMasterVolume=" << values.volumes.master
             << "\r\nMusicVolume=" << values.volumes.music << "\r\nEffectsVolume=" << values.volumes.effects
             << "\r\n\r\n[Controls]\r\nRestartKey=" << values.restartKey << "\r\n";
        for (const auto &[mode, bindings] : values.modeKeys)
        {
            file << "\r\n[Keys." << mode << "]\r\n";
            for (const auto &[id, keys] : bindings)
            {
                file << id << '=';
                for (std::size_t i = 0; i < keys.size(); ++i)
                    file << (i ? "," : "") << keys[i];
                file << "\r\n";
            }
        }
        file.close();
        if (!file || !MoveFileExW(temporary.c_str(), path_.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to save Option.ini beside the executable.");
        values_ = std::move(candidate);
        loadWarning_.clear();
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = exception.what();
        return false;
    }
}
