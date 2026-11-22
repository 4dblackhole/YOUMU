#include "Catalog/ChartHash.h"
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr std::size_t DigestBytes = 32;

    void Require(const NTSTATUS status)
    {
        if (!BCRYPT_SUCCESS(status))
            throw std::runtime_error("Cannot compute a chart hash.");
    }

    // Closes the CNG handles even when hashing throws.
    struct HashHandles
    {
        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        ~HashHandles()
        {
            if (hash) BCryptDestroyHash(hash);
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    };

    // One SHA-256 over bytes given in pieces.
    class Sha256 final
    {
      public:
        Sha256()
        {
            Require(BCryptOpenAlgorithmProvider(&handles_.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
            Require(BCryptCreateHash(handles_.algorithm, &handles_.hash, nullptr, 0, nullptr, 0, 0));
        }
        void Add(const std::string_view bytes)
        {
            constexpr std::size_t Chunk = 1U << 30;
            for (std::size_t offset = 0; offset < bytes.size(); offset += Chunk)
            {
                const auto count = static_cast<ULONG>((std::min)(Chunk, bytes.size() - offset));
                // CNG takes a mutable pointer but only reads the input.
                auto *data = reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data() + offset));
                Require(BCryptHashData(handles_.hash, data, count, 0));
            }
        }
        [[nodiscard]] std::string Finish()
        {
            std::array<UCHAR, DigestBytes> digest{};
            Require(BCryptFinishHash(handles_.hash, digest.data(), static_cast<ULONG>(digest.size()), 0));
            constexpr char Hex[] = "0123456789abcdef";
            std::string text;
            text.reserve(DigestBytes * 2);
            for (const auto byte : digest)
            {
                text.push_back(Hex[byte >> 4]);
                text.push_back(Hex[byte & 15]);
            }
            return text;
        }

      private:
        HashHandles handles_;
    };

    // Reads the file a megabyte at a time, so a long song never sits in memory whole.
    [[nodiscard]] std::string FileHash(const std::filesystem::path &path, const char *what)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error(std::string("Cannot open the ") + what + " file to hash it.");
        Sha256 hash;
        std::string buffer(1U << 20, '\0');
        while (file)
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            hash.Add(std::string_view(buffer.data(), static_cast<std::size_t>(file.gcount())));
        }
        if (file.bad())
            throw std::runtime_error(std::string("Cannot read the ") + what + " file to hash it.");
        return hash.Finish();
    }

    [[nodiscard]] bool IsHexDigit(const char ch) noexcept
    {
        return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
    }
} // namespace

std::string ChartBytesHash(const std::string_view bytes)
{
    Sha256 hash;
    hash.Add(bytes);
    return hash.Finish();
}

std::string ChartFileHash(const std::filesystem::path &path)
{
    return FileHash(path, "chart");
}

std::string AudioFileHash(const std::filesystem::path &path)
{
    return FileHash(path, "audio");
}

bool IsChartHash(const std::string_view text) noexcept
{
    return text.size() == DigestBytes * 2 && std::ranges::all_of(text, IsHexDigit);
}
