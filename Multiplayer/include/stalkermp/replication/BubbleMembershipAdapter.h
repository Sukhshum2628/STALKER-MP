// STALKER-MP — Bubble membership adapter (Sprint-009, Step 12)
//
// Binds the engine-free IBubbleMembershipSource seam (Step 5) to the Sprint-004
// world::BubbleManager, by forwarding MembershipOf to the manager's read-only
// activation query. Composition glue only — read-only observation (ADR-008);
// Preserve Before Replace (activation is reused, not reimplemented). Engine-free.
//
// ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/replication/IBubbleMembershipSource.h"
#include "stalkermp/world/BubbleManager.h" // world::BubbleManager

namespace stalkermp::replication
{
    class BubbleMembershipAdapter final : public IBubbleMembershipSource
    {
    public:
        explicit BubbleMembershipAdapter(const world::BubbleManager& bubble) noexcept : m_bubble(bubble) {}

        [[nodiscard]] world::BubbleMembership MembershipOf(const world::EntityId id) const noexcept override
        {
            return m_bubble.MembershipOf(id);
        }

    private:
        const world::BubbleManager& m_bubble; // non-owning; read-only
    };
} // namespace stalkermp::replication
