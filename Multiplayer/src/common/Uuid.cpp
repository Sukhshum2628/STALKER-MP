#include "stalkermp/common/Uuid.h"

#include <random>

namespace stalkermp::common
{
    namespace
    {
        [[nodiscard]] int HexValue(const char c) noexcept
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F')
            {
                return c - 'A' + 10;
            }
            return -1;
        }

        constexpr char kHexDigits[] = "0123456789abcdef";
    } // namespace

    Uuid Uuid::Generate()
    {
        // random_device is used per call: UUID generation is rare
        // (sessions, log correlation) and never on a hot path.
        std::random_device device;
        std::mt19937_64 engine(
            (static_cast<std::uint64_t>(device()) << 32) ^ device());
        std::uniform_int_distribution<std::uint64_t> distribution;

        Uuid uuid;
        const std::uint64_t high = distribution(engine);
        const std::uint64_t low = distribution(engine);
        for (std::size_t i = 0; i < 8; ++i)
        {
            uuid.m_bytes[i] = static_cast<std::uint8_t>(high >> (i * 8));
            uuid.m_bytes[8 + i] = static_cast<std::uint8_t>(low >> (i * 8));
        }

        // RFC 4122: version 4, variant 1.
        uuid.m_bytes[6] = static_cast<std::uint8_t>((uuid.m_bytes[6] & 0x0f) | 0x40);
        uuid.m_bytes[8] = static_cast<std::uint8_t>((uuid.m_bytes[8] & 0x3f) | 0x80);
        return uuid;
    }

    std::optional<Uuid> Uuid::Parse(const std::string_view text)
    {
        // Canonical form: 8-4-4-4-12 hex digits, 36 characters.
        if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
        {
            return std::nullopt;
        }

        Uuid uuid;
        std::size_t byteIndex = 0;
        for (std::size_t i = 0; i < text.size();)
        {
            if (text[i] == '-')
            {
                ++i;
                continue;
            }
            const int high = HexValue(text[i]);
            const int low = HexValue(text[i + 1]);
            if (high < 0 || low < 0)
            {
                return std::nullopt;
            }
            uuid.m_bytes[byteIndex++] = static_cast<std::uint8_t>((high << 4) | low);
            i += 2;
        }
        return uuid;
    }

    bool Uuid::IsNil() const noexcept
    {
        for (const std::uint8_t byte : m_bytes)
        {
            if (byte != 0)
            {
                return false;
            }
        }
        return true;
    }

    std::string Uuid::ToString() const
    {
        std::string text;
        text.reserve(36);
        for (std::size_t i = 0; i < m_bytes.size(); ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
            {
                text += '-';
            }
            text += kHexDigits[m_bytes[i] >> 4];
            text += kHexDigits[m_bytes[i] & 0x0f];
        }
        return text;
    }
} // namespace stalkermp::common
