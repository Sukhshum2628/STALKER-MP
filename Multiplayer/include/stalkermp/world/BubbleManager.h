// STALKER-MP — Bubble Manager core (Sprint-004, §7.1/§7.7; Design Review §1/§2)
//
// Owns the Bubble subsystem's state (configuration + activation sets + per-tick
// summary) and exposes the read-only public query interface. This step
// establishes state and API ONLY — there is no bubble computation, no tick/update
// logic, no player or spatial sources, no registry/ALife integration, and no
// events. Query methods return safe defaults until those are implemented.
//
// Ownership / lifetime: engine-agnostic value-owning component. Created at the
// composition root (a later step) with an injected BubbleConfiguration.
//
// Invariants honored here: engine-free (B6, no engine headers/pointers), ADR-007
// (no exceptions, no throwing STL), no gameplay logic (B2), no entity ownership
// (B3 — activation sets hold EntityId values, not entities). The activation sets
// are kept sorted ascending so any future iteration is deterministic (B9).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "stalkermp/world/BubbleConfiguration.h"
#include "stalkermp/world/BubbleTypes.h"
#include "stalkermp/world/EntityView.h" // world::EntityId, world::Vec3

namespace stalkermp::world
{
    // Read-only seams consumed by the activation algorithm (both engine-free, B6).
    // Forward-declared; stored as non-owning pointers.
    class ISpatialQueries;        // Sprint-002 entity positions / radius membership
    class IPlayerPositionSource;  // Sprint-004 positions-only player source (B11)

    // Result of a read-only bubble consistency scan (Sprint-004 §7.12 equivalent).
    // Every field is true on a healthy manager. Reports (never repairs).
    struct BubbleConsistencyReport
    {
        bool onlineSorted = true;          // m_online strictly ascending & unique
        bool previousSorted = true;        // m_previousOnline strictly ascending & unique
        bool radiiValid = true;            // ActivationRadius() <= DeactivationRadius()
        bool transitionsConsistent = true; // stored transitions == online/previous set diff

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return onlineSorted && previousSorted && radiiValid && transitionsConsistent;
        }
    };

    class BubbleManager
    {
    public:
        // spatialQueries: Sprint-002 read-only entity-position source (§7.6, B6;
        //   reused, not redesigned).
        // playerSource:   Sprint-004 positions-only player source (§3A, B11).
        // Both non-owning and must outlive the manager. Defaulted to nullptr so
        // earlier construction stays valid; Update() treats a missing source as
        // "no active region".
        explicit BubbleManager(BubbleConfiguration configuration,
                               const ISpatialQueries* spatialQueries = nullptr,
                               const IPlayerPositionSource* playerSource = nullptr);

        // --- Activation computation (Sprint-004 §7.3/§7.6; Design Review §3C) ---
        // Recomputes the online set for the given simulation tick: union of the
        // players' activation spheres, deterministic ascending order, squared-
        // distance comparisons, and hysteresis (R_on to enter, R_off to leave,
        // decided against the prior online set). Sole writer of the activation
        // state (B10). Reads both seams read-only; no engine access, no gameplay,
        // no ALife, no events.
        void Update(std::uint64_t currentTick);

        // Per-entity classification derived from the prior vs current online sets
        // after the last Update (§7.6). Read-only.
        [[nodiscard]] BubbleMembership MembershipOf(EntityId id) const noexcept;

        // --- Activation transitions (Sprint-004 §7.6; Design Review §3C) ------
        // Transitions produced by the last Update, derived strictly from
        // m_previousOnline vs m_online. In ascending EntityId order (B9), stable
        // across runs. These are computed only; the Bubble Manager never notifies,
        // dispatches, or invokes any subsystem (Sprint-005 consumes them).

        // Entities that turned online this Update (previous offline -> current online).
        [[nodiscard]] const std::vector<EntityId>& Activations() const noexcept { return m_activations; }

        // Entities that turned offline this Update (previous online -> current offline).
        [[nodiscard]] const std::vector<EntityId>& Deactivations() const noexcept { return m_deactivations; }

        // --- Read-only accessors -------------------------------------------
        [[nodiscard]] const BubbleConfiguration& Configuration() const noexcept { return m_configuration; }
        [[nodiscard]] const BubbleState& State() const noexcept { return m_state; }
        [[nodiscard]] std::size_t OnlineEntityCount() const noexcept { return m_online.size(); }

        // --- Public query interface (§7.7) ---------------------------------
        // These return safe defaults until the activation algorithm and the
        // player/spatial sources are implemented in later steps.

        // Whether an entity is currently in the unified online set.
        [[nodiscard]] bool IsEntityInside(EntityId id) const noexcept;

        // Whether a world position falls inside the unified region. Requires
        // player positions (a later step); returns false until then.
        [[nodiscard]] bool IsPositionInside(const Vec3& position) const noexcept;

        // The player whose bubble is nearest a position. Requires the player
        // source (a later step); returns nullopt until then.
        [[nodiscard]] std::optional<PlayerPosition> NearestBubble(const Vec3& position) const noexcept;

        // Effective active radius (config activation radius while any player is
        // active, else 0).
        [[nodiscard]] double ActiveBubbleRadius() const noexcept;

        // Number of players contributing to the current bubble.
        [[nodiscard]] std::size_t ActivePlayerCount() const noexcept { return m_state.activePlayerCount; }

        // --- Diagnostics (Sprint-004 §7.9/§7.10 §7.12 equivalents) ----------
        // All read-only, const, deterministic, engine-free, and safe to call at
        // runtime. Human-readable output uses common::Format (no iostream).

        // Snapshot of the active players (forwards the read-only player source),
        // in ascending PlayerId order; empty if no source. Read-only.
        [[nodiscard]] std::vector<PlayerPosition> ActivePlayers() const;

        // The current online set as EntityIds, in canonical ascending order.
        [[nodiscard]] std::vector<EntityId> OnlineEntities() const;

        // Human-readable one-line summary of the current bubble state.
        [[nodiscard]] std::string DescribeState() const;

        // Human-readable dump of the current online entity ids (ascending).
        [[nodiscard]] std::string DumpOnline() const;

        // Human-readable dump of the last Update's activation/deactivation lists.
        [[nodiscard]] std::string DumpTransitions() const;

        // Read-only consistency scan; all-true report on a healthy manager.
        [[nodiscard]] BubbleConsistencyReport ValidateConsistency() const;

    private:
        BubbleConfiguration m_configuration;

        // Read-only seams (B6). Non-owning; never mutated; never expose engine
        // details. Consumed by Update() for entity positions and player positions.
        const ISpatialQueries* m_spatialQueries = nullptr;
        const IPlayerPositionSource* m_playerSource = nullptr;

        BubbleState m_state{};

        // Current and previous online sets, kept sorted ascending by EntityId
        // value. The previous set backs the hysteresis rule (B10) in the compute
        // step. Sorted storage keeps membership tests and iteration deterministic
        // (B9). These hold EntityId values only — never entities (B3).
        std::vector<std::uint32_t> m_online;
        std::vector<std::uint32_t> m_previousOnline;

        // Transitions produced by the last Update, in ascending EntityId order
        // (B9). Derived strictly from m_previousOnline vs m_online; no external
        // state. Rebuilt every Update.
        std::vector<EntityId> m_activations;
        std::vector<EntityId> m_deactivations;
    };
} // namespace stalkermp::world
