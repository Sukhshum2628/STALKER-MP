// STALKER-MP — Bubble-based interest policy (Sprint-009, Step 5)
//
// See BubbleInterestPolicy.h. Deterministic, value-only relevance selection over
// an immutable snapshot; append-only ascending unique output. Engine-free.

#include "stalkermp/replication/BubbleInterestPolicy.h"

namespace stalkermp::replication
{
    namespace
    {
        // An entity in (or entering) the active region is relevant to every client.
        [[nodiscard]] bool InActiveRegion(const world::BubbleMembership membership) noexcept
        {
            return membership == world::BubbleMembership::Inside ||
                   membership == world::BubbleMembership::PendingActivation;
        }

        [[nodiscard]] bool WithinRadius(const world::Vec3& a, const world::Vec3& b,
                                        const std::uint32_t radiusMeters) noexcept
        {
            if (radiusMeters == 0)
            {
                return false; // radius disabled -> rely on bubble membership only
            }
            const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
            const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
            const double dz = static_cast<double>(a.z) - static_cast<double>(b.z);
            const double distSq = dx * dx + dy * dy + dz * dz;
            const double r = static_cast<double>(radiusMeters);
            return distSq <= r * r;
        }
    } // namespace

    void BubbleInterestPolicy::SelectRelevant(const ClientRecord& client, const snapshot::ISnapshotView& snapshot,
                                              std::vector<world::EntityId>& out) const
    {
        // Snapshot entities are ascending, unique EntityId (Sprint-008 invariant),
        // so iterating in order and appending keeps `out`'s appended subset
        // ascending and unique. Append-only: pre-existing entries are preserved.
        for (const snapshot::EntitySnapshot& entity : snapshot.Entities())
        {
            if (entity.id.value == 0)
            {
                continue; // reserved none (defensive)
            }
            const bool relevant = InActiveRegion(m_membership.MembershipOf(entity.id)) ||
                                  WithinRadius(entity.position, client.focus.position, m_interestRadiusMeters);
            if (relevant)
            {
                out.push_back(entity.id); // value copy only
            }
        }
    }
} // namespace stalkermp::replication
