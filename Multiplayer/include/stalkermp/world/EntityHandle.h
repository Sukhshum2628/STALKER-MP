// STALKER-MP — Entity handle (Sprint-003, §7.2; invariant I4)
//
// Separates the persistent identity from the runtime reference:
//   - world::EntityId (Sprint-002 seam type) is the PERSISTENT identity. It is
//     stable, immutable for an entity's life, and is what gets serialized as
//     identity (format deferred to Sprint-011). Sprint-002 declared it "semantics
//     belong to Sprint-003"; this sprint assigns those semantics.
//   - EntityHandle pairs that identity with a SESSION-LOCAL generation for
//     stale-reference detection. The generation is NEVER persisted as identity
//     (Sprint-003 invariant I4).
//
// Pure value type: no engine headers (One Engine Boundary / invariant I2), no
// exceptions and no throwing STL (ADR-007 invariants 4/6, I10). Trivially
// C4530-clean. Hashable for use as a key in std::unordered_map/std::unordered_set
// (Sprint-003 Evidence Gate 1, approved).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "stalkermp/world/EntityView.h" // world::EntityId (persistent identity)

namespace stalkermp::world
{
    // Session-local generation counter. Incremented when a persistent EntityId is
    // reused across registrations so that handles referring to a prior occupant
    // are detectably stale. Never serialized as identity (I4).
    using EntityGeneration = std::uint32_t;

    // Reserved "no generation" value. A valid handle has generation != 0.
    inline constexpr EntityGeneration kInvalidGeneration = 0;

    // Runtime reference to a registered entity (I4). Combines the persistent
    // identity with the session-local generation. A handle is a reference, never
    // an owner, and holds no engine pointer (invariant C2).
    struct EntityHandle
    {
        EntityId id{};                                   // persistent identity (value 0 = none)
        EntityGeneration generation = kInvalidGeneration; // session-local (0 = invalid)

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return id.value == 0 && generation == kInvalidGeneration;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return id.value != 0 && generation != kInvalidGeneration;
        }

        [[nodiscard]] constexpr bool operator==(const EntityHandle& other) const noexcept
        {
            return id == other.id && generation == other.generation;
        }

        [[nodiscard]] constexpr bool operator!=(const EntityHandle& other) const noexcept
        {
            return !(*this == other);
        }
    };

    // The null handle (Sprint-003 §7.2). Distinct from any valid handle.
    inline constexpr EntityHandle kNullEntityHandle{};
} // namespace stalkermp::world

namespace std
{
    // Deterministic, allocation-free, non-throwing hash of a runtime handle.
    template <>
    struct hash<stalkermp::world::EntityHandle>
    {
        [[nodiscard]] std::size_t operator()(const stalkermp::world::EntityHandle& handle) const noexcept
        {
            const std::uint64_t combined =
                (static_cast<std::uint64_t>(handle.id.value) << 32) ^
                static_cast<std::uint64_t>(handle.generation);
            // 64-bit FNV prime mix; deterministic across runs.
            return static_cast<std::size_t>(combined * 1099511628211ULL);
        }
    };
} // namespace std
