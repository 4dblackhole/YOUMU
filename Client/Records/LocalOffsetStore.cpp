#include "LocalOffsetStore.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cwctype>
#include <fstream>
#include <map>
#include <sstream>

namespace
{
    std::string PatternKey(const std::filesystem::path &pattern)
    {
        std::error_code error;
        auto normalized = std::filesystem::weakly_canonical(pattern, error);
        if (error)
            normalized = std::filesystem::absolute(pattern);
        auto wide = normalized.lexically_normal().generic_wstring();
        for (auto &ch : wide)
            ch = static_cast<wchar_t>(std::towlower(ch));
        const auto utf8 = std::filesystem::path(wide).generic_u8string();
        return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
    }

    std::map<std::string, std::int32_t> Read(const std::filesystem::path &file)
    {
        std::map<std::string, std::int32_t> values;
        std::ifstream stream(file, std::ios::binary);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto tab = line.find('\t');
            if (tab == std::string::npos || tab + 1 == line.size())
                continue;
            std::int32_t value{};
            const auto [end, result] = std::from_chars(line.data(), line.data() + tab, value);
            if (result != std::errc{} || end != line.data() + tab)
                continue; // A damaged line is skipped; the other patterns keep their offsets.
            values[line.substr(tab + 1)] = std::clamp(value, -LocalOffsetStore::Limit, LocalOffsetStore::Limit);
        }
        return values;
    }
} // namespace

std::int32_t LocalOffsetStore::Get(const std::filesystem::path &pattern) const
{
    if (file_.empty() || pattern.empty())
        return 0;
    const auto values = Read(file_);
    const auto found = values.find(PatternKey(pattern));
    return found == values.end() ? 0 : found->second;
}

bool LocalOffsetStore::Set(const std::filesystem::path &pattern, const std::int32_t milliseconds,
                           std::string &error) const
{
    error.clear();
    if (file_.empty() || pattern.empty())
    {
        error = "No local offset file or pattern.";
        return false;
    }
    auto values = Read(file_);
    const auto key = PatternKey(pattern);
    const auto clamped = std::clamp(milliseconds, -Limit, Limit);
    if (clamped == 0)
        values.erase(key);
    else
        values[key] = clamped;
    std::ostringstream text;
    for (const auto &[storedKey, value] : values)
        text << value << '\t' << storedKey << "\r\n";
    std::error_code status;
    std::filesystem::create_directories(file_.parent_path(), status);
    auto temporary = file_;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << text.str();
        stream.close();
        if (!stream)
        {
            error = "Cannot write the local offset file.";
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::filesystem::remove(temporary, status);
        error = "Cannot replace the local offset file.";
        return false;
    }
    return true;
}
