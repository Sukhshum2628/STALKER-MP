// STALKER-MP — Recording script API facades (Sprint-013, Step 9)
//
// Engine-free test double implementing all seven Public API facade seams. Reads return
// scriptable canned values; controlled writes and log calls are recorded into public
// vectors for inspection (mirrors the project's recording-double convention, e.g.
// RecordingRestoreSinks). No VM, no engine, no OS — exactly what the engine-free build
// uses in place of the real EngineAdapters facades (Step 17).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stalkermp/core/Error.h"      // core::MakeError / ErrorCode
#include "stalkermp/lua/ScriptApi.h"

namespace stalkermp::lua
{
    class RecordingScriptApi final : public IWorldApi,
                                     public IEnvironmentApi,
                                     public IEntityApi,
                                     public IPlayerApi,
                                     public IInventoryApi,
                                     public ILoggingApi,
                                     public IConfigApi
    {
    public:
        // --- Scriptable canned read state ------------------------------------
        std::uint64_t simulationTick = 0;
        std::uint32_t timeOfDaySeconds = 0;
        std::size_t entityCount = 0;
        std::size_t playerCount = 0;
        std::size_t itemCount = 0;
        bool entityExistsResult = false;
        EnvironmentInfo environment{};
        EntityInfo entityInfo{};
        PlayerInfo playerInfo{};
        bool entityQueryFound = true;
        bool playerQueryFound = true;
        ScriptOutcome writeOutcome = ScriptOutcome::Ok; // forced outcome for controlled writes

        // --- Recorded controlled writes / effects ----------------------------
        std::vector<std::uint32_t> setWeatherCalls;
        struct SetPositionCall
        {
            world::EntityId id{};
            world::Vec3 position{};
        };
        std::vector<SetPositionCall> setPositionCalls;
        struct GiveItemCall
        {
            world::EntityId owner{};
            std::uint32_t itemType = 0;
            std::uint32_t count = 0;
        };
        std::vector<GiveItemCall> giveItemCalls;
        struct LogCall
        {
            ScriptLogLevel level = ScriptLogLevel::Info;
            std::string category;
            std::string message;
        };
        std::vector<LogCall> logCalls;

        // --- IWorldApi -------------------------------------------------------
        [[nodiscard]] std::uint64_t SimulationTick() const override { return simulationTick; }
        [[nodiscard]] std::uint32_t TimeOfDaySeconds() const override { return timeOfDaySeconds; }
        [[nodiscard]] std::size_t EntityCount() const override { return entityCount; }
        [[nodiscard]] bool EntityExists(world::EntityId) const override { return entityExistsResult; }

        // --- IEnvironmentApi -------------------------------------------------
        [[nodiscard]] EnvironmentInfo Current() const override { return environment; }
        [[nodiscard]] ScriptOutcome SetWeather(std::uint32_t weatherId) override
        {
            setWeatherCalls.push_back(weatherId);
            return writeOutcome;
        }

        // --- IEntityApi ------------------------------------------------------
        [[nodiscard]] core::Expected<EntityInfo> Query(world::EntityId) const override
        {
            if (!entityQueryFound)
            {
                return core::MakeError(core::ErrorCode::NotFound, "entity not found");
            }
            return entityInfo;
        }
        [[nodiscard]] ScriptOutcome SetPosition(world::EntityId id, const world::Vec3& position) override
        {
            setPositionCalls.push_back(SetPositionCall{id, position});
            return writeOutcome;
        }

        // --- IPlayerApi ------------------------------------------------------
        [[nodiscard]] std::size_t PlayerCount() const override { return playerCount; }
        [[nodiscard]] core::Expected<PlayerInfo> Query(player::PlayerId) const override
        {
            if (!playerQueryFound)
            {
                return core::MakeError(core::ErrorCode::NotFound, "player not found");
            }
            return playerInfo;
        }

        // --- IInventoryApi ---------------------------------------------------
        [[nodiscard]] std::size_t ItemCount(world::EntityId) const override { return itemCount; }
        [[nodiscard]] ScriptOutcome GiveItem(world::EntityId owner, std::uint32_t itemType,
                                             std::uint32_t count) override
        {
            giveItemCalls.push_back(GiveItemCall{owner, itemType, count});
            return writeOutcome;
        }

        // --- ILoggingApi -----------------------------------------------------
        void Log(ScriptLogLevel level, std::string_view category, std::string_view message) override
        {
            logCalls.push_back(LogCall{level, std::string(category), std::string(message)});
        }

        // --- IConfigApi ------------------------------------------------------
        std::string configString;
        std::int64_t configInt = 0;
        bool configBool = false;
        bool configFound = true;

        [[nodiscard]] core::Expected<std::string> GetString(std::string_view, std::string_view) const override
        {
            if (!configFound)
            {
                return core::MakeError(core::ErrorCode::NotFound, "config key not found");
            }
            return configString;
        }
        [[nodiscard]] core::Expected<std::int64_t> GetInt(std::string_view, std::string_view) const override
        {
            if (!configFound)
            {
                return core::MakeError(core::ErrorCode::NotFound, "config key not found");
            }
            return configInt;
        }
        [[nodiscard]] core::Expected<bool> GetBool(std::string_view, std::string_view) const override
        {
            if (!configFound)
            {
                return core::MakeError(core::ErrorCode::NotFound, "config key not found");
            }
            return configBool;
        }

        // Expose the whole set as a ScriptApiSet (all seams are this one instance).
        [[nodiscard]] ScriptApiSet AsSet() noexcept
        {
            return ScriptApiSet{*this, *this, *this, *this, *this, *this, *this};
        }
    };
} // namespace stalkermp::lua
