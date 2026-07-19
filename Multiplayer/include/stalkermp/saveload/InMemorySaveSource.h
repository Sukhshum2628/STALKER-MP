// STALKER-MP — In-memory save source (Sprint-012, Step 8)
//
// An ISaveSource that holds saves in memory (no OS, no file). It is the default/test
// read backend: tests populate it with save artifacts and observe enumerate/read.
// The real filesystem backend is Step 9. Enumeration is deterministic (ascending
// saveId). Engine-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/saveload/ISaveSource.h"

namespace stalkermp::saveload
{
    class InMemorySaveSource final : public ISaveSource
    {
    public:
        // Store (or replace) a save's descriptor + bytes. Kept ordered by saveId.
        void Store(const SaveDescriptor& descriptor, std::vector<std::uint8_t> bytes);

        // Remove a save by id (benign if absent).
        void Remove(std::uint64_t saveId) noexcept;

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }

        [[nodiscard]] std::vector<SaveDescriptor> Enumerate() const override;
        [[nodiscard]] core::Expected<std::vector<std::uint8_t>> Read(std::uint64_t saveId) const override;
        [[nodiscard]] bool Exists(std::uint64_t saveId) const override;

    private:
        struct Entry
        {
            SaveDescriptor descriptor{};
            std::vector<std::uint8_t> bytes;
        };

        std::vector<Entry> m_entries; // kept sorted ascending by descriptor.saveId
    };
} // namespace stalkermp::saveload
