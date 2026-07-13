// STALKER-MP — ALife switch gateway seam (Sprint-005, Architecture §9/§15)
//
// The engine-free boundary through which the Transition Layer drives the engine's
// vanilla ALife online/offline switching. The TransitionManager depends ONLY on
// this abstract interface; the single concrete implementation that touches engine
// headers (adapters::EngineAlifeSwitchGateway) is introduced in a LATER step and
// lives exclusively in EngineAdapters.cpp (One Engine Boundary).
//
// Frozen architecture references:
//   - §9 Interfaces: Apply(commands) -> per-command outcomes (parallel to input);
//     IsOnline(EntityId) -> optional<bool> (nullopt if unknown to the engine).
//   - §15 Engine boundary: this interface is engine-free; only the concrete
//     adapter resolves EntityId -> object and calls the vanilla switch.
//   - ADR-008: the sanctioned mutation (Cooperative Flag Override via
//     set_switch_online/offline) is entirely BEHIND this seam; the seam itself
//     names no engine call and exposes no engine type.
//   - ADR-007: no engine headers, no exceptions; results are value outcomes.
//
// This step (Step 2) introduces the interface and an inert null counterpart ONLY
// — no manager, no pipeline, no engine adapter.

#pragma once

#include <optional>
#include <vector>

#include "stalkermp/world/EntityView.h"       // world::EntityId
#include "stalkermp/world/TransitionTypes.h"  // world::TransitionCommand, world::TransitionOutcome

namespace stalkermp::world
{
    // Abstract, engine-free switch gateway (Architecture §9). Implementations
    // never throw; every fallible step is reported as a value TransitionOutcome.
    class IAlifeSwitchGateway
    {
    public:
        virtual ~IAlifeSwitchGateway() = default;

        // Apply an ordered batch of switch commands in a single phase. Returns
        // one TransitionOutcome per command, in the SAME order as the input
        // (result[i] corresponds to commands[i]). Never throws.
        [[nodiscard]] virtual std::vector<TransitionOutcome>
        Apply(const std::vector<TransitionCommand>& commands) = 0;

        // Read-only query of the engine's current online state for reconciliation.
        // Returns nullopt when the entity is unknown to the engine. Never throws.
        [[nodiscard]] virtual std::optional<bool> IsOnline(EntityId id) const = 0;
    };
} // namespace stalkermp::world
