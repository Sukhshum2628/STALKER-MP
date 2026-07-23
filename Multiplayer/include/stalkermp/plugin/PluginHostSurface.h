// STALKER-MP — Plugin host-service exposure seam (Sprint-014, Step 9)
//
// The engine-free surface through which plugins are EXPOSED the host's gameplay services.
// Per the frozen [AR-P3] Option A decision, this exposes ONLY the reused Sprint-013
// gameplay Public API facades (World / Player / Entity / Environment / Inventory / Logging
// / Config). Administration, tooling, editor, server-information, command-registration,
// and diagnostics surfaces are DEFERRED — they are not part of this seam (they are absent
// from the service-type vocabulary, so filtering is enforced by the type system).
//
// This step defines the SEAM ONLY: it exposes the available gameplay services and their
// immutable metadata catalog, with deterministic lookup. It performs NO service execution,
// no engine call, no plugin callback, and no wiring — the real engine facades are added at
// Step-17 and the binding into a plugin is Step-12.
//
// Value-oriented; deterministic. Engine-free / platform-free / OS-free. ADR-007: no
// exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/lua/ScriptApi.h" // lua::ScriptApiSet (reused gameplay facades, [AR-P3] Option A)

namespace stalkermp::plugin
{
    // The gameplay host-service categories exposed to plugins ([AR-P3] Option A). Fixed
    // std::uint8_t layout; reserved order; administration/tooling are intentionally absent.
    enum class PluginServiceType : std::uint8_t
    {
        World = 0,
        Player,
        Entity,
        Environment,
        Inventory,
        Logging,
        Config,
    };

    [[nodiscard]] constexpr const char* PluginServiceTypeName(const PluginServiceType t) noexcept
    {
        switch (t)
        {
        case PluginServiceType::World:       return "World";
        case PluginServiceType::Player:      return "Player";
        case PluginServiceType::Entity:      return "Entity";
        case PluginServiceType::Environment: return "Environment";
        case PluginServiceType::Inventory:   return "Inventory";
        case PluginServiceType::Logging:     return "Logging";
        case PluginServiceType::Config:      return "Config";
        }
        return "Unknown";
    }

    // The count of gameplay service categories exposed under Option A.
    inline constexpr std::size_t kGameplayServiceCount = 7;

    // Immutable metadata describing one exposed host service (value-only).
    struct PluginServiceInfo
    {
        PluginServiceType type = PluginServiceType::World;
        const char* name = "";  // stable identifier
        bool available = false; // whether the service is exposed on this surface
    };

    // The host-service exposure seam plugins consume. Deterministic; value-oriented; no
    // service execution here.
    class IPluginHostSurface
    {
    public:
        virtual ~IPluginHostSurface() = default;

        // The reused gameplay facade bundle (the actual services; NOT invoked here — this
        // only exposes them for a later step to hand to a plugin).
        [[nodiscard]] virtual lua::ScriptApiSet Services() const = 0;

        // Deterministic catalog of the exposed gameplay services, ascending by type.
        [[nodiscard]] virtual std::vector<PluginServiceInfo> Catalog() const = 0;

        // Deterministic lookup: whether `type` is available on this surface.
        [[nodiscard]] virtual bool IsAvailable(PluginServiceType type) const = 0;
    };

    // The default/test gameplay host surface: exposes the seven gameplay facades. Each
    // service's availability is configurable (all available by default); the catalog is
    // always the seven gameplay types in ascending order (admin/tooling never appear).
    class GameplayHostSurface final : public IPluginHostSurface
    {
    public:
        explicit GameplayHostSurface(lua::ScriptApiSet services) noexcept : m_services(services) {}

        // Mark a gameplay service available/unavailable (default: all available).
        void SetAvailable(PluginServiceType type, bool available) noexcept
        {
            m_available[static_cast<std::size_t>(type)] = available;
        }

        [[nodiscard]] lua::ScriptApiSet Services() const override { return m_services; }

        [[nodiscard]] std::vector<PluginServiceInfo> Catalog() const override
        {
            std::vector<PluginServiceInfo> out;
            out.reserve(kGameplayServiceCount);
            for (std::size_t i = 0; i < kGameplayServiceCount; ++i)
            {
                const auto t = static_cast<PluginServiceType>(i);
                out.push_back(PluginServiceInfo{t, PluginServiceTypeName(t), m_available[i]});
            }
            return out; // ascending by type (enum order)
        }

        [[nodiscard]] bool IsAvailable(const PluginServiceType type) const override
        {
            return m_available[static_cast<std::size_t>(type)];
        }

    private:
        lua::ScriptApiSet m_services;
        bool m_available[kGameplayServiceCount] = {true, true, true, true, true, true, true};
    };
} // namespace stalkermp::plugin
