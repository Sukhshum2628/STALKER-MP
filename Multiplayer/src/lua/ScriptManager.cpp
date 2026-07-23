// STALKER-MP — Script manager (Sprint-013, Steps 13-14)
//
// See ScriptManager.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI; value
// outcomes. Deterministic dispatch order; per-script fault isolation. The ILuaRuntime
// seam is a test double in this build — no VM is instantiated and no real script runs.

#include "stalkermp/lua/ScriptManager.h"

#include <algorithm>

#include "stalkermp/lua/ScriptContext.h"

namespace stalkermp::lua
{
    namespace
    {
        // All ten host-dispatched events (ScriptEvent 0..OnTick), for whole-script unbind.
        constexpr std::uint8_t kEventCount = static_cast<std::uint8_t>(ScriptEvent::OnTick) + 1;
    } // namespace

    ScriptLoadReport ScriptManager::LoadAll()
    {
        ScriptLoader loader(m_source, m_runtime, m_registry, m_apiVersion, m_strict);
        return loader.LoadAll();
    }

    bool ScriptManager::IsDisabled(const ScriptId id) const noexcept
    {
        const auto it = std::lower_bound(m_disabled.begin(), m_disabled.end(), id);
        return it != m_disabled.end() && *it == id;
    }

    void ScriptManager::Disable(const ScriptId id)
    {
        const auto it = std::lower_bound(m_disabled.begin(), m_disabled.end(), id);
        if (it == m_disabled.end() || *it != id)
        {
            m_disabled.insert(it, id); // keep sorted, dedup
        }
        if (ScriptContext* c = m_registry.Find(id))
        {
            c->state = ScriptState::Failed;
        }
    }

    ScriptOutcome ScriptManager::Unload(const ScriptId id)
    {
        if (!m_registry.Contains(id))
        {
            return ScriptOutcome::NotFound;
        }
        // Unbind every callback this script bound, across all events.
        for (std::uint8_t e = 0; e < kEventCount; ++e)
        {
            const auto event = static_cast<ScriptEvent>(e);
            for (const EventCallback& cb : m_events.CallbacksFor(event))
            {
                if (cb.script == id)
                {
                    (void)m_events.Unbind(event, id, cb.callback);
                }
            }
        }
        // Clear any disabled flag.
        const auto it = std::lower_bound(m_disabled.begin(), m_disabled.end(), id);
        if (it != m_disabled.end() && *it == id)
        {
            m_disabled.erase(it);
        }
        return m_registry.Unregister(id);
    }

    ScriptOutcome ScriptManager::Reload(const ScriptId id)
    {
        // Locate the descriptor in the source.
        const std::vector<ScriptDescriptor> descriptors = m_source.Enumerate();
        const ScriptDescriptor* found = nullptr;
        for (const ScriptDescriptor& d : descriptors)
        {
            if (d.id == id)
            {
                found = &d;
                break;
            }
        }
        if (found == nullptr)
        {
            return ScriptOutcome::NotFound;
        }
        // Unload if currently loaded (benign if not), then load fresh.
        if (m_registry.Contains(id))
        {
            (void)Unload(id);
        }
        ScriptLoader loader(m_source, m_runtime, m_registry, m_apiVersion, m_strict);
        return loader.LoadOne(*found);
    }

    ScriptOutcome ScriptManager::BindCallback(const ScriptEvent event, const ScriptId script,
                                              const CallbackId callback)
    {
        ScriptContext* context = m_registry.Find(script);
        if (context == nullptr)
        {
            return ScriptOutcome::NotFound;
        }
        const ScriptOutcome o = m_events.Bind(event, script, callback);
        if (o == ScriptOutcome::Ok)
        {
            context->callbacks.push_back(callback);
        }
        return o;
    }

    ScriptDispatchReport ScriptManager::DispatchEvent(const ScriptEvent event, const ScriptArgs& args)
    {
        ScriptDispatchReport report;
        // CallbacksFor is deterministic ascending by (script, callback).
        for (const EventCallback& cb : m_events.CallbacksFor(event))
        {
            if (IsDisabled(cb.script) || !m_registry.Contains(cb.script))
            {
                ++report.skipped; // disabled or missing script -> isolated skip
                continue;
            }
            const ScriptOutcome outcome = m_runtime.InvokeCallback(cb.callback, args);
            ++report.invoked;
            if (IsFault(outcome))
            {
                Disable(cb.script); // fault isolation: engine continues, script disabled
                ++report.isolated;
            }
        }
        return report;
    }

    std::vector<std::string> ScriptManager::Dependencies() const
    {
        // Ordering-only edges: scripting runs after the simulation producers and before
        // the snapshot producer (Gameplay phase, [AR-1]). The concrete tick subscription
        // is Step-17; these names only order registration.
        return {"World", "EntityRegistry", "PlayerManager", "BubbleManager", "TransitionManager"};
    }

    core::Expected<void> ScriptManager::Initialize()
    {
        return core::Success();
    }

    void ScriptManager::Shutdown() noexcept
    {
        m_disabled.clear();
    }

    void ScriptManager::Tick(const double deltaSeconds)
    {
        (void)deltaSeconds; // DIAGNOSTIC only; never in control flow (deterministic)
        ++m_tick;
        ScriptArgs args{};
        args.count = 1;
        args.words[0] = m_tick; // deterministic tick counter, not wall-clock
        (void)DispatchEvent(ScriptEvent::OnTick, args);
    }
} // namespace stalkermp::lua
