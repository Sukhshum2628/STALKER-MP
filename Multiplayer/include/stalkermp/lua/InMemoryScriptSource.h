// STALKER-MP — In-memory script source (Sprint-013, Step 7)
//
// An IScriptSource that holds scripts in memory (no OS, no file). It is the
// default/test read backend: tests populate it with script artifacts and observe
// enumerate/read. The real filesystem backend is Step-08. Enumeration is deterministic
// (ascending script id). Engine-free / VM-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/lua/IScriptSource.h"

namespace stalkermp::lua
{
    class InMemoryScriptSource final : public IScriptSource
    {
    public:
        // Store (or replace) a script's descriptor + bytes. Kept ordered by script id.
        void Store(const ScriptDescriptor& descriptor, std::vector<std::byte> bytes);

        // Remove a script by id (benign if absent).
        void Remove(ScriptId id) noexcept;

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }

        [[nodiscard]] std::vector<ScriptDescriptor> Enumerate() const override;
        [[nodiscard]] core::Expected<std::vector<std::byte>> Read(ScriptId id) const override;
        [[nodiscard]] bool Exists(ScriptId id) const override;

    private:
        struct Entry
        {
            ScriptDescriptor descriptor{};
            std::vector<std::byte> bytes;
        };

        std::vector<Entry> m_entries; // kept sorted ascending by descriptor.id
    };
} // namespace stalkermp::lua
