// STALKER-MP — Transition intent store (Sprint-005, Architecture §10/§16)
//
// The deterministic, engine-free store of per-entity transition INTENT owned by
// the TransitionManager. This is the module's own bookkeeping — never the
// engine's authoritative online/offline status, which the engine alone owns
// (§11). Untracked entities are implicitly Offline.
//
// Canonical representation (Architecture §10 / §16, and Sprint-003 BC-005):
//   - a sorted std::vector keyed ascending by EntityId is the CANONICAL store,
//     guaranteeing deterministic ascending iteration (no engine/hash order leaks);
//   - a std::unordered_map<u32, index> is a SECONDARY accelerator for O(1) lookup
//     (BC-005-approved container), rebuilt to stay consistent with the vector.
//
// This step (Step 3) provides the store and the manager's read-only surface only.
// It contains NO Update pipeline, NO Bubble reads, NO gateway apply, NO engine
// access (ADR-007 / One Engine Boundary; ADR-008 names no engine call here).

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "stalkermp/world/EntityView.h"       // world::EntityId
#include "stalkermp/world/TransitionTypes.h"  // world::TransitionState

namespace stalkermp::world
{
    // Report of a read-only store consistency scan. Healthy when the canonical
    // vector is strictly ascending & unique and the accelerator is consistent.
    struct TransitionStoreReport
    {
        bool sortedUnique = true;         // canonical vector strictly ascending, unique ids
        bool acceleratorConsistent = true; // hash index matches the canonical vector

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return sortedUnique && acceleratorConsistent;
        }
    };

    class TransitionIntentStore
    {
    public:
        struct Entry
        {
            EntityId id{};
            TransitionState state = TransitionState::Offline;
        };

        TransitionIntentStore() = default;

        // Upsert intent for an entity, preserving canonical ascending order.
        void Set(const EntityId id, const TransitionState state)
        {
            const auto pos = LowerBound(id.value);
            if (pos != m_entries.end() && pos->id.value == id.value)
            {
                pos->state = state; // in-place update; ordering unchanged
                return;
            }
            m_entries.insert(pos, Entry{id, state});
            RebuildAccelerator();
        }

        // Remove an entity's tracked intent (returns to implicit Offline).
        void Erase(const EntityId id)
        {
            const auto it = m_index.find(id.value);
            if (it == m_index.end())
            {
                return;
            }
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(it->second));
            RebuildAccelerator();
        }

        // Intent for an entity; Offline if untracked.
        [[nodiscard]] TransitionState Get(const EntityId id) const
        {
            const auto it = m_index.find(id.value);
            if (it == m_index.end())
            {
                return TransitionState::Offline;
            }
            return m_entries[it->second].state;
        }

        [[nodiscard]] bool Contains(const EntityId id) const
        {
            return m_index.find(id.value) != m_index.end();
        }

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }

        // Tracked entities in canonical ascending EntityId order.
        [[nodiscard]] std::vector<EntityId> Entities() const
        {
            std::vector<EntityId> ids;
            ids.reserve(m_entries.size());
            for (const Entry& entry : m_entries)
            {
                ids.push_back(entry.id);
            }
            return ids;
        }

        // Read-only snapshot of tracked entries (ascending). Used by later steps
        // and diagnostics; never exposes a mutable handle.
        [[nodiscard]] const std::vector<Entry>& Entries() const noexcept { return m_entries; }

        void Clear()
        {
            m_entries.clear();
            m_index.clear();
        }

        // Read-only consistency scan (reports, never repairs).
        [[nodiscard]] TransitionStoreReport ValidateConsistency() const
        {
            TransitionStoreReport report;

            for (std::size_t i = 1; i < m_entries.size(); ++i)
            {
                if (!(m_entries[i - 1].id.value < m_entries[i].id.value))
                {
                    report.sortedUnique = false;
                    break;
                }
            }

            if (m_index.size() != m_entries.size())
            {
                report.acceleratorConsistent = false;
            }
            else
            {
                for (std::size_t i = 0; i < m_entries.size(); ++i)
                {
                    const auto it = m_index.find(m_entries[i].id.value);
                    if (it == m_index.end() || it->second != i)
                    {
                        report.acceleratorConsistent = false;
                        break;
                    }
                }
            }

            return report;
        }

    private:
        [[nodiscard]] std::vector<Entry>::iterator LowerBound(const std::uint32_t value)
        {
            return std::lower_bound(
                m_entries.begin(), m_entries.end(), value,
                [](const Entry& entry, const std::uint32_t v) { return entry.id.value < v; });
        }

        void RebuildAccelerator()
        {
            m_index.clear();
            m_index.reserve(m_entries.size());
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                m_index.emplace(m_entries[i].id.value, i);
            }
        }

        std::vector<Entry> m_entries;                       // canonical, ascending, unique
        std::unordered_map<std::uint32_t, std::size_t> m_index; // secondary accelerator (id -> index)
    };
} // namespace stalkermp::world
