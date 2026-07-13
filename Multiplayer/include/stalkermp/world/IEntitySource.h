// STALKER-MP — Entity source seam (Sprint-002, §7.5/7.6)
//
// The seam between world queries and whatever provides entities:
//   Sprint-002: EngineEntitySource wraps the engine's live object list.
//   Sprint-003: the authoritative Entity Registry implements this
//               interface; the query API above it does not change.
//
// Implementations expose only what their backing store already knows —
// no registry semantics, no ownership, no lifetime management here.

#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "stalkermp/world/EntityView.h"

namespace stalkermp::world
{
    class IEntitySource
    {
    public:
        virtual ~IEntitySource() = default;

        [[nodiscard]] virtual std::size_t Count() const = 0;

        // Invokes visitor for every entity. Enumeration order is the
        // backing store's order; callers must not depend on it.
        virtual void Enumerate(const std::function<void(const EntityView&)>& visitor) const = 0;

        [[nodiscard]] virtual std::optional<EntityView> FindById(EntityId id) const = 0;
        [[nodiscard]] virtual std::optional<EntityView> FindByName(std::string_view name) const = 0;
    };
} // namespace stalkermp::world
