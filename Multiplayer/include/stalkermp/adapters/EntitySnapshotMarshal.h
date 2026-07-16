// STALKER-MP — Entity snapshot marshaling helper (Sprint-008, Step 9)
//
// The engine-free, value-only core of the entity capture seam. The engine
// adapter (adapters::EngineEntitySnapshotSource, EngineAdapters.cpp — the one
// engine TU) reads per-object transform/state on demand into plain
// snapshot::EntitySnapshot VALUES (no engine pointer retained, no engine state
// mutated) and hands that raw list here; this helper produces the deterministic,
// ascending-by-EntityId, unique, id!=0 result and APPENDS it to `out` (append-
// only — never clears pre-existing entries), exactly as world::IEntitySnapshotSource
// requires and as SnapshotBuilder::AddEntity consumes.
//
// Placing the ordering/value contract here keeps engine code thin and makes the
// contract unit-testable without the engine (the engine capture itself is smoke-
// tested on Windows). No engine header is included; no pointer or engine type
// crosses this boundary.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI.

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "stalkermp/snapshot/SnapshotTypes.h"

namespace stalkermp::adapters::detail
{
    // Deterministically append `raw` entity VALUES to `out`: sorted ascending by
    // EntityId, dropping id 0 (the reserved none) and any duplicate id (keeping the
    // first occurrence in ascending order). `raw` is taken by value-mutable ref so
    // it can be sorted in place; only value copies are appended — no references to
    // any engine object survive this call. Existing entries in `out` are preserved.
    inline void AppendAscendingUnique(std::vector<snapshot::EntitySnapshot>& out,
                                      std::vector<snapshot::EntitySnapshot>& raw)
    {
        std::sort(raw.begin(), raw.end(),
                  [](const snapshot::EntitySnapshot& a, const snapshot::EntitySnapshot& b) noexcept {
                      return a.id.value < b.id.value;
                  });

        std::uint32_t lastAppended = 0; // 0 can never be appended, so a safe sentinel
        for (const snapshot::EntitySnapshot& entity : raw)
        {
            if (entity.id.value == 0)
            {
                continue; // reserved none
            }
            if (entity.id.value == lastAppended)
            {
                continue; // duplicate id (raw is sorted, so equals cluster) -> keep first
            }
            out.push_back(entity); // value copy only
            lastAppended = entity.id.value;
        }
    }
} // namespace stalkermp::adapters::detail
