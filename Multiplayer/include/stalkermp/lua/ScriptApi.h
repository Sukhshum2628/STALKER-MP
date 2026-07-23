// STALKER-MP — Public scripting API facade seams (Sprint-013, Step 9)
//
// The engine-free Public API surface through which scripts observe and (in controlled,
// validated ways) affect the authoritative world. This is the ADR-008 boundary for
// scripting: scripts talk ONLY to these value-only facades; the REAL implementations
// are added at Step-17 in EngineAdapters.cpp and route every read/effect through the
// sanctioned existing seams per [AR-4] (World/Environment → Sprint-002 + Sprint-008;
// Entity → Sprint-003; ALife → Sprint-005; Player/spawn → Sprint-007). No internal
// engine object, engine header, or engine type is EVER exposed here (scope §7.4).
//
// Every operation exchanges value ids / value structs only; controlled writes return a
// `ScriptOutcome` value outcome (ADR-007). Read operations that can fail return
// `Expected<T>`. This step introduces the SEAMS ONLY — no engine implementation
// (Step 17), no VM binding (Step 12).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "stalkermp/core/Expected.h"          // core::Expected
#include "stalkermp/lua/LuaScriptTypes.h"     // ScriptOutcome
#include "stalkermp/player/PlayerTypes.h"     // player::PlayerId / PlayerConnectionState (value only)
#include "stalkermp/world/EntityView.h"       // world::EntityId / world::Vec3 (value only)

namespace stalkermp::lua
{
    // ------------------------------------------------------------- Value shapes
    struct EnvironmentInfo
    {
        std::uint32_t weatherId = 0;
        std::uint32_t timeOfDaySeconds = 0;
        std::uint32_t lighting = 0;
    };

    struct EntityInfo
    {
        world::EntityId id{};
        world::Vec3 position{};
        std::uint32_t stateFlags = 0;
    };

    struct PlayerInfo
    {
        player::PlayerId id{};
        world::EntityId entity{};
        std::uint32_t factionId = 0;
        player::PlayerConnectionState connection = player::PlayerConnectionState::Connected;
    };

    // Script-facing log severity (decoupled from the core logger — a scripting API must
    // not force engine/core enums on script authors).
    enum class ScriptLogLevel : std::uint8_t
    {
        Info = 0,
        Warning,
        Error,
    };

    // ------------------------------------------------------------- Facade seams

    // World queries + simulation time + entity lookup (§7.4 World). Read-only.
    class IWorldApi
    {
    public:
        virtual ~IWorldApi() = default;
        [[nodiscard]] virtual std::uint64_t SimulationTick() const = 0;
        [[nodiscard]] virtual std::uint32_t TimeOfDaySeconds() const = 0;
        [[nodiscard]] virtual std::size_t EntityCount() const = 0;
        [[nodiscard]] virtual bool EntityExists(world::EntityId id) const = 0;
    };

    // Environment observation + controlled weather change (§7.4 Environment).
    class IEnvironmentApi
    {
    public:
        virtual ~IEnvironmentApi() = default;
        [[nodiscard]] virtual EnvironmentInfo Current() const = 0;
        [[nodiscard]] virtual ScriptOutcome SetWeather(std::uint32_t weatherId) = 0; // controlled write
    };

    // Entity discovery / state inspection + controlled state change (§7.4 Entity).
    class IEntityApi
    {
    public:
        virtual ~IEntityApi() = default;
        [[nodiscard]] virtual core::Expected<EntityInfo> Query(world::EntityId id) const = 0;
        [[nodiscard]] virtual ScriptOutcome SetPosition(world::EntityId id, const world::Vec3& position) = 0;
    };

    // Player info / connection status (§7.4 Player). Read-only.
    class IPlayerApi
    {
    public:
        virtual ~IPlayerApi() = default;
        [[nodiscard]] virtual std::size_t PlayerCount() const = 0;
        [[nodiscard]] virtual core::Expected<PlayerInfo> Query(player::PlayerId id) const = 0;
    };

    // Inventory queries + controlled item grant (§7.4 Inventory).
    class IInventoryApi
    {
    public:
        virtual ~IInventoryApi() = default;
        [[nodiscard]] virtual std::size_t ItemCount(world::EntityId owner) const = 0;
        [[nodiscard]] virtual ScriptOutcome GiveItem(world::EntityId owner, std::uint32_t itemType,
                                                     std::uint32_t count) = 0; // controlled write
    };

    // Structured logging from scripts (§7.4 Logging). Side-effecting only.
    class ILoggingApi
    {
    public:
        virtual ~ILoggingApi() = default;
        virtual void Log(ScriptLogLevel level, std::string_view category, std::string_view message) = 0;
    };

    // Read-only configuration access (§7.4 Configuration).
    class IConfigApi
    {
    public:
        virtual ~IConfigApi() = default;
        [[nodiscard]] virtual core::Expected<std::string> GetString(std::string_view section,
                                                                    std::string_view key) const = 0;
        [[nodiscard]] virtual core::Expected<std::int64_t> GetInt(std::string_view section,
                                                                  std::string_view key) const = 0;
        [[nodiscard]] virtual core::Expected<bool> GetBool(std::string_view section,
                                                           std::string_view key) const = 0;
    };

    // The full set of public API facades, passed together to the (later) VM binding.
    // References only; the owner outlives the binding.
    struct ScriptApiSet
    {
        IWorldApi& world;
        IEnvironmentApi& environment;
        IEntityApi& entity;
        IPlayerApi& player;
        IInventoryApi& inventory;
        ILoggingApi& logging;
        IConfigApi& config;
    };
} // namespace stalkermp::lua
