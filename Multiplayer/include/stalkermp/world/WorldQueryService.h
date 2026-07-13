// STALKER-MP — World query service (Sprint-002, §7.5/7.6/7.11)
//
// Implements the entity and spatial query interfaces over an injected
// IEntitySource, and counts queries for diagnostics (§7.11/7.13).
//
// Ownership: owned by the ServiceRegistry; holds a non-owning reference to
// the entity source (owned by the Bootstrap runtime).
//
// Threading: engine main thread only (queries walk live engine state
// through the source; cross-thread access arrives with RFC-0003 snapshots).

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/world/IEntitySource.h"
#include "stalkermp/world/ISpatialQueries.h"
#include "stalkermp/world/IWorldQueries.h"

namespace stalkermp::world
{
    struct QueryStatistics
    {
        std::uint64_t entityQueries = 0;
        std::uint64_t spatialQueries = 0;
    };

    class WorldQueryService final : public core::IService,
                                    public IWorldQueries,
                                    public ISpatialQueries
    {
    public:
        explicit WorldQueryService(const IEntitySource& source);

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "WorldQueries"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {"World"}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- IWorldQueries ---------------------------------------------------
        [[nodiscard]] bool EntityExists(EntityId id) const override;
        [[nodiscard]] std::optional<EntityView> GetEntity(EntityId id) const override;
        [[nodiscard]] std::optional<EntityView> FindByID(EntityId id) const override;
        [[nodiscard]] std::optional<EntityView> FindEntity(std::string_view name) const override;
        [[nodiscard]] std::vector<EntityView>
        GetEntities(const std::function<bool(const EntityView&)>& predicate) const override;

        // --- ISpatialQueries ---------------------------------------------------
        [[nodiscard]] std::vector<EntityView> QueryRadius(const Vec3& center, float radius) const override;
        [[nodiscard]] std::vector<EntityView> QueryBox(const Vec3& minimum, const Vec3& maximum) const override;
        [[nodiscard]] std::optional<Vec3> PositionOf(EntityId id) const override;
        [[nodiscard]] std::optional<EntityView> NearestEntity(const Vec3& center) const override;

        [[nodiscard]] QueryStatistics Statistics() const noexcept { return m_statistics; }

    private:
        const IEntitySource& m_source; // Non-owning.
        mutable QueryStatistics m_statistics{};
    };
} // namespace stalkermp::world
