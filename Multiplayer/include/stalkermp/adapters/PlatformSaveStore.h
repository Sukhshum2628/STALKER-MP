// STALKER-MP — Platform save store factory (Sprint-012, Step 9)
//
// The engine-free factory header for the durable save backend. This is the Singular
// PLATFORM BOUNDARY seam for save/load: ALL filesystem access lives in exactly one
// translation unit (adapters/PlatformSaveStore.cpp) behind these factories — no OS/
// file header appears anywhere else (ADR-009), mirroring adapters/PlatformSockets.cpp.
//
// A backend exposes BOTH the frozen Sprint-011 write seam (persistence::
// IPersistenceStore — reused UNCHANGED) and the Step-8 read seam (saveload::
// ISaveSource) over one save location, plus a Delete. The real filesystem backend
// composes the Step-4 SaveWriter to produce bytes and writes them atomically
// (temp-file + rename); the engine-free in-memory backend is the default/test
// counterpart (write<->read consistent, no OS).
//
// This step introduces the platform seam + backends ONLY — no recovery, restoration,
// diagnostics, engine dependency, or wiring.
//
// Engine-free / OS-free HEADER. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "stalkermp/core/Expected.h"                  // core::Expected
#include "stalkermp/persistence/IPersistenceStore.h"  // persistence::IPersistenceStore (reused)
#include "stalkermp/saveload/ISaveSource.h"           // saveload::ISaveSource
#include "stalkermp/saveload/SaveLoadTypes.h"         // saveload::SaveLoadOutcome

namespace stalkermp::adapters
{
    // Combined access to a save backend: the write store and the read source over one
    // save location, plus a delete. Owns the backend.
    class ISaveBackend
    {
    public:
        virtual ~ISaveBackend() = default;

        // The write seam (Begin/Write/Commit/Abort) — the frozen Sprint-011 interface.
        [[nodiscard]] virtual persistence::IPersistenceStore& Store() noexcept = 0;

        // The read seam (Enumerate/Read/Exists).
        [[nodiscard]] virtual saveload::ISaveSource& Source() noexcept = 0;

        // Delete a save by id. Ok if removed or already absent; StorageUnavailable on
        // a backend failure.
        [[nodiscard]] virtual saveload::SaveLoadOutcome Delete(std::uint64_t saveId) = 0;
    };

    // Real filesystem backend over the opaque `saveDirectoryToken` (created if
    // missing). The ONLY OS/file code in the module lives behind this factory.
    // Fails (IoError) when the directory cannot be created.
    [[nodiscard]] core::Expected<std::unique_ptr<ISaveBackend>>
    CreatePlatformSaveBackend(const std::string& saveDirectoryToken);

    // Engine-free, OS-free in-memory backend (default/test build). Writes committed
    // through Store() are readable through Source().
    [[nodiscard]] std::unique_ptr<ISaveBackend> CreateInMemorySaveBackend();
} // namespace stalkermp::adapters
