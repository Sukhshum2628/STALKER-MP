// STALKER-MP — Plugin configuration parsing (Sprint-014, Step 2)
//
// See PluginConfiguration.h. Engine-free / loader-free / OS-free; no exceptions,
// Expected<T>. Mirrors the Sprint-011…013 *Config::FromConfig parse/validate style. The
// plugin directory is an opaque token — no path resolution is performed here.

#include "stalkermp/plugin/PluginConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::plugin
{
    namespace
    {
        constexpr const char* kSection = "plugins";

        [[nodiscard]] core::Expected<void>
        ReadU32(const core::ConfigStore& config, const char* key, std::uint32_t& target, std::int64_t min,
                std::int64_t max)
        {
            if (const auto raw = config.GetInt(kSection, key))
            {
                if (*raw < min || *raw > max)
                {
                    return core::MakeError(
                        core::ErrorCode::InvalidArgument,
                        common::Format("[plugins] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<PluginConfiguration> PluginConfiguration::FromConfig(const core::ConfigStore& config)
    {
        PluginConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (const auto token = config.GetString(kSection, "plugin_directory"))
        {
            if (token->empty())
            {
                return core::MakeError(core::ErrorCode::InvalidArgument,
                                       "[plugins] plugin_directory must be a non-empty token");
            }
            result.pluginDirectoryToken = *token;
        }
        if (auto r = ReadU32(config, "max_plugins", result.maxPlugins, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (const auto b = config.GetBool(kSection, "enabled"))
        {
            result.enabled = *b;
        }
        if (const auto b = config.GetBool(kSection, "validate_on_load"))
        {
            result.validateOnLoad = *b;
        }
        if (const auto b = config.GetBool(kSection, "strict_api_version"))
        {
            result.strictApiVersion = *b;
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules (defensive; per-field minimums already enforce these).
        if (result.maxPlugins < 1 || result.version < 1 || result.pluginDirectoryToken.empty())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[plugins] maxPlugins/version must be >= 1; plugin_directory non-empty");
        }

        return result;
    }
} // namespace stalkermp::plugin
