// STALKER-MP — Null ALife switch gateway (Sprint-005, Architecture §6/§9)
//
// Inert, engine-free counterpart of the (later-step) EngineAlifeSwitchGateway.
// It implements the IAlifeSwitchGateway seam WITHOUT any engine dependency so the
// engine-free test build and the composition root stay identical in shape while
// no engine is present. It performs no real switching.
//
// Test-controllable behavior (kept minimal for Step 2):
//   - defaultOutcome: the outcome reported for every applied command
//     (default AlreadyInState — a no-op, per the idempotency posture in §13).
//   - SetOnline(id, bool): seed the online-state map consulted by IsOnline;
//     IsOnline returns nullopt for ids never seeded (unknown to the "engine").
//   - applied: an append-only log of commands seen by Apply, for test inspection.
//
// ADR-007: no engine headers, no exceptions, value outcomes only.
// ADR-008: names no engine call; the sanctioned mutation lives only in the real
// adapter (a later step). This null gateway mutates nothing.
//
// Header-only and directly instantiable so unit tests can construct and configure
// it without any factory or link-time engine plumbing.

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "stalkermp/world/EntityView.h"           // world::EntityId
#include "stalkermp/world/IAlifeSwitchGateway.h"   // world::IAlifeSwitchGateway
#include "stalkermp/world/TransitionTypes.h"       // world::TransitionCommand, world::TransitionOutcome

namespace stalkermp::adapters
{
    class NullAlifeSwitchGateway final : public world::IAlifeSwitchGateway
    {
    public:
        // Outcome reported for each applied command. Default is a benign no-op.
        world::TransitionOutcome defaultOutcome = world::TransitionOutcome::AlreadyInState;

        // Append-only record of commands passed to Apply (for test inspection).
        std::vector<world::TransitionCommand> applied;

        NullAlifeSwitchGateway() = default;

        // Seed the online-state map used by IsOnline. Ids never seeded are
        // reported as unknown (nullopt).
        void SetOnline(const world::EntityId id, const bool online)
        {
            m_online[id.value] = online;
        }

        // Clear seeded online state and the applied-command log.
        void Reset()
        {
            m_online.clear();
            applied.clear();
        }

        [[nodiscard]] std::vector<world::TransitionOutcome>
        Apply(const std::vector<world::TransitionCommand>& commands) override
        {
            std::vector<world::TransitionOutcome> outcomes;
            outcomes.reserve(commands.size());
            for (const world::TransitionCommand& command : commands)
            {
                applied.push_back(command);
                outcomes.push_back(defaultOutcome);
            }
            return outcomes; // one outcome per command, input order preserved
        }

        [[nodiscard]] std::optional<bool> IsOnline(const world::EntityId id) const override
        {
            const auto it = m_online.find(id.value);
            if (it == m_online.end())
            {
                return std::nullopt; // unknown to the "engine"
            }
            return it->second;
        }

    private:
        std::unordered_map<std::uint32_t, bool> m_online;
    };
} // namespace stalkermp::adapters
