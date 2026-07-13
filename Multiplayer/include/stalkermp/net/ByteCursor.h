// STALKER-MP — Byte cursors (Sprint-006, Step 2)
//
// Non-owning, bounds-checked, little-endian byte cursors over a caller-owned
// span. The reusable primitive the wire codec is built on.
//
// ADR-007: no exceptions; over/underflow return core::Expected. ADR-010:
// little-endian by EXPLICIT shift/mask (host-endian independent) — no OS
// byte-order helpers, no intrinsics, no reinterpret/type-pun, byte-granular (no
// alignment assumption). Engine-free and OS-free.

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/core/Error.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::net
{
    // Appends little-endian into a fixed, caller-owned buffer. Non-owning.
    class ByteWriter
    {
    public:
        ByteWriter(std::uint8_t* buffer, std::size_t size) noexcept
            : m_buffer(buffer), m_size(size), m_position(0)
        {
        }

        [[nodiscard]] core::Expected<void> WriteU8(const std::uint8_t value)
        {
            if (m_position + 1 > m_size)
            {
                return Overflow();
            }
            m_buffer[m_position++] = value;
            return core::Success();
        }

        [[nodiscard]] core::Expected<void> WriteU16(const std::uint16_t value)
        {
            if (m_position + 2 > m_size)
            {
                return Overflow();
            }
            m_buffer[m_position++] = static_cast<std::uint8_t>(value & 0xFF);
            m_buffer[m_position++] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
            return core::Success();
        }

        [[nodiscard]] core::Expected<void> WriteU32(const std::uint32_t value)
        {
            if (m_position + 4 > m_size)
            {
                return Overflow();
            }
            m_buffer[m_position++] = static_cast<std::uint8_t>(value & 0xFF);
            m_buffer[m_position++] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
            m_buffer[m_position++] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
            m_buffer[m_position++] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
            return core::Success();
        }

        [[nodiscard]] std::size_t BytesWritten() const noexcept { return m_position; }
        [[nodiscard]] std::size_t Remaining() const noexcept { return m_size - m_position; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_size; }

    private:
        [[nodiscard]] static core::Expected<void> Overflow()
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "byte cursor overflow");
        }

        std::uint8_t* m_buffer;
        std::size_t m_size;
        std::size_t m_position;
    };

    // Extracts little-endian from a fixed, caller-owned buffer. Non-owning.
    class ByteReader
    {
    public:
        ByteReader(const std::uint8_t* buffer, std::size_t size) noexcept
            : m_buffer(buffer), m_size(size), m_position(0)
        {
        }

        [[nodiscard]] core::Expected<std::uint8_t> ReadU8()
        {
            if (m_position + 1 > m_size)
            {
                return Underflow();
            }
            return m_buffer[m_position++];
        }

        [[nodiscard]] core::Expected<std::uint16_t> ReadU16()
        {
            if (m_position + 2 > m_size)
            {
                return Underflow();
            }
            const std::uint16_t b0 = m_buffer[m_position++];
            const std::uint16_t b1 = m_buffer[m_position++];
            return static_cast<std::uint16_t>(b0 | (b1 << 8));
        }

        [[nodiscard]] core::Expected<std::uint32_t> ReadU32()
        {
            if (m_position + 4 > m_size)
            {
                return Underflow();
            }
            const std::uint32_t b0 = m_buffer[m_position++];
            const std::uint32_t b1 = m_buffer[m_position++];
            const std::uint32_t b2 = m_buffer[m_position++];
            const std::uint32_t b3 = m_buffer[m_position++];
            return static_cast<std::uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
        }

        [[nodiscard]] std::size_t BytesRead() const noexcept { return m_position; }
        [[nodiscard]] std::size_t Remaining() const noexcept { return m_size - m_position; }
        [[nodiscard]] std::size_t Size() const noexcept { return m_size; }

    private:
        // Returns core::Error; implicitly converts to Expected<T> for any T.
        [[nodiscard]] static core::Error Underflow()
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "byte cursor underflow");
        }

        const std::uint8_t* m_buffer;
        std::size_t m_size;
        std::size_t m_position;
    };
} // namespace stalkermp::net
