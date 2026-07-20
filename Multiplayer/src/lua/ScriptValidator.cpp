// STALKER-MP — Script validation (Sprint-013, Step 6)
//
// See ScriptValidator.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI;
// value outcomes. The runtime seam is used ONLY for a syntax load; the registry is
// read-only for duplicate and dependency checks. No mutation of authoritative state.

#include "stalkermp/lua/ScriptValidator.h"

namespace stalkermp::lua
{
    ScriptOutcome ScriptValidator::ValidateSyntax(ILuaRuntime& runtime, const std::string_view name,
                                                  const std::vector<std::byte>& bytes)
    {
        // A load compiles the chunk; a failed load is a syntax error (value outcome).
        if (const auto loaded = runtime.LoadChunk(name, bytes); !loaded.HasValue())
        {
            return ScriptOutcome::SyntaxError;
        }
        return ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptValidator::ValidateVersion(const std::uint32_t candidateApiVersion,
                                                   const std::uint32_t currentApiVersion,
                                                   const bool strict) noexcept
    {
        if (candidateApiVersion > currentApiVersion)
        {
            return ScriptOutcome::ApiIncompatible; // requires an unavailable API surface
        }
        if (strict && candidateApiVersion != currentApiVersion)
        {
            return ScriptOutcome::VersionMismatch;
        }
        return ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptValidator::ValidateDuplicate(const ScriptId id, const ScriptRegistry& registry)
    {
        return registry.Contains(id) ? ScriptOutcome::DuplicateScript : ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptValidator::ValidateCallbacks(const std::vector<CallbackId>& callbacks)
    {
        for (const CallbackId cb : callbacks)
        {
            if (cb.IsNone())
            {
                return ScriptOutcome::InvalidCallback;
            }
        }
        return ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptValidator::ValidateDependencies(const std::vector<ScriptId>& dependencies,
                                                        const ScriptRegistry& registry)
    {
        for (const ScriptId dep : dependencies)
        {
            if (dep.IsNone() || !registry.Contains(dep))
            {
                return ScriptOutcome::MissingDependency;
            }
        }
        return ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptValidator::Validate(const ScriptValidationInput& input, ILuaRuntime& runtime,
                                            const ScriptRegistry& registry, const std::uint32_t currentApiVersion,
                                            const bool strictApiVersion, const std::string_view name)
    {
        // Fixed deterministic order; short-circuit at the first failure. Cheap value
        // checks precede the runtime syntax load.
        if (const auto o = ValidateDuplicate(input.id, registry); o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateVersion(input.apiVersion, currentApiVersion, strictApiVersion);
            o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateCallbacks(input.callbacks); o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateDependencies(input.dependencies, registry); o != ScriptOutcome::Ok)
        {
            return o;
        }
        return ValidateSyntax(runtime, name, input.bytes);
    }
} // namespace stalkermp::lua
