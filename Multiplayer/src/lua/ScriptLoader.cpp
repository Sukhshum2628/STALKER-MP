// STALKER-MP — Script loader (Sprint-013, Step 11)
//
// See ScriptLoader.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI; value
// outcomes. Deterministic ascending order; each script is loaded independently so a
// single failure isolates only that script.

#include "stalkermp/lua/ScriptLoader.h"

#include <string>

#include "stalkermp/lua/ScriptContext.h"    // ScriptContext (Step 4)
#include "stalkermp/lua/ScriptLifecycle.h"  // ScriptLifecycle (Step 10)
#include "stalkermp/lua/ScriptValidator.h"  // ScriptValidator (Step 6)

namespace stalkermp::lua
{
    ScriptOutcome ScriptLoader::LoadOne(const ScriptDescriptor& descriptor)
    {
        // 1) Read the script bytes from the source.
        auto bytes = m_source.Read(descriptor.id);
        if (!bytes.HasValue())
        {
            return ScriptOutcome::LoadFailed;
        }

        // 2) Validate the checks the loader has data for (Step 6): must be new to the
        //    registry, and API-version compatible. (Callback/dependency validation is
        //    declared by the script at a later stage; nothing to check at load.)
        if (const auto o = ScriptValidator::ValidateDuplicate(descriptor.id, m_registry); o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ScriptValidator::ValidateVersion(descriptor.apiVersion, m_currentApiVersion,
                                                            m_strictApiVersion);
            o != ScriptOutcome::Ok)
        {
            return o;
        }

        // 3) Load the chunk into the runtime seam (Step 3). This compiles the chunk (the
        //    syntax check) and yields a runtime handle. NEVER Execute here.
        const std::string name = "script_" + std::to_string(descriptor.id.value);
        if (!m_runtime.LoadChunk(name, bytes.Value()).HasValue())
        {
            return ScriptOutcome::SyntaxError;
        }

        // 4) Build the context and advance its lifecycle: Unloaded -> Loaded ->
        //    Initialized (deterministic, Step 10). Execution is a later step.
        ScriptContext context;
        context.id = descriptor.id;
        context.descriptor = descriptor;
        context.state = ScriptState::Unloaded;
        if (const auto o = ScriptLifecycle::Apply(context.state, LifecycleAction::Load); o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ScriptLifecycle::Apply(context.state, LifecycleAction::Initialize);
            o != ScriptOutcome::Ok)
        {
            return o;
        }

        // 5) Register the initialized context.
        return m_registry.Register(context);
    }

    ScriptLoadReport ScriptLoader::LoadAll()
    {
        ScriptLoadReport report;
        const std::vector<ScriptDescriptor> descriptors = m_source.Enumerate(); // ascending by id
        for (const ScriptDescriptor& d : descriptors)
        {
            ++report.attempted;
            const ScriptOutcome outcome = LoadOne(d);
            if (outcome == ScriptOutcome::Ok)
            {
                ++report.loaded;
            }
            else
            {
                ++report.failed;
                report.failures.emplace_back(d.id, outcome); // isolate; continue
            }
        }
        return report;
    }
} // namespace stalkermp::lua
