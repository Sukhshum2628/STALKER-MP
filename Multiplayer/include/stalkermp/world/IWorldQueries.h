// STALKER-MP — Entity query interface (Sprint-002, §7.5)
//
// Read-only entity lookup. No multiplayer logic, no mutation.

#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "stalkermp/world/EntityView.h"

namespace stalkermp::world
{
    class IWorldQueries
    {
    public:
        virtual ~IWorldQueries() = default;

        [[nodiscard]] virtual bool EntityExists(EntityId id) const = 0;
        [[nodiscard]] virtual std::optional<EntityView> GetEntity(EntityId id) const = 0;
        [[nodiscard]] virtual std::optional<EntityView> FindByID(EntityId id) const = 0; // Alias of GetEntity (spec §7.5).
        [[nodiscard]] virtual std::optional<EntityView> FindEntity(std::string_view name) const = 0;

        // All entities satisfying the predicate (nullptr = all entities).
        [[nodiscard]] virtual std::vector<EntityView>
        GetEntities(const std::function<bool(const EntityView&)>& predicate) const = 0;
    };
} // namespace stalkermp::world
