// STALKER-MP — Lua configuration parsing (Sprint-013, Step 2)
//
// See LuaConfiguration.h. Engine-free / VM-free / OS-free; no exceptions, Expected<T>.
// Mirrors the Sprint-008…012 *Config::FromConfig parse/validate style. The script
// directory is an opaque token — no path resolution is performed here.

#include "stalkermp/lua/LuaConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::lua
{
    namespace
    {
        constexpr const char* kSection = "lua";

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
                        common::Format("[lua] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<LuaConfiguration> LuaConfiguration::FromConfig(const core::ConfigStore& config)
    {
        LuaConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (const auto token = config.GetString(kSection, "script_directory"))
        {
            if (token->empty())
            {
                return core::MakeError(core::ErrorCode::InvalidArgument,
                                       "[lua] script_directory must be a non-empty token");
            }
            result.scriptDirectoryToken = *token;
        }
        if (auto r = ReadU32(config, "max_scripts", result.maxScripts, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "execution_budget_micros", result.executionBudgetMicros, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "recursion_limit", result.recursionLimit, 1, 0xFFFFFFFFll); !r)
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
        if (result.maxScripts < 1 || result.executionBudgetMicros < 1 || result.recursionLimit < 1 ||
            result.version < 1 || result.scriptDirectoryToken.empty())
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                "[lua] maxScripts/executionBudgetMicros/recursionLimit/version must be >= 1; "
                "script_directory non-empty");
        }

        return result;
    }
} // namespace stalkermp::lua
