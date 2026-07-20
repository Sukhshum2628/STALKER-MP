// STALKER-MP — Script validation (Sprint-013, Step 6)
//
// Pure validators over a candidate script: syntax (via the `ILuaRuntime` seam's load —
// the ONLY runtime use here), API compatibility, version, duplicate registration,
// invalid callbacks, and missing dependencies. Each check returns a `ScriptOutcome`
// value outcome (ADR-007); none mutates authoritative state or the registry. Loading
// orchestration is a later step (Step 11); this validates only.
//
// The validator is stateless (static methods) and decoupled from configuration: the
// caller supplies the current API version and strictness so the pure checks stay
// independent of `LuaConfiguration`.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "stalkermp/lua/ILuaRuntime.h"    // ILuaRuntime (syntax load)
#include "stalkermp/lua/LuaScriptTypes.h" // ScriptId / CallbackId / ScriptOutcome
#include "stalkermp/lua/ScriptRegistry.h" // ScriptRegistry (duplicate / dependency checks)

namespace stalkermp::lua
{
    // Value description of a candidate script to validate (value-only).
    struct ScriptValidationInput
    {
        ScriptId id{};                        // the id the script would register under
        std::uint32_t apiVersion = 0;         // API surface the script requires
        std::vector<std::byte> bytes;         // chunk bytes (for the syntax load)
        std::vector<CallbackId> callbacks;    // callbacks the script declares
        std::vector<ScriptId> dependencies;   // required script ids
    };

    class ScriptValidator
    {
    public:
        // Syntax check via a runtime load. Load failure -> SyntaxError.
        [[nodiscard]] static ScriptOutcome ValidateSyntax(ILuaRuntime& runtime, std::string_view name,
                                                          const std::vector<std::byte>& bytes);

        // API/version compatibility. candidate > current -> ApiIncompatible; when strict,
        // candidate != current -> VersionMismatch; otherwise Ok.
        [[nodiscard]] static ScriptOutcome ValidateVersion(std::uint32_t candidateApiVersion,
                                                           std::uint32_t currentApiVersion, bool strict) noexcept;

        // The id must not already be registered. Present -> DuplicateScript.
        [[nodiscard]] static ScriptOutcome ValidateDuplicate(ScriptId id, const ScriptRegistry& registry);

        // Declared callbacks must all be addressable (non-none). None -> InvalidCallback.
        [[nodiscard]] static ScriptOutcome ValidateCallbacks(const std::vector<CallbackId>& callbacks);

        // Every declared dependency must be present in the registry (and non-none).
        // Absent/none -> MissingDependency.
        [[nodiscard]] static ScriptOutcome ValidateDependencies(const std::vector<ScriptId>& dependencies,
                                                                const ScriptRegistry& registry);

        // Runs all checks in a fixed deterministic order, short-circuiting at the first
        // failure: duplicate -> version -> callbacks -> dependencies -> syntax. Ok when
        // all pass.
        [[nodiscard]] static ScriptOutcome Validate(const ScriptValidationInput& input, ILuaRuntime& runtime,
                                                    const ScriptRegistry& registry, std::uint32_t currentApiVersion,
                                                    bool strictApiVersion, std::string_view name);
    };
} // namespace stalkermp::lua
