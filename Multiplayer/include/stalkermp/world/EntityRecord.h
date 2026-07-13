// STALKER-MP — Entity record value object (Sprint-003, §7.1/§7.10)
//
// The registry-owned value object for one persistent entity. It composes:
//   - handle:   the runtime EntityHandle (persistent EntityId + session-local
//               generation, invariant I4).
//   - name:     the entity name, for FindByName (§7.4). Stored as std::string
//               exactly as the Sprint-002 EntityView does.
//   - metadata: the §7.10 metadata (category, section, flags, creation tick,
//               spawn source, registration state).
//
// A record is a pure value type. It owns no engine object, stores no CObject*
// and no engine pointer (invariant C2), includes no engine headers (invariant
// I2 / One Engine Boundary), and uses no exceptions or throwing STL (ADR-007 /
// I10). This step introduces the type only — no registry logic, containers,
// synchronization, reconciliation, relcase integration, or events.

#pragma once

#include <string>

#include "stalkermp/world/EntityHandle.h"   // EntityHandle (identity + generation)
#include "stalkermp/world/EntityMetadata.h" // EntityMetadata

namespace stalkermp::world
{
    struct EntityRecord
    {
        EntityHandle handle{};   // persistent identity + session-local generation
        std::string name;        // for FindByName (§7.4)
        EntityMetadata metadata; // §7.10 metadata

        // Convenience accessors (do not add behavior; identity/metadata only).
        [[nodiscard]] EntityId Id() const noexcept { return handle.id; }
        [[nodiscard]] EntityGeneration Generation() const noexcept { return handle.generation; }
        [[nodiscard]] bool IsValid() const noexcept { return handle.IsValid(); }
    };
} // namespace stalkermp::world
