// STALKER-MP — Plugin validation (Sprint-014, Step 6)
//
// Pure validators over a candidate plugin's manifest (descriptor): configuration
// admissibility, descriptor/identifier well-formedness, duplicate registration, API
// version negotiation, required-API availability, and dependency-declaration validity.
// Each check returns a `PluginOutcome` value outcome (ADR-007); none mutates authoritative
// state or the registry. No loading, execution, or lifecycle management.
//
// The validator is stateless (static methods) and depends only on previously-implemented
// components (`PluginTypes`, `PluginRegistry`, `PluginConfiguration`).
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/plugin/PluginConfiguration.h" // PluginConfiguration
#include "stalkermp/plugin/PluginRegistry.h"      // PluginRegistry
#include "stalkermp/plugin/PluginTypes.h"         // PluginDescriptor / PluginId / PluginOutcome

namespace stalkermp::plugin
{
    class PluginValidator
    {
    public:
        // The framework must be enabled for a plugin to be admissible. Disabled -> PluginDisabled.
        [[nodiscard]] static PluginOutcome ValidateConfiguration(const PluginConfiguration& config);

        // Descriptor/identifier well-formedness: a none (0) id -> NotFound; an inverted
        // API-version range (minApiVersion > maxApiVersion) -> VersionMismatch.
        [[nodiscard]] static PluginOutcome ValidateDescriptor(const PluginDescriptor& descriptor);

        // The id must not already be registered. Present -> DuplicatePlugin.
        [[nodiscard]] static PluginOutcome ValidateDuplicate(PluginId id, const PluginRegistry& registry);

        // API version negotiation against the host API version. host < min -> ApiIncompatible
        // (plugin needs a newer host); when strict, host > max -> VersionMismatch (plugin too
        // old for this host); otherwise Ok.
        [[nodiscard]] static PluginOutcome ValidateVersion(const PluginDescriptor& descriptor,
                                                           std::uint32_t hostApiVersion, bool strict) noexcept;

        // Every required-API capability id must be available on the host. Missing -> ApiIncompatible.
        [[nodiscard]] static PluginOutcome ValidateRequiredApis(const std::vector<std::uint32_t>& required,
                                                                const std::vector<std::uint32_t>& available);

        // Dependency-declaration validity: each declared (optional) dependency must be a
        // non-none id, not a self-reference, and not duplicated. Violation -> MissingDependency.
        [[nodiscard]] static PluginOutcome ValidateDependencies(const PluginDescriptor& descriptor);

        // Runs all checks in a fixed deterministic order, short-circuiting at the first
        // failure: configuration -> descriptor -> duplicate -> version -> required-APIs ->
        // dependencies. Uses config.version as the host API version and config.strictApiVersion.
        [[nodiscard]] static PluginOutcome Validate(const PluginDescriptor& descriptor,
                                                    const PluginRegistry& registry,
                                                    const std::vector<std::uint32_t>& availableApis,
                                                    const PluginConfiguration& config);
    };
} // namespace stalkermp::plugin
