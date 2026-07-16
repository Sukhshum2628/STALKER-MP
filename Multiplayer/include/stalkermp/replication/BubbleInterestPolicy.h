// STALKER-MP — Bubble-based interest policy (Sprint-009, Step 5)
//
// The concrete IInterestPolicy: an entity is relevant to a client when it is in
// the active Bubble region (Inside or entering — PendingActivation) OR, when a
// positive interest radius is configured, within interestRadiusMeters of the
// client's focus. Reads the immutable snapshot and the engine-free Bubble
// membership seam only; appends ascending, unique EntityIds; deterministic.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/replication/IBubbleMembershipSource.h"
#include "stalkermp/replication/IInterestPolicy.h"

namespace stalkermp::replication
{
    class BubbleInterestPolicy final : public IInterestPolicy
    {
    public:
        BubbleInterestPolicy(const IBubbleMembershipSource& membership,
                             std::uint32_t interestRadiusMeters) noexcept
            : m_membership(membership), m_interestRadiusMeters(interestRadiusMeters)
        {
        }

        void SelectRelevant(const ClientRecord& client, const snapshot::ISnapshotView& snapshot,
                            std::vector<world::EntityId>& out) const override;

    private:
        const IBubbleMembershipSource& m_membership; // non-owning; read-only
        std::uint32_t m_interestRadiusMeters;
    };
} // namespace stalkermp::replication
