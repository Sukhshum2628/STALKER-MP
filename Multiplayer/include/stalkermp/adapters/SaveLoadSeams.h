// STALKER-MP — Save/load composition-root seams (Sprint-012, Step 17)
//
// The two factories the composition root (Bootstrap.cpp) uses to obtain the
// build-appropriate save/load boundary WITHOUT naming the concrete engine/platform
// types:
//
//   * CreateEngineSaveBackend  — the persistence write store + paired read source
//     over one save location. The engine build binds the real filesystem backend
//     (PlatformSaveStore, the one platform TU); the test build binds the in-memory
//     backend. Selection is by which TU is linked (EngineAdapters.cpp vs
//     tests/support/NullSaveLoadSeams.cpp), mirroring the prediction-seam pattern.
//
//   * CreateEngineRestoreSinks — the restoration write boundary as one bundle. The
//     engine build returns the real EngineAdapters sinks (writing authoritative
//     state through the sanctioned Sprint-003/005/007 seams); the test build returns
//     inert null sinks. The engine-free core never sees an engine type.
//
// This header is engine-free and OS-free: it declares factories over engine-free
// interfaces only. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <memory>
#include <string>

#include "stalkermp/adapters/PlatformSaveStore.h"       // adapters::ISaveBackend
#include "stalkermp/core/Expected.h"                     // core::Expected
#include "stalkermp/saveload/IRestoreSinkBundle.h"       // saveload::IRestoreSinkBundle
#include "stalkermp/saveload/SaveLoadConfiguration.h"    // saveload::SaveLoadConfiguration

namespace stalkermp::adapters
{
    // The save backend for `config` (write store + read source over one location).
    // Engine build: real filesystem backend under config.saveDirectoryToken. Test
    // build: in-memory backend (no filesystem). Value-outcome on failure.
    [[nodiscard]] core::Expected<std::unique_ptr<ISaveBackend>>
    CreateEngineSaveBackend(const saveload::SaveLoadConfiguration& config);

    // The restoration write boundary. Engine build: the real EngineAdapters sinks.
    // Test build: inert null sinks. Never fails.
    [[nodiscard]] std::unique_ptr<saveload::IRestoreSinkBundle> CreateEngineRestoreSinks();
} // namespace stalkermp::adapters
