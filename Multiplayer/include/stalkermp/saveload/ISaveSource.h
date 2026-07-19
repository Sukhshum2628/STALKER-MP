// STALKER-MP — Save source read seam (Sprint-012, Step 8)
//
// The additive engine-free LOAD-side read boundary. The Sprint-011 write seam
// `IPersistenceStore` is reused unchanged for writing; this interface is the
// counterpart for reading/enumerating saves. It is engine-free and OS-free — the
// REAL filesystem backend is Step 9 (a single platform TU behind an engine-free
// factory); the default/test build uses the in-memory / null implementations here.
//
// This step introduces the interface + in-memory/null doubles ONLY — no platform
// filesystem, no restoration, no wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes / Expected<T>.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"           // core::Expected
#include "stalkermp/saveload/SaveLoadTypes.h"  // SaveDescriptor

namespace stalkermp::saveload
{
    class ISaveSource
    {
    public:
        virtual ~ISaveSource() = default;

        // All available saves, ascending by saveId (deterministic order).
        [[nodiscard]] virtual std::vector<SaveDescriptor> Enumerate() const = 0;

        // The raw save bytes for `saveId`, or an error (NotFound) when absent.
        [[nodiscard]] virtual core::Expected<std::vector<std::uint8_t>> Read(std::uint64_t saveId) const = 0;

        // Whether a save with `saveId` exists.
        [[nodiscard]] virtual bool Exists(std::uint64_t saveId) const = 0;
    };
} // namespace stalkermp::saveload
