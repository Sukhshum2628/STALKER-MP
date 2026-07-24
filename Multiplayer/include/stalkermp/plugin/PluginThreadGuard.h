// STALKER-MP — Plugin thread-safety guard (Sprint-014, Step 16)
//
// A deterministic, engine-free guard enforcing the project's single-threaded model for the
// plugin runtime (ADR-011 / scope §7.9): every plugin entry point MUST run on the
// sanctioned Simulation Thread, and worker threads must NEVER enter a plugin (snapshots
// remain the only worker channel). The guard binds an opaque owner "thread token" (the real
// thread identity is supplied by the composition root at Step-17; tests supply simulated
// tokens) and verifies every entry against it. A violation is a value outcome (ADR-007) and
// is counted; nothing here creates a thread, blocks, locks, uses an atomic, uses TLS, or
// calls an OS API.
//
// This step introduces the guard ONLY — no integration into the manager/host entry points
// (Step 17), no threading-model change.
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; no
// thread created and no threading primitive used (ADR-011).

#pragma once

#include <cstdint>

#include "stalkermp/plugin/PluginTypes.h" // PluginOutcome

namespace stalkermp::plugin
{
    class PluginThreadGuard
    {
    public:
        // Bind the sanctioned owner (Simulation Thread) token. Call once, from the owner.
        void Bind(std::uint64_t ownerToken) noexcept
        {
            m_owner = ownerToken;
            m_bound = true;
        }

        void Reset() noexcept
        {
            m_bound = false;
            m_owner = 0;
            m_violations = 0;
        }

        [[nodiscard]] bool IsBound() const noexcept { return m_bound; }

        // Whether `callerToken` is the bound owner.
        [[nodiscard]] bool IsOnOwner(std::uint64_t callerToken) const noexcept
        {
            return m_bound && callerToken == m_owner;
        }

        // Verify a plugin entry: Ok on the owner thread; RuntimeError if unbound or from a
        // non-owner (e.g. a worker) thread. A violation is counted (diagnostic).
        [[nodiscard]] PluginOutcome Verify(std::uint64_t callerToken) noexcept
        {
            if (m_bound && callerToken == m_owner)
            {
                return PluginOutcome::Ok;
            }
            ++m_violations;
            return PluginOutcome::RuntimeError; // single-thread invariant breach
        }

        // Number of verification failures observed (diagnostic; never gates control flow).
        [[nodiscard]] std::uint64_t Violations() const noexcept { return m_violations; }

    private:
        std::uint64_t m_owner = 0;
        std::uint64_t m_violations = 0;
        bool m_bound = false;
    };
} // namespace stalkermp::plugin
