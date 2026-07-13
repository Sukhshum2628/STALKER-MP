// STALKER-MP — Hashing utilities (Sprint-001, §7.11)
//
// FNV-1a in 32- and 64-bit variants. constexpr so string identifiers can be
// hashed at compile time (future: category IDs, packet type IDs).

#pragma once

#include <cstdint>
#include <string_view>

namespace stalkermp::common
{
    [[nodiscard]] constexpr std::uint32_t Fnv1a32(const std::string_view data) noexcept
    {
        std::uint32_t hash = 0x811c9dc5u;
        for (const char c : data)
        {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 0x01000193u;
        }
        return hash;
    }

    [[nodiscard]] constexpr std::uint64_t Fnv1a64(const std::string_view data) noexcept
    {
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (const char c : data)
        {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 0x00000100000001b3ull;
        }
        return hash;
    }
} // namespace stalkermp::common
