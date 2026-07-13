// STALKER-MP — UUID utility (Sprint-001, §7.11)
//
// Random (version 4, RFC 4122) UUIDs for non-deterministic identities such
// as session identifiers and log correlation. NOT for persistent entity
// IDs — those remain owned by the engine's entity registry (02_Engine.md
// §14) and must stay deterministic.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace stalkermp::common
{
    class Uuid
    {
    public:
        // The nil UUID (all zero).
        Uuid() = default;

        // Generates a random version-4 UUID.
        [[nodiscard]] static Uuid Generate();

        // Parses "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (case-insensitive).
        [[nodiscard]] static std::optional<Uuid> Parse(std::string_view text);

        [[nodiscard]] bool IsNil() const noexcept;

        // Lowercase canonical form: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".
        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] bool operator==(const Uuid& other) const noexcept { return m_bytes == other.m_bytes; }
        [[nodiscard]] bool operator!=(const Uuid& other) const noexcept { return !(*this == other); }
        [[nodiscard]] bool operator<(const Uuid& other) const noexcept { return m_bytes < other.m_bytes; }

    private:
        std::array<std::uint8_t, 16> m_bytes{};
    };
} // namespace stalkermp::common
