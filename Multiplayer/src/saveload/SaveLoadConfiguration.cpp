// STALKER-MP — Save/Load configuration parsing (Sprint-012, Step 2)
//
// See SaveLoadConfiguration.h. Engine-free / OS-free; no exceptions, Expected<T>.
// Mirrors the Sprint-008…011 *Config::FromConfig parse/validate style. The save
// directory is an opaque token — no path resolution is performed here.

#include "stalkermp/saveload/SaveLoadConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::saveload
{
    namespace
    {
        constexpr const char* kSection = "saveload";

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
                        common::Format("[saveload] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<SaveLoadConfiguration> SaveLoadConfiguration::FromConfig(const core::ConfigStore& config)
    {
        SaveLoadConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (const auto token = config.GetString(kSection, "save_directory"))
        {
            if (token->empty())
            {
                return core::MakeError(core::ErrorCode::InvalidArgument,
                                       "[saveload] save_directory must be a non-empty token");
            }
            result.saveDirectoryToken = *token;
        }
        if (auto r = ReadU32(config, "max_generations", result.maxGenerations, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (const auto b = config.GetBool(kSection, "load_on_startup"))
        {
            result.loadOnStartup = *b;
        }
        if (const auto b = config.GetBool(kSection, "strict_integrity"))
        {
            result.strictIntegrity = *b;
        }
        if (const auto b = config.GetBool(kSection, "migration_enabled"))
        {
            result.migrationEnabled = *b;
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules (defensive; per-field minimums already enforce these).
        if (result.maxGenerations < 1 || result.version < 1 || result.saveDirectoryToken.empty())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[saveload] maxGenerations/version must be >= 1; save_directory non-empty");
        }

        return result;
    }
} // namespace stalkermp::saveload
