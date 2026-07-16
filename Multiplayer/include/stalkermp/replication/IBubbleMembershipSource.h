// STALKER-MP — Bubble membership seam for replication interest (Sprint-009, Step 5)
//
// A thin, engine-free, read-only seam over the Sprint-004 Bubble activation
// region. The BubbleInterestPolicy consumes this to decide whether an entity is
// in the active region. The real binding forwards to
// world::BubbleManager::MembershipOf (added at the Bootstrap-wiring step); the
// test build uses a fake. Preserve Before Replace: activation is reused, not
// reimplemented; this seam only observes it (ADR-008 read-only).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/world/BubbleTypes.h" // world::BubbleMembership
#include "stalkermp/world/EntityView.h"  // world::EntityId

namespace stalkermp::replication
{
    class IBubbleMembershipSource
    {
    public:
        virtual ~IBubbleMembershipSource() = default;

        // The entity's current membership in the unified activation region
        // (read-only observation; never mutates the bubble).
        [[nodiscard]] virtual world::BubbleMembership MembershipOf(world::EntityId id) const noexcept = 0;
    };
} // namespace stalkermp::replication
