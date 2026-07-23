// STALKER-MP — Script thread-safety guard (Sprint-013, Step 16)
//
// A deterministic, engine-free guard enforcing the project's single-threaded model for
// scripting: every script entry point MUST run on the sanctioned Simulation Thread, and
// worker threads must NEVER enter scripts (snapshots remain the only worker channel —
// 09_Threading.md, RFC-0006). The guard binds an opaque owner "thread token" (the real
// thread identity is supplied by the composition root at Step-17; tests supply simulated
// tokens) and verifies every entry against it. A violation is a value outcome (ADR-007)
// and is counted; nothing here creates a thread, blocks, or locks.
//
// This step introduces the guard ONLY — no integration into the manager entry points
// (Step 17), no threading-model change.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; no
// thread created (ADR-011).

#pragma once

#include <cstdint>

#include "stalkermp/lua/LuaScriptTypes.h" // ScriptOutcome

namespace stalkermp::lua
{
    class ScriptThreadGuard
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

        // Verify a script entry: Ok on the owner thread; RuntimeError if unbound or from a
        // non-owner (e.g. a worker) thread. A violation is counted (diagnostic).
        [[nodiscard]] ScriptOutcome Verify(std::uint64_t callerToken) noexcept
        {
            if (m_bound && callerToken == m_owner)
            {
                return ScriptOutcome::Ok;
            }
            ++m_violations;
            return ScriptOutcome::RuntimeError; // single-thread invariant breach
        }

        // Number of verification failures observed (diagnostic; never gates control flow).
        [[nodiscard]] std::uint64_t Violations() const noexcept { return m_violations; }

    private:
        std::uint64_t m_owner = 0;
        std::uint64_t m_violations = 0;
        bool m_bound = false;
    };
} // namespace stalkermp::lua
