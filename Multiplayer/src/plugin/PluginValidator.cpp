// STALKER-MP — Plugin validation (Sprint-014, Step 6)
//
// See PluginValidator.h. Engine-free / loader-free / OS-free; no exceptions, no RTTI;
// value outcomes. The registry is read-only (duplicate check); no mutation of state.

#include "stalkermp/plugin/PluginValidator.h"

#include <algorithm>

namespace stalkermp::plugin
{
    PluginOutcome PluginValidator::ValidateConfiguration(const PluginConfiguration& config)
    {
        return config.enabled ? PluginOutcome::Ok : PluginOutcome::PluginDisabled;
    }

    PluginOutcome PluginValidator::ValidateDescriptor(const PluginDescriptor& descriptor)
    {
        if (descriptor.id.IsNone())
        {
            return PluginOutcome::NotFound; // an addressable id is required
        }
        if (descriptor.minApiVersion > descriptor.maxApiVersion)
        {
            return PluginOutcome::VersionMismatch; // inverted supported-API range
        }
        return PluginOutcome::Ok;
    }

    PluginOutcome PluginValidator::ValidateDuplicate(const PluginId id, const PluginRegistry& registry)
    {
        return registry.Contains(id) ? PluginOutcome::DuplicatePlugin : PluginOutcome::Ok;
    }

    PluginOutcome PluginValidator::ValidateVersion(const PluginDescriptor& descriptor,
                                                   const std::uint32_t hostApiVersion, const bool strict) noexcept
    {
        if (hostApiVersion < descriptor.minApiVersion)
        {
            return PluginOutcome::ApiIncompatible; // plugin requires a newer host API
        }
        if (strict && hostApiVersion > descriptor.maxApiVersion)
        {
            return PluginOutcome::VersionMismatch; // plugin too old for this host (strict)
        }
        return PluginOutcome::Ok;
    }

    PluginOutcome PluginValidator::ValidateRequiredApis(const std::vector<std::uint32_t>& required,
                                                        const std::vector<std::uint32_t>& available)
    {
        for (const std::uint32_t api : required)
        {
            if (std::find(available.begin(), available.end(), api) == available.end())
            {
                return PluginOutcome::ApiIncompatible; // a required capability is unavailable
            }
        }
        return PluginOutcome::Ok;
    }

    PluginOutcome PluginValidator::ValidateDependencies(const PluginDescriptor& descriptor)
    {
        const std::vector<PluginId>& deps = descriptor.optionalDependencies;
        for (std::size_t i = 0; i < deps.size(); ++i)
        {
            if (deps[i].IsNone() || deps[i] == descriptor.id) // none id or self-reference
            {
                return PluginOutcome::MissingDependency;
            }
            for (std::size_t j = i + 1; j < deps.size(); ++j) // duplicate declaration
            {
                if (deps[i] == deps[j])
                {
                    return PluginOutcome::MissingDependency;
                }
            }
        }
        return PluginOutcome::Ok;
    }

    PluginOutcome PluginValidator::Validate(const PluginDescriptor& descriptor, const PluginRegistry& registry,
                                            const std::vector<std::uint32_t>& availableApis,
                                            const PluginConfiguration& config)
    {
        // Fixed deterministic order; short-circuit at the first failure.
        if (const auto o = ValidateConfiguration(config); o != PluginOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateDescriptor(descriptor); o != PluginOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateDuplicate(descriptor.id, registry); o != PluginOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateVersion(descriptor, config.version, config.strictApiVersion);
            o != PluginOutcome::Ok)
        {
            return o;
        }
        if (const auto o = ValidateRequiredApis(descriptor.requiredApis, availableApis); o != PluginOutcome::Ok)
        {
            return o;
        }
        return ValidateDependencies(descriptor);
    }
} // namespace stalkermp::plugin
