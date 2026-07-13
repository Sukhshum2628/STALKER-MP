# Engine Research: Movement Subsystem Reference

This document maps out the verified technical structure and execution model of the X-Ray navigation and movement manager subsystems within `xray-monolith`.

---

## 1. Subsystem Cross-Reference

### `MovementManager` (`CMovementManager`)
- **Defined In:**
  - Header: [movement_manager.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/movement_manager.h) (Line 85)
  - CPP: [movement_manager.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/movement_manager.cpp)
- **Used By:** `CCustomMonster` (Line 46), `CAI_Stalker` (Line 106)
- **Calls:** `level_path().update()`, `detail_path().update()`, `restrictions().update()`
- **Called By:** `CCustomMonster::UpdateCL()`, `CAI_Stalker::shedule_Update()`
- **Writes:** Target velocity vectors, desired look orientation, entity position transforms (`XFORM()`).
- **Reads:** Level graph nodes, restriction zones, player coordinates, environment collisions.
- **Dependencies:** `CLevelPathManager`, `CDetailPathManager`, `CPatrolPathManager`, `CRestrictedObject`
- **Thread:** Primary Thread (scheduled execution)
- **Lua Exports:** Exported to Lua binding tables for script-controlled target points.
- **Virtual Functions:** `on_travel_point_change()`, `on_restrictions_change()`, `update()`

### `DetailPathManager` (`CDetailPathManager`)
- **Defined In:**
  - Header: [detail_path_manager.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/detail_path_manager.h) (Line 17)
  - CPP: [detail_path_manager.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/detail_path_manager.cpp)
- **Used By:** `CMovementManager` (as member `m_detail_path_manager`)
- **Calls:** Path smoothing algorithms, raycasts against static geometry via `xrCDB`
- **Called By:** `CMovementManager::update_path()`
- **Writes:** Segment coordinates, travel point lists, velocity directions.
- **Reads:** Level path nodes, physical obstacles list.
- **Dependencies:** `xrCDB` (bv-tree query engine)
- **Thread:** Primary Thread
- **Lua Exports:** None (internal navigation helper)
- **Virtual Functions:** None

---

## 2. Movement Call Chain Trace

When the engine schedules a movement tick, the execution flows as follows:

1. **`CAI_Stalker::shedule_Update`** [ai_stalker.cpp:1324]
   - Calls `movement().update(update_delta)`
2. **`stalker_movement_manager_base::update(u32 time_delta)`** [stalker_movement_manager_base.cpp:623]
   - Calls `update(m_current)`
3. **`stalker_movement_manager_base::update(stalker_movement_params& movement_params)`** [stalker_movement_manager_base.cpp:634]
   - Calls `update_path()`
4. **`CMovementManager::update_path()`** [movement_manager.cpp]
   - Builds paths using `CDetailPathManager::build_path` if target location changed.
5. **`CDetailPathManager::update()`** [detail_path_manager.cpp]
   - Updates target waypoint index and computes the direction vector towards the current node.
6. **`CCharacterPhysicsSupport::in_UpdateCL()`** [CharacterPhysicsSupport.cpp]
   - Copies displacement vector into the physical movement shell.
7. **`CPHCharacter::SetPosition()`** / **`CPHCharacter::SetVelocity()`** [xrPhysics]
   - Actually writes updated coordinates to the rigid character controller.

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Target positions, path queries, and speed modifiers.
- **Unsafe To Hook:** Frame-by-frame waypoint traversal or raw CDB collision queries.
- **Engine Rewrite Required?** No. Can intercept `desired_position()` updates to verify movement legality on the server.
- **Lua Accessible?** Yes, paths and movement targets can be set in scripts.
- **DLL Modification Required?** Yes, in `xrGame.dll` to prevent speedhacking or teleportation.
- **Risk Level:** High (crucial for movement cheat protection).
