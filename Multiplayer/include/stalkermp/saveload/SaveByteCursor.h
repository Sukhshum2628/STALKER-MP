// STALKER-MP — Save byte cursor primitives (Sprint-012, Step 3)
//
// Deterministic, little-endian, engine-free byte writer/reader primitives for the
// save artifact. Independent of the networking wire protocol (ADR-010 untouched):
// these are save-local serialization primitives, not wire framing. The writer
// grows into an owned buffer; the reader is bounds-checked over a borrowed span and
// reports overrun as a value outcome (never throws; ADR-007).
//
// This file introduces the byte PRIMITIVES only — no SaveWriter (Step 4), no
// SaveReader (Step 5), no domain serialization, no filesystem.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring> // std::memcpy (deterministic float bit access)
#include <vector>

namespace stalkermp::saveload
{
    // Grows into an owned byte buffer, appending values in little-endian order.
    class ByteWriter
    {
    public:
        void PutU8(std::uint8_t v) { m_bytes.push_back(v); }

        void PutU16(std::uint16_t v)
        {
            m_bytes.push_back(static_cast<std::uint8_t>(v & 0xFFu));
            m_bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
        }

        void PutU32(std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
            {
                m_bytes.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
            }
        }

        void PutU64(std::uint64_t v)
        {
            for (int i = 0; i < 8; ++i)
            {
                m_bytes.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
            }
        }

        // IEEE-754 bit pattern, little-endian (no aliasing UB).
        void PutF32(float f)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &f, sizeof(bits));
            PutU32(bits);
        }

        void PutBytes(const std::uint8_t* data, std::size_t count)
        {
            if (data != nullptr && count > 0)
            {
                m_bytes.insert(m_bytes.end(), data, data + count);
            }
        }

        [[nodiscard]] std::size_t Size() const noexcept { return m_bytes.size(); }
        [[nodiscard]] const std::vector<std::uint8_t>& Bytes() const noexcept { return m_bytes; }
        void Clear() noexcept { m_bytes.clear(); }

    private:
        std::vector<std::uint8_t> m_bytes;
    };

    // Bounds-checked little-endian reader over a borrowed span. On overrun the read
    // returns false, leaves the out value unchanged, and latches an error; every
    // subsequent read then fails (value-outcome, never throws).
    class ByteReader
    {
    public:
        ByteReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

        [[nodiscard]] bool GetU8(std::uint8_t& out) noexcept
        {
            if (!Take(1))
            {
                return false;
            }
            out = m_data[m_pos - 1];
            return true;
        }

        [[nodiscard]] bool GetU16(std::uint16_t& out) noexcept
        {
            if (!Take(2))
            {
                return false;
            }
            const std::size_t p = m_pos - 2;
            out = static_cast<std::uint16_t>(static_cast<std::uint16_t>(m_data[p]) |
                                             (static_cast<std::uint16_t>(m_data[p + 1]) << 8));
            return true;
        }

        [[nodiscard]] bool GetU32(std::uint32_t& out) noexcept
        {
            if (!Take(4))
            {
                return false;
            }
            const std::size_t p = m_pos - 4;
            std::uint32_t v = 0;
            for (int i = 0; i < 4; ++i)
            {
                v |= static_cast<std::uint32_t>(m_data[p + static_cast<std::size_t>(i)]) << (8 * i);
            }
            out = v;
            return true;
        }

        [[nodiscard]] bool GetU64(std::uint64_t& out) noexcept
        {
            if (!Take(8))
            {
                return false;
            }
            const std::size_t p = m_pos - 8;
            std::uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
            {
                v |= static_cast<std::uint64_t>(m_data[p + static_cast<std::size_t>(i)]) << (8 * i);
            }
            out = v;
            return true;
        }

        [[nodiscard]] bool GetF32(float& out) noexcept
        {
            std::uint32_t bits = 0;
            if (!GetU32(bits))
            {
                return false;
            }
            std::memcpy(&out, &bits, sizeof(out));
            return true;
        }

        // Copy `count` bytes into `dst`. Fails (no copy) if fewer remain.
        [[nodiscard]] bool GetBytes(std::uint8_t* dst, std::size_t count) noexcept
        {
            if (!Take(count))
            {
                return false;
            }
            if (dst != nullptr && count > 0)
            {
                std::memcpy(dst, m_data + (m_pos - count), count);
            }
            return true;
        }

        // Advance over `count` bytes without copying (skip a section payload).
        [[nodiscard]] bool Skip(std::size_t count) noexcept { return Take(count); }

        [[nodiscard]] bool Ok() const noexcept { return !m_error; }
        [[nodiscard]] std::size_t Position() const noexcept { return m_pos; }
        [[nodiscard]] std::size_t Remaining() const noexcept { return m_error ? 0 : m_size - m_pos; }
        [[nodiscard]] const std::uint8_t* Current() const noexcept { return m_data + m_pos; }

    private:
        [[nodiscard]] bool Take(std::size_t n) noexcept
        {
            if (m_error || n > m_size - m_pos)
            {
                m_error = true;
                return false;
            }
            m_pos += n;
            return true;
        }

        const std::uint8_t* m_data = nullptr;
        std::size_t m_size = 0;
        std::size_t m_pos = 0;
        bool m_error = false;
    };
} // namespace stalkermp::saveload
