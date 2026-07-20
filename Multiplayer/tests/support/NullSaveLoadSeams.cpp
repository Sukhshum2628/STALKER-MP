// STALKER-MP — Null save/load seams (test build only)
//
// Test-build counterpart of the save/load composition-root factories defined in
// src/adapters/EngineAdapters.cpp (CreateEngineSaveBackend / CreateEngineRestoreSinks).
// It provides the same factory symbols with no engine or filesystem dependency, so
// composition-root code (Bootstrap.cpp) is identical in both builds:
//
//   * CreateEngineSaveBackend  -> the engine-free in-memory backend (no filesystem),
//     so Bootstrap unit tests never touch the disk.
//   * CreateEngineRestoreSinks -> inert null sinks (records accepted, nothing written).
//
// Both are deterministic and engine-free. ADR-007: no exceptions, no RTTI.

#include "stalkermp/adapters/SaveLoadSeams.h"

#include "stalkermp/saveload/NullRestoreSinks.h"

namespace stalkermp::adapters
{
    core::Expected<std::unique_ptr<ISaveBackend>>
    CreateEngineSaveBackend(const saveload::SaveLoadConfiguration& /*config*/)
    {
        // In-memory backend: writes through Store() are readable through Source().
        return CreateInMemorySaveBackend();
    }

    std::unique_ptr<saveload::IRestoreSinkBundle> CreateEngineRestoreSinks()
    {
        return std::make_unique<saveload::NullRestoreSinkBundle>();
    }
} // namespace stalkermp::adapters
